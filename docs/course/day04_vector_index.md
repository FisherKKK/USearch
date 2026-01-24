# USearch 源码深度解析：第4天
## 向量索引实现

---

## 📚 今日学习目标

- 深入理解 `index_dense_gt` 类的设计
- 掌握向量存储策略和内存布局
- 学习多向量支持（Multi-Vector）
- 理解索引配置和限制机制
- 分析序列化和反序列化实现

---

## 1. 向量索引架构

### 1.1 类层次结构

```
index_gt (index.hpp)
    ↓ 泛型 HNSW 实现
index_dense_gt (index_dense.hpp)
    ↓ 密集向量专用实现
C/Python/JavaScript 绑定
    ↓ 语言特定接口
用户代码
```

**代码位置**（index_dense.hpp:100-300）：

```cpp
template <typename vector_key_at = std::uint64_t,           // 向量键类型
          typename compressed_slot_at = std::uint32_t,       // 槽位类型
          typename allocator_at = std::allocator<byte_t>>    // 分配器
class index_dense_gt {
    // 核心成员
    using index_t = index_gt<vector_key_at, compressed_slot_at, allocator_at>;
    index_t index_;

    // 向量维度
    std::size_t dimensions_;

    // 配置
    scalar_kind_t scalar_kind_;
    metric_kind_t metric_kind_;
};
```

### 1.2 设计理念

**为什么需要 `index_dense_gt`？**

```cpp
// index_gt - 通用实现
// - 支持任意数据类型（通过迭代器）
// - 需要用户管理向量存储
// - 更灵活但使用复杂

// index_dense_gt - 密集向量专用
// - 固定维度向量
// - 自动管理向量存储
// - 更简单易用
```

**对比**：

| 特性 | index_gt | index_dense_gt |
|------|----------|----------------|
| 数据类型 | 任意 | 密集向量 |
| 向量存储 | 外部 | 内置 |
| 维度 | 可变 | 固定 |
| 易用性 | 复杂 | 简单 |
| 灵活性 | 高 | 中 |

---

## 2. 向量存储策略

### 2.1 内存布局

**向量和节点的关系**：

```
┌─────────────────────────────────────────┐
│ nodes_[i] - 第 i 个节点                  │
├─────────────────────────────────────────┤
│ key: 123                                │
│ level: 2                                │
│ neighbors: [...]                        │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│ vectors_[slot] - 第 i 个向量的数据       │
├─────────────────────────────────────────┤
│ dim[0]: 0.1                             │
│ dim[1]: 0.5                             │
│ dim[2]: -0.3                            │
│ ...                                     │
└─────────────────────────────────────────┘
```

**代码实现**（index_dense.hpp:180-220）：

```cpp
class index_dense_gt {
    // 向量数据存储
    buffer_gt<byte_t, allocator_at> vectors_;

    // 获取向量的起始位置
    byte_t* vector_data_(compressed_slot_t slot) noexcept {
        return vectors_.data() + static_cast<std::size_t>(slot) * vector_bytes_();
    }

    byte_t const* vector_data_(compressed_slot_t slot) const noexcept {
        return vectors_.data() + static_cast<std::size_t>(slot) * vector_bytes_();
    }

    // 单个向量的大小（字节）
    std::size_t vector_bytes_() const noexcept {
        return dimensions_ * scalar_size(scalar_kind_);
    }
};
```

### 2.2 标量类型支持

**支持的类型**（index.hpp:138-159）：

```cpp
enum class scalar_kind_t : std::uint8_t {
    // 浮点类型
    f64_k = 10,  // 双精度（8 字节）
    f32_k = 11,  // 单精度（4 字节）
    f16_k = 12,  // 半精度（2 字节）
    bf16_k = 4,  // 脑浮点（2 字节）

    // 整数类型
    i8_k = 23,   // 8位有符号整数
    u40_k = 2,   // 40位无符号整数

    // 二值类型
    b1x8_k = 1,  // 1-bit x 8 压缩
};
```

