# 内存序补充：挂钩、以及 acq/rel 为什么还能 (0, 0)

主文档：[`memory_order.md`](memory_order.md)  
仓库示例：[`relaxed.cpp`](../concurrent/memory_order/relaxed.cpp)、[`release_acquire.cpp`](../concurrent/memory_order/release_acquire.cpp)

主文档讲术语。这里只补：

1. 多线程之间怎样才算 **挂钩**（同步于）。
2. 实验 B 那种 `(r1, r2) == (0, 0)`：acq/rel **可以出现**，不是「有人先写了就会同步」。
3. **先序于**和 **携带依赖** 差在哪：两个无关变量有没有这种关系。
4. **`volatile` 干什么**、和内存序 / 多线程的关系。

标签和主文档一样：没写 T1/T2 的就是同一个线程。

---

## 1. 挂钩是什么【只在跨线程】

挂钩 = **同步于（synchronizes-with）**。单线程里没有这个概念，先序于就够了。

一次挂钩必须同时满足：

1. **同一个**原子变量（不能 x 上写、y 上读就当挂上了）。
2. 写的那侧是 **release**（或更强：`acq_rel` / `seq_cst`）。
3. 读的那侧是 **acquire**（或更强：`acq_rel` / `seq_cst`；`consume` 是窄挂钩，见下）。
4. 这次 acquire **读到了**那次 release 写进去的值（或这条 release sequence 上更后面的值）。

缺第 4 条就不算挂上。读到初始 0、读到别人 relaxed 写的无关值，都没挂钩。

```cpp
// ========== 线程 T1 ==========              // ========== 线程 T2 ==========
data = 42;                                    // 普通写，先序于下面的 store
flag.store(1, release);                       while (flag.load(acquire) != 1) {}
                                              // 读到了 1 → T1 的 store 同步于这次 load
                                              assert(data == 42);  // 捎带过来了
```

挂钩拴在 `flag` 这一对上。T1 里 release **之前**的写（`data = 42`），对 T2 里 acquire **之后**的读可见。

---

## 2. 哪些内存序能挂钩

| 写（T1） | 读（T2） | 读到 T1 的值之后 | 挂钩？ |
|----------|----------|------------------|--------|
| `relaxed` | 任意 | 只看见这个原子自己 | **无** |
| 任意 | `relaxed` | 只看见这个原子自己 | **无** |
| `release` | `acquire` | T1 在 store 前的写，T2 在 load 后都可见 | **有**（完整挂钩） |
| `release` | `consume` | 只有「地址/值来自这次 load」的读可见（`*p2`） | **窄挂钩** |
| `acq_rel`（CAS / `fetch_add`） | `acquire` 或 `acq_rel` | 同 release-acquire | **有** |
| `seq_cst` store | `seq_cst` load | 也有同步于；另外所有 seq_cst 操作还在一条全局轴上 | **有，且更强** |

`seq_cst` 能挂钩，是因为它 **包含** release/acquire 的同步于，不是因为「全局时间轴」本身。全局轴多出来的是：没读到对方值时，也不允许实验 B 那种两边都是 0。

---

## 3. 怎么挂上：读者必须自旋到「看见那个值」

光 `store(release)` 不会通知别的线程。别的线程必须 **acquire 读到非初始值**。

```cpp
// ========== 线程 T1 ==========              // ========== 线程 T2 ==========
payload = 1;
ready.store(true, release);                   if (ready.load(acquire))   // 只看一次
                                                  use(payload);           // 可能还是 false，没挂上

                                              while (!ready.load(acquire)) {}  // 等到 true
                                              use(payload);                     // 这时才挂上
```

`if (load == true)` 成功才挂钩；失败就是读到了 `false`，和 T1 没关系。

---

## 4. 实验 B：(0, 0) 在 acq/rel 里确定能出现

主文档第 8 节那个形状：

