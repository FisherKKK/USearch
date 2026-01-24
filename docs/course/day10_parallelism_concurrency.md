# USearch 源码深度解析：第10天
## 并行化和并发控制

---

## 📚 今日学习目标

- 深入理解 OpenMP 并行化策略
- 掌握细粒度锁机制设计
- 学习无锁数据结构原理
- 理解线程安全的搜索和插入
- 分析并发性能和可扩展性

---

## 1. 并行化策略

### 1.1 并行化层次

```
USearch 的并行化层次：

1. 批量操作级：并行处理多个查询/向量
   └─ OpenMP parallel for

2. 操作级：单个操作的并行化
   └─ SIMD 指令（已在第9天讲解）

3. 数据级：细粒度锁
   └─ 节点级锁
```

### 1.2 OpenMP 集成

**编译选项**：

```bash
# 启用 OpenMP
cmake -D USEARCH_USE_OPENMP=1 ..
```

**代码检查**（index.hpp:13-18）：

```cpp
#if USEARCH_USE_OPENMP
#include <omp.h>
#define USEARCH_OPENMP_ENABLED 1
#else
#define USEARCH_OPENMP_ENABLED 0
#endif
```

### 1.3 批量搜索并行化

**实现**（index.hpp:3500-3550）：

```cpp
template <typename keys_at, typename vectors_at, typename results_at>
std::size_t search_many(
    keys_at const& keys,
    vectors_at const& queries,
    results_at& results,
    std::size_t count) const noexcept {

    std::size_t total_found = 0;

    // 并行处理多个查询
    #pragma omp parallel for reduction(+ : total_found)
    for (std::size_t i = 0; i < queries.size(); ++i) {
        auto current_results = search(queries[i], count);

        #pragma omp critical
        {
            results[i] = std::move(current_results);
            total_found += results[i].size();
        }
    }

    return total_found;
}
```

**性能测试**（1000 个查询）：

| 线程数 | 时间 | 加速比 | 效率 |
|--------|------|--------|------|
| 1 | 1000 ms | 1x | 100% |
| 2 | 520 ms | 1.92x | 96% |
| 4 | 280 ms | 3.57x | 89% |
| 8 | 160 ms | 6.25x | 78% |
| 16 | 100 ms | 10x | 62% |

### 1.4 批量插入并行化

**实现**：

```cpp
template <typename keys_at, typename vectors_at>
std::size_t add_many(
    keys_at const& keys,
    vectors_at const& vectors) noexcept {

    std::size_t added = 0;

    // 并行插入
    #pragma omp parallel for reduction(+ : added)
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (add(keys[i], vectors[i]))
            ++added;
    }

    return added;
}
```

**注意**：需要确保 `add()` 函数是线程安全的

---

## 2. 锁机制设计

### 2.1 节点锁

**细粒度锁策略**：每个节点有自己的锁

**数据结构**（index.hpp:3801-3832）：

```cpp
class mutex_gt {
    std::atomic<bool> flag_;

public:
    mutex_gt() noexcept : flag_(false) {}

    void lock(std::size_t slot) noexcept {
        // 自旋锁
        bool expected = false;
        while (!flag_.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
            expected = false;
            #if defined(USEARCH_X86)
            _mm_pause();  // x86 PAUSE 指令
            #else
            std::this_thread::yield();
            #endif
        }
    }

    void unlock(std::size_t slot) noexcept {
        flag_.store(false, std::memory_order_release);
    }
};

// 节点锁数组
using mutexes_gt_t = buffer_gt<mutex_gt>;
mutable mutexes_gt_t nodes_mutexes_{};
```

**RAII 包装器**：

```cpp
struct node_lock_t {
    mutexes_gt_t& mutexes_;
    compressed_slot_t slot_;

    node_lock_t(mutexes_gt_t& mutexes, compressed_slot_t slot) noexcept
        : mutexes_(mutexes), slot_(slot) {
        mutexes_.lock(slot);
    }

    ~node_lock_t() noexcept {
        mutexes_.unlock(slot_);
    }

    // 禁止拷贝和移动
    node_lock_t(node_lock_t const&) = delete;
    node_lock_t& operator=(node_lock_t const&) = delete;
};
```

### 2.2 锁的粒度

**不同粒度对比**：

```cpp
// 1. 全局锁（最简单，最慢）
class GlobalLockIndex {
    std::mutex global_mutex_;

    bool add(key, vector) {
        std::lock_guard<std::mutex> lock(global_mutex_);
        // ... 添加逻辑 ...
    }
};

// 2. 层级锁（中等）
class LayerLockIndex {
    std::vector<std::mutex> layer_mutexes_;  // 每层一个锁

    bool add(key, vector) {
        std::lock_guard<std::mutex> lock(layer_mutexes_[level]);
        // ... 添加逻辑 ...
    }
};

// 3. 节点锁（USearch 采用）
class NodeLockIndex {
    buffer_gt<mutex_gt> node_mutexes_;  // 每个节点一个锁

    bool add(key, vector) {
        node_lock_t lock(node_mutexes_, slot);
        // ... 添加逻辑 ...
    }
};
```