**大小计算**（index.hpp:161-175）：

```cpp
inline std::size_t scalar_size(scalar_kind_t kind) noexcept {
    switch (kind) {
        case scalar_kind_t::f64_k: return 8;
        case scalar_kind_t::f32_k: return 4;
        case scalar_kind_t::f16_k: return 2;
        case scalar_kind_t::bf16_k: return 2;
        case scalar_kind_t::i8_k: return 1;
        case scalar_kind_t::b1x8_k: return 1;
        case scalar_kind_t::u40_k: return 5;
        default: return 0;
    }
}
```

**示例计算**：

```cpp
// 128维 f32 向量
std::size_t size = 128 * scalar_size(scalar_kind_t::f32_k);
// size = 128 * 4 = 512 字节

// 768维 f16 向量（BERT embeddings）
std::size_t size = 768 * scalar_size(scalar_kind_t::f16_k);
// size = 768 * 2 = 1536 字节
```

### 2.3 向量访问器

**迭代器设计**（index.hpp:2925-2950）：

```cpp
// 向量迭代器（用于距离计算）
class vector_iterator_t {
    byte_t const* data_;
    scalar_kind_t kind_;

public:
    vector_iterator_t(byte_t const* data, scalar_kind_t kind)
        : data_(data), kind_(kind) {}

    // 转换到 float（用于距离计算）
    float to_float_at(std::size_t i) const noexcept {
        switch (kind_) {
            case scalar_kind_t::f32_k:
                return reinterpret_cast<float const*>(data_)[i];
            case scalar_kind_t::f16_k:
                return f16_to_f32(reinterpret_cast<std::uint16_t const*>(data_)[i]);
            case scalar_kind_t::bf16_k:
                return bf16_to_f32(reinterpret_cast<std::uint16_t const*>(data_)[i]);
            case scalar_kind_t::i8_k:
                return static_cast<float>(reinterpret_cast<std::int8_t const*>(data_)[i]);
            // ...
        }
    }
};
```

---

## 3. 多向量支持（Multi-Vector）

### 3.1 应用场景

**什么是多向量？**

```
单个实体用多个向量表示：
- 文档：段落向量 + 标题向量
- 图像：全局特征 + 局部特征
- 视频：帧级特征 + 视频级特征
```

### 3.2 存储布局

**单向量 vs 多向量**：

```
单向量（multi = false）：
┌─────────┬─────────┬─────────┐
│ vec[0]  │ vec[1]  │ vec[2]  │
│ 128 dim │ 128 dim │ 128 dim │
└─────────┴─────────┴─────────┘

多向量（multi = true）：
┌─────────┬─────────┬─────────┐
│ vec[0]  │ vec[0]  │ vec[0]  │  ← 实体0的3个向量
│ vec[1]  │ vec[1]  │ vec[1]  │  ← 实体1的3个向量
└─────────┴─────────┴─────────┘
  向量1     向量2     向量3
```

**代码实现**（index_dense.hpp:42-79）：

```cpp
struct index_dense_head_t {
    // ... 其他字段 ...
    misaligned_ref_gt<bool> multi;  // 多向量标志
};
```

### 3.3 距离聚合

**策略**：

```cpp
// 当实体有多个向量时，如何计算距离？
enum class multi_vector_strategy {
    max_k,    // 最大距离（最坏情况）
    min_k,    // 最小距离（最好情况）
    avg_k,    // 平均距离
};
```

**实现**（index.hpp:3100-3150）：

```cpp
distance_t measure(
    value_at&& query,
    citerator_at&& data,
    metric_at&& metric) const noexcept {

    if (!multi_) {
        // 单向量：直接计算距离
        return metric(query, data);
    } else {
        // 多向量：聚合多个距离
        distance_t result = 0;
        for (std::size_t i = 0; i < vectors_per_entity_; ++i) {
            distance_t dist = metric(query + i * dimensions_, data + i * dimensions_);
            result = std::max(result, dist);  // max 策略
        }
        return result;
    }
}
```