```cpp
// ========== 线程 T1 ==========              // ========== 线程 T2 ==========
x.store(1, release);                          y.store(1, release);
r1 = y.load(acquire);                         r2 = x.load(acquire);
```

`r1`、`r2` 各自只能是 0 或 1。seq_cst 下 **不能同时为 0**。acq/rel 下四种都可以，包括 `(0, 0)`。

| `r1` | `r2` | seq_cst | acq/rel |
|------|------|---------|---------|
| 0 | 0 | 禁止 | **允许** |
| 0 | 1 | 允许 | 允许 |
| 1 | 0 | 允许 | 允许 |
| 1 | 1 | 允许 | 允许 |

### 不是「有一方先写就会同步」

挂钩条件第 4 条：acquire 必须读到那次 release 的值。

- `r1==0`：T1 读 `y` 读到的是初始 0，**不是** T2 的 `y.store(1)` → `y` 上没挂钩。
- `r2==0`：T2 读 `x` 读到的是初始 0，**不是** T1 的 `x.store(1)` → `x` 上没挂钩。
- 两边都是 0 = **一次同步于都没有**。

「先写」是按 seq_cst 那条全局时间轴在想。acq/rel 没有这条轴：两个人都可以先把 store 丢进 store buffer，再去 load 对方，此时对方的写还不可见。x86 上 acq/rel 就是普通 `mov`，`(0, 0)` **能打出来**；seq_cst 加全屏障把 store buffer 刷掉，所以禁止。

这里还踩了另一条：挂钩必须在 **同一个**原子上。T1 写的是 `x`、读的是 `y`，T2 写的是 `y`、读的是 `x`。没有「读到对方刚写的那个变量的新值」，两根线对不齐。

---

## 5. 对照：怎样才算挂上，怎样只是看见了这个原子

```cpp
// 挂上了【跨线程，同一个 flag】
flag.store(1, release);                 v = flag.load(acquire);  // v==1
// T1 在 store 前写的 data，T2 在 load 后看得见

// 没挂上：读到了值，但是 relaxed
flag.store(1, release);                 v = flag.load(relaxed);  // 哪怕 v==1
// 只保证 flag 这个原子走到了 1；data 不保证

// 没挂上：acquire 了，但读到的是 0
flag.store(1, release);                 v = flag.load(acquire);  // v==0
// 看见的是初始值，不是那次 store

// 没挂上：两个变量交叉看
x.store(1, release);                    r = y.load(acquire);
y.store(1, release);                    s = x.load(acquire);
// 两边都可能是 0；这不是一对 release-acquire

// 没挂上：load 用了 acquire，store 没用 release（下面第 5.1 节）
flag.store(1, relaxed);                 v = flag.load(acquire);  // 哪怕 v==1
```

### 5.1 load 用了 acquire，store 没用 release

挂钩要**两侧配对**。一边 acquire、另一边不是 release（也不是 `acq_rel` / `seq_cst`），读到值也没挂钩。

```cpp
// ========== 线程 T1 ==========              // ========== 线程 T2 ==========
data = 42;
flag.store(1, relaxed);                       while (flag.load(acquire) != 1) {}
                                              assert(data == 42);  // 不保证
```

T2 一定能看见 `flag==1`（这个原子的修改序）。  
`data` 不保证：T1 的 store 不是 release，没把前面的写发布出去；T2 的 acquire 找不到对侧的 release，同步于建立不了。

acquire 只约束 **T2 自己**：这次 load 之后的读不能挪到 load 前面。没有对侧 release，这个约束捎带不到 T1 的 `data`。

反过来一样：`store(release)` + `load(relaxed)`，哪怕读到 1，也只看见 `flag`，看不见 `data`。

注意：`flag.store(1)` **不写序时默认是 `seq_cst`**，里面已经包含 release，这时和 `load(acquire)` 是能挂钩的。只有你显式写成 `relaxed`（或 store 用了不能当 release 的序）才会踩这个坑。

