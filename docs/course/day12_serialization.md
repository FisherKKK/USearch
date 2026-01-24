# USearch 源码深度解析：第12天
## 序列化和持久化

---

## 📚 今日学习目标

- 深入理解 USearch 二进制格式设计
- 掌握跨平台兼容性策略
- 学习内存映射实现细节
- 理解增量更新机制
- 分析版本迁移方案

---

## 1. 序列化概览

### 1.1 设计目标

**USearch 序列化的核心目标**：

```
1. 跨平台兼容
   - 小端字节序
   - 固定宽度类型
   - 对齐保证

2. 零拷贝加载
   - 内存映射
   - 直接访问

3. 向后兼容
   - 版本号机制
   - 可选字段
   - 扩展性

4. 紧凑存储
   - 无冗余数据
   - 高效编码
```

### 1.2 文件格式

**总体结构**：

```
┌────────────────────────────┐
│ 头部 (index_dense_head_t)  │  64 字节
├────────────────────────────┤
│ 节点数据                    │  可变
├────────────────────────────┤
│ 向量数据                    │  可变
└────────────────────────────┘
```

---

## 2. 头部设计

### 2.1 头部结构

**完整定义**（index_dense.hpp:42-79）：

```cpp
struct index_dense_head_t {
    // 1. 魔数 (7 字节)
    char const* magic;

    // 2. 版本号 (6 字节)
    misaligned_ref_gt<version_t> version_major;
    misaligned_ref_gt<version_t> version_minor;
    misaligned_ref_gt<version_t> version_patch;

    // 3. 类型信息 (16 字节)
    misaligned_ref_gt<metric_kind_t> kind_metric;
    misaligned_ref_gt<scalar_kind_t> kind_scalar;
    misaligned_ref_gt<scalar_kind_t> kind_key;
    misaligned_ref_gt<scalar_kind_t> kind_compressed_slot;

    // 4. 统计信息 (24 字节)
    misaligned_ref_gt<std::uint64_t> count_present;
    misaligned_ref_gt<std::uint64_t> count_deleted;
    misaligned_ref_gt<std::uint64_t> dimensions;

    // 5. 多向量标志 (1 字节)
    misaligned_ref_gt<bool> multi;

    // 6. 配置 (可变)
    // 保留给未来扩展

    // 总大小：64 字节（固定）
    static constexpr std::size_t size_bytes() noexcept { return 64; }
};
```

### 2.2 魔数和版本

**魔数**：

```cpp
#define USEARCH_MAGIC "usearch"

// 验证
bool is_valid_file(char const* path) noexcept {
    std::ifstream file(path, std::ios::binary);
    char magic[7];
    file.read(magic, 7);

    return std::memcmp(magic, USEARCH_MAGIC, 7) == 0;
}
```

**版本号**：

```cpp
// 版本号类型
using version_t = std::uint16_t;

// 当前版本
#define USEARCH_VERSION_MAJOR 2
#define USEARCH_VERSION_MINOR 23
#define USEARCH_VERSION_PATCH 0

// 版本比较
bool is_compatible(version_t major, version_t minor, version_t patch) {
    // 主版本号必须相同
    if (major != USEARCH_VERSION_MAJOR)
        return false;

    // 次版本号向下兼容
    if (minor > USEARCH_VERSION_MINOR)
        return false;

    return true;
}
```

### 2.3 类型信息

**度量类型**：

```cpp
enum class metric_kind_t : std::uint8_t {
    unknown_k = 0,
    ip_k = 'i',           // 内积
    cos_k = 'c',          // 余弦
    l2sq_k = 'e',         // L2 平方
    // ...
};
```

**标量类型**：

```cpp
enum class scalar_kind_t : std::uint8_t {
    f64_k = 10,  // 8 字节
    f32_k = 11,  // 4 字节
    f16_k = 12,  // 2 字节
    bf16_k = 4,  // 2 字节
    i8_k = 23,   // 1 字节
    b1x8_k = 1,  // 1/8 字节
};
```

### 2.4 兼容性检查

**完整检查**：

```cpp
serialization_result_t check_compatibility(
    index_dense_head_t const& head,
    metric_kind_t expected_metric,
    scalar_kind_t expected_scalar,
    std::size_t expected_dimensions) noexcept {

    // 1. 检查魔数
    if (std::memcmp(head.magic, "usearch", 7) != 0)
        return serialization_result_t{"Invalid magic number"};

    // 2. 检查版本
    if (!is_compatible(head.version_major, head.version_minor, head.version_patch))
        return serialization_result_t{"Incompatible version"};

    // 3. 检查度量
    if (head.kind_metric != expected_metric)
        return serialization_result_t{"Metric mismatch"};

    // 4. 检查标量类型
    if (head.kind_scalar != expected_scalar)
        return serialization_result_t{"Scalar type mismatch"};

    // 5. 检查维度
    if (head.dimensions != expected_dimensions)
        return serialization_result_t{"Dimension mismatch"};

    // 6. 检查键类型
    if (head.kind_key != scalar_kind_of<vector_key_t>())
        return serialization_result_t{"Key type mismatch"};

    return serialization_result_t{};  // 成功
}
```

