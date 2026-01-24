# USearch 故障排除指南

## 常见问题及解决方案

### 1. 编译问题

#### 问题 1.1：编译错误 "undefined reference to `__builtin_prefetch`"

**症状**：
```
error: undefined reference to '__builtin_prefetch'
```

**原因**：使用了不支持内置预取的编译器

**解决方案**：
```bash
# 方法1：更新 GCC
sudo apt-get update
sudo apt-get install g++-12

# 方法2：在 CMakeLists.txt 中禁用预取
add_definitions(-DUSEARCH_DISABLE_PREFETCH)

# 方法3：使用 Clang
sudo apt-get install clang
export CC=clang
export CXX=clang++
```

#### 问题 1.2：链接错误 "undefined reference to `omp_get_num_threads`"

**症状**：
```
undefined reference to `omp_get_num_threads'
```

**原因**：未链接 OpenMP 库

**解决方案**：
```bash
# 方法1：在 CMake 中启用 OpenMP
cmake -D USEARCH_USE_OPENMP=1 ..

# 方法2：手动添加链接标志
export LDFLAGS="-fopenmp"
cmake ..

# 方法3：禁用 OpenMP
cmake -D USEARCH_USE_OPENMP=OFF ..
```

#### 问题 1.3：SIMD 相关错误

**症状**：
```
error: 'AVX2' was not declared in this scope
```

**解决方案**：
```bash
# 检查 CPU 是否支持 AVX2
lscpu | grep avx2

# 如果不支持，禁用 SIMD
cmake -D USEARCH_USE_SIMSIMD=OFF ..

# 或使用更低的 SIMD 级别
export CXXFLAGS="-msse4.2"
```

---

### 2. 运行时问题

#### 问题 2.1：段错误（Segmentation Fault）

**症状**：
```
Segmentation fault (core dumped)
```

**调试步骤**：

1. **使用 GDB**：
```bash
gdb ./your_program
(gdb) run
# 程序崩溃后
(gdb) backtrace
# 查看调用栈
(gdb) frame 0
# 查看崩溃位置的变量
```

2. **使用 Valgrind**：
```bash
valgrind --leak-check=full --show-leak-kinds=all ./your_program
```

3. **常见原因**：
   - 数组越界
   - 空指针解引用
   - 未初始化的变量
   - 栈溢出

**解决方案**：
```cpp
// 1. 添加边界检查
#include <vector>
std::vector<float> vec;
if (index < vec.size()) {
    value = vec[index];
}

// 2. 使用 at() 方法
value = vec.at(index);  // 会抛出异常

// 3. 添加断言
#include <cassert>
assert(index < vec.size() && "Index out of bounds");
```

#### 问题 2.2：内存泄漏

**症状**：程序运行一段时间后内存持续增长

**检测方法**：
```bash
# 使用 Valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./your_program

# 使用 massif（可视化内存使用）
valgrind --tool=massif ./your_program
ms_print massif.out.xxxx
```

**常见原因及解决**：
```cpp
// 问题1：忘记释放内存
float* data = new float[100];
// ... 使用 ...
delete[] data;  // 必须释放

// 解决方案1：使用智能指针
#include <memory>
std::unique_ptr<float[]> data(new float[100]);

// 问题2：循环中的内存泄漏
for (int i = 0; i < n; ++i) {
    float* temp = new float[100];
    // 忘记 delete
}

// 解决方案2：移出循环
std::vector<float> temp(100);
for (int i = 0; i < n; ++i) {
    // 使用 temp
}

// 问题3：USearch 相关
index_dense_gt<float> index;
// 如果忘记调用析构或 reset
```

#### 问题 2.3：搜索结果不正确

**症状**：搜索返回的结果与预期不符

**调试步骤**：

1. **检查向量归一化**：
```python
import numpy as np

# 余弦距离需要归一化
def normalize(v):
    norm = np.linalg.norm(v)
    if norm > 0:
        return v / norm
    return v