`store` 不能标 `acquire` / `acq_rel`（那是 load / RMW 的序），标了是未定义行为。

---

---

## 6. 其它能挂钩的写法

### CAS / `fetch_add`：不是「用 acq_rel 实现的」

它们是 **RMW**（读改写，整次原子），内存序是**你传入的参数**，不是内置死成 `acq_rel`。

```cpp
a.fetch_add(1);                            // 默认 seq_cst
a.fetch_add(1, relaxed);                   // 只保证这个计数不撕、不丢
a.fetch_add(1, acq_rel);                   // 这次既 acquire 又 release
a.compare_exchange_strong(e, d);           // 默认成功/失败都是 seq_cst
a.compare_exchange_strong(e, d, acq_rel);  // 成功时 acq_rel
a.compare_exchange_strong(e, d, acq_rel, acquire); // 成功 acq_rel，失败只 acquire（失败没有写，不能标 release）
```

| 你写的序 | 这次 RMW 当什么 |
|----------|-----------------|
| `relaxed` | 只动这一个原子，不挂钩 |
| `acquire` | 读半边挂钩（看见别人 release 过的写）；本线程这次写不发布 |
| `release` | 写半边挂钩（发布本线程前面的写）；这次读不看别人） |
| `acq_rel` | 两边都挂钩：读是 acquire，写是 release |
| `seq_cst`（默认） | 含 acq_rel，再加全局一条轴 |

无锁队列的 `head`/`tail` **常常选** `acq_rel`：改指针和发布节点一次完成。那是选型，不是 CAS 的实现方式。

```cpp
old = head.load(acquire);
head.compare_exchange_strong(old, next, acq_rel);  // 成功才既看见旧节点，又发布新的
```

失败的 CAS **没有写**，所以失败序不能是 `release` / `acq_rel`。

### `seq_cst`

读到对方的值时，同样建立同步于（挂钩效果不低于 acq/rel）。  
多出来的是：即使 **没读到** 对方，所有 seq_cst 操作仍排在一条全局轴上，所以 Dekker 那种「两人各写自己的旗标再读对方」不允许都看见 0。旗标 + 数据不必上 seq_cst，acq/rel 就够。

### `consume`（窄挂钩，新代码不要用）

`release` + `consume` 且读到了值：只保证顺着这次 load 返回值走的读（`*p2`），不保证旁路的 `data`。编译器现在几乎都把 consume 当 acquire。日常发布指针用 acquire。

### 互斥锁

`unlock` 同步于下一次 `lock`。锁里的普通读写，靠的是这副挂钩，不必自己标 memory_order。

---

## 7. 先序于 vs 携带依赖【仅单线程】

两条都只在**同一个线程**里谈。携带依赖是先序于的**加严**：A 必须先序于 B，并且 B 的数据真的来自 A。

```text
先序于（sequenced-before）     源码里先 A 后 B → 本线程永远不能观察到颠倒
携带依赖（carries-dependency） 先序于 且 下面三条至少一条成立
```

A 先序于 B 时，若任一条件为真，A **携带依赖**到 B：

**规则 1：** A 的值是 B 的操作数。

```cpp
// ========== 同一个线程 ==========
int a = x.load();   // A
int b = a + 1;      // B  用了 a → A 携带依赖到 B
int c = *p2;        // 若 p2 来自某次 load P，则 P 携带依赖到这次解引用
```

两个例外，不算携带依赖：

```cpp
int a = x.load();                          // A
int b = std::kill_dependency(a) + 1;       // 人为掐断：A 不携带依赖到后面
int d = (a && foo());                      // a 是 && 的左操作数：控制依赖，不算
int e = (a ? foo() : bar());               // a 是 ?: 的左操作数：同上
int f = (a, foo());                        // a 是逗号的左操作数：值被丢掉
```

`&&` / `?:` **会看** `a` 的值，但那是决定走哪条路，不是把 `a` 送进右边当数据。标准把这种叫控制依赖，**明确排除**出携带依赖。