---

## 4. 索引配置

### 4.1 配置结构

**完整配置**（index.hpp:2240-2260）：

```cpp
struct index_config_t {
    // 基础连接数（第0层）
    std::size_t connectivity_base = 16;

    // 其他层的连接数
    std::size_t connectivity_layer = 16;

    // 层级乘数
    double ml = 1.0 / std::log(16.0);

    // 扩展因子（搜索时的候选集大小）
    std::size_t expansion = 64;
};
```

### 4.2 参数影响分析

**connectivity（连接数）**：

```
connectivity = 8:
  - 内存：低
  - 构建速度：快
  - 搜索精度：低
  - 适用场景：内存受限

connectivity = 16:
  - 内存：中
  - 构建速度：中
  - 搜索精度：中
  - 适用场景：平衡场景

connectivity = 64:
  - 内存：高
  - 构建速度：慢
  - 搜索精度：高
  - 适用场景：高精度要求
```

**expansion（扩展因子）**：

```python
# Python 示例
index = usearch.Index(
    ndim=128,
    metric='cos',
    connectivity=16,
    expansion=128,  # 候选集大小
)

# expansion 越大，搜索越精确，但越慢
```

**代码验证**（index.hpp:2262-2280）：

```cpp
// 搜索时的 ef 参数
std::size_t expansion_;

// 设置
void expansion(std::size_t v) noexcept { expansion_ = v; }

// 获取
std::size_t expansion() const noexcept { return expansion_; }
```

### 4.3 限制机制

**索引限制**（index.hpp:2212-2238）：

```cpp
struct index_limits_t {
    // 最大节点数
    std::size_t max_nodes = 0;  // 0 = 无限制

    // 最大层级
    level_t max_level = 16;     // 防止无限增长

    // 最大边数
    std::size_t max_edges = 0;  // 0 = 无限制
};

// 使用
index_limits_t limits;
limits.max_nodes = 1000000;  // 最多 100 万向量
limits.max_level = 12;       // 最多 12 层

index.init(config, limits);
```

**动态检查**（index.hpp:2350-2370）：

```cpp
bool add(vector_key_t key, vector_data_t const* vector) noexcept {
    // 检查限制
    if (limits_.max_nodes > 0 && nodes_count_ >= limits_.max_nodes) {
        return false;  // 达到上限
    }

    if (max_level_ >= limits_.max_level) {
        return false;  // 层级过高
    }

    // 继续添加...
}
```

---

## 5. 核心操作实现

### 5.1 初始化索引

**C++ 接口**（index_dense.hpp:300-350）：

```cpp
bool init(std::size_t dimensions,
          metric_kind_t metric = metric_kind_t::cos_k,
          scalar_kind_t scalar = scalar_kind_t::f32_k,
          index_config_t config = index_config_t(),
          index_limits_t limits = index_limits_t()) noexcept {

    // 1. 保存配置
    dimensions_ = dimensions;
    metric_kind_ = metric;
    scalar_kind_ = scalar;
    config_ = config;
    limits_ = limits;

    // 2. 初始化 HNSW 索引
    if (!index_.init(config, limits))
        return false;

    // 3. 配置距离度量
    if (!index_.metric_(metric, scalar))
        return false;

    return true;
}
```

**Python 接口**：

```python
index = usearch.Index(
    ndim=128,           # 维度
    metric='cos',       # 度量
    dtype='f32',        # 类型
    connectivity=16,    # 连接数
    expansion=64,       # 扩展因子
)
```

### 5.2 添加向量

**C++ 实现**（index_dense.hpp:400-450）：

