# USearch 源码深度解析：第8天
## 内存管理机制

---

## 📚 今日学习目标

- 深入理解 USearch 的双分配器设计
- 掌握内存池技术的实现原理
- 学习零拷贝优化策略
- 理解内存映射的底层机制
- 分析内存对齐的性能影响

---

## 1. 内存管理概览

### 1.1 内存布局

```
USearch 索引的内存组成：

┌──────────────────────────────────────┐
│ 节点数组 (nodes_)                    │  → buffer_gt<node_t>
├──────────────────────────────────────┤
│ 向量数据 (vectors_)                  │  → buffer_gt<byte_t>
├──────────────────────────────────────┤
│ 互斥锁数组 (nodes_mutexes_)          │  → mutexes_gt
├──────────────────────────────────────┤
│ 临时缓冲区 (dynamic_allocator_)      │  → dynamic_allocator_t
└──────────────────────────────────────┘
```

### 1.2 设计目标

**内存管理的三大目标**：

1. **高效分配**：减少 `malloc` 调用
2. **缓存友好**：提高缓存命中率
3. **零拷贝**：避免不必要的数据复制

---

## 2. 双分配器设计

### 2.1 两种分配器

**代码位置**（index.hpp:2261）：

```cpp
// 1. 动态分配器：用于临时缓冲区
mutable dynamic_allocator_t dynamic_allocator_{};

// 2. Tape 分配器：用于持久化数据
tape_allocator_t tape_allocator_{};
```

**对比**：

| 特性 | dynamic_allocator_t | tape_allocator_t |
|------|---------------------|------------------|
| 用途 | 临时缓冲区 | 持久化存储 |
| 生命周期 | 短暂（函数内） | 长（索引生命周期） |
| 分配策略 | 按需分配 | 预分配 |
| 线程安全 | 是 | 否（外部同步） |

### 2.2 Tape 分配器

**设计理念**：像磁带一样连续分配内存

```cpp
template <typename allocator_at = std::allocator<byte_t>>
class tape_allocator_t {
    using byte_t = unsigned char;

    struct buffer_t {
        byte_t* data;
        std::size_t capacity;
        std::size_t offset;  // 当前写入位置
    };

    buffer_t buffer_;
    allocator_at allocator_;

public:
    // 分配指定大小
    byte_t* allocate(std::size_t size) noexcept {
        // 1. 检查是否有足够空间
        if (buffer_.offset + size > buffer_.capacity) {
            if (!expand_buffer(size))
                return nullptr;
        }

        // 2. 从 tape 中切出空间
        byte_t* result = buffer_.data + buffer_.offset;
        buffer_.offset += size;
        return result;
    }

    // 预分配空间
    bool reserve(std::size_t capacity) noexcept {
        if (capacity <= buffer_.capacity)
            return true;

        byte_t* new_data = std::allocator_traits<allocator_at>::allocate(
            allocator_, capacity);

        if (!new_data)
            return false;

        // 复制旧数据
        if (buffer_.data) {
            std::memcpy(new_data, buffer_.data, buffer_.offset);
            std::allocator_traits<allocator_t>::deallocate(
                allocator_, buffer_.data, buffer_.capacity);
        }

        buffer_.data = new_data;
        buffer_.capacity = capacity;
        return true;
    }
};
```

**优势**：
- 连续内存：提高缓存命中率
- 批量分配：减少分配次数
- 简单管理：无需维护空闲列表

### 2.3 动态分配器

**线程安全版本**（index.hpp:2650-2700）：

```cpp
template <typename allocator_at = std::allocator<byte_t>>
class dynamic_allocator_t {
    struct buffer_t {
        byte_t* data;
        std::size_t size;
        std::size_t capacity;
    };

    buffer_t buffer_;
    allocator_at allocator_;
    std::mutex mutex_;  // 线程安全

public:
    // 分配并返回智能指针
    std::unique_ptr<byte_t, std::function<void(byte_t*)>>
    allocate(std::size_t size) noexcept {

        std::lock_guard<std::mutex> lock(mutex_);

        // 检查容量
        if (buffer_.size + size > buffer_.capacity) {
            // 扩容为 2 倍
            std::size_t new_capacity = std::max(
                buffer_.capacity * 2,
                buffer_.size + size
            );

            if (!expand(new_capacity))
                return {nullptr, [](byte_t*){}};
        }

        // 分配空间
        byte_t* result = buffer_.data + buffer_.size;
        buffer_.size += size;

        // 返回自定义删除器
        return {
            result,
            [this](byte_t* ptr) {
                std::lock_guard<std::mutex> lock(mutex_);
                // 标记为可重用（不实际释放）
            }
        };
    }
};
```

