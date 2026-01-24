# USearch 源码深度解析：第1天
## USearch 概览与环境搭建

---

## 📚 今日学习目标

- 理解 USearch 的设计哲学和核心特性
- 掌握项目的整体架构
- 搭建完整的学习和开发环境
- 运行第一个示例程序

---

## 1. USearch 简介

### 1.1 什么是 USearch？

**USearch** 是一个高性能的**单文件向量搜索引擎**，实现了 HNSW（Hierarchical Navigable Small World）算法用于**近似最近邻（ANN）搜索**。

**核心特点：**
- 🎯 **单文件设计**：核心代码仅 4.6K 行，包含在 `include/usearch/index.hpp`
- 🌍 **多语言支持**：原生支持 10+ 种语言（Python、JavaScript、Rust、Go、Java 等）
- ⚡ **极致性能**：硬件感知的 SIMD 优化，支持 AVX-512、NEON、SVE 等
- 🔧 **零依赖**：C++ 核心不依赖任何第三方库
- 📦 **多精度支持**：f64、f32、f16、bf16、i8、b1 等多种量化方式

### 1.2 设计哲学

USearch 遵循以下核心设计原则：

```cpp
// 从代码中可以看出设计的核心理念：
// 1. 单文件头文件库 - index.hpp 是完全独立的
// 2. 模板化设计 - 支持任意距离度量和数据类型
// 3. 硬件无关性 - 默认情况下不依赖特定硬件
// 4. 可选优化 - 通过编译选项启用 SIMD、OpenMP 等
```

**为什么选择单文件设计？**
- ✅ 易于集成：只需复制一个头文件
- ✅ 版本控制：不需要管理复杂的依赖关系
- ✅ 编译速度：减少头文件包含开销
- ✅ 内联优化：编译器可以更好地优化小函数

### 1.3 应用场景

USearch 适用于以下场景：

| 场景 | 说明 | 示例 |
|------|------|------|
| **语义搜索** | 文本、图像向量检索 | RAG 系统、图像搜索 |
| **推荐系统** | 找到相似用户/物品 | 协同过滤、内容推荐 |
| **聚类分析** | 找到相似的点集合 | KNN、密度聚类 |
| **异常检测** | 找到远离正常点的数据 | 欺诈检测、故障诊断 |
| **向量数据库** | 大规模向量索引 | Milvus、Qdrant 后端 |

---

## 2. 核心架构概览

### 2.1 三层架构

USearch 采用清晰的分层设计：

```
┌─────────────────────────────────────────┐
│  语言绑定层 (Language Bindings)         │
│  Python, JS, Rust, Go, Java, C#, ...   │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  高层接口层 (High-Level API)            │
│  index_dense.hpp - 密集向量索引         │
│  index_plugins.hpp - 工具和序列化       │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  核心层 (Core)                          │
│  index.hpp - HNSW 图实现                │
│  独立的 C++11 实现                      │
└─────────────────────────────────────────┘
```

### 2.2 核心文件说明

#### `include/usearch/index.hpp` (4600+ 行)

**作用**：HNSW 图的核心实现

**关键类**：
- `index_gt<...>` - 通用 HNSW 索引类模板
- `node_t` - 图节点表示
- `neighbors_ref_t` - 邻接表引用
- `metric_punned_t` - 距离计算函数包装

**代码位置**：`include/usearch/index.hpp:2080-4107`

#### `include/usearch/index_dense.hpp` (1500+ 行)

**作用**：固定维度密集向量的专用实现

**关键类**：
- `index_dense_gt<...>` - 密集向量索引
- `index_dense_head_t` - 二进制格式头部

**代码位置**：`include/usearch/index_dense.hpp:42-79`

#### `include/usearch/index_plugins.hpp`

**作用**：工具函数、内存管理、序列化

**关键组件**：
- `memory_mapped_file_t` - 内存映射文件
- `bitset_gt` - 高效位集合
- 各类距离度量的实现

### 2.3 数据流图

```
用户向量
    ↓
[预处理: 归一化/量化]
    ↓
[搜索: search_for_one_]
    ↓
[层间遍历: 高层 → 低层]
    ↓
[贪婪搜索: 局部最优]
    ↓
[Beam Search: 候选集扩展]
    ↓
[返回: 最近邻结果]
```

---