---

## 3. 保存实现

### 3.1 完整流程

**代码实现**（index_dense.hpp:600-700）：

```cpp
serialization_result_t save(char const* path) const noexcept {
    // 1. 打开文件
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return serialization_result_t{"Failed to open file for writing"};

    // 2. 写入头部
    index_dense_head_t head;
    head.fill(
        metric_kind_,
        scalar_kind_,
        dimensions_,
        index_.size(),
        multi_
    );
    out.write(reinterpret_cast<char const*>(&head), index_dense_head_t::size_bytes());

    // 3. 写入节点数据
    std::size_t nodes_bytes = index_.nodes_bytes();
    out.write(reinterpret_cast<char const*>(index_.nodes_.data()), nodes_bytes);

    // 4. 写入向量数据
    std::size_t vectors_bytes = vectors_.size() * vector_bytes_();
    out.write(reinterpret_cast<char const*>(vectors_.data()), vectors_bytes);

    // 5. 验证
    if (!out)
        return serialization_result_t{"Failed to write data"};

    return serialization_result_t{};  // 成功
}
```

### 3.2 Python 绑定

**Python 实现**：

```python
import usearch
import numpy as np

# 创建索引
index = usearch.Index(ndim=128, metric='cos')
vectors = np.random.rand(1000, 128).astype(np.float32)
index.add(np.arange(1000), vectors)

# 保存
index.save("my_index.usearch")

# 支持的参数：
# - path: 文件路径
# - progress: 是否显示进度条
# - dtype: 数据类型（覆盖默认）
```

---

## 4. 加载实现

### 4.1 完整加载

**代码实现**（index_dense.hpp:750-850）：

```cpp
serialization_result_t load(char const* path) noexcept {
    // 1. 打开文件
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return serialization_result_t{"Failed to open file for reading"};

    // 2. 读取头部
    index_dense_head_t head;
    in.read(reinterpret_cast<char*>(&head), index_dense_head_t::size_bytes());

    // 3. 验证兼容性
    auto compatibility_result = check_compatibility(
        head, metric_kind_, scalar_kind_, dimensions_);
    if (!compatization_result)
        return compatibility_result;

    // 4. 读取节点数据
    std::size_t count_present = head.count_present;
    std::size_t nodes_bytes = calculate_nodes_bytes(count_present);

    if (!index_.nodes_.resize(nodes_bytes / sizeof(node_t)))
        return serialization_result_t{"Failed to allocate nodes"};

    in.read(reinterpret_cast<char*>(index_.nodes_.data()), nodes_bytes);

    // 5. 读取向量数据
    std::size_t vectors_bytes = count_present * vector_bytes_();

    if (!vectors_.resize(vectors_bytes))
        return serialization_result_t{"Failed to allocate vectors"};

    in.read(reinterpret_cast<char*>(vectors_.data()), vectors_bytes);

    // 6. 更新元数据
    index_.max_level_ = calculate_max_level();
    index_.entry_point_slot_ = find_entry_point();

    return serialization_result_t{};
}
```

### 4.2 内存映射加载

**零拷贝版本**（index_dense.hpp:3508-3510）：

```cpp
serialization_result_t view(
    memory_mapped_file_t file,
    std::size_t offset = 0,
    index_limits_t limits = {}) noexcept {

    // 1. 映射文件
    memory_mapped_file_t viewed_file = file;
    if (!viewed_file.open(file.path()))
        return serialization_result_t{"Failed to map file"};

    // 2. 验证头部
    byte_t const* data = static_cast<byte_t const*>(viewed_file.data()) + offset;
    index_dense_head_t const* head = reinterpret_cast<index_dense_head_t const*>(data);

    auto compatibility_result = check_compatibility(*head, ...);
    if (!compatibility_result)
        return compatibility_result;

    // 3. 设置视图（不复制数据）
    byte_t const* nodes_data = data + index_dense_head_t::size_bytes();
    byte_t const* vectors_data = nodes_data + head->count_present * node_size_bytes();

    index_.nodes_.data_ = const_cast<node_t*>(reinterpret_cast<node_t const*>(nodes_data));
    vectors_.data_ = const_cast<byte_t*>(vectors_data);

    // 4. 保存文件句柄（防止提前释放）
    viewed_file_ = std::move(viewed_file);

    return serialization_result_t{};
}
```