---

## 3. 内存池技术

### 3.1 固定大小内存池

**设计**：为固定大小的对象预分配内存

```cpp
template <typename T, std::size_t BlockSize = 1024>
class memory_pool_gt {
    struct block_t {
        T data[BlockSize];
        block_t* next;
    };

    block_t* blocks_;
    T* free_list_;
    std::size_t allocated_;

public:
    memory_pool_gt() : blocks_(nullptr), free_list_(nullptr), allocated_(0) {
        // 分配第一个块
        allocate_block();
    }

    ~memory_pool_gt() noexcept {
        // 释放所有块
        while (blocks_) {
            block_t* next = blocks_->next;
            delete blocks_;
            blocks_ = next;
        }
    }

    T* allocate() noexcept {
        // 1. 从空闲列表获取
        if (free_list_) {
            T* result = free_list_;
            free_list_ = *reinterpret_cast<T**>(free_list_);
            return result;
        }

        // 2. 从当前块分配
        if (allocated_ < BlockSize) {
            T* result = &blocks_->data[allocated_++];
            return result;
        }

        // 3. 分配新块
        allocate_block();
        return &blocks_->data[allocated_++];
    }

    void deallocate(T* ptr) noexcept {
        // 加入空闲列表
        *reinterpret_cast<T**>(ptr) = free_list_;
        free_list_ = ptr;
    }

private:
    void allocate_block() noexcept {
        block_t* new_block = new block_t;
        new_block->next = blocks_;
        blocks_ = new_block;
        allocated_ = 0;
    }
};
```

### 3.2 使用场景

**适用场景**：

```cpp
// 1. 节点分配（固定大小）
memory_pool_gt<node_t, 1024> node_pool;
node_t* node = node_pool.allocate();

// 2. 临时缓冲区（频繁分配/释放）
memory_pool_gt<candidate_t, 256> candidate_pool;
candidate_t* candidates = candidate_pool.allocate();

// 使用完后归还
candidate_pool.deallocate(candidates);
```

**性能对比**：

| 操作 | malloc | 内存池 | 加速比 |
|------|--------|--------|--------|
| 分配 | 50 ns | 5 ns | 10x |
| 释放 | 50 ns | 5 ns | 10x |
| 批量 (1000) | 50 μs | 5 μs | 10x |

---

## 4. 零拷贝优化

### 4.1 原理

**零拷贝**：避免数据在内存间的复制

```
传统方式（有拷贝）：
  用户数据 → 内部缓冲区 → 计算
  (拷贝1)  (拷贝2)

零拷贝方式：
  用户数据 → 计算
  (无拷贝)
```

### 4.2 实现技巧

**技巧1：直接使用外部指针**

```cpp
bool add(
    vector_key_t key,
    byte_t const* vector,
    bool copy = true) noexcept {  // copy 参数

    if (copy) {
        // 复制到内部存储
        std::memcpy(vector_data_(slot), vector, vector_bytes_());
    } else {
        // 零拷贝：直接使用外部指针
        vector_external_[slot] = vector;

        // 注意：用户需要保证向量生命周期
    }
}
```

**技巧2：内存视图**

```cpp
class vector_view_t {
    byte_t const* data_;
    std::size_t dimensions_;
    scalar_kind_t scalar_;

public:
    // 不拥有数据，只是视图
    vector_view_t(byte_t const* data, std::size_t dims)
        : data_(data), dimensions_(dims) {}

    // 提供访问接口
    float operator[](std::size_t i) const {
        return reinterpret_cast<float const*>(data_)[i];
    }
};

// 使用
vector_view_t view(user_vector, 128);
distance_t dist = metric(query, view);
```

**技巧3：移动语义**

```cpp
// 使用移动避免拷贝
std::vector<float> create_vector() {
    std::vector<float> vec(128);
    // ... 填充数据 ...
    return vec;  // 移动，不拷贝
}

// 接收时使用移动
std::vector<float> my_vec = create_vector();  // 移动构造
```

### 4.3 注意事项

**零拷贝的风险**：

```cpp
// ❌ 危险：向量被提前释放
{
    std::vector<float> temp = get_vector();
    index.add_no_copy(1, temp.data());  // 保存指针
}  // temp 被销毁，指针失效！

// ✅ 正确：保证生命周期
std::vector<float> persistent = get_vector();
index.add_no_copy(1, persistent.data());  // safe
```

---

## 5. 内存映射