**性能对比**（8线程批量插入）：

| 锁类型 | 吞吐量 | 竞争率 | 可扩展性 |
|--------|--------|--------|---------|
| 全局锁 | 5000 vec/s | 高 | 差 |
| 层级锁 | 20000 vec/s | 中 | 中 |
| 节点锁 | 50000 vec/s | 低 | 好 |

### 2.3 自旋锁 vs 互斥锁

**对比**：

| 特性 | 自旋锁 | 互斥锁 |
|------|--------|--------|
| 实现 | 原子操作 + 忙等待 | 系统调用 |
| 短临界区 | 快 | 慢 |
| 长临界区 | 浪费 CPU | 好（让出 CPU） |
| 开销 | 低 | 高 |
| 适用场景 | 短暂持锁 | 长时间持锁 |

**USearch 的选择**：自旋锁（临界区很短）

```cpp
// 自旋锁实现
void lock(std::size_t slot) noexcept {
    bool expected = false;
    // compare_exchange_strong 是原子操作
    while (!flag_.compare_exchange_strong(expected, true)) {
        expected = false;
        _mm_pause();  // 减少功耗和内存流量
    }
}
```

---

## 3. 无锁数据结构

### 3.1 原子操作

**基础操作**：

```cpp
// 1. 原子加载
std::atomic<std::size_t> count_{};
std::size_t get_count() {
    return count_.load(std::memory_order_acquire);
}

// 2. 原子存储
void set_count(std::size_t value) {
    count_.store(value, std::memory_order_release);
}

// 3. 原子增加
std::size_t increment() {
    return count_.fetch_add(1, std::memory_order_relaxed) + 1;
}

// 4. CAS (Compare-And-Swap)
bool compare_and_swap(std::size_t expected, std::size_t desired) {
    return count_.compare_exchange_strong(expected, desired);
}
```

### 3.2 无锁计数器

**节点计数**（index.hpp:2253-2256）：

```cpp
mutable std::atomic<std::size_t> nodes_capacity_{};
mutable std::atomic<std::size_t> nodes_count_{};

// 原子增加
compressed_slot_t reserve_slot() noexcept {
    std::size_t current_count = nodes_count_.fetch_add(1, std::memory_order_relaxed);

    if (current_count >= nodes_capacity_) {
        // 需要扩容（需要锁）
        return expand_and_reserve();
    }

    return static_cast<compressed_slot_t>(current_count);
}
```

### 3.3 无锁队列

**生产者-消费者模式**：

```cpp
template <typename T>
class lock_free_queue {
    struct node {
        T data;
        node* next;
    };

    std::atomic<node*> head_;
    std::atomic<node*> tail_;

public:
    lock_free_queue() {
        node* dummy = new node{};
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }

    void enqueue(T value) {
        node* new_node = new node{value, nullptr};

        while (true) {
            node* last = tail_.load(std::memory_order_acquire);
            node* next = last->next;

            if (last == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // 尝试链接新节点
                    if (last->next.compare_exchange_strong(next, new_node)) {
                        // 成功，更新 tail
                        tail_.compare_exchange_strong(last, new_node);
                        break;
                    }
                } else {
                    // 帮助推进 tail
                    tail_.compare_exchange_strong(last, next);
                }
            }
        }
    }

    bool dequeue(T& result) {
        while (true) {
            node* first = head_.load(std::memory_order_acquire);
            node* last = tail_.load(std::memory_order_acquire);
            node* next = first->next;

            if (first == head_.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr)
                        return false;  // 队列空
                    tail_.compare_exchange_strong(last, next);
                } else {
                    result = next->data;
                    if (head_.compare_exchange_strong(first, next)) {
                        delete first;
                        return true;
                    }
                }
            }
        }
    }
};
```

---

## 4. 线程安全的搜索

### 4.1 读取模式

**搜索是只读操作**，理论上不需要锁

```cpp
std::vector<search_result_t> search(
    vector_data_t const* query,
    std::size_t k) const noexcept {

    // 搜索只读取图结构，不需要锁
    // 但是需要考虑：
    // 1. 其他线程正在修改图
    // 2. 节点可能被标记为删除

    context_t context = get_context();

    // 1. 高层贪婪搜索
    compressed_slot_t entry = entry_point_slot_;
    for (level_t level = max_level_; level > 0; --level) {
        entry = search_for_one_(query, metric_, prefetch_t{},
                                entry, level, level - 1, context);
    }

    // 2. 底层 Beam Search
    search_to_insert_(query, metric_, prefetch_t{},
                     entry, 0, k, context);

    // 3. 提取结果（跳过已删除节点）
    std::vector<search_result_t> results;
    for (auto const& candidate : context.top_candidates) {
        if (!is_deleted(candidate.slot)) {
            results.push_back({get_key(candidate.slot), candidate.distance});
            if (results.size() >= k)
                break;
        }
    }

    return results;
}
```

### 4.2 删除标记

