# USearch 高级设计模式与架构决策

## 深入解析关键设计决策和权衡

---

## 1. 模板元编程实战

### 1.1 类型萃取（Type Traits）

**问题**：如何在不牺牲性能的前提下，支持多种数据类型？

**USearch 的解决方案**：

```cpp
// 标量类型萃取
template <scalar_kind_t kind>
struct scalar_traits;

template <>
struct scalar_traits<scalar_kind_t::f32_k> {
    using type = float;
    static constexpr std::size_t size = 4;
    static constexpr bool is_floating_point = true;
};

template <>
struct scalar_traits<scalar_kind_t::f16_k> {
    using type = std::uint16_t;  // 存储类型
    using compute_type = float;    // 计算类型
    static constexpr std::size_t size = 2;
    static constexpr bool is_floating_point = true;
};

// 使用示例
template <scalar_kind_t kind>
void process_vector(void* data, std::size_t n) {
    using scalar_type = typename scalar_traits<kind>::type;

    scalar_type* ptr = static_cast<scalar_type*>(data);

    // 编译期分支
    if constexpr (scalar_traits<kind>::is_floating_point) {
        // 浮点特化处理
    } else {
        // 整数特化处理
    }
}
```

**优势**：
- ✅ 编译期类型检查
- ✅ 零运行时开销
- ✅ 代码复用

### 1.2 策略模式（Strategy Pattern）

**距离计算策略**：

```cpp
// 策略接口
template <scalar_kind_t kind>
struct distance_strategy {
    static float distance(void const* a, void const* b, std::size_t dims);
};

// 具体策略
template <>
struct distance_strategy<scalar_kind_t::cos_k> {
    static float distance(void const* a, void const* b, std::size_t dims) {
        float const* fa = static_cast<float const*>(a);
        float const* fb = static_cast<float const*>(b);

        float ab = 0, a2 = 0, b2 = 0;
        #pragma omp simd reduction(+ : ab, a2, b2)
        for (std::size_t i = 0; i < dims; ++i) {
            ab += fa[i] * fb[i];
            a2 += fa[i] * fa[i];
            b2 += fb[i] * fb[i];
        }

        return ab / std::sqrt(a2 * b2);
    }
};

// 使用
template <scalar_kind_t kind>
float compute_distance(void const* a, void const* b, std::size_t dims) {
    return distance_strategy<kind>::distance(a, b, dims);
}
```

---

## 2. RAII 惯用

### 2.1 资源管理

**节点锁（RAII）**：

```cpp
// RAII 锁管理器
class node_lock_t {
    mutexes_gt_t& mutexes_;
    compressed_slot_t slot_;

public:
    explicit node_lock_t(mutexes_gt_t& mutexes, compressed_slot_t slot) noexcept
        : mutexes_(mutexes), slot_(slot) {
        mutexes_.lock(slot_);  // 构造时加锁
    }

    ~node_lock_t() noexcept {
        mutexes_.unlock(slot_);  // 析构时解锁
    }

    // 禁止拷贝和移动
    node_lock_t(const node_lock_t&) = delete;
    node_lock_t& operator=(const node_lock_t&) = delete;
};

// 使用（异常安全）
void safe_function(compressed_slot_t slot) {
    node_lock_t lock(mutexes_, slot);

    // 如果抛出异常，lock 会被自动析构，释放锁
    // 即使有异常，也不会死锁
    do_something_that_may_throw();

}  // lock 自动释放
```

### 2.2 缓冲区管理

**智能指针包装器**：

```cpp
template <typename T, typename Allocator = std::allocator<T>>
class managed_buffer {
    T* data_;
    std::size_t size_;
    Allocator allocator_;

public:
    managed_buffer(std::size_t size, Allocator const& allocator = Allocator())
        : data_(nullptr), size_(size), allocator_(allocator) {

        data_ = std::allocator_traits<Allocator>::allocate(allocator_, size);
    }

    ~managed_buffer() noexcept {
        if (data_) {
            std::allocator_traits<Allocator>::deallocate(allocator_, data_, size_);
        }
    }

    // 移动构造
    managed_buffer(managed_buffer&& other) noexcept
        : data_(other.data_), size_(other.size_), allocator_(other.allocator_) {
        other.data_ = nullptr;
    }

    // 访问接口
    T& operator[](std::size_t index) noexcept {
        return data_[index];
    }

    T const& operator[](std::size_t index) const noexcept {
        return data_[index];
    }

    T* data() noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }
};
```