# 检查
vectors = np.array([...])
norms = np.linalg.norm(vectors, axis=1)
print(f"向量范数: {norms}")  # 应该接近 1.0
```

2. **检查度量类型**：
```python
# 确认使用了正确的度量
print(index.metric)  # 应该是 'cos' 或 'l2sq'

# 重新创建索引
index = usearch.Index(ndim=128, metric='cos')
```

3. **验证索引**：
```python
# 添加向量后立即搜索验证
index.add(0, vector)
results = index.search(vector, k=1)
print(f"最相似的向量: {results[0][0]}")  # 应该是 0
print(f"距离: {results[0][1]}")  # 应该接近 0
```

---

### 3. 性能问题

#### 问题 3.1：构建速度慢

**症状**：添加向量速度远低于预期

**诊断**：
```python
import time

start = time.time()
index.add(keys, vectors)
elapsed = time.time() - start
throughput = len(vectors) / elapsed
print(f"吞吐量: {throughput:.0f} vectors/s")

# 预期: > 10000 vectors/s
```

**优化方案**：

1. **批量添加**：
```python
# ❌ 不好：循环添加
for key, vec in zip(keys, vectors):
    index.add(key, vec)

# ✅ 好：批量添加
index.add(keys, vectors)
```

2. **调整参数**：
```python
# 降低 ef_construction
index = usearch.Index(
    ndim=128,
    metric='cos',
    connectivity=16,
    expansion=100  # 从 200 降到 100
)
```

3. **预分配**：
```cpp
// C++
index.reserve(1000000);  // 预分配空间
```

#### 问题 3.2：搜索速度慢

**症状**：搜索延迟过高（> 1ms）

**诊断**：
```python
import time

latencies = []
for _ in range(100):
    start = time.perf_counter()
    index.search(query, k=10)
    latencies.append(time.perf_counter() - start)

avg_latency = np.mean(latencies) * 1000  # ms
p95_latency = np.percentile(latencies, 95) * 1000
print(f"平均延迟: {avg_latency:.3f} ms")
print(f"P95 延迟: {p95_latency:.3f} ms")
```

**优化方案**：

1. **调整 ef 参数**：
```python
# 降低 ef（牺牲精度）
index.expansion = 32  # 从 64 降到 32
```

2. **使用量化**：
```python
# 使用 f16 而不是 f32
index = usearch.Index(ndim=128, dtype='f16')
```

3. **启用 SIMD**：
```python
# 检查是否启用
print(index.hardware_acceleration)  # 应该不是 'none'

# 如果未启用，重新编译
cmake -D USEARCH_USE_SIMSIMD=1 ..
```

4. **批量搜索**：
```python
from concurrent.futures import ThreadPoolExecutor

def batch_search(index, queries, k=10, n_workers=4):
    with ThreadPoolExecutor(max_workers=n_workers) as executor:
        results = list(executor.map(
            lambda q: index.search(q, k),
            queries
        ))
    return results
```

#### 问题 3.3：召回率低

**症状**：搜索结果遗漏了很多相关向量

**诊断**：
```python
# 计算召回率
def calculate_recall(index, queries, ground_truth, k=10):
    recalls = []
    for query, true_neighbors in zip(queries, ground_truth):
        results = index.search(query, k)
        retrieved = set([key for key, _ in results])
        recall = len(retrieved & true_neighbors) / len(true_neighbors)
        recalls.append(recall)
    return np.mean(recalls)

recall = calculate_recall(index, test_queries, ground_truth)
print(f"召回率: {recall:.3f}")  # 应该 > 0.90
```

**解决方案**：

1. **增大 ef**：
```python
index.expansion = 128  # 从 64 增加到 128
```

2. **增大 M**：
```python
index = usearch.Index(
    ndim=128,
    metric='cos',
    connectivity=32  # 从 16 增加到 32
)
```

3. **增大 ef_construction**：
```python
# 重建索引
new_index = usearch.Index(
    ndim=128,
    metric='cos',
    connectivity=16,
    expansion=200  # 从 100 增加到 200
)
new_index.add(keys, vectors)
```

---

### 4. 内存问题

#### 问题 4.1：内存占用过高

**症状**：索引占用内存远超预期

**诊断**：
```python
import psutil
import os