```cpp
int k = foo(a);        // 有携带依赖：a 的位真的进了 foo 的参数
int d = (a && foo());  // 无携带依赖：foo() 的参数列表里没有 a，a 只决定 foo 调不调
int e = (a ? foo() : bar());  // 同上：a 只选哪一个函数，两个函数都没用 a 当操作数
```

`a && foo()` 里，`a == 0` 就不算 `foo()`；`a != 0` 才算。右边拿到的不是 `a` 这个数。  
CPU 可以预测分支，在 `a` 算完之前就把 `foo()` 里的访存做了，所以 consume **不能**顺着这条链捎带。  
`foo(a)` 不行：没拿到 `a` 就没有实参，这才是数据依赖。

「要先读 a 再决定」保证的是 **先序于**，不是携带依赖。关系没有写反：

```text
携带依赖  ⇒  先序于     （有数据流，一定已经先序于）
先序于    ≠   携带依赖   （先算完 a 再走分支，仍然可以没有数据流）
```

| | `foo(a)` | `a && foo()` |
|--|--|--|
| 先序于 | 有（先算出 a，再调 foo） | **有**（抽象机必须先算出 a，再决定调不调 foo） |
| 携带依赖 | **有** | **无**（标准把 && 左操作数排除了） |

所以 `&&` / `?:` 恰恰是「有先序于、无携带依赖」的例子，和写 `N` 再读 `M` 是同一类缺口，不是定义互相矛盾。

`||` 同 `&&`。逗号 `(a, foo())` 更干脆：先算 `a` 再丢掉，整个表达式的值只是 `foo()`。先序于仍有，携带依赖没有。

**规则 2：** A 写入标量 **M**，B 从 **同一个 M** 读。

```cpp
// ========== 同一个线程 ==========
M = 1;          // A 写 M
int r = M;      // B 读 M → A 携带依赖到 B

N = 1;          // 写的是 N
int s = M;      // 读的是 M → 无关变量，没有携带依赖（仍有先序于）
```

先序于 **不是**「CPU 必须按这个顺序执行」。它管的是**抽象机 / 本线程自己的观察**：

- 本线程后来再读 `N`，一定能看见这次写成的 1（自己不能看见自己的写还没发生）。
- `N` 和 `M` 没有数据依赖，**CPU 可以把 `load M` 发到 `store N` 前面**。别的线程可以先看见 `M` 被读、却还没看见 `N==1`。

有先序于、没有携带依赖，意思就是：源码顺序仍在，但没有那条「值从 A 流到 B」的链，硬件乱序、别的线程反着看见，都是允许的。主文档第 1 节「先 `store x` 再 `store y`」是同一件事。

CPU 乱序和内存序 **有关系，但不是一回事**：

| | 管什么 |
|--|--|
| 内存序（抽象机） | **允许被看见什么**。先序于、同步于、happens-before 都在这一层 |
| CPU 乱序 / store buffer / 编译器重排 | **为什么** relaxed 时别的线程能看见乱序（物理原因） |

标 `acquire` / `release` / `seq_cst` 时，编译器会插屏障或换指令，**限制**这些乱序对外可见。所以不是「和乱序没关系」，而是：先序于不管死 CPU 流水线；内存序通过禁止某些可见结果，反过来约束乱序能不能被别的线程看见。

**规则 3：** 传递。A 携带依赖到 X，X 再携带依赖到 B → A 也携带依赖到 B。

```cpp
// ========== 同一个线程 ==========
int a = x.load(); // A
int t = a + 1;    // X  规则 1：A → X
int b = t;        // B  规则 1：X → B，于是规则 3：A → B
int c = *p2;      // 若 p2 来自 load P，则 P → 取地址 → *p2，同样传递
```

consume 跨线程只顺着这条链捎带；acquire 不管有没有携带依赖，load 之后全都能看见。

### 两个无关变量：有先序于，没有携带依赖