## 3. 环境搭建

### 3.1 系统要求

**最低要求**：
- C++11 兼容编译器（GCC 5+, Clang 3.4+, MSVC 2015+）
- CMake 3.15+
- 2GB RAM

**推荐配置**：
- C++17 编译器（GCC 12+, Clang 14+）
- 8GB+ RAM
- 支持 SIMD 的 CPU（AVX2/AVX-512/NEON）

### 3.2 安装依赖（Ubuntu）

```bash
# 更新包管理器
sudo apt-get update

# 安装核心构建工具
sudo apt-get install -y \
    cmake \
    build-essential \
    g++-12 \
    gcc-12 \
    git

# 安装可选依赖（性能优化）
sudo apt-get install -y \
    libjemalloc-dev     # 更好的内存分配器
    libomp-dev          # OpenMP 支持

# 验证安装
g++ --version
cmake --version
```

### 3.3 克隆仓库

```bash
# 克隆主仓库
git clone https://github.com/unum-cloud/usearch.git
cd usearch

# 初始化子模块（可选，用于 SIMD 加速）
git submodule update --init --recursive

# 验证结构
ls -la
# 应该看到：
# - include/usearch/   核心头文件
# - cpp/               C++ 测试
# - python/            Python 绑定
# - javascript/        JavaScript 绑定
# - ...
```

### 3.4 编译核心库

#### Debug 构建（用于学习）

```bash
# 创建 Debug 构建目录
cmake -D USEARCH_BUILD_TEST_CPP=1 \
      -D CMAKE_BUILD_TYPE=Debug \
      -B build_debug

# 编译
cmake --build build_debug --config Debug

# 运行测试
./build_debug/test_cpp
```

#### Release 构建（用于性能测试）

```bash
# 创建 Release 构建目录
cmake -D CMAKE_BUILD_TYPE=Release \
      -D USEARCH_USE_OPENMP=1 \
      -D USEARCH_USE_SIMSIMD=1 \
      -D USEARCH_USE_JEMALLOC=1 \
      -D USEARCH_BUILD_TEST_CPP=1 \
      -D USEARCH_BUILD_BENCH_CPP=1 \
      -B build_release

# 编译（使用所有核心）
cmake --build build_release --config Release -j$(nproc)

# 运行基准测试
./build_release/bench_cpp
```

### 3.5 安装 Python 绑定

```bash
# 创建虚拟环境
python3 -m venv .venv
source .venv/bin/activate

# 安装依赖
pip install --upgrade pip
pip install numpy pytest

# 安装 USearch（开发模式）
pip install -e .

# 验证安装
python -c "import usearch; print(usearch.__version__)"

# 运行 Python 测试
python -m pytest python/scripts/test_index.py -v
```

---

## 4. 第一个示例程序

### 4.1 C++ 示例

创建文件 `hello_usearch.cpp`：

```cpp
#include <usearch/index.hpp>
#include <usearch/index_dense.hpp>
#include <iostream>

using namespace unum::usearch;

int main() {
    // 1. 创建索引 - 128维向量，余弦距离
    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k, scalar_kind_t::f32_k);

    std::cout << "✓ 索引创建成功" << std::endl;

    // 2. 添加一些向量
    for (std::uint32_t i = 0; i < 100; ++i) {
        std::vector<float> vector(128);
        // 填充随机向量（实际应用中应该是真实数据）
        for (float& val : vector)
            val = static_cast<float>(rand()) / RAND_MAX;

        index.add(i, vector.data());
    }

    std::cout << "✓ 添加了 100 个向量" << std::endl;

    // 3. 搜索最近邻
    std::vector<float> query(128, 0.5f);

    auto results = index.search(query.data(), 5);
    std::cout << "✓ 找到 " << results.size() << " 个最近邻" << std::endl;

    // 4. 打印结果
    for (std::size_t i = 0; i < results.size(); ++i) {
        std::cout << "  [" << i << "] key=" << results[i].key
                  << " distance=" << results[i].distance << std::endl;
    }

    return 0;
}
```

编译运行：

```bash
g++ -std=c++11 -I include hello_usearch.cpp -o hello_usearch
./hello_usearch
```

### 4.2 Python 示例

创建文件 `hello_usearch.py`：