process = psutil.Process(os.getpid())
memory_mb = process.memory_info().rss / 1024 / 1024
print(f"内存使用: {memory_mb:.1f} MB")

# 估算预期内存
def estimate_memory(n, d, scalar='f32', M=16):
    bytes_per_scalar = {'f32': 4, 'f16': 2, 'i8': 1}
    vector_memory = n * d * bytes_per_scalar[scalar]

    import math
    avg_levels = math.log(n) * 0.25
    graph_memory = n * M * avg_levels * 4  # 假设 4 字节指针

    return (vector_memory + graph_memory) / 1024 / 1024

estimated = estimate_memory(len(vectors), 128)
print(f"估算内存: {estimated:.1f} MB")
```

**解决方案**：

1. **使用量化**：
```python
# 从 f32 切换到 f16
index = usearch.Index(ndim=128, dtype='f16')

# 或 i8
index = usearch.Index(ndim=128, dtype='i8')
```

2. **降低 M**：
```python
index = usearch.Index(connectivity=8)  # 从 16 降到 8
```

3. **使用内存映射**：
```python
# 不加载到内存，直接映射文件
index = usearch.Index.restore("large_index.usearch", view=True)
```

#### 问题 4.2：内存不足（OOM）

**症状**：
```
std::bad_alloc
Out of memory
Killed
```

**解决方案**：

1. **增加交换空间**：
```bash
# 创建 swap 文件
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

2. **分批处理**：
```python
batch_size = 10000
for i in range(0, len(vectors), batch_size):
    batch = vectors[i:i+batch_size]
    batch_keys = keys[i:i+batch_size]
    index.add(batch_keys, batch)
```

3. **使用更激进的量化**：
```python
# 使用 i8 或甚至 b1x8
index = usearch.Index(ndim=128, dtype='i8')
```

---

### 5. Python 特定问题

#### 问题 5.1：ImportError

**症状**：
```
ImportError: No module named 'usearch'
```

**解决方案**：
```bash
# 安装
pip install usearch

# 或从源码安装
cd python
pip install -e .

# 验证
python -c "import usearch; print(usearch.__version__)"
```

#### 问题 5.2：版本不兼容

**症状**：
```
AttributeError: 'Index' object has no attribute 'expansion'
```

**解决方案**：
```bash
# 检查版本
python -c "import usearch; print(usearch.__version__)"

# 更新到最新版本
pip install --upgrade usearch

# 或安装特定版本
pip install usearch==2.23.0
```

#### 问题 5.3：NumPy 类型问题

**症状**：
```
TypeError: Expected dtype float32, got float64
```

**解决方案**：
```python
import numpy as np

# 确保类型正确
vectors = np.random.rand(100, 128).astype(np.float32)  # 注意 astype
keys = np.arange(100, dtype=np.uint64)

# 检查类型
print(vectors.dtype)  # 应该是 float32
print(keys.dtype)     # 应该是 uint64
```

---

### 6. 并发问题

#### 问题 6.1：线程竞争

**症状**：多线程环境下出现不一致或崩溃

**解决方案**：
```cpp
// 1. 使用互斥锁
std::mutex index_mutex;

void thread_safe_add(int key, float* vector) {
    std::lock_guard<std::mutex> lock(index_mutex);
    index.add(key, vector);
}

// 2. 只读操作不需要锁
void thread_safe_search(float* query) {
    // 搜索是线程安全的（只读）
    auto results = index.search(query, 10);
}
```

#### 问题 6.2：死锁

**症状**：程序挂起，无响应