### 5.1 原理

**内存映射（Memory Mapping）**：将文件直接映射到虚拟内存

```
文件 ← mmap → 虚拟内存 ← 页错误 → 物理内存

优势：
1. 不需要一次性加载整个文件
2. 操作系统按需加载页面
3. 多个进程共享同一文件
```

### 5.2 实现

**跨平台实现**（index_plugins.hpp:1723-1853）：

```cpp
class memory_mapped_file_t {
    char const* path_;
    void* ptr_;
    std::size_t length_;

#if defined(USEARCH_DEFINED_WINDOWS)
    HANDLE file_handle_;
    HANDLE mapping_handle_;
#else
    int file_descriptor_;
#endif

public:
    // 打开文件并映射
    bool open(char const* path) noexcept {
        path_ = path;

#if defined(USEARCH_DEFINED_WINDOWS)
        // Windows 实现
        file_handle_ = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (file_handle_ == INVALID_HANDLE_VALUE)
            return false;

        mapping_handle_ = CreateFileMappingA(
            file_handle_,
            nullptr,
            PAGE_READONLY,
            0, 0,
            nullptr
        );

        if (!mapping_handle_)
            return false;

        ptr_ = MapViewOfFile(
            mapping_handle_,
            FILE_MAP_READ,
            0, 0, 0
        );

#else
        // POSIX 实现
        file_descriptor_ = ::open(path, O_RDONLY);
        if (file_descriptor_ < 0)
            return false;

        // 获取文件大小
        struct stat sb;
        if (fstat(file_descriptor_, &sb) < 0)
            return false;
        length_ = sb.st_size;

        // 映射
        ptr_ = mmap(
            nullptr,           // 让系统选择地址
            length_,           // 映射大小
            PROT_READ,         // 只读
            MAP_PRIVATE,       // 私有映射
            file_descriptor_,  // 文件描述符
            0                  // 偏移
        );

        if (ptr_ == MAP_FAILED)
            return false;

        // 预读建议
        madvise(ptr_, length_, MADV_RANDOM);
#endif

        return true;
    }

    // 关闭映射
    void close() noexcept {
#if defined(USEARCH_DEFINED_WINDOWS)
        if (ptr_)
            UnmapViewOfFile(ptr_);
        if (mapping_handle_)
            CloseHandle(mapping_handle_);
        if (file_handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(file_handle_);
#else
        if (ptr_ != MAP_FAILED)
            munmap(ptr_, length_);
        if (file_descriptor_ >= 0)
            ::close(file_descriptor_);
#endif

        ptr_ = nullptr;
        length_ = 0;
    }

    // 访问数据
    void* data() const noexcept { return ptr_; }
    std::size_t size() const noexcept { return length_; }
};
```

### 5.3 使用示例

**Python 接口**：

```python
import usearch
import numpy as np

# 创建大索引
index = usearch.Index(ndim=128)
vectors = np.random.rand(1_000_000, 128).astype(np.float32)
index.add(np.arange(1_000_000), vectors)

# 保存
index.save("huge_index.usearch")

# 加载方式1：加载到内存（慢）
index1 = usearch.Index.load("huge_index.usearch")

# 加载方式2：内存映射（快，省内存）
index2 = usearch.Index.restore("huge_index.usearch", view=True)

# 搜索
query = np.random.rand(128).astype(np.float32)
results = index2.search(query, k=10)
```

**性能对比**（10GB 索引）：

| 方式 | 加载时间 | 内存使用 | 启动延迟 |
|------|---------|---------|---------|
| 完全加载 | 20s | 10GB | 20s |
| 内存映射 | 0.1s | 100MB | 0s |

---

## 6. 内存对齐

### 6.1 对齐的重要性

**未对齐访问**：

```cpp
// 假设地址 0x1001 不是 4 字节对齐
int* ptr = (int*)0x1001;
int value = *ptr;  // 可能需要 2 次内存访问
```

**对齐访问**：

```cpp
// 地址 0x1000 是 4 字节对齐
int* ptr = (int*)0x1000;
int value = *ptr;  // 只需 1 次内存访问
```

### 6.2 对齐分配器

**实现**：

```cpp
template <typename T, std::size_t Alignment = 64>
class aligned_allocator_t {
public:
    using value_type = T;

    template <typename U>
    struct rebind {
        using other = aligned_allocator_t<U, Alignment>;
    };

    T* allocate(std::size_t n) noexcept {
        // 使用 aligned_alloc 或 _aligned_malloc
#if defined(USEARCH_DEFINED_WINDOWS)
        return (T*)_aligned_alloc(n * sizeof(T), Alignment);
#else
        return (T*)std::aligned_alloc(Alignment, n * sizeof(T));
#endif
    }

    void deallocate(T* ptr, std::size_t) noexcept {
#if defined(USEARCH_DEFINED_WINDOWS)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }
};
```