---

## 3. 惰性分配器（Policy-Based Design）

### 3.1 内存分配策略

**分层分配器设计**：

```cpp
// 分配器策略
template <typename BaseAllocator>
class tracking_allocator : public BaseAllocator {
    std::size_t total_allocated_ = 0;
    std::size_t total_freed_ = 0;

public:
    template <typename T>
    T* allocate(std::size_t n) {
        T* ptr = BaseAllocator::allocate(n);
        total_allocated_ += n * sizeof(T);
        return ptr;
    }

    template <typename T>
    void deallocate(T* ptr, std::size_t n) {
        BaseAllocator::deallocate(ptr, n);
        total_freed_ += n * sizeof(T);
    }

    std::size_t usage() const noexcept {
        return total_allocated_ - total_freed_;
    }
};

// 使用
template <typename T>
using tracked_vector = std::vector<T, tracking_allocator<std::allocator<T>>>;
```

---

## 4. 访问者模式

### 4.1 隐藏实现细节

**节点数据访问**：

```cpp
class node_t {
    byte_t* tape_;

public:
    // 公共接口：隐藏内部表示
    vector_key_t key() const noexcept {
        // 使用代理类，不暴露内部 tape_
        return {tape_};
    }

    level_t level() const noexcept {
        return {tape_ + sizeof(vector_key_t)};
    }

    byte_t* data() const noexcept {
        return tape_;
    }
};

// 代理类：提供类型安全的访问
template <typename T>
class misaligned_ref_gt {
    byte_t* data_;

public:
    explicit misaligned_ref_gt(byte_t* data) noexcept : data_(data) {}

    // 读取操作（类型安全）
    operator T() const noexcept {
        T result;
        std::memcpy(&result, data_, sizeof(T));
        return result;
    }

    // 写入操作
    misaligned_ref_gt& operator=(T const& value) noexcept {
        std::memcpy(data_, &value, sizeof(T));
        return *this;
    }
};
```

---

## 5. 观察者模式

### 5.1 性能监控

**性能观察者**：

```cpp
class performance_observer {
    std::atomic<std::size_t> distance_calculations_{0};
    std::atomic<std::size_t> nodes_visited_{0};

public:
    void record_distance() noexcept {
        distance_calculations_.fetch_add(1, std::memory_order_relaxed);
    }

    void record_node_visit() noexcept {
        nodes_visited_.fetch_add(1, std::memory_order_relaxed);
    }

    void report() const {
        std::cout << "Distance calculations: " << distance_calculations_ << "\n";
        std::cout << "Nodes visited: " << nodes_visited_ << "\n";
    }

    void reset() noexcept {
        distance_calculations_ = 0;
        nodes_visited_ = 0;
    }
};

// 集成到索引中
class index_gt {
    performance_observer perf_;

    // 在关键操作中记录
    compressed_slot_t search(...) {
        perf_.record_node_visit();

        // ... 搜索逻辑 ...

        for (auto neighbor : neighbors) {
            perf_.record_distance();
            // ...
        }

        return result;
    }
};
```

---

## 6. 工厂模式

### 6.1 索引工厂

**创建不同配置的索引**：