**诊断**：
```bash
# 使用 GDB
gdb ./your_program
(gdb) run
# Ctrl+C 暂停
(gdb) info threads
(gdb) thread apply all bt
```

**解决方案**：
```cpp
// 1. 避免嵌套锁
std::mutex mutex1, mutex2;

// ❌ 不好：可能导致死锁
void bad_function() {
    std::lock_guard<std::mutex> lock1(mutex1);
    std::lock_guard<std::mutex> lock2(mutex2);
}

// ✅ 好：使用 std::lock
void good_function() {
    std::lock(mutex1, mutex2);
    std::lock_guard<std::mutex> lock1(mutex1, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(mutex2, std::adopt_lock);
}
```

---

### 7. 性能分析工具

#### 7.1 使用 perf

```bash
# CPU 性能分析
perf record -g ./your_program
perf report

# 缓存分析
perf stat -e cache-references,cache-misses ./your_program

# 分支预测分析
perf stat -e branches,branch-misses ./your_program
```

#### 7.2 使用 Flame Graph

```bash
# 生成火焰图
perf record -F 99 -g ./your_program
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg

# 在浏览器中查看
firefox flamegraph.svg
```

#### 7.3 使用 Python 分析器

```python
import cProfile
import pstats

# 分析
profiler = cProfile.Profile()
profiler.enable()

# 运行代码
index.search(query, 10)

profiler.disable()

# 查看结果
stats = pstats.Stats(profiler)
stats.sort_stats('cumulative')
stats.print_stats(10)  # 打印前 10 个最耗时的函数
```

---

### 8. 获取帮助

如果以上方法都无法解决问题：

1. **查看文档**：
   - 课程 README
   - USearch GitHub Wiki
   - API 文档

2. **搜索已知问题**：
   - GitHub Issues
   - Stack Overflow
   - 课程讨论区

3. **提交 Bug**：
   ```bash
   # 收集信息
   uname -a
   g++ --version
   cmake --version
   python --version

   # 提交到 GitHub
   https://github.com/unum-cloud/usearch/issues
   ```

4. **课程社区**：
   - 课程仓库 Discussions
   - 学习群组（如果有的话）

---

## 预防措施

### 1. 编码规范

```cpp
// ✅ 好：清晰的代码
void add_vector_safe(int key, const std::vector<float>& vector) {
    if (vector.size() != dimensions_) {
        throw std::invalid_argument("Vector dimension mismatch");
    }
    index.add(key, vector.data());
}

// ❌ 不好：不安全的代码
void add_vector_unsafe(int key, const std::vector<float>& vector) {
    index.add(key, vector.data());  // 没有检查
}
```

### 2. 单元测试

```cpp
#include <cassert>

void test_basic_search() {
    index_dense_gt<float> index;
    index.init(128, metric_kind_t::cos_k);

    std::vector<float> vec(128, 1.0f);
    index.add(0, vec.data());

    auto results = index.search(vec.data(), 1);

    assert(results.size() == 1);
    assert(results[0].key == 0);
    assert(results[0].distance < 0.001);
}

int main() {
    test_basic_search();
    std::cout << "All tests passed!\n";
}
```

### 3. 日志记录

```cpp
#include <iostream>

#define LOG(msg) std::cout << "[LOG] " << msg << "\n"
#define ERROR(msg) std::cout << "[ERROR] " << msg << "\n"

bool add_vector_with_logging(int key, const std::vector<float>& vector) {
    LOG("Adding vector " << key);

    if (vector.size() != dimensions_) {
        ERROR("Dimension mismatch: expected " << dimensions_ << ", got " << vector.size());
        return false;
    }

    bool success = index.add(key, vector.data());
    if (success) {
        LOG("Vector added successfully");
    } else {
        ERROR("Failed to add vector");
    }

    return success;
}
```

---

记住：预防胜于治疗！良好的编码习惯和测试可以避免大部分问题。