**Python 接口**：

```python
import usearch

# 方式1：完全加载到内存
index1 = usearch.Index.load("large_index.usearch")

# 方式2：内存映射（零拷贝）
index2 = usearch.Index.restore("large_index.usearch", view=True)

# 内存映射的优势：
# - 启动快（不加载整个文件）
# - 省内存（操作系统按需加载页面）
# - 多进程共享
```

---

## 5. 跨平台兼容性

### 5.1 字节序

**问题**：不同平台使用不同的字节序

```
小端（Little-Endian）: Intel/AMD x86
  0x12345678 → 78 56 34 12

大端（Big-Endian）: ARM/PowerPC（某些配置）
  0x12345678 → 12 34 56 78
```

**USearch 的策略**：统一使用小端序

```cpp
// 写入（转换到小端）
template <typename T>
void write_little_endian(std::ofstream& out, T value) {
    T le_value;
    if constexpr (std::endian::native == std::endian::little) {
        le_value = value;
    } else {
        // 字节序交换
        le_value = byteswap(value);
    }
    out.write(reinterpret_cast<char const*>(&le_value), sizeof(T));
}

// 读取（从小端转换）
template <typename T>
T read_little_endian(std::ifstream& in) {
    T le_value;
    in.read(reinterpret_cast<char*>(&le_value), sizeof(T));

    if constexpr (std::endian::native == std::endian::little) {
        return le_value;
    } else {
        return byteswap(le_value);
    }
}
```

### 5.2 数据类型宽度

**固定宽度类型**：

```cpp
// 使用固定宽度类型
using version_t = std::uint16_t;       // 始终 2 字节
using vector_key_t = std::uint64_t;    // 始终 8 字节
using compressed_slot_t = std::uint32_t; // 始终 4 字节
using distance_t = float;              // IEEE 754 单精度
```

**避免的平台特定类型**：

```cpp
// ❌ 不好：大小取决于平台
size_t   // 可能是 4 或 8 字节
intptr_t // 可能是 4 或 8 字节
long     // Windows 4 字节，Linux 8 字节

// ✅ 好：固定大小
std::uint64_t
std::int32_t
```

### 5.3 结构体对齐

**问题**：不同编译器的对齐策略不同

**解决方案**：使用 `#pragma pack` 或手动序列化

```cpp
// 方式1：手动序列化（USearch 采用）
void write_header(std::ofstream& out, index_dense_head_t const& head) {
    // 逐个字段写入，不依赖结构体布局
    write_bytes(out, head.magic, 7);
    write_little_endian(out, head.version_major);
    write_little_endian(out, head.version_minor);
    write_little_endian(out, head.version_patch);
    // ...
}

// 方式2：确保对齐（如果使用结构体）
#pragma pack(push, 1)
struct PackedHeader {
    char magic[7];
    std::uint16_t version_major;
    // ...
};
#pragma pack(pop)
```

---

## 6. 增量更新

### 6.1 追加写入

**策略**：支持增量添加向量

```cpp
serialization_result_t save(char const* path, bool append) const noexcept {
    if (append) {
        // 打开现有文件追加
        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out)
            return serialization_result_t{"Failed to open for appending"};

        // 只写入新增的节点和向量
        std::size_t new_count = index_.size() - saved_count_;
        write_new_nodes(out, new_count);
        write_new_vectors(out, new_count);

        // 更新头部的计数
        update_header_count(path, index_.size());
    } else {
        // 完全重写
        return save(path);
    }

    return serialization_result_t{};
}
```

### 6.2 合并索引

**场景**：分布式训练后合并

```cpp
index_dense_gt merge(std::vector<index_dense_gt> const& indexes) {
    index_dense_gt merged;
    merged.init(indexes[0].dimensions_, indexes[0].metric_kind_, indexes[0].scalar_kind_);

    for (auto const& index : indexes) {
        // 遍历索引中的所有向量
        for (std::size_t i = 0; i < index.size(); ++i) {
            auto key = index.get_key(i);
            auto vector = index.get_vector(i);
            merged.add(key, vector);
        }
    }

    return merged;
}
```

---

## 7. 版本迁移

### 7.1 向后兼容

**策略**：旧版本可以读取新版本（在兼容范围内）

