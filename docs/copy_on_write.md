# Copy-on-Write：读的时候不拷贝，写的时候看有没有人在读

对应代码（教学草稿）：[`concurrent/copy_on_write/copy_on_write.cpp`](../concurrent/copy_on_write/copy_on_write.cpp)

场景：一份全局 `Foo` 列表，读路径 `traverse()` 可能很慢（回调 `doit()`），写路径 `post()` 往列表里追加。若全程持锁遍历，写会被读堵住。做法是 **`shared_ptr` 快照 + 写时拷贝**：读只在锁内拷贝指针，写发现有人拿着旧指针才深拷贝整表。

```cpp
typedef std::vector<Foo> FooList;
typedef std::shared_ptr<FooList> FooListPtr;
MutexLock mutex;
FooListPtr g_foos;  // 使用前需初始化，例如 g_foos = std::make_shared<FooList>();
```

---

## 正确写法：锁内拷指针，锁外遍历；写之前先问 unique

```cpp
void traverse() {
    FooListPtr foos;
    {
        MutexLockGuard lock(mutex);
        foos = g_foos;              // 引用计数 +1，拿到当前列表的快照指针
        assert(!g_foos.unique());   // 现在至少有 g_foos 和 foos 两个指针
    }                               // 锁已释放，写者可以进来

    for (std::vector<Foo>::const_iterator it = foos->begin();
        it != foos->end(); ++it) {
        it->doit();                 // 慢操作不占锁；看到的是一份不会被改的旧列表
    }
}

void post(const Foo& f) {
    printf("post\n");
    MutexLockGuard lock(mutex);
    if (!g_foos.unique()) {
        g_foos.reset(new FooList(*g_foos));  // 有读者正拿着旧表 → 深拷贝，读者不受影响
        printf("copy the whole list\n");
    }
    assert(g_foos.unique());        // 只有自己能改这份 vector
    g_foos->push_back(f);           // 原地追加
}
```

读、写怎么配合：

```text
时刻  g_foos          读者 foos       说明
 1    ptr → listA     （无）          unique == true，post 可原地 push
 2    ptr → listA     ptr → listA     traverse 拷了指针，unique == false
 3    ptr → listB     ptr → listA     post 发现非 unique，拷出 listB 再 push
 4                    （析构）        读者走完，listA 引用计数归零被释放
```

要点：

- **锁保护的是 `g_foos` 这个指针，不是整次遍历。** `foos = g_foos` 必须在锁内，否则和 `post` 里 `reset` / 赋值数据竞争。
- **`unique()` 也必须在同一把锁里看。** `traverse` 增加引用计数、`post` 判断是否该拷贝，都由这把锁串起来。
- **`unique() == true`**：没有读者拿着当前这块 `vector`，原地 `push_back` 安全，避免每次写都拷整表。
- **`unique() == false`**：有人正在（或刚开始）遍历旧表。必须 `new FooList(*g_foos)` 拷一份再改；读者继续走旧 `vector`，迭代器不会失效。
- `assert(!g_foos.unique())` 在 `traverse` 里成立，是因为刚拷过指针；`assert(g_foos.unique())` 在 `post` 里成立，是因为要么本来就 unique，要么刚深拷贝完。

`shared_ptr::unique()` 在 C++17 起被弃用、C++20 删除（标准不保证多线程下当同步原语用）。这里能用，是因为 **所有对 `g_foos` 的拷贝都发生在同一把 mutex 下**；读者局部 `foos` 析构不持锁，最坏是 `unique()` 多看到一次「非 unique」、多拷一次表，不会少拷。

---

## 错误 Case1：锁内直接改共享 vector

```cpp
void post(const Foo& f) {
    MutexLockGuard lock(mutex);
    g_foos->push_back(f);
}
```

`traverse` 已经把指针拷走并 **释放了锁**，然后在锁外 `doit()`。此时 `post` 再 `push_back` 改的是 **同一块 `vector`**：

- 和正在进行的遍历数据竞争
- `push_back` 可能 realloc，读者迭代器失效 → 未定义行为

锁挡不住这件事：读者早就离开临界区了。COW 的前提就是「有人拿着旧指针时，不能原地改」。

---

## 错误 Case2：无锁读旧表，拷完再挂回去