```cpp
bool add(vector_key_t key,
         vector_data_t const* vector,
         bool copy = true) noexcept {

    // 1. 分配槽位
    compressed_slot_t slot = reserve_slot_();
    if (slot == missing_slot())
        return false;

    // 2. 复制向量数据
    if (copy) {
        std::memcpy(vector_data_(slot), vector, vector_bytes_());
    } else {
        // 零拷贝：直接使用外部指针
        // 注意：调用者需确保向量生命周期
    }

    // 3. 添加到 HNSW 图
    vector_iterator_t iterator(vector_data_(slot), scalar_kind_);
    if (!index_.add(key, slot, iterator)) {
        release_slot_(slot);
        return false;
    }

    return true;
}
```

**Python 接口**：

```python
# 添加单个向量
index.add(1, vector_1)

# 批量添加
keys = np.array([1, 2, 3], dtype=np.uint64)
vectors = np.random.rand(3, 128).astype(np.float32)
index.add(keys, vectors)
```

### 5.3 搜索向量

**C++ 实现**（index_dense.hpp:500-550）：

```cpp
std::vector<search_result_t> search(
    vector_data_t const* query,
    std::size_t count = 10) const noexcept {

    // 1. 创建查询迭代器
    vector_iterator_t query_iter(query, scalar_kind_);

    // 2. 执行搜索
    return index_.search(query_iter, count);
}
```

**返回结构**（index.hpp:2103-2108）：

```cpp
struct search_result_t {
    vector_key_t key;        // 向量键
    distance_t distance;     // 距离值
};
```

**Python 接口**：

```python
# 搜索
query = np.random.rand(128).astype(np.float32)
results = index.search(query, k=10)

# 遍历结果
for key, distance in results:
    print(f"key={key}, distance={distance:.4f}")
```

---

## 6. 序列化和持久化

### 6.1 文件格式

**头部结构**（index_dense.hpp:42-79）：

```cpp
struct index_dense_head_t {
    // 魔数（7 字节）
    char const* magic = "usearch";

    // 版本（6 字节）
    misaligned_ref_gt<version_t> version_major;
    misaligned_ref_gt<version_t> version_minor;
    misaligned_ref_gt<version_t> version_patch;

    // 类型信息（12 字节）
    misaligned_ref_gt<metric_kind_t> kind_metric;
    misaligned_ref_gt<scalar_kind_t> kind_scalar;
    misaligned_ref_gt<scalar_kind_t> kind_key;
    misaligned_ref_gt<scalar_kind_t> kind_compressed_slot;

    // 统计信息（24 字节）
    misaligned_ref_gt<std::uint64_t> count_present;
    misaligned_ref_gt<std::uint64_t> count_deleted;
    misaligned_ref_gt<std::uint64_t> dimensions;

    // 多向量标志（1 字节）
    misaligned_ref_gt<bool> multi;

    // 总大小：64 字节
};
```

### 6.2 保存索引

**实现**（index_dense.hpp:600-700）：

```cpp
serialization_result_t save(char const* path) const noexcept {
    // 1. 打开文件
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return serialization_result_t{"Failed to open file"};

    // 2. 写入头部
    index_dense_head_t head;
    head.fill(
        metric_kind_,
        scalar_kind_,
        dimensions_,
        index_.size(),
        multi_
    );
    out.write(reinterpret_cast<char const*>(&head), sizeof(head));

    // 3. 写入节点数据
    std::size_t nodes_bytes = index_.nodes_bytes();
    out.write(reinterpret_cast<char const*>(index_.nodes_.data()), nodes_bytes);

    // 4. 写入向量数据
    std::size_t vectors_bytes = vectors_.size() * vector_bytes_();
    out.write(reinterpret_cast<char const*>(vectors_.data()), vectors_bytes);

    return serialization_result_t{};
}
```

### 6.3 加载索引

**实现**（index_dense.hpp:750-850）：