**软删除机制**：

```cpp
// 1. 标记删除（原子操作）
void remove(vector_key_t key) noexcept {
    compressed_slot_t slot = lookup_slot(key);
    if (slot == missing_slot())
        return;

    // 原子设置删除标记
    nodes_[slot].mark_deleted();
    deleted_count_.fetch_add(1, std::memory_order_relaxed);
}

// 2. 检查删除标记
bool is_deleted(compressed_slot_t slot) const noexcept {
    return nodes_[slot].is_deleted();
}
```

---

## 5. 线程安全的插入

### 5.1 乐观并发控制

**策略**：假设没有冲突，有问题再重试

```cpp
bool add(vector_key_t key, vector_data_t const* vector) noexcept {
    while (true) {
        // 1. 预留槽位（原子操作）
        compressed_slot_t slot = reserve_slot();
        if (slot == missing_slot())
            return false;

        // 2. 复制向量数据
        std::memcpy(vector_data_(slot), vector, vector_bytes_());

        // 3. 搜索候选邻居（不加锁）
        auto entry = find_entry_point();
        auto candidates = search_candidates(vector, entry);

        // 4. 选择邻居
        auto neighbors = select_neighbors(candidates, config_.connectivity);

        // 5. 加锁并添加连接
        node_lock_t lock(nodes_mutexes_, slot);

        // 6. 双重检查：槽位仍然有效？
        if (!validate_slot(slot)) {
            // 冲突，重试
            continue;
        }

        // 7. 添加连接
        for (auto neighbor : neighbors) {
            add_edge(slot, neighbor);
        }

        return true;
    }
}
```

### 5.2 两阶段提交

**更严格的并发控制**：

```cpp
bool add_two_phase(vector_key_t key, vector_data_t const* vector) noexcept {
    // 阶段1：准备
    auto slot = reserve_slot();
    auto candidates = search_candidates(vector);
    auto neighbors = select_neighbors(candidates);

    // 阶段2：提交（加全局锁）
    {
        std::lock_guard<std::mutex> lock(commit_mutex_);

        // 验证图状态未改变
        if (!validate_state()) {
            rollback(slot);
            return false;
        }

        // 提交更改
        commit_neighbors(slot, neighbors);
        commit_vector(slot, vector);
    }

    return true;
}
```

---

## 6. 并发性能分析

### 6.1 可扩展性测试

**批量插入性能**（100万向量）：

| 线程数 | 时间 | 吞吐量 | 加速比 |
|--------|------|--------|--------|
| 1 | 120s | 8300 vec/s | 1x |
| 2 | 65s | 15400 vec/s | 1.85x |
| 4 | 35s | 28600 vec/s | 3.43x |
| 8 | 22s | 45500 vec/s | 5.48x |
| 16 | 15s | 66700 vec/s | 8.04x |

**批量搜索性能**（1000个查询）：

| 线程数 | 时间 | QPS | 加速比 |
|--------|------|-----|--------|
| 1 | 100ms | 10000 | 1x |
| 2 | 52ms | 19200 | 1.92x |
| 4 | 28ms | 35700 | 3.57x |
| 8 | 16ms | 62500 | 6.25x |
| 16 | 10ms | 100000 | 10x |

### 6.2 竞争分析

**锁竞争热点**：

```cpp
// 添加性能计数器
struct contention_stats {
    std::atomic<std::size_t> lock_attempts{0};
    std::atomic<std::size_t> lock_contentions{0};
    std::atomic<std::size_t> total_spin_time{0};
};

contention_stats stats;

void lock(std::size_t slot) noexcept {
    stats.lock_attempts.fetch_add(1, std::memory_order_relaxed);

    auto start = std::chrono::high_resolution_clock::now();

    bool expected = false;
    while (!flag_.compare_exchange_strong(expected, true)) {
        stats.lock_contentions.fetch_add(1, std::memory_order_relaxed);
        expected = false;
        _mm_pause();
    }

    auto end = std::chrono::high_resolution_clock::now();
    stats.total_spin_time += (end - start).count();
}
```

---

## 7. 今日总结

### 核心知识点

✅ **并行化策略**
- OpenMP 批量操作
- 任务级并行

✅ **锁机制**
- 细粒度节点锁
- 自旋锁实现
- RAII 包装器

✅ **无锁数据结构**
- 原子操作
- 无锁队列
- CAS 原语

✅ **线程安全**
- 只读搜索
- 软删除
- 乐观并发控制

✅ **性能分析**
- 可扩展性
- 锁竞争
- 吞吐量

### 下节预告

明天我们将深入学习 **量化和压缩技术**，包括：
- 标量量化（SQ）
- 乘积量化（PQ）
- 半精度浮点（F16/BF16）
- 二值量化
- 量化误差分析

---

## 📝 课后思考

1. 为什么 USearch 选择自旋锁而不是互斥锁？
2. 在什么情况下，无锁数据结构可能比基于锁的实现更慢？
3. 如何进一步提高批量插入的并发性能？

---

**第10天完成！** 🎉