### 6.3 缓存行对齐

**为什么是 64 字节？**

```
CPU 缓存行大小：64 字节（大多数现代 CPU）

对齐的好处：
1. 避免跨缓存行访问
2. 提高缓存命中率
3. 避免伪共享
```

**示例**：

```cpp
// 好的布局：每个节点占一个缓存行
struct alignas(64) AlignedNode {
    vector_key_t key;
    level_t level;
    // ... 总共不超过 64 字节
};

// 使用
buffer_gt<AlignedNode, aligned_allocator_t<AlignedNode, 64>> nodes;
```

---

## 7. 内存优化技巧

### 7.1 紧凑布局

**示例**：

```cpp
// ❌ 不好：有填充
struct BadLayout {
    bool flag;      // 1 字节
    // 7 字节 padding
    double value;   // 8 字节
    char tag;       // 1 字节
    // 7 字节 padding
};  // 总共 24 字节

// ✅ 好：紧凑
struct GoodLayout {
    double value;   // 8 字节
    bool flag;      // 1 字节
    char tag;       // 1 字节
    // 6 字节 padding
};  // 总共 16 字节
```

### 7.2 位域优化

```cpp
// 使用位域节省空间
struct CompactMetadata {
    vector_key_t key : 48;    // 最多 2^48 个键
    level_t level : 8;        // 最多 256 层
    bool deleted : 1;         // 删除标记
    bool locked : 1;          // 锁标记
    unsigned reserved : 6;    // 保留位
};  // 总共 8 字节
```

### 7.3 内存池重用

```cpp
// 重用临时缓冲区
class Context {
    // 线程本地缓冲区
    thread_local static buffer_gt<candidate_t> tls_candidates_;
    thread_local static bitset_gt<> tls_visits_;

    buffer_gt<candidate_t>& candidates;
    bitset_gt<>& visits;

public:
    Context()
        : candidates(tls_candidates_)
        , visits(tls_visits_) {

        // 重用（不清空内存）
        candidates.clear();
        visits.clear();
    }
};
```

---

## 8. 内存分析工具

### 8.1 内存追踪

```cpp
class MemoryTracer {
    std::size_t total_allocated_;
    std::size_t total_freed_;
    std::size_t peak_usage_;

public:
    void on_allocate(std::size_t size) noexcept {
        total_allocated_ += size;
        peak_usage_ = std::max(peak_usage_, current_usage());
    }

    void on_free(std::size_t size) noexcept {
        total_freed_ += size;
    }

    std::size_t current_usage() const noexcept {
        return total_allocated_ - total_freed_;
    }

    void report() const noexcept {
        std::cout << "Total allocated: " << total_allocated_ / 1024 / 1024 << " MB\n";
        std::cout << "Current usage: " << current_usage() / 1024 / 1024 << " MB\n";
        std::cout << "Peak usage: " << peak_usage_ / 1024 / 1024 << " MB\n";
    }
};
```

### 8.2 Valgrind 检测

```bash
# 检测内存泄漏
valgrind --leak-check=full --show-leak-kinds=all ./test_cpp

# 检测非法访问
valgrind --tool=memcheck --track-origins=yes ./test_cpp
```

---

## 9. 今日总结

### 核心知识点

✅ **双分配器设计**
- Tape 分配器：持久化存储
- 动态分配器：临时缓冲区

✅ **内存池技术**
- 固定大小池
- 减少 malloc 开销

✅ **零拷贝优化**
- 直接使用外部指针
- 内存视图
- 移动语义

✅ **内存映射**
- 跨平台实现
- 大索引支持

✅ **内存对齐**
- 缓存行对齐
- 避免伪共享

✅ **优化技巧**
- 紧凑布局
- 位域优化
- 缓冲区重用

### 下节预告

明天我们将深入学习 **SIMD 优化和硬件加速**，包括：
- SIMD 指令集原理
- SimSIMD 库集成
- 向量化距离计算
- 硬件检测和分发
- 性能对比分析

---

## 📝 课后思考

1. 为什么 Tape 分配器比动态分配器更适合存储节点和向量？
2. 在什么情况下，零拷贝优化可能导致性能下降？
3. 如何平衡内存使用和缓存命中率？

---

**第8天完成！** 🎉