```cpp
serialization_result_t load(char const* path) noexcept {
    // 1. 打开文件
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return serialization_result_t{"Failed to open file"};

    // 2. 读取并验证头部
    index_dense_head_t head;
    in.read(reinterpret_cast<char*>(&head), sizeof(head));

    if (!std::strcmp(head.magic, "usearch"))
        return serialization_result_t{"Invalid magic number"};

    // 3. 检查兼容性
    if (head.kind_metric != metric_kind_ ||
        head.kind_scalar != scalar_kind_ ||
        head.dimensions != dimensions_)
        return serialization_result_t{"Incompatible index format"};

    // 4. 读取节点数据
    std::size_t nodes_bytes = ...;
    index_.nodes_.resize(nodes_bytes / sizeof(node_t));
    in.read(reinterpret_cast<char*>(index_.nodes_.data()), nodes_bytes);

    // 5. 读取向量数据
    std::size_t vectors_bytes = ...;
    vectors_.resize(vectors_bytes / vector_bytes_());
    in.read(reinterpret_cast<char*>(vectors_.data()), vectors_bytes);

    return serialization_result_t{};
}
```

### 6.4 内存映射

**零拷贝加载**（index_dense.hpp:3508-3510）：

```cpp
serialization_result_t view(
    memory_mapped_file_t file,
    std::size_t offset = 0,
    index_limits_t limits = {}) noexcept {

    // 直接映射文件到内存，不复制数据
    memory_mapped_file_t viewed_file = file;

    // 验证头部
    index_dense_head_t* head = reinterpret_cast<index_dense_head_t*>(
        viewed_file.data() + offset);

    // 设置视图
    index_.nodes_.data_ = reinterpret_cast<node_t*>(
        viewed_file.data() + offset + sizeof(index_dense_head_t));

    vectors_.data_ = reinterpret_cast<byte_t*>(
        index_.nodes_.data_ + head->count_present);

    return serialization_result_t{};
}
```

**Python 使用**：

```python
# 加载到内存
index = usearch.Index.load("index.usearch")

# 内存映射（适合大索引）
index = usearch.Index.restore("large_index.usearch", view=True)
```

---

## 7. 内存估算

### 7.1 计算公式

**总内存 = 节点内存 + 向量内存 + 图边内存**

```cpp
std::size_t memory_usage() const noexcept {
    // 1. 节点数组
    std::size_t nodes_mem = nodes_.size() * sizeof(node_t);

    // 2. 向量数据
    std::size_t vectors_mem = vectors_.size() * vector_bytes_();

    // 3. 图边（包含在节点中，但额外计算）
    std::size_t edges_mem = 0;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        node_t node = nodes_[i];
        level_t level = node.level();
        edges_mem += (level + 1) * (
            sizeof(neighbors_count_t) +
            config_.connectivity * sizeof(compressed_slot_t)
        );
    }

    return nodes_mem + vectors_mem + edges_mem;
}
```

### 7.2 实例计算

**场景**：100万个128维f32向量

```python
N = 1_000_000
D = 128
bytes_per_float = 4
M = 16
avg_levels = 3

# 向量内存
vectors_mem = N * D * bytes_per_float  # = 512 MB

# 节点内存（元数据）
nodes_mem = N * (8 + 2)  # key(8) + level(2) = 10 MB

# 边内存
edges_mem = N * avg_levels * (2 + M * 4)  # = 192 MB

# 总内存
total_mem = vectors_mem + nodes_mem + edges_mem  # ≈ 714 MB
```

**对比**：

| 数据类型 | 向量大小 | 总内存（100万） |
|---------|---------|----------------|
| f32 | 512 bytes | 714 MB |
| f16 | 256 bytes | 458 MB |
| i8 | 128 bytes | 330 MB |
| b1x8 | 16 bytes | 218 MB |

---

## 8. 性能优化

### 8.1 批量操作

**批量添加**（index_dense.hpp:400-450）：

```cpp
template <typename keys_at, typename vectors_at>
std::size_t add_many(
    keys_at const& keys,
    vectors_at const& vectors,
    std::size_t batch_size = 1000) noexcept {

    std::size_t added = 0;

    // 分批处理
    for (std::size_t i = 0; i < keys.size(); i += batch_size) {
        std::size_t batch_end = std::min(i + batch_size, keys.size());

        #pragma omp parallel for
        for (std::size_t j = i; j < batch_end; ++j) {
            if (add(keys[j], vectors[j]))
                ++added;
        }
    }

    return added;
}
```