```cpp
class index_factory {
public:
    template <typename T, typename Allocator = std::allocator<T>>
    static index_dense_gt<T, std::uint32_t, Allocator>
    create_dense_index(std::size_t dimensions, metric_kind_t metric) {
        index_dense_gt<T, std::uint32_t, Allocator> index;
        index.init(dimensions, metric, scalar_kind_t::f32_k);
        return index;
    }

    template <typename T, typename Allocator = std::allocator<T>>
    static index_dense_gt<T, std::uint32_t, Allocator>
    create_dense_index(std::size_t dimensions, metric_kind_t metric, scalar_kind_t scalar) {
        index_dense_gt<T, std::uint32_t, Allocator> index;
        index.init(dimensions, metric, scalar);
        return index;
    }

    // 工厂方法：根据用途创建
    static auto create_fast_index(std::size_t dimensions) {
        return create_dense_index<float>(dimensions, metric_kind_t::cos_k);
    }

    static auto create_small_index(std::size_t dimensions) {
        index_dense_gt<float, std::uint32_t> index;
        index.init(dimensions, metric_kind_t::cos_k);
        index.expansion(16);  // 小 ef
        return index;
    }

    static auto create_high_precision_index(std::size_t dimensions) {
        index_dense_gt<float, std::uint32_t> index;
        index.init(dimensions, metric_kind_t::cos_k);
        index.expansion(256);  // 大 ef
        return index;
    }
};

// 使用
auto index = index_factory::create_fast_index(128);
```

---

## 7. 模板方法模式

### 7.1 算法骨架

**搜索算法框架**：

```cpp
template <typename Derived>
class search_algorithm_base {
public:
    // 模板方法：子类实现
    compressed_slot_t find_entry_point();
    void greedy_search_level(compressed_slot_t& current, level_t level);
    void beam_search_layer(compressed_slot_t entry, level_t level);

    // 算法骨架
    std::vector<search_result_t> search(float const* query, std::size_t k) {
        // 1. 找到入口点
        compressed_slot_t entry = static_cast<Derived*>(this)->find_entry_point();

        // 2. 高层贪婪搜索
        for (level_t level = max_level_ - 1; level > 0; --level) {
            static_cast<Derived*>(this)->greedy_search_level(entry, level);
        }

        // 3. 底层 Beam Search
        static_cast<Derived*>(this)->beam_search_layer(entry, 0, k);

        // 4. 返回结果
        return extract_results();
    }
};

// 具体实现
class hnsw_algorithm : public search_algorithm_base<hnsw_algorithm> {
public:
    compressed_slot_t find_entry_point() override {
        return entry_point_slot_;
    }

    void greedy_search_level(compressed_slot_t& current, level_t level) override {
        // 实现...
    }

    void beam_search_layer(compressed_slot_t entry, level_t level, std::size_t k) override {
        // 实现...
    }
};
```

---

## 8. 适配器模式

### 8.1 接口适配

**Python 适配器**：

```cpp
// C++ 核心接口
class index_core {
public:
    virtual bool add(vector_key_t key, void const* vector) = 0;
    virtual std::vector<result_t> search(void const* query, std::size_t k) = 0;
};

// Python 适配器
template <typename T>
class python_index_adapter : public index_core {
    index_dense_gt<T, std::uint32_t> index_;

public:
    python_index_adapter(std::size_t dimensions) {
        index_.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k);
    }

    bool add(vector_key_t key, void const* vector) override {
        T const* vec = static_cast<T const*>(vector);
        return index_.add(key, vec);
    }

    std::vector<result_t> search(void const* query, std::size_t k) override {
        T const* q = static_cast<T const*>(query);
        return index_.search(q, k);
    }
};
```

---

## 9. 原型模式

### 9.1 原型管理

**索引克隆**：

```cpp
// 原型接口
class index_prototype {
public:
    virtual index_prototype* clone() const = 0;
    virtual ~index_prototype() = default;
};

// 具体实现
template <typename T, typename Slot>
class index_dense_gt : public index_prototype {
public:
    index_prototype* clone() const override {
        return new index_dense_gt(*this);  // 深拷贝
    }
};

// 使用
index_dense_gt<float, std::uint32_t> original;
// ... 配置 original ...

index_prototype* copy = original.clone();
```

---

## 10. 迭代器模式

### 10.1 邻居迭代器

**STL 风格的迭代器**：