「无关」= 没有把一个的值送进另一个（也不是对**同一个**标量先写后读）。

```cpp
// ========== 同一个线程 ==========
int a = x.load(consume); // A  读的是 x
int b = y.load(relaxed); // B  读的是 y，没用到 a
int c = data;            // C  读的是另一个对象 data
```

| 关系 | A → B（x 和 y） | A → `*p2` | A → `data` |
|------|-----------------|-----------|------------|
| 先序于 | **有**（同一线程，先 A 后 B） | 有 | 有 |
| 携带依赖 | **无**（B 没用 A 的值；也不是同一标量先写后读） | **有**（`*p2` 的地址就是 A 读出来的指针） | **无**（`data` 和指针无关） |

```cpp
p2 = ptr.load(consume); // A
v  = *p2;               // 用到了 p2 → A 携带依赖到这次读。consume 保证看见堆上的 "Hello"
d  = data;              // 没用到 p2 → 只有先序于，没有携带依赖。consume 不保证 data==42
```

规则 2 也要求 **同一个 M**：`x = 1` 再 `r = x` 有携带依赖；`x = 1` 再 `r = y` 没有。

把无关的两个值接起来，才会有：

```cpp
int a = x.load();  // A
int b = a + 1;     // B  用了 a → A 携带依赖到 B（规则 1）
int c = b;         // C  规则 3 传递：A 也携带依赖到 C
```

`if (p2) d = data;` 里 `data` 仍然没用到 `p2` 的值当操作数，只有控制依赖，**不是**携带依赖（和 `&&` / `?:` 左操作数那条例外是一类意思）。

### 在内存序里干什么用

**只给 `consume` 用。** acquire / release / seq_cst / relaxed 都不看携带依赖。

consume 的挂钩比同步于窄：T1 的 release 被 T2 的 consume 读到之后，T1 在 store 前的写，只对 T2 里 **consume 携带依赖到的那些读** 可见。携带依赖就是那根「哪些后续操作算在这条窄链上」的尺子。

```text
T1:  写 *p / 写 data   ──先序于──►  ptr.store(p, release)
                                      │
                                      │  T2 读到了这个 p
                                      ▼
T2:  p2 = ptr.load(consume)  ──携带依赖──►  *p2     可见
                         └──仅先序于──►  data    不可见（consume 不认）
```

| 内存序 | 读到 release 的值之后，T2 里谁能看见 T1 之前的写 |
|--------|--------------------------------------------------|
| `relaxed` | 谁都不保证（不挂钩） |
| `consume` | 只有 consume **携带依赖到** 的那些（`*p2`、`p2->left`） |
| `acquire` | load **之后全部**（不看携带依赖，`data` 也行） |

没有携带依赖，标准就没法写出「保证 `*p2`、不保证 `data`」；acquire 不需要这根尺子，因为它不挑。  
`std::kill_dependency(p2)` 就是人为把尺子剪断，后面的读不再吃 consume 的捎带，编译器可以重排。

新代码仍用 acquire。编译器现在几乎把 consume 当 acquire，携带依赖在纸面上才有意义。

---

## 8. `volatile` 干什么【先看单线程；标准语义不挂钩】

`volatile` 管的是 **编译器别把这次访问优化掉**，不是原子，也不建立同步于。

该用的地方：MMIO 寄存器、同一线程里和 `std::signal` 处理函数通信（`sig_atomic_t`）。  
不该用的地方：当 `std::atomic` 用、当旗标发布 `data`。

### 那段 cppreference 在说什么

### 「可观察副作用」直白说

**副作用** = 不只是脑子里算了个数，还改了「外面的世界」（写了内存、写了设备、打了字）。  
**可观察** = 从程序外面能察觉到这次改动，编译器就不能说「反正你看不出来，我偷懒不做了」。

标准里算「外面能察觉」的，主要就这几类：