```cpp
void post(const Foo& f) {
    FooListPtr newFoos(new FooList(*g_foos));  // 无锁读 g_foos / 拷贝 vector
    newFoos->push_back(f);
    MutexLockGuard lock(mutex);
    g_foos = newFoos;
}
```

`push_back` 确实打在**新对象** `newFoos` 上，两个 Case2 写者不会在同一块 `vector` 上 `push_back`。错在别处：
1. `new FooList(*g_foos)`。它没有先执行`FooListPtr pin = g_foos`（引用计数 +1），只是解引用全局那个 `shared_ptr`，再去拷里面的`vector`。拷贝过程中旧表往往只有 `g_foos` 自己握着。另一个线程一旦`g_foos = newFoos`，旧表引用计数变成`0`，对象被析构，第一个线程还在按元素拷 → 理论上是`use-after-free`。另外，对同一个全局`g_foos`变量，一边无锁读、一边赋值，`C++`里也算数据竞争。
2. **丢失更新**：两个 `post` 都基于同一份旧表拷贝，各自`push` 一个`Foo`，再先后赋值 `g_foos`。后写的整表覆盖先写的，先那次 `post` 的元素消失。

```text
g_foos → [a,b]
post(c) 拷贝 [a,b] → [a,b,c]
post(d) 拷贝 [a,b] → [a,b,d]
g_foos = [a,b,c]
g_foos = [a,b,d]     // c 丢了
```

---

## 错误 Case3：锁内只快照指针，拷贝和发布不在一个临界区

```cpp
void post(const Foo& f) {
    FooListPtr oldFoos;
    {
        MutexLockGuard lock(mutex);
        oldFoos = g_foos;          // 读指针有锁，Case2 的数据竞争没了
    }
    FooListPtr newFoos(new FooList(*oldFoos));
    newFoos->push_back(f);         // 锁外拷贝，读不阻塞
    MutexLockGuard lock(mutex);
    g_foos = newFoos;              // 无条件覆盖
}
```

读 `g_foos` 有锁，旧表被 `oldFoos` pin 住了，Case2 那种「拷着拷着旧表被删」没有了。**真正还在的问题，仍是两个 `post` 并发会漏数据。**

```text
g_foos → [a,b]

post(c)                      post(d)
锁内 oldFoos = g_foos        锁内 oldFoos = g_foos
         都拿到 [a,b]
锁外拷贝 → [a,b,c]           锁外拷贝 → [a,b,d]
锁内 g_foos = [a,b,c]
                             锁内 g_foos = [a,b,d]   // c 丢了
```

正确 `post` 不是「先拷一份再挂回去」，而是整段写都握着锁：要么确认没人读、原地 `push_back`，要么拷完立刻改、立刻成为新的 `g_foos`。两个 `post` 进不了同一段临界区，第二个看到的已经是第一个 push 过的表，`c` 和 `d` 都会在。

Case3 把「按哪一版来拷」和「挂成当前表」拆成两次加锁，中间对方已经发布了。你第二次进锁还是把基于旧版做出的表整份赋上去，等于无视别人刚写的那个 Foo。

若坚持锁外拷贝，发布前要看 `g_foos` 是不是还等于 `oldFoos`，变了就重试。教学里不如锁内 unique + 原地写简单。

---

## 对照

| | 读 `g_foos` | 改 vector | 并发两个 post | 有读者时 |
|--|--|--|--|--|
| 正确 | 锁内拷指针 / 锁内 unique | unique 才原地写 | 锁串行，不丢元素 | 深拷贝，读者继续走旧表 |
| Case1 | — | 总是原地 `push_back` | 不丢（有锁） | **和遍历数据竞争** |
| Case2 | **无锁** | 拷贝后覆盖 | **丢失更新** | 拷贝本身就有数据竞争 |
| Case3 | 锁内快照指针 | 无条件覆盖 | **丢失更新** | 读者安全，写者互相覆盖 |

一句话：读要的是 **稳定快照**（`shared_ptr` 拷贝），写要的是 **没有别的指针时才原地改，有人在读就先拷**；「拷贝」和「挂回 `g_foos`」必须跟 unique 


据我们测试，大多数情况下更新都是在原本数据上进行的，拷贝的比例还不到1%，准确的说，这不是copy-on-write，应该是copy-on-other-reading.