```cpp
class neighbors_ref_t {
    byte_t* tape_;

public:
    // 迭代器类型
    class iterator {
        byte_t* tape_;
        std::size_t i_;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = compressed_slot_t;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = compressed_slot_t;

        iterator(byte_t* tape, std::size_t i) : tape_(tape), i_(i) {}

        // 解引用
        compressed_slot_t operator*() const noexcept {
            return load<compressed_slot_t>(tape_ + shift(i_));
        }

        // 前置递增
        iterator& operator++() noexcept {
            ++i_;
            return *this;
        }

        // 后置递增
        iterator operator++(int) noexcept {
            iterator tmp = *this;
            ++i_;
            return tmp;
        }

        // 比较
        bool operator!=(iterator const& other) const noexcept {
            return i_ != other.i_;
        }
    };

    iterator begin() const noexcept { return {tape_, 0}; }
    iterator end() const noexcept {
        return {tape_, size()};
    }
};
```

**使用 range-based for loop**：

```cpp
neighbors_ref_t neighbors = get_neighbors(node, level);

for (compressed_slot_t neighbor : neighbors) {
    process(neighbor);
}

// 编译器展开为：
// auto&& __range = neighbors;
// auto __begin = __range.begin();
// auto __end = __range.end();
// for (; __begin != __end; ++__begin) {
//     compressed_slot_t neighbor = *__begin;
//     process(neighbor);
// }
```

---

## 11. 构建器模式

### 11.1 索引构建器

**复杂对象的构建**：

```cpp
class IndexBuilder {
    index_dense_gt<float, std::uint32_t> index_;
    std::size_t dimensions_;
    metric_kind_t metric_;
    scalar_kind_t scalar_;
    index_config_t config_;

public:
    IndexBuilder(std::size_t dim) : dimensions_(dim), metric_(metric_kind_t::cos_k),
                                 scalar_(scalar_kind_t::f32_k) {}

    IndexBuilder& set_metric(metric_kind_t metric) {
        metric_ = metric;
        return *this;
    }

    IndexBuilder& set_scalar(scalar_kind_t scalar) {
        scalar_ = scalar;
        return *this;
    }

    IndexBuilder& set_config(index_config_t config) {
        config_ = config;
        return *this;
    }

    IndexBuilder& reserve(std::size_t capacity) {
        // 预分配空间
        return *this;
    }

    index_dense_gt<float, std::uint32_t> build() {
        index_.init(dimensions_, metric_, scalar_);
        index_.reserve(config_.max_nodes);
        return index_;
    }
};

// 使用
auto index = IndexBuilder(128)
    .set_metric(metric_kind_t::l2sq_k)
    .set_scalar(scalar_kind_t::f16_k)
    .set_config({16, 64})
    .reserve(1000000)
    .build();
```

---

## 12. 单例模式（线程安全）

### 12.1 全局配置管理

```cpp
class GlobalConfig {
    static GlobalConfig* instance_;
    static std::mutex mutex_;

    // 配置参数
    std::size_t default_connectivity_ = 16;
    std::size_t default_expansion_ = 64;
    bool use_simd_ = true;

    GlobalConfig() = default;

public:
    // 禁止拷贝
    GlobalConfig(const GlobalConfig&) = delete;
    GlobalConfig& operator=(const GlobalConfig&) = delete;

    static GlobalConfig& instance() {
        std::call_once(init_flag, []() {
            instance_ = new GlobalConfig();
        });
        return *instance_;
    }

    static void set_connectivity(std::size_t M) {
        std::lock_guard<std::mutex> lock(mutex_);
        instance_.default_connectivity_ = M;
    }

    static std::size_t get_connectivity() {
        std::lock_guard<std::mutex> lock(mutex_);
        return instance_.default_connectivity_;
    }
};

// 使用
GlobalConfig::set_connectivity(32);
auto M = GlobalConfig::get_connectivity();
```

---

## 13. 命令模式

### 13.1 序列化命令

**保存和加载命令**：