| 算可观察 | 不算（编译器可以偷懒） |
|----------|------------------------|
| 读/写 `volatile` 变量 | 普通局部变量 `int x = 1;` 后面再也没用 |
| `printf` / 写文件 / 读键盘 | 普通全局 `data = 42`（没有 volatile、没有 I/O） |
| 程序结束、向环境交卷 | 编译器能证明谁也看不到的中间计算 |

```cpp
int x = 1;          // 若后面不用 x，编译器可以整段删掉：外面看不见
volatile int v = 1;
v = 2;              // 必须真写两次：也许是设备寄存器，外面在盯着
printf("hi");       // 必须真打印：屏幕上能看见
```

所以那段话里的「volatile 不能和可观察副作用乱排」，换成大白话就是：

**本线程里，该真发生的事（volatile 读写、打印、写文件）必须按源码顺序真的发生；编译器不能把它们对调或删掉。**  
旁边那个普通的 `data = 42` 不在「外面能看见」的清单里，所以可以挪到 volatile 后面。

别的线程盯着内存看，**不算**这里的「可观察」。那是同步于的事，volatile 不负责。

**第二段：两件它不做的事。**

1. **不是原子**：两个线程一个写一个读同一个 `volatile int`，仍是数据竞争（未定义行为）。要无撕、无竞争，用 `std::atomic`。
2. **不做内存排序**：旁边的**非** volatile 访问可以绕过它重排。

```cpp
// ========== 同一个线程 ==========
data = 42;                 // 普通写，不是可观察副作用
v = 1;                     // volatile 写
// 标准允许编译器把 data=42 挪到 v=1 后面
```

所以就算本线程里 `v` 的访问排好了，也**不能**指望它把前面的 `data` 捎给别的线程。这点和 `relaxed` 原子一样弱在「不挂钩」，但比 relaxed 更差：连原子性都没有。

**第三段：MSVC 是例外，不是标准。**

默认 `/volatile:ms`：每次 volatile **写当 release、读当 acquire**，所以在 MSVC 默认设置下可以用它做线程间挂钩。  
`/volatile:iso` 才跟标准走。标准 volatile **不够**做多线程；够用的是同一线程的 signal 处理函数读 `volatile sig_atomic_t`。

| | 标准 `volatile` | MSVC 默认 `volatile` | `std::atomic` + acquire/release |
|--|--|--|--|
| 编译器必须真的访问 | 是 | 是 | 是（原子操作） |
| 原子、无数据竞争 | **否** | 硬件上往往碰巧是 | **是** |
| 跨线程挂钩 | **否** | 写=release，读=acquire | **是**（你选的序） |
| 捎带旁边的 `data` | **否**（普通写可绕过它） | 近似 acq/rel | **是** |
| 典型用途 | MMIO、同线程 signal | 旧 Windows 代码 | 多线程旗标 + 数据 |

多线程请用 `atomic`。不要把 Java 的 `volatile`（有 acq/rel）和标准 C++ 的 `volatile` 当成同一个词。

---

## 口诀

1. 挂钩 = 同步于，只在两个线程、**同一个**原子上。
2. 写 release（或更强），读 acquire（或更强），**并且读到了那个值**。
3. 读到 0 / 用 relaxed / 写 x 读 y，都没挂钩。
4. 「有人先写了」不会自动挂钩；那是 seq_cst 全局轴的想法。
5. 只问这一个数、不捎带别的内存 → relaxed，本来就不挂钩。
6. 要捎带 `data` → 挂上（release/acquire）。要「没看见对方也不许两人都以为对方没动」→ seq_cst。
7. 携带依赖只给 **consume** 当尺子：release 的写只捎带到这条链上（`*p2`），不捎带旁路 `data`。acquire 不看它。
8. 标准 `volatile` 只逼编译器别省掉访问；不原子、不挂钩。多线程用 `atomic`。
9. **可观察副作用** = 外面能察觉的改动（volatile、打印、写文件）；普通 `data=42` 不算。