```cpp
// 版本迁移表
struct migration_entry {
    version_t from_major;
    version_t from_minor;
    version_t from_patch;
    bool (*migrate)(byte_t* data, std::size_t size);
};

std::vector<migration_entry> migrations = {
    {2, 10, 0, migrate_2_10_to_2_11},
    {2, 15, 0, migrate_2_15_to_2_16},
    // ...
};

bool migrate(byte_t* data, std::size_t size, index_dense_head_t& head) {
    // 找到适用的迁移
    for (auto const& entry : migrations) {
        if (head.version_major == entry.from_major &&
            head.version_minor == entry.from_minor &&
            head.version_patch == entry.from_patch) {
            return entry.migrate(data, size);
        }
    }
    return true;  // 不需要迁移
}
```

### 7.2 向前兼容

**策略**：新版本可以读取旧版本

```cpp
// 1. 检测版本并设置默认值
void load_legacy_format(index_dense_head_t const& head) {
    if (head.version_minor < 20) {
        // 旧版本没有 multi 标志
        multi_ = false;
    }

    if (head.version_minor < 15) {
        // 旧版本没有 deleted 计数
        deleted_count_ = 0;
    }

    // ...
}
```

---

## 8. 性能优化

### 8.1 批量写入

**减少系统调用**：

```cpp
void write_batch(std::ofstream& out, void const* data, std::size_t size) {
    // 设置大缓冲区
    constexpr std::size_t buffer_size = 1024 * 1024;  // 1 MB
    out.rdbuf()->pubsetbuf(nullptr, buffer_size);

    // 一次性写入
    out.write(reinterpret_cast<char const*>(data), size);
}
```

### 8.2 压缩

**可选压缩**：

```cpp
serialization_result_t save_compressed(char const* path) const noexcept {
    // 1. 序列化到内存
    std::vector<byte_t> buffer;
    serialize_to_buffer(buffer);

    // 2. 压缩（使用 LZ4/ZSTD）
    std::vector<byte_t> compressed;
    compress(buffer, compressed);

    // 3. 写入
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char const*>(compressed.data()), compressed.size());

    return serialization_result_t{};
}
```

---

## 9. 实战练习

### 练习 1：验证文件格式

```python
import struct

def verify_usearch_file(path):
    with open(path, 'rb') as f:
        # 读取头部
        magic = f.read(7)
        version = struct.unpack('<HHH', f.read(6))
        metric = struct.unpack('<B', f.read(1))[0]
        scalar = struct.unpack('<B', f.read(1))[0]
        key_scalar = struct.unpack('<B', f.read(1))[0]
        slot_scalar = struct.unpack('<B', f.read(1))[0]
        count = struct.unpack('<Q', f.read(8))[0]
        deleted = struct.unpack('<Q', f.read(8))[0]
        dimensions = struct.unpack('<Q', f.read(8))[0]
        multi = struct.unpack('<?', f.read(1))[0]

        print(f"Magic: {magic}")
        print(f"Version: {version}")
        print(f"Metric: {metric}")
        print(f"Scalar: {scalar}")
        print(f"Count: {count}")
        print(f"Dimensions: {dimensions}")
        print(f"Multi: {multi}")

verify_usearch_file("my_index.usearch")
```

### 练习 2：合并索引

```python
import usearch
import numpy as np

# 创建两个索引
index1 = usearch.Index(ndim=128)
index2 = usearch.Index(ndim=128)

# 添加数据（使用不同的键范围）
vectors1 = np.random.rand(1000, 128).astype(np.float32)
index1.add(np.arange(1000), vectors1)

vectors2 = np.random.rand(1000, 128).astype(np.float32)
index2.add(np.arange(1000, 2000), vectors2)

# 保存
index1.save("part1.usearch")
index2.save("part2.usearch")

# 加载并合并
merged = usearch.Index(ndim=128)
merged.load("part1.usearch")
merged.merge("part2.usearch")  # 假设实现了 merge

# 验证
print(f"Total vectors: {merged.size()}")  # 应该是 2000
```

---

## 10. 今日总结

### 核心知识点

✅ **头部设计**
- 64 字节固定格式
- 魔数和版本
- 类型信息

✅ **序列化实现**
- 保存流程
- 加载流程
- 兼容性检查

✅ **内存映射**
- 零拷贝加载
- 操作系统管理

✅ **跨平台兼容**
- 字节序处理
- 固定宽度类型
- 对齐保证

✅ **增量更新**
- 追加写入
- 索引合并

✅ **版本迁移**
- 向后兼容
- 向前兼容
- 迁移脚本

### 下节预告

明天我们将深入学习 **性能优化技巧**，包括：
- 缓存优化
- 预取策略
- 分支预测
- 算法优化
- 性能分析工具

---

## 📝 课后思考

1. 为什么 USearch 选择固定64字节的头部而不是可变长度？
2. 在什么情况下，内存映射加载可能比完全加载更慢？
3. 如何设计一个既向后兼容又向前兼容的文件格式？

---

**第12天完成！** 🎉