```cpp
class serialization_command {
public:
    virtual ~serialization_command() = default;

    virtual void execute(index_dense_gt<float, std::uint32_t>& index,
                       char const* path) = 0;
};

class save_command : public serialization_command {
public:
    void execute(index_dense_gt<float, std::uint32_t>& index, char const* path) override {
        auto result = index.save(path);
        if (result)
            throw std::runtime_error(result.what());
    }
};

class load_command : public serialization_command {
public:
    void execute(index_dense_gt<float, std::uint32_t>& index, char const* path) override {
        auto result = index.load(path);
        if (result)
            throw std::runtime_error(result.what());
    }
};

// 命令调用者
class index_manager {
    std::vector<std::unique_ptr<serialization_command>> commands_;

public:
    void add_command(std::unique_ptr<serialization_command> cmd) {
        commands_.push_back(std::move(cmd));
    }

    void execute_all(index_dense_gt<float, std::uint32_t>& index, char const* path) {
        for (auto& cmd : commands_) {
            cmd->execute(index, path);
        }
    }
};

// 使用
index_manager mgr;
mgr.add_command(std::make_unique<save_command>());
mgr.add_command(std::make_unique<load_command>());
mgr.execute_all(index, "index.usearch");
```

---

## 14. 组合模式

### 14.1 复合索引

**多索引组合**：

```cpp
// 组件接口
class index_component {
public:
    virtual ~index_component() = default;
    virtual void add(vector_key_t key, void const* vector) = 0;
    virtual std::vector<result_t> search(void const* query, std::size_t k) = 0;
};

// 基础索引
class base_index : public index_component {
    index_dense_gt<float, std::uint32_t> index_;

public:
    base_index(std::size_t dims) {
        index_.init(dims, metric_kind_t::cos_k);
    }

    void add(vector_key_t key, void const* vector) override {
        index_.add(key, static_cast<float const*>(vector));
    }

    std::vector<result_t> search(void const* query, std::size_t k) override {
        return index_.search(static_cast<float const*>(query), k);
    }
};

// 缓存索引（装饰器）
class cached_index : public index_component {
    std::unique_ptr<index_component> wrapped_;
    std::unordered_map<size_t, std::vector<result_t>> cache_;

public:
    cached_index(std::unique_ptr<index_component> index)
        : wrapped_(std::move(index)) {}

    void add(vector_key_t key, void const* vector) override {
        wrapped_->add(key, vector);
        cache_.clear();  // 清空缓存
    }

    std::vector<result_t> search(void const* query, std::size_t k) override {
        // 计算查询哈希
        size_t query_hash = hash_query(query);

        // 检查缓存
        auto it = cache_.find(query_hash);
        if (it != cache_.end()) {
            return it->second;  // 缓存命中
        }

        // 缓存未命中，执行搜索
        auto results = wrapped_->search(query, k);
        cache_[query_hash] = results;
        return results;
    }

private:
    size_t hash_query(void const* query) const {
        // 简化的哈希函数
        size_t hash = 0;
        float const* q = static_cast<float const*>(query);
        for (std::size_t i = 0; i < 128; ++i) {
            hash ^= std::hash<float>{}(q[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

// 使用
auto base = std::make_unique<base_index>(128);
auto cached = std::make_unique<cached_index>(std::move(base));

cached->add(1, vector);
auto results = cached->search(query, 10);
```

---

## 总结：设计模式的价值

### 优点

1. **可维护性**：代码结构清晰，易于理解和修改
2. **可扩展性**：易于添加新功能
3. **可测试性**：每个组件可独立测试
4. **性能**：零开销抽象

### 关键模式

| 模式 | 应用场景 | 性能影响 |
|------|---------|---------|
| 策略模式 | 距离计算 | 无虚函数 |
| RAII | 资源管理 | 自动清理 |
| 模板方法 | 算法框架 | 编译期多态 |
| 适配器 | 语言绑定 | 轻量级包装 |
| 工厂模式 | 索引创建 | 灵活构建 |
| 迭代器模式 | 数据遍历 | STL兼容 |

这些设计模式共同构成了 USearch 高性能、易用的架构基础。