```python
import usearch
import numpy as np

# 1. 创建索引
index = usearch.Index(
    ndim=128,           # 128维
    metric='cos',       # 余弦距离
    dtype='f32',        # 32位浮点
)

print("✓ 索引创建成功")

# 2. 添加向量
vectors = np.random.rand(100, 128).astype(np.float32)
keys = np.arange(100, dtype=np.uint32)

index.add(keys, vectors)

print(f"✓ 添加了 {len(keys)} 个向量")

# 3. 搜索
query = np.random.rand(128).astype(np.float32)
results = index.search(query, 5)

print(f"✓ 找到 {len(results)} 个最近邻")
for i, (key, distance) in enumerate(results):
    print(f"  [{i}] key={key}, distance={distance:.4f}")
```

运行：

```bash
python hello_usearch.py
```

---

## 5. 核心概念预览

### 5.1 HNSW 图结构

```
Level 2:  ●─────●
           ╲     ╲
Level 1:   ●───●─●─●
            ╲ ╲ ╲ ╲
Level 0:  ●─●─●─●─●─●─●─●
```

**关键概念**：
- **层级结构**：高层节点少，连接稀疏；低层节点多，连接密集
- **对数层级**：节点数量和层级数呈对数关系
- **贪婪搜索**：每层局部最优，最终接近全局最优

### 5.2 核心数据类型

```cpp
// 向量键的类型
using vector_key_t = std::uint64_t;

// 图层级的类型
using level_t = std::int16_t;

// 压缩的槽位（节点在数组中的索引）
using compressed_slot_t = std::uint32_t;

// 距离类型
using distance_t = float;
```

### 5.3 配置参数

```cpp
// 连接参数：每层最大邻居数
std::size_t connectivity = 16;  // 默认值

// 扩展因子：搜索时的候选集大小
std::size_t expansion = 64;     // 默认值

// 这些参数平衡精度和速度
// connectivity 越大 → 精度越高，内存越大，速度越慢
// expansion 越大 → 精度越高，速度越慢
```

---

## 6. 调试工具

### 6.1 GDB 配置

创建 `.gdbinit` 文件：

```gdb
# 设置pretty print
set print pretty on

# 关键断点
b usearch_raise_runtime_error
b __asan::ReportGenericError

# 运行参数
set args --test
```

### 6.2 Valgrind 内存检查

```bash
# 编译 Debug 版本
cmake -D CMAKE_BUILD_TYPE=Debug -B build_valgrind
cmake --build build_valgrind

# 运行 Valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./build_valgrind/test_cpp
```

### 6.3 性能分析

```bash
# 使用 perf 分析
perf record -g ./build_release/bench_cpp
perf report

# 使用火焰图
git clone https://github.com/brendangregg/FlameGraph
perl FlameGraph/flamegraph.pl perf.out > flamegraph.svg
```

---

## 7. 今日练习

### 练习 1：环境验证
```bash
# 编译并运行测试
./build_debug/test_cpp

# 应该看到所有测试通过
```

### 练习 2：修改示例
修改 `hello_usearch.cpp`，尝试：
1. 改变向量维度（256, 512, 1024）
2. 改变距离度量（l2sq, ip）
3. 改变数据类型（f16, i8）

### 练习 3：性能对比
```bash
# 分别编译 Debug 和 Release 版本
# 对比运行速度差异

time ./build_debug/test_cpp
time ./build_release/test_cpp
```

---

## 8. 今日总结

### 核心知识点

✅ **USearch 设计哲学**
- 单文件头文件库
- 模板化设计
- 硬件无关性

✅ **三层架构**
- 核心层：`index.hpp` - HNSW 实现
- 接口层：`index_dense.hpp` - 密集向量
- 绑定层：多语言支持

✅ **关键概念**
- HNSW 图结构
- 层级索引
- 近似最近邻搜索

### 下节预告

明天我们将深入学习 **HNSW 算法的基础理论**，包括：
- NSW（Navigable Small World）图
- 层级结构的设计原理
- 概率跳跃机制
- 复杂度分析

---

## 📝 课后思考

1. 为什么 USearch 选择单文件设计而不是多文件模块化？
2. HNSW 相比暴力搜索和 KD-Tree 有什么优势？
3. 在什么情况下应该使用不同的距离度量（cos vs l2sq vs ip）？

---

**第1天完成！** 🎉
