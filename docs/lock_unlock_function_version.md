如果一个函数既可能在已加锁的情况下调用，又可能在未加锁的情况下调用，那么就拆成两个函数：
1. 跟原来的函数同名，函数加锁，转而调用第二个函数
2. 给函数加上后缀withLockHold，不加锁，把原来的函数体搬过来。
```cpp
void post(const Foo& f) {
    std::lock_guard lock(mutex);
    postWithLockGuard(f);
}

void postWIthLockHold(const Foo& f) {
    // 确定要在加锁的情况下调用
    assert(mutex.isLockedByThisThread());
    foos.push_back(f);
}
```

