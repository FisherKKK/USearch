# USearch 测试指南
## Testing Guide for USearch

---

## 📚 目录

1. [测试框架概述](#测试框架概述)
2. [单元测试](#单元测试)
3. [集成测试](#集成测试)
4. [性能测试](#性能测试)
5. [压力测试](#压力测试)
6. [回归测试](#回归测试)
7. [测试最佳实践](#测试最佳实践)

---

## 1. 测试框架概述

### 1.1 测试金字塔

```
          /\
         /  \     E2E Tests (少量)
        /____\    - 集成测试
       /      \   - 性能测试
      /        \  - 压力测试
     /__________\
    /            \ Unit Tests (大量)
```

**USearch 测试分布**：
- 单元测试：70%（快速、隔离）
- 集成测试：20%（关键流程）
- 性能测试：8%（基准对比）
- 压力测试：2%（极限情况）

---

## 2. 单元测试

### 2.1 Google Test 框架

```cpp
// test_index.cpp
#include <gtest/gtest.h>
#include <usearch/index_dense.hpp>
#include <vector>
#include <random>

using namespace unum::usearch;

class IndexTest : public ::testing::Test {
protected:
    index_dense_gt<float, std::uint32_t> index;
    std::size_t dimensions = 128;

    void SetUp() override {
        index.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k);
    }

    void TearDown() override {
        // 清理
    }

    // 辅助函数：生成随机向量
    std::vector<float> generate_random_vector(std::size_t dims) {
        std::vector<float> vec(dims);
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (auto& v : vec) {
            v = dist(rng);
        }

        return vec;
    }

    // 辅助函数：归一化
    void normalize(std::vector<float>& vec) {
        float norm = std::sqrt(std::inner_product(
            vec.begin(), vec.end(), vec.begin(), 0.0f
        ));
        for (auto& v : vec) {
            v /= norm;
        }
    }
};

// ============================================================================
// 基础功能测试
// ============================================================================

TEST_F(IndexTest, AddSingleVector) {
    auto vec = generate_random_vector(dimensions);
    normalize(vec);

    EXPECT_TRUE(index.add(1, vec.data()));
    EXPECT_EQ(index.size(), 1);
}

TEST_F(IndexTest, AddBatchVectors) {
    constexpr std::size_t n = 1000;
    std::vector<std::uint32_t> keys(n);
    std::vector<float> vectors(n * dimensions);

    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = i;
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        std::copy(vec.begin(), vec.end(), vectors.begin() + i * dimensions);
    }

    EXPECT_TRUE(index.add(keys.data(), vectors.data(), n));
    EXPECT_EQ(index.size(), n);
}

TEST_F(IndexTest, SearchReturnsCorrectK) {
    // 添加 100 个向量
    constexpr std::size_t n = 100;
    for (std::size_t i = 0; i < n; ++i) {
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        index.add(i, vec.data());
    }

    // 搜索
    auto query = generate_random_vector(dimensions);
    normalize(query);

    auto results = index.search(query.data(), 10);
    EXPECT_LE(results.size(), 10);
}

TEST_F(IndexTest, SearchFindNearest) {
    // 添加已知向量
    std::vector<float> target(dimensions, 1.0f);
    normalize(target);
    index.add(999, target.data());

    // 添加干扰向量
    for (std::size_t i = 0; i < 50; ++i) {
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        index.add(i, vec.data());
    }

    // 搜索应该找到 target
    auto query = target;  // 完全相同
    auto results = index.search(query.data(), 5);

    ASSERT_GT(results.size(), 0);
    EXPECT_EQ(results[0].key, 999);  // 最接近的应该是自己
    EXPECT_NEAR(results[0].distance, 0.0f, 0.001f);
}

// ============================================================================
// 边界条件测试
// ============================================================================

TEST_F(IndexTest, EmptyIndexSearch) {
    auto query = generate_random_vector(dimensions);
    auto results = index.search(query.data(), 10);

    EXPECT_EQ(results.size(), 0);
}

TEST_F(IndexTest, SearchKLargerThanSize) {
    // 只添加 5 个向量
    for (std::size_t i = 0; i < 5; ++i) {
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        index.add(i, vec.data());
    }

    auto query = generate_random_vector(dimensions);
    normalize(query);

    auto results = index.search(query.data(), 100);  // k > size

    EXPECT_LE(results.size(), 5);  // 最多返回 5 个
}

TEST_F(IndexTest, AddDuplicateKey) {
    auto vec = generate_random_vector(dimensions);
    normalize(vec);

    index.add(1, vec.data());
    index.add(1, vec.data());  // 重复添加

    // USearch 应该处理重复键
    EXPECT_EQ(index.size(), 1);
}

// ============================================================================
// 序列化测试
// ============================================================================

TEST_F(IndexTest, SaveAndLoad) {
    // 添加向量
    constexpr std::size_t n = 100;
    for (std::size_t i = 0; i < n; ++i) {
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        index.add(i, vec.data());
    }

    // 保存
    EXPECT_TRUE(index.save("/tmp/test_index.usearch"));

    // 加载到新索引
    index_dense_gt<float, std::uint32_t> new_index;
    new_index.init(dimensions, metric_kind_t::cos_k);
    EXPECT_TRUE(new_index.load("/tmp/test_index.usearch"));

    EXPECT_EQ(new_index.size(), n);

    // 验证搜索结果一致
    auto query = generate_random_vector(dimensions);
    normalize(query);

    auto results1 = index.search(query.data(), 10);
    auto results2 = new_index.search(query.data(), 10);

    ASSERT_EQ(results1.size(), results2.size());
    for (std::size_t i = 0; i < results1.size(); ++i) {
        EXPECT_EQ(results1[i].key, results2[i].key);
        EXPECT_NEAR(results1[i].distance, results2[i].distance, 0.001f);
    }
}

// ============================================================================
// 并发测试
// ============================================================================

TEST_F(IndexTest, ConcurrentAdds) {
    constexpr std::size_t n_threads = 4;
    constexpr std::size_t n_per_thread = 1000;

    std::vector<std::thread> threads;

    for (std::size_t t = 0; t < n_threads; ++t) {
        threads.emplace_back([this, t, n_per_thread]() {
            for (std::size_t i = 0; i < n_per_thread; ++i) {
                auto vec = generate_random_vector(dimensions);
                normalize(vec);
                std::uint32_t key = t * n_per_thread + i;
                index.add(key, vec.data());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(index.size(), n_threads * n_per_thread);
}

TEST_F(IndexTest, ConcurrentSearches) {
    // 先添加数据
    constexpr std::size_t n = 10000;
    for (std::size_t i = 0; i < n; ++i) {
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        index.add(i, vec.data());
    }

    // 并发搜索
    constexpr std::size_t n_threads = 8;
    constexpr std::size_t n_searches = 100;
    std::vector<std::thread> threads;

    for (std::size_t t = 0; t < n_threads; ++t) {
        threads.emplace_back([this, n_searches]() {
            for (std::size_t i = 0; i < n_searches; ++i) {
                auto query = generate_random_vector(dimensions);
                normalize(query);
                auto results = index.search(query.data(), 10);
                EXPECT_LE(results.size(), 10);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

// ============================================================================
// 性能测试
// ============================================================================

TEST_F(IndexTest, BuildPerformance) {
    constexpr std::size_t n = 100000;

    std::vector<std::uint32_t> keys(n);
    std::vector<float> vectors(n * dimensions);

    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = i;
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        std::copy(vec.begin(), vec.end(), vectors.begin() + i * dimensions);
    }

    auto start = std::chrono::high_resolution_clock::now();
    index.add(keys.data(), vectors.data(), n);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Build time: " << duration.count() << " ms\n";
    std::cout << "Throughput: " << (n * 1000 / duration.count()) << " vectors/sec\n";

    EXPECT_LT(duration.count(), 10000);  // 应该在 10 秒内完成
}

TEST_F(IndexTest, SearchPerformance) {
    constexpr std::size_t n = 100000;

    // 添加数据
    for (std::size_t i = 0; i < n; ++i) {
        auto vec = generate_random_vector(dimensions);
        normalize(vec);
        index.add(i, vec.data());
    }

    // 性能测试
    constexpr std::size_t n_searches = 1000;
    std::vector<float> latencies;

    for (std::size_t i = 0; i < n_searches; ++i) {
        auto query = generate_random_vector(dimensions);
        normalize(query);

        auto start = std::chrono::high_resolution_clock::now();
        auto results = index.search(query.data(), 10);
        auto end = std::chrono::high_resolution_clock::now();

        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        latencies.push_back(latency.count());
    }

    // 计算统计
    std::sort(latencies.begin(), latencies.end());

    float p50 = latencies[latencies.size() * 0.5];
    float p95 = latencies[latencies.size() * 0.95];
    float p99 = latencies[latencies.size() * 0.99];

    std::cout << "P50 latency: " << p50 / 1000.0 << " ms\n";
    std::cout << "P95 latency: " << p95 / 1000.0 << " ms\n";
    std::cout << "P99 latency: " << p99 / 1000.0 << " ms\n";

    EXPECT_LT(p99, 10000);  // P99 应该 < 10 ms
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

### 2.2 编译和运行

```bash
# 编译测试
g++ -std=c++17 -O3 -pthread \
    -I../../../include \
    -lgtest -lgtest_main \
    test_index.cpp -o test_index

# 运行测试
./test_index

# 运行特定测试
./test_index --gtest_filter=IndexTest.Search*

# 输出到 XML
./test_index --gtest_output=xml:test_results.xml
```

---

## 3. 集成测试

### 3.1 端到端测试

```cpp
// test_e2e.cpp
#include <gtest/gtest.h>
#include <usearch/index_dense.hpp>
#include <fstream>
#include <filesystem>

class E2ETest : public ::testing::Test {
protected:
    std::string test_dir = "/tmp/usearch_e2e";

    void SetUp() override {
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir);
    }
};

TEST_F(E2ETest, CompleteWorkflow) {
    // 1. 创建索引
    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k);

    // 2. 添加数据
    constexpr std::size_t n = 10000;
    std::vector<std::uint32_t> keys(n);
    std::vector<float> vectors(n * 128);

    // 填充数据
    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = i;
        for (std::size_t j = 0; j < 128; ++j) {
            vectors[i * 128 + j] = static_cast<float>(rand()) / RAND_MAX;
        }
    }

    ASSERT_TRUE(index.add(keys.data(), vectors.data(), n));
    EXPECT_EQ(index.size(), n);

    // 3. 搜索
    float query[128];
    for (auto& v : query) {
        v = static_cast<float>(rand()) / RAND_MAX;
    }

    auto results = index.search(query, 10);
    EXPECT_LE(results.size(), 10);

    // 4. 保存
    std::string index_path = test_dir + "/index.usearch";
    ASSERT_TRUE(index.save(index_path.c_str()));
    EXPECT_TRUE(std::filesystem::exists(index_path));

    // 5. 加载
    index_dense_gt<float, std::uint32_t> loaded_index;
    loaded_index.init(128, metric_kind_t::cos_k);
    ASSERT_TRUE(loaded_index.load(index_path.c_str()));
    EXPECT_EQ(loaded_index.size(), n);

    // 6. 验证搜索结果
    auto loaded_results = loaded_index.search(query, 10);
    EXPECT_EQ(results.size(), loaded_results.size());

    for (std::size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].key, loaded_results[i].key);
    }
}

TEST_F(E2ETest, IncrementalUpdates) {
    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k);

    // 初始添加
    std::vector<std::uint32_t> keys1(1000);
    std::vector<float> vectors1(1000 * 128);
    index.add(keys1.data(), vectors1.data(), 1000);

    // 增量添加
    std::vector<std::uint32_t> keys2(1000);
    std::vector<float> vectors2(1000 * 128);
    index.add(keys2.data(), vectors2.data(), 1000);

    EXPECT_EQ(index.size(), 2000);

    // 删除
    std::vector<std::uint32_t> to_remove = {100, 101, 102};
    auto removed = index.remove(to_remove.data(), to_remove.size());
    EXPECT_EQ(removed, 3);
}

TEST_F(E2ETest, MultiThreadedWorkflow) {
    index_dense_gt<float, std::uint32_t> index;
    index.init(128, metric_kind_t::cos_k);

    constexpr std::size_t n_threads = 4;
    constexpr std::size_t n_per_thread = 2500;

    std::vector<std::thread> threads;

    // 并发添加
    for (std::size_t t = 0; t < n_threads; ++t) {
        threads.emplace_back([&index, t, n_per_thread]() {
            std::vector<std::uint32_t> keys(n_per_thread);
            std::vector<float> vectors(n_per_thread * 128);

            std::uint32_t offset = t * n_per_thread;
            for (std::size_t i = 0; i < n_per_thread; ++i) {
                keys[i] = offset + i;
                for (std::size_t j = 0; j < 128; ++j) {
                    vectors[i * 128 + j] = static_cast<float>(rand()) / RAND_MAX;
                }
            }

            index.add(keys.data(), vectors.data(), n_per_thread);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(index.size(), n_threads * n_per_thread);

    // 并发搜索
    std::atomic<std::size_t> success_count{0};

    threads.clear();
    for (std::size_t t = 0; t < n_threads; ++t) {
        threads.emplace_back([&index, &success_count, n_per_thread]() {
            for (std::size_t i = 0; i < n_per_thread; ++i) {
                float query[128];
                for (auto& v : query) {
                    v = static_cast<float>(rand()) / RAND_MAX;
                }

                auto results = index.search(query, 10);
                if (results.size() <= 10) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, n_threads * n_per_thread);
}
```

---

## 4. Python 测试

### 4.1 PyTest 框架

```python
# test_index.py
import pytest
import numpy as np
from usearch.index import Index

class TestIndexBasic:
    """基础功能测试"""

    def test_create_index(self):
        """测试创建索引"""
        index = Index(ndim=128, metric='cos', dtype='f32')
        assert index is not None

    def test_add_single_vector(self):
        """测试添加单个向量"""
        index = Index(ndim=128, metric='cos')
        vector = np.random.rand(128).astype(np.float32)
        vector = vector / np.linalg.norm(vector)  # 归一化

        result = index.add([1], [vector])
        assert result

    def test_add_batch_vectors(self):
        """测试批量添加"""
        index = Index(ndim=128, metric='cos')
        n = 1000

        vectors = np.random.rand(n, 128).astype(np.float32)
        # L2 归一化
        vectors = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)

        keys = np.arange(n, dtype=np.uint32)

        result = index.add(keys, vectors)
        assert result

    def test_search(self):
        """测试搜索"""
        index = Index(ndim=128, metric='cos')

        # 添加数据
        n = 100
        vectors = np.random.rand(n, 128).astype(np.float32)
        vectors = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)
        keys = np.arange(n, dtype=np.uint32)

        index.add(keys, vectors)

        # 搜索
        query = vectors[0]
        matches = index.search(query, k=10)

        assert len(matches) <= 10
        assert matches[0].key == 0  # 最接近的应该是自己


class TestIndexPersistence:
    """持久化测试"""

    def test_save_and_load(self, tmp_path):
        """测试保存和加载"""
        import tempfile

        # 创建并填充索引
        index = Index(ndim=128, metric='cos')
        n = 1000

        vectors = np.random.rand(n, 128).astype(np.float32)
        vectors = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)
        keys = np.arange(n, dtype=np.uint32)

        index.add(keys, vectors)

        # 保存
        index_path = tmp_path / "test_index.usearch"
        index.save(str(index_path))

        assert index_path.exists()

        # 加载
        new_index = Index(ndim=128, metric='cos')
        new_index.load(str(index_path))

        assert new_index.size == n

        # 验证搜索一致
        query = vectors[0]
        results1 = index.search(query, k=10)
        results2 = new_index.search(query, k=10)

        assert len(results1) == len(results2)
        for i in range(len(results1)):
            assert results1[i].key == results2[i].key


class TestIndexPerformance:
    """性能测试"""

    def test_build_performance(self):
        """测试构建性能"""
        import time

        index = Index(ndim=128, metric='cos')
        n = 100000

        vectors = np.random.rand(n, 128).astype(np.float32)
        vectors = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)
        keys = np.arange(n, dtype=np.uint32)

        start = time.time()
        index.add(keys, vectors)
        duration = time.time() - start

        print(f"Build time: {duration:.2f} seconds")
        print(f"Throughput: {n / duration:,.0f} vectors/sec")

        assert duration < 30  # 应该在 30 秒内完成

    def test_search_performance(self):
        """测试搜索性能"""
        import time

        index = Index(ndim=128, metric='cos')
        n = 100000

        vectors = np.random.rand(n, 128).astype(np.float32)
        vectors = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)
        keys = np.arange(n, dtype=np.uint32)

        index.add(keys, vectors)

        # 性能测试
        n_queries = 1000
        latencies = []

        for i in range(n_queries):
            query = np.random.rand(128).astype(np.float32)
            query = query / np.linalg.norm(query)

            start = time.time()
            matches = index.search(query, k=10)
            duration = (time.time() - start) * 1000  # ms

            latencies.append(duration)

        # 统计
        latencies.sort()
        p50 = latencies[len(latencies) // 2]
        p95 = latencies[int(len(latencies) * 0.95)]
        p99 = latencies[int(len(latencies) * 0.99)]

        print(f"P50: {p50:.2f} ms")
        print(f"P95: {p95:.2f} ms")
        print(f"P99: {p99:.2f} ms")

        assert p99 < 20  # P99 应该 < 20 ms
```

### 4.2 运行 Python 测试

```bash
# 安装依赖
pip install pytest pytest-cov numpy usearch

# 运行所有测试
pytest test_index.py -v

# 运行特定测试
pytest test_index.py::TestIndexBasic::test_search -v

# 生成覆盖率报告
pytest test_index.py --cov=usearch --cov-report=html

# 并行运行
pytest test_index.py -n auto
```

---

## 5. 基准测试

### 5.1 性能基准

使用提供的 `test_performance.py`：

```bash
# 运行完整基准测试
python tools/test_performance.py

# 运行特定测试
python tools/test_performance.py --scaling
python tools/test_performance.py --configs
python tools/test_performance.py --quantization
```

### 5.2 对比基准

```python
# benchmark_comparison.py
"""
与其他向量搜索库的性能对比
"""

import numpy as np
import time
from usearch.index import Index

def benchmark_usearch(n_vectors, dimensions):
    """USearch 基准测试"""
    index = Index(ndim=dimensions, metric='cos')

    vectors = np.random.rand(n_vectors, dimensions).astype(np.float32)
    vectors /= np.linalg.norm(vectors, axis=1, keepdims=True)
    keys = np.arange(n_vectors, dtype=np.uint32)

    # 构建
    start = time.time()
    index.add(keys, vectors)
    build_time = time.time() - start

    # 搜索
    n_queries = 100
    latencies = []

    for i in range(n_queries):
        query = vectors[i]
        start = time.time()
        index.search(query, k=10)
        latencies.append(time.time() - start)

    return {
        'build_time': build_time,
        'avg_latency': np.mean(latencies),
        'p99_latency': np.percentile(latencies, 99),
    }

if __name__ == '__main__':
    results = benchmark_usearch(100000, 768)
    print("USearch 性能:")
    print(f"  构建时间: {results['build_time']:.2f} 秒")
    print(f"  平均延迟: {results['avg_latency']*1000:.2f} ms")
    print(f"  P99 延迟: {results['p99_latency']*1000:.2f} ms")
```

---

## 6. 测试最佳实践

### 6.1 AAA 模式

```cpp
TEST(IndexTest, SearchReturnsResults) {
    // Arrange（准备）
    constexpr std::size_t n = 100;
    for (std::size_t i = 0; i < n; ++i) {
        auto vec = generate_vector(dimensions);
        index.add(i, vec.data());
    }

    // Act（执行）
    auto query = generate_vector(dimensions);
    auto results = index.search(query.data(), 10);

    // Assert（断言）
    EXPECT_LE(results.size(), 10);
}
```

### 6.2 参数化测试

```cpp
class IndexParameterizedTest : public ::testing::TestWithParam<std::size_t> {
    // ...
};

TEST_P(IndexParameterizedTest, DifferentDimensions) {
    std::size_t dims = GetParam();
    index_dense_gt<float, std::uint32_t> index;
    index.init(dims, metric_kind_t::cos_k);

    auto vec = generate_random_vector(dims);
    EXPECT_TRUE(index.add(1, vec.data()));
}

INSTANTIATE_TEST_SUITE_P(
    DimensionTests,
    IndexParameterizedTest,
    ::testing::Values(64, 128, 256, 512, 768, 1024)
);
```

### 6.3 测试夹具

```cpp
class IndexFixture {
protected:
    index_dense_gt<float, std::uint32_t> index;
    std::size_t dimensions = 128;

    void SetUp() {
        index.init(dimensions, metric_kind_t::cos_k);
    }

    void add_test_data(std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            auto vec = generate_random_vector(dimensions);
            index.add(i, vec.data());
        }
    }
};
```

---

## 7. CI/CD 集成

### 7.1 GitHub Actions

```yaml
# .github/workflows/test.yml
name: Tests

on: [push, pull_request]

jobs:
  test-cpp:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++ libgtest-dev

      - name: Build
        run: |
          mkdir build && cd build
          cmake .. -DUSEARCH_BUILD_TEST_CPP=ON
          make -j$(nproc)

      - name: Run tests
        run: |
          cd build
          ctest --output-on-failure

  test-python:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Set up Python
        uses: actions/setup-python@v2
        with:
          python-version: '3.10'

      - name: Install dependencies
        run: |
          pip install pytest pytest-cov numpy usearch

      - name: Run tests
        run: |
          pytest python/tests/ -v --cov=usearch
```

### 7.2 Docker 测试

```dockerfile
# Dockerfile.test
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake g++ libgtest-dev python3 python3-pip

RUN pip3 install pytest pytest-cov numpy usearch

COPY . /usearch
WORKDIR /usearch

RUN mkdir build && cd build && \
    cmake .. -DUSEARCH_BUILD_TEST_CPP=ON && \
    make -j$(nproc)

CMD ["bash", "-c", "cd build && ctest --output-on-failure && pytest ../python/tests/"]
```

---

## 8. 调试测试

### 8.1 GDB 调试

```bash
# 编译带调试符号
g++ -g -std=c++17 -I../../../include \
    test_index.cpp -lgtest -lgtest_main -o test_index

# 运行 GDB
gdb ./test_index
(gdb) run
(gdb) bt  # 查看堆栈
```

### 8.2 Valgrind 检测

```bash
# 内存泄漏检测
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./test_index

# 线程检测
valgrind --tool=helgrind ./test_index
```

---

**版本**: v1.0
**最后更新**: 2025-01-24
**维护者**: USearch 社区