### 8.2 预分配

**避免重复分配**：

```cpp
// 预先分配空间
index_dense_gt<float> index;
index.init(128);

// 预分配 100 万个向量的空间
index.reserve(1'000'000);

// 现在添加操作会更快（不需要重新分配）
for (std::size_t i = 0; i < 1'000'000; ++i) {
    index.add(i, vectors[i]);
}
```

**实现**（index_dense.hpp:280-290）：

```cpp
bool reserve(std::size_t capacity) noexcept {
    // 预分配节点空间
    if (!index_.nodes_.reserve(capacity))
        return false;

    // 预分配向量空间
    if (!vectors_.reserve(capacity))
        return false;

    return true;
}
```

---

## 9. 实战练习

### 练习 1：创建自定义索引

```cpp
// 创建一个支持 bf16 的索引
index_dense_gt<std::uint64_t, std::uint32_t> index;
index.init(
    768,                          // BERT 维度
    metric_kind_t::cos_k,
    scalar_kind_t::bf16_k        // 使用脑浮点
);

// 添加向量
std::vector<std::uint16_t> bert_vector(768);  // bf16 数据
index.add(1, bert_vector.data());

// 搜索
std::vector<std::uint16_t> query(768);
auto results = index.search(query.data(), 10);
```

### 练习 2：内存对比实验

```python
import usearch
import numpy as np

dimensions = [128, 256, 512, 768, 1024]
scalars = ['f32', 'f16', 'i8']

for dim in dimensions:
    for scalar in scalars:
        index = usearch.Index(ndim=dim, dtype=scalar)

        # 添加 10000 个向量
        vectors = np.random.rand(10000, dim).astype(scalar)
        index.add(np.arange(10000), vectors)

        # 估算内存
        memory = index.memory_usage()
        print(f"dim={dim}, scalar={scalar}, memory={memory/1024/1024:.2f} MB")
```

### 练习 3：序列化测试

```python
import usearch
import numpy as np

# 创建并保存索引
index1 = usearch.Index(ndim=128)
vectors = np.random.rand(1000, 128).astype(np.float32)
index1.add(np.arange(1000), vectors)

# 保存
index1.save("test_index.usearch")

# 加载
index2 = usearch.Index(ndim=128)
index2.load("test_index.usearch")

# 验证
query = np.random.rand(128).astype(np.float32)
results1 = index1.search(query, 10)
results2 = index2.search(query, 10)

assert len(results1) == len(results2)
for r1, r2 in zip(results1, results2):
    assert r1[0] == r2[0]  # 键相同
    assert abs(r1[1] - r2[1]) < 1e-6  # 距离相同
```

---

## 10. 今日总结

### 核心知识点

✅ **index_dense_gt 设计**
- 密集向量专用实现
- 自动向量存储管理
- 简化的用户接口

✅ **向量存储**
- 紧凑的内存布局
- 多种标量类型支持
- 高效的访问模式

✅ **多向量支持**
- 单实体多向量表示
- 距离聚合策略

✅ **配置和限制**
- 连接数和扩展因子
- 动态限制检查

✅ **序列化**
- 64字节头部设计
- 跨平台兼容性
- 内存映射支持

✅ **性能优化**
- 批量操作
- 预分配策略
- 内存估算

### 下节预告

明天我们将深入学习 **距离计算系统**，包括：
- 各种距离度量的数学原理
- SIMD 优化的距离计算
- 自定义度量支持
- 硬件加速集成

---

## 📝 课后思考

1. 为什么 `index_dense_gt` 比 `index_gt` 更适合生产环境？
2. 在什么情况下应该使用多向量功能？
3. 内存映射相比普通加载有什么优势和劣势？

---

**第4天完成！** 🎉
