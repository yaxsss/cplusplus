# C++ 内存序术语（对照 cppreference）

英文原文：[std::memory_order](https://cppreference.cn/w/cpp/atomic/memory_order)  
仓库示例：[`relaxed.cpp`](../concurrent/memory_order/relaxed.cpp)、[`release_acquire.cpp`](../concurrent/memory_order/release_acquire.cpp)、[`release_consume.cpp`](../concurrent/memory_order/release_consume.cpp)

多核上「你按 1、2、3 写」不等于「别的线程按 1、2、3 看见」。标准用下面这几个词描述**允许看见什么**，不是描述 CPU 电路怎么走。

**先看标签再看代码**（没写「线程 1 / 线程 2」的，就是同一个线程里连续两行）：

| 标签 | 意思 |
|------|------|
| 【仅单线程】 | 只谈这一条线程内部，代码从上到下就是一个人 |
| 【跨线程】 | 必须有至少两个线程；注释里的 `T1`/`T2` 是不同线程 |
| 【单线程 + 跨线程】 | 词本身两条腿：线程内用先序于，线程间用同步于 |

三层分别管什么：

| 层次 | 范围 | 管什么 | 谁保证 |
|------|------|--------|--------|
| 原子性 | 一次操作 | 这一次读写不会撕成半个 `int` | `std::atomic` 本身 |
| 定序 | 先看单线程，再问别的线程看不看得见 | 本线程里 A 和 B 谁先谁后 | `memory_order` |
| 同步 | **只在多线程之间** | 线程 A 的一串写入，能否变成线程 B 的可见副作用 | release 对上 acquire（或 seq_cst） |

`relaxed` 只有原子性；`release`/`acquire` 加上跨线程同步；`seq_cst` 再加「所有 seq_cst 操作像排在一条全局时间轴上」。

---

## 1. 先序于（sequenced-before）【仅单线程】

只在**同一个线程**里谈。下面 A、B 是一个人从上到下执行，没有第二个线程。

```cpp
// ========== 同一个线程 ==========
r1 = y.load(std::memory_order_relaxed); // A
x.store(r1, std::memory_order_relaxed); // B
```

A **先序于** B：这个线程的抽象机里先算 `r1` 再 store。**这个线程自己**永远不能观察到 A、B 颠倒。

**先序于照样只约束这一条线程。** 有 `r1` 这个数据依赖时，**本线程**不能先 `store` 再 `load`：没读出 `r1` 就没有可写进 `x` 的值。这点和「先 `x=1` 再 `y=1`」那种两次独立 store 不一样。

**别的线程仍然没有 happens-before。** 原因是：

A 是 **load**，不往内存「发布」什么，别的线程看不见「A 发生了」。别的线程最多看见 B：`x` 变成了某个数。  
看见 `x==42` **不能**推出「这个线程已经按顺序读完 `y`、并且我对 `y` 的看法和它一致」。中间没有同步于。

所以「别的核没有义务按 A→B 看见这两下」在这里更准确的意思是：

- 不是「别的线程会看到 x 先被写成一个本线程还没 load 出来的值」（那种本线程自己也做不到）；
- 而是「别的线程不会因为你源码里先 A 后 B，就和你在 `y` / `x` 上排好序」。

cppreference 里 `r1 == r2 == 42` 乱的是 **T2**：它可以把 `y.store(42)` 挪到 `x.load` 前面。T1 仍是先 load `y` 再 store `x`。允许的结果来自 **两个线程之间没有同步**，不是 T1 把 A、B 颠倒执行。

| 同一线程 | 本线程能不能颠倒 | 别的线程 |
|----------|------------------|----------|
| `load y` 再 `store x=r1`（有依赖） | 不能（没 `r1` 没法 store） | 只看见 `x` 的写，和 `y` 无挂钩 |
| `store x` 再 `store y`（无依赖） | 抽象机里不能，硬件可能让别的线程反着看见两次写 | 可以 `y==1 && x==0` |

第 39 行那句更容易误会。针对 34–35 行，应理解成：**有依赖也只保证你自己先读后写；不能当成发给别的线程的顺序保证。**

---

## 2. 修改序（modification order）【跨线程，但每次只针对一个变量】

对**某一个**原子变量的所有修改，排成一条特定于该变量的总序。不是「整个程序一条时间轴」，只是「这一个 `a` 的写记录」。relaxed 写也进这条序，不靠 release/acquire。

```cpp
std::atomic<int> a{0};

// ========== 线程 t1 ==========     // ========== 线程 t2 ==========
a.store(1);                          a.store(9);
a.store(2);
```

`a` 的修改序可能是 `0→1→2→9` 或 `0→1→9→2` 等；不会出现某线程认为「2 在 1 前」、另一线程认为「1 在 2 前」。  
两次写之间**没有** happens-before 时，只保证进同一条总序，谁先谁后不确定；有 happens-before 才锁死相对位置。下面四条就是这件事（happens-before 可以来自同一线程的先序于，也可以来自跨线程同步）。

合在一起：**对同一原子，一旦两次操作有 happens-before，就不能顺着这条关系在修改序里往回走。**

### 写-写一致性【单线程 + 跨线程】

写 A happens-before 写 B（都改 M）→ 修改序里 A 在 B 前。

例子【仅单线程】：A 先序于 B，于是 A happens-before B。

```cpp
// ========== 同一个线程 ==========
M.store(1); // A
M.store(2); // B  修改序一定是 1 再 2，不会是 2 再 1
```

【跨线程】：两次写分属两个线程，同样不会自动带 happens-before。

- 若 A happens-before B（中间有同步于）：修改序里 A 一定在 B 前。
- 若没有同步于：只保证进同一条总序，谁先谁后不定。

### 读-读一致性【单线程 + 跨线程】

读 A happens-before 读 B，且 A 读到了写 X → B 读到的要么是 X，要么是修改序里 X **之后**的某次写。不能读到 X 之前的旧值。

例子【仅单线程】：同一线程连续两次 load。

```cpp
// ========== 同一个线程 ==========
// 假设 M 的修改序是 0 → 1 → 2
int a = M.load(); // A 读到 1
int b = M.load(); // B 不能读到 0；只能是 1 或 2
```

【跨线程】：仍假设修改序 `0→1→2`，T1 的 A 读到了 1。两个线程的两次 load **不会自动**带 happens-before，中间要有同步于才算。

- 若 A happens-before B（中间有同步于）：T2 **不能**读到 0，**能**读到 1 或 2（不必已经读到 2）。
- 若只是两个线程各自 `relaxed` load、没有同步于：没有 happens-before，这条用不上，T2 **可以**读到 0、1、2 中任意一个（哪怕 T1 已经读到 1）。

### 读-写一致性【单线程 + 跨线程】

读 A happens-before 写 B → A 的值来自修改序里 B **之前**的某次写。A 看不见这次 B，也看不见 B 之后的写。

例子【仅单线程】：同一线程先 load 再 store。

```cpp
// ========== 同一个线程 ==========
int a = M.load(); // A
M.store(5);       // B
// a 不是这次 store 写进去的 5
```

【跨线程】：T1 先 `load`（A），T2 再 `store(5)`（B）。两次操作同样不会自动带 happens-before。

- 若 A happens-before B：A **看不见**这次 5，也看不见 B 之后的写。
- 若没有同步于：这条用不上，A **可以**读到 5，也可以读到更早的值。

### 写-读一致性【单线程 + 跨线程】

写 X happens-before 读 B → B 读到的要么是 X，要么是修改序里 X **之后**的某次写。不能读到比 X 更早的值。

例子【仅单线程】：同一线程先 store 再 load。

```cpp
// ========== 同一个线程 ==========
M.store(1);       // X
int b = M.load(); // B 不能还是初始 0；至少是 1，或修改序里更后面的值
```

【跨线程】：T1 写 `store(1)`（X），T2 再 `load`（B）。没有同步于就不会自动有 happens-before。

- 若 X happens-before B：T2 **不能**读到 0，**能**读到 1 或修改序里更后面的值。
- 若没有同步于：这条用不上，T2 **可以**仍读到 0。

| 一致性 | happens-before | 修改序上不许怎样 |
|--------|----------------|------------------|
| 写-写 | 写 A 先于 写 B | B 不能排到 A 前面 |
| 读-读 | 读 A 先于 读 B，A 看见 X | B 不能看见比 X 更早的写 |
| 读-写 | 读 A 先于 写 B | A 不能看见 B 或更晚的写 |
| 写-读 | 写 X 先于 读 B | B 不能看见比 X 更早的写 |

`x` 和 `y` 是两个变量，各有各的修改序，**不会因为都在 t1 里先写 x 再写 y，就自动对齐**。这四条也只约束**同一个**原子。

---

## 3. 同步于（synchronizes-with）【只在跨线程】

单线程里没有「同步于」。必须是 **一个线程 release store，另一个线程 acquire load，并且读到了那次 store 的值**。

```cpp
// ========== 线程 A ==========          // ========== 线程 B ==========
data = 42;
ptr.store(p, std::memory_order_release); // S
                                         p2 = ptr.load(std::memory_order_acquire); // L
                                         // L 读到了 S 写入的那个 p
```

**S 同步于 L**（线程 A → 线程 B）。挂钩只拴在 `ptr` 这一对上。  
若还有线程 C 用 `relaxed` 读 `ptr`，C **没有**和 A 同步，不能靠 L 顺便断言自己也看到 `data = 42`。

对应 [`release_acquire.cpp`](../concurrent/memory_order/release_acquire.cpp) 的 `test_order_release()`。

---

## 4. 先发生于（happens-before）【单线程 + 跨线程】

两种来源，拼成一条可见性链：

```text
【仅单线程】 A 先序于 B                    →  A happens-before B
【只跨线程】 S 同步于 L                    →  S happens-before L
【传递】     A hb B 且 B hb C              →  A happens-before C（可跨线程）
```

**若 A happens-before B，则 A 的副作用在 B 处可见。** 没有这条链，就不能说「我看见了 y，所以一定也看见 x」。

```cpp
// ========== 线程 A（一个人先 W 再 S）==========
data = 42;                             // W  普通写
ptr.store(p, memory_order_release);    // S  W 先序于 S（单线程）

// ========== 线程 B ==========
p2 = ptr.load(memory_order_acquire);   // L  若读到 S 的值：S 同步于 L（跨线程）
assert(data == 42);                    // W happens-before 这句：单线程链 + 跨线程链接上了
```

---

## 5. 宽松定序（relaxed）【每条操作仍在各自线程里；问题出在跨线程怎么看见】

每一次 load/store 仍然只在执行它的那个线程里发生，且原子。  
**不**建立同步于，所以**不能**把「某线程里先写 x 再写 y」变成「另一个线程必须先看到 x」。

### 例子：T1 里两个 store 顺序固定；T2 可以反着看见【跨线程】

```cpp
// ========== 线程 T1（先序于：先 x 后 y）==========
x.store(1, relaxed);
y.store(1, relaxed);

// ========== 线程 T2（先序于：先读 y 后读 x）==========
int a = y.load(relaxed);
int b = x.load(relaxed);
// 允许 a==1 && b==0 ：T2 先看到后写的 y，还没看到先写的 x
```

T1 内部 x 仍先序于 y；打破的是 **T2 看到的顺序**。见 `relaxed.cpp` 实验 A（x86 上很少，ARM 上会）。

### 例子：cppreference 的 r1==r2==42【两个线程；每线程内部仍有先序于】

```cpp
// ========== 线程 T1 ==========     // ========== 线程 T2 ==========
r1 = y.load(relaxed);   // A         r2 = x.load(relaxed);   // C
x.store(r1, relaxed);   // B         y.store(42, relaxed);   // D
```

T1 里 A 先序于 B，T2 里 C 先序于 D。**T1 和 T2 之间没有同步于**。  
允许硬件把 T1 的 A 挪到 B 后、把 T2 的 D 挪到 C 前，于是 D 写入的 42 被 A 读到，再经 B 被 C 读到，`r1 == r2 == 42`。这是「允许的结果」，不是 D happens-before A。

### 例子：两边都读到 0【两个线程，x86 上能碰到】

```cpp
// ========== 线程 T1 ==========     // ========== 线程 T2 ==========
x.store(1, relaxed);                 y.store(1, relaxed);
r1 = y.load(relaxed);                r2 = x.load(relaxed);
// 允许 r1==0 && r2==0（各人先序于仍是「先 store 再 load」）
```

每人自己线程里都是先写后读；**跨线程**谁也还没看见对方的写。`seq_cst` 禁止这个结果。见 `relaxed.cpp` 实验 B。

### 典型场景

你的理解对：**只关心这一个原子自己的值**（不撕成半个 `int`、能读到修改序上的某个值），**不**拿它当「别的内存已经可见」的信号。跨线程时序确实不保证；本线程先序于、这一变量的四条一致性还在。

```cpp
std::atomic<int> hits{0};
std::atomic<bool> stop{false};

hits.fetch_add(1, relaxed);          // 统计：加漏几次、看见略旧的数都无所谓
if (stop.load(relaxed)) return;      // 停循环：只问这一位，不顺带读别的数据
```

| 用 relaxed | 不要用 relaxed |
|------------|----------------|
| 计数、打点、进度条（`fetch_add` / 偶尔 load） | 「数据写好了」这种旗标（读者还要看旁边的 `data`） |
| 纯停转标志：看见就退出，不读发布方的其它内存 | 读到指针再解引用（要用 acquire） |
| `shared_ptr` 的引用计数 **加一**（你已经持有一份，顺序早有了） | 引用计数减到 0 要析构（要用 acq_rel，才能看见别人写过的对象） |

判断口诀：这行 load/store **之后**还要不要相信别的变量？要 → acquire/release；不要、只看这一个数 → relaxed。

---

## 6. 释放-获取定序（release-acquire）【跨线程配对；每侧内部仍是单线程先序于】

**跨线程：** 线程 A 的 release store 被线程 B 的 acquire load 读到 → 同步于。  
**单线程：** A 里 release **之前**的写，先序于这次 store，于是会捎带到 B 里 acquire **之后**。

```cpp
// ========== 线程 T1 ==========              // ========== 线程 T2 ==========
data = 42;
data2 = "ddd";
ptr.store(p, release);
                                              while (!(p2 = ptr.load(acquire))) {}
                                              assert(*p2 == "Hello");
                                              assert(data == 42);   // OK，捎带过来了
```

T2 若改成 `relaxed` load，即使读到非空 `ptr`，也**没有**和 T1 同步，不能断言 `data == 42`。

第三者（线程 T3）用 relaxed 看同一块内存，没有加入这对本，顺序仍可以和 T1、T2 都不同。

`test_order_release2`：线程 a release 写 `flag`；线程 b **relaxed** CAS（不获得 a 的同步）；线程 c acquire 读到 2。a 和 c 中间隔了 b 的 relaxed，不保证 c 看到 `data.push_back(42)`。

---

## 7. 释放-消费定序（release-consume）【跨线程；比 acquire 捎带得少】

这里有**三块不同的内存**，不要混成一件事：

| 对象 | 是什么 | T1 做了什么 |
|------|--------|-------------|
| `ptr` | 原子指针 | `ptr.store(p, release)` |
| `*p` | 堆上那个 `string` | `new std::string("Hello")`，发生在 store **之前** |
| `data` | 另一个 `int` | `data = 42`，也在 store 之前，但和指针**无关** |

```cpp
// ========== 线程 T1 ==========
std::string* p = new std::string("Hello"); // 写的是堆上的 string，不是 ptr
data = 42;                                 // 写的是另一个 int
ptr.store(p, release);                     // 只把「地址」发布出去

// ========== 线程 T2 ==========
p2 = ptr.load(consume);   // 这一行只读到一个地址（指针值）
assert(*p2 == "Hello");   // 另一块内存：顺着地址去读堆上的 string
assert(data == 42);       // 第三块内存：根本没经过 p2
```

`p2` 读出来了，只说明原子 `ptr` 里的**地址**看见了。  
`p2 == "Hello"` 不成立：`p2` 是指针，不是那串字符。要比的是 **`*p2 == "Hello"`**。

consume 多保证的就是这一下解引用：因为读 `*p2` 的**地址来自这次 load**，T1 里构造 `"Hello"` 的那些写，对这次 `*p2` 可见。  
`data` 的地址是写死的全局变量，**算地址时没用到 `p2`**，consume 不管它。

| T2 已经读到非空 `p2` | relaxed | consume | acquire |
|--|--|--|--|
| 指针值 `p2` 本身（`ptr` 这块） | 可以看见 | 可以看见 | 可以看见 |
| 堆上的 `*p2`（地址来自 `p2`） | **不保证** | **保证** | 保证 |
| 旁路的 `data`（地址和 `p2` 无关） | 不保证 | **不保证** | 保证 |

「用到 p2」= 这次读的地址是从 load 返回值算出来的（`*p2`、`p2->...`）。  
「没用到 p2」= 就算把这行 load 删掉，这个读照样写得出来（`data`）。

acquire 不管你用不用 `p2`：T2 里 load **之后**的读全部能看见 T1 release **之前**的写。  
consume 只给「顺着指针走进去」这一条链；所以它保证 `*p2`，不保证 `data`。

`test_release_consume2` 是「没用到」的具体样子：`gg = &Playload` 在 load **之前**就算好了，后面 `*gg` 走的是这条预先知道的地址，不是 consume 返回的指针，于是和 relaxed 一样不保证看到 42。

典型场景：**发布一个指针，读者只顺着这个指针去读对象**。

```cpp
// T1：把一棵树 / 一张表填好，最后把根指针 release 出去
node->left = ...;
node->right = ...;
root.store(node, release);

// T2：load 到根，只走 node->left、node->right
node* n = root.load(consume);
use(n->left);   // 地址来自 n，consume 管
```

Linux 内核的 RCU（`rcu_dereference`）就是这个模式：读者拿到指针后只解引用，不要全屏障。弱序 CPU（POWER、早期 ARM）上，硬件本来就尊重「先知道地址再访问」，consume 设计成**不加 acquire 那种额外屏障**，只吃这条地址依赖。x86 上 acquire 本来就几乎免费，consume 没便宜可占。

**新 C++ 代码不要用 consume，用 acquire。** 编译器很难保证「数据依赖」不被优化成控制依赖，C++17 起实现都直接把 consume 当 acquire。你需要旁路数据（上面的 `data`）时，本来就该 acquire。

现代编译器常把 consume 当成 acquire。对应 [`release_consume.cpp`](../concurrent/memory_order/release_consume.cpp)。

---

## 8. 顺序一致（seq_cst）【跨线程：所有 seq_cst 操作一条全局时间轴】

每个线程内部照旧有先序于。另外：**所有**标了 `seq_cst` 的原子操作（不限同一个变量）还要能排进全程序**一条**总序，所有线程对这条序看法一致。  
这比「每个变量自己一条修改序」更强，也比 release-acquire 更强。单线程里多标 `seq_cst` 不会让先序于更真。

默认的 `load()` / `store()` 就是 `seq_cst`。

### 例子：两边都读到 0【跨线程】——seq_cst 禁止，acq/rel 仍允许

就是 `relaxed.cpp` 实验 B，四次操作都改成 `seq_cst`：

```cpp
// ========== 线程 T1 ==========     // ========== 线程 T2 ==========
x.store(1, seq_cst);                 y.store(1, seq_cst);
r1 = y.load(seq_cst);                r2 = x.load(seq_cst);
```

每人自己线程里仍是先 store 再 load。`r1`、`r2` 各自只能是 0 或 1，但 **不能同时为 0**。

| `r1` | `r2` | 含义 | seq_cst | relaxed / acq-rel |
|------|------|------|---------|-------------------|
| 0 | 0 | 两人都没看见对方 | **禁止** | 允许 |
| 0 | 1 | T1 先跑完 store+load，T2 的 store 还没被 T1 看见 | 允许 | 允许 |
| 1 | 0 | T2 先跑完，T1 的 store 还没被 T2 看见 | 允许 | 允许 |
| 1 | 1 | 两人都看见了对方的写 | 允许 | 允许 |

【跨线程】若是 relaxed，允许 `r1==0 && r2==0`（各人还没看见对方的写）。全是 `seq_cst` 时，这四下必须能排进一条时间轴。谁的 store 在这条轴上更早，另一个线程的 load 就一定能看见它，所以不可能两个人都读到 0。x86 上 relaxed 就能打出两边都是 0；改成 `seq_cst` 打不出来。

改成 release store + acquire load **仍然允许**两边都是 0。不是「有一方先写就会同步」。

```cpp
// ========== 线程 T1 ==========              // ========== 线程 T2 ==========
x.store(1, release);                          y.store(1, release);
r1 = y.load(acquire);                         r2 = x.load(acquire);
```

同步于的条件是：acquire **读到了**对方那次 release 存进去的值。  
`r1==0` 说明 T1 读 `y` 读到的是初始 0，**不是** T2 的 `y.store(1)`，T1 和 T2 在 `y` 上没挂钩。  
`r2==0` 同理，在 `x` 上也没挂钩。两边都是 0 = **一次同步于都没有**。

「先写」是 seq_cst 那条全局时间轴上的想法。acq/rel 没有这条轴：两个人都可以先把自己的 store 放进 store buffer，再去 load 对方，此时对方的写还不可见。x86 上 acq/rel 就是普通 `mov`，实验 B 那种 (0, 0) **确实能打出来**；seq_cst 会加全屏障，把 store buffer 刷掉，所以禁止。

这就是 seq_cst 比 acq/rel 多出来的那一截。

### 例子：消息传递【跨线程】——acq/rel 已经够，不必 seq_cst

```cpp
// ========== 线程 T1 ==========     // ========== 线程 T2 ==========
x.store(1, seq_cst);                 if (y.load(seq_cst) == 1)
y.store(1, seq_cst);                     assert(x.load(seq_cst) == 1);
```

T2 看见 `y==1` 就必须看见 `x==1`。把 `y` 做成 release/acquire、`x` 做成 relaxed，结论一样：读到 `y` 就同步于，捎带看见 `x`。  
所以「旗标 + 数据」用 release-acquire 即可，不必所有操作都 `seq_cst`。

seq_cst 真正要上场的，是**好几个变量、好几个人，要对「谁先发生」达成同一个看法**（Dekker / Peterson 互斥、上面那种「会不会两个人都以为对方还没动」）。代价是全屏障，比 acq/rel 重。

### 典型场景

**要用 seq_cst 的**：两个线程各自写自己的旗标、再读对方的旗标，并且**不能两个人同时以为对方还没动**。

```cpp
// Dekker 互斥的骨架【跨线程】
// ========== 线程 T1 ==========          // ========== 线程 T2 ==========
flag1.store(true, seq_cst);              flag2.store(true, seq_cst);
if (!flag2.load(seq_cst))                if (!flag1.load(seq_cst))
    // 进临界区                                 // 进临界区
```

若改成 acq/rel，允许两边都读到 `false`，两人一起进临界区。实验 B 就是这个形状。Peterson 锁、某些无锁「先手/后手」判定，同理。

**不必 seq_cst 的**：

| 场景 | 用什么 |
|------|--------|
| 旗标 + 数据（一边发布，另一边看见旗标再读数据） | release / acquire |
| 只累加计数、不在乎和其他变量的顺序 | relaxed |
| 发布指针，读者只解引用 | release / acquire（不要 consume） |

拿不准时用默认的 `seq_cst` 是对的，只是更重（通常是全屏障）。想清楚「我是不是在要求所有人对多个变量有同一个全局顺序」再决定要不要留下它。

---

## 对照

| 术语 | 单线程？ | 多线程？ |
|------|----------|----------|
| 先序于 | **只在这里谈** | 不管 |
| 修改序 | 本线程的写也进这条序；四条一致性用 happens-before 钉住读写相对位置 | 多线程写**同一变量**时大家看到同一条写记录 |
| 同步于 | 无此概念 | **必须**两个线程、同一原子、release 对上 acquire |
| happens-before | 先序于可推出 | 同步于可推出；两条还能跨线程传递 |
| relaxed | 本线程仍有先序于 | **不**当跨线程信号；只适合计数 / 纯停转这种「只看这一个数」 |
| release-acquire | 本线程 release 前的写先序于 store | **另一线程** acquire 到该值后才能看见那些写 |
| consume | 本线程里只有「地址来自这次 load」的读被排在后面 | 保证 `*p2`，不保证旁路的 `data`；比 relaxed 多的就是解引用这一块 |
| seq_cst | 本线程先序于仍在 | 所有 seq_cst 操作一条全局时间轴；禁止实验 B 那种两边都读到 0（acq/rel 仍允许） |

推理口诀：

1. 没写 T1/T2 的代码 = **一个线程**，只用 **先序于**。
2. 出现两个线程才问有没有 **同步于**（release 的值被 acquire 读到）。
3. 接得成链才有跨线程的 **happens-before**。
4. 一个变量的 **修改序** 解释不了两个变量谁先被另一线程看见。
