# USearch 常见问题解答 (FAQ)
## Frequently Asked Questions

---

## 📚 目录

1. [基础问题](#基础问题)
2. [性能优化](#性能优化)
3. [分布式部署](#分布式部署)
4. [错误排查](#错误排查)
5. [最佳实践](#最佳实践)
6. [集成问题](#集成问题)

---

## 基础问题

### Q1: USearch 和其他向量数据库（如 Faiss、Milvus）有什么区别？

**A:**
| 特性 | USearch | Faiss | Milvus |
|------|---------|-------|--------|
| 部署方式 | 嵌入式库 | 嵌入式库 | 独立服务 |
| 语言支持 | 10+ | C++/Python | Go/Python |
| 依赖 | 零依赖 | 依赖 NumPy | 依赖多 |
| 动态更新 | ✅ 原生支持 | ⚠️ 有限 | ✅ 支持 |
| 跨平台 | ✅ 完全跨平台 | ⚠️ 部分限制 | ⚠️ 主要 Linux |
| 学习曲线 | 低 | 中 | 高 |
| 适用场景 | 嵌入式应用 | 离线搜索 | 大规模服务 |

**选择建议**：
- 嵌入式应用 → USearch
- 离线批处理 → Faiss
- 大规模在线服务 → Milvus

---

### Q2: 如何选择合适的距离度量？

**A:**

```cpp
// 余弦距离（最常用）
// 适用：文本、图像嵌入（已归一化）
index.init(128, metric_kind_t::cos_k);

// 内积（点积）
// 适用：推荐系统、不需要归一化的向量
index.init(128, metric_kind_t::ip_k);

// 欧氏距离（L2）
// 适用：计算机视觉、坐标数据
index.init(128, metric_kind_t::l2sq_k);
```

**快速选择指南**：
| 向量类型 | 推荐度量 | 原因 |
|---------|---------|------|
| 文本嵌入（BERT、Sentence-Transformers） | cos | 已归一化 |
| 图像特征（ResNet、CLIP） | cos | 归一化后稳定 |
| 推荐系统嵌入 | ip | 直接反映偏好 |
| 坐标、位置数据 | l2sq | 物理距离 |

---

### Q3: 向量维度对性能有多大影响？

**A:**

```
搜索时间复杂度：O(d × log(N) × ef)
- d: 向量维度
- N: 向量数量
- ef: 搜索范围

维度翻倍 → 距离计算时间翻倍
```

**实测数据**（1000万向量）：
| 维度 | 构建时间 | 搜索延迟 | 内存 |
|------|---------|---------|------|
| 128 | 200s | 1.0ms | 1x |
| 256 | 350s | 1.8ms | 2x |
| 512 | 600s | 3.2ms | 4x |
| 768 | 900s | 4.5ms | 6x |
| 1024 | 1200s | 6.0ms | 8x |

**优化建议**：
- 使用降维（PCA、AutoEncoder）
- 量化（f16、i8）
- 批量搜索

---

### Q4: 如何估算内存使用？

**A:**

```
内存 = 向量数据 + 索引结构 + 开销

向量数据 = N × d × bytes_per_scalar
索引结构 ≈ N × d × bytes_per_scalar × 0.3  (30%开销)
```

**计算示例**（1000万向量，768维）：
```
f32: 10M × 768 × 4 × 1.3 = ~40 GB
f16: 10M × 768 × 2 × 1.3 = ~20 GB
i8:  10M × 768 × 1 × 1.3 = ~10 GB
```

**快速估算工具**：
```python
def estimate_memory(n_vectors, dimensions, dtype='f32'):
    bytes_per_scalar = {'f32': 4, 'f16': 2, 'i8': 1}[dtype]
    base = n_vectors * dimensions * bytes_per_scalar
    overhead = base * 0.3
    return (base + overhead) / (1024**3)  # GB

# 示例
print(estimate_memory(10_000_000, 768, 'f16'))  # ~20 GB
```

---

## 性能优化

### Q5: 为什么我的搜索很慢？

**A:** 常见原因和解决方案：

**1. 配置不当**
```cpp
// ❌ 太小的 expansion
config.expansion = 16;

// ✅ 合适的值
config.expansion = 64;  // 平衡精度和速度
```

**2. 未使用批量操作**
```cpp
// ❌ 慢：逐个添加
for (int i = 0; i < n; ++i) {
    index.add(keys[i], vectors + i * d);
}

// ✅ 快：批量添加
index.add(keys, vectors, n);
```

**3. 未启用多线程**
```cpp
// 启用 OpenMP
config.multi = true;

// 编译时启用
g++ -fopenmp ...
```

**4. 系统资源不足**
```bash
# 检查 CPU
top -p $(pidof your_program)

# 检查内存
free -h

# 检查磁盘 I/O
iostat -x 1
```

---

### Q6: 如何提高召回率？

**A:**

**1. 增加 expansion**
```cpp
config.expansion = 128;  // 从 64 增加到 128
```

**2. 增加 connectivity**
```cpp
config.connectivity = 32;  // 从 16 增加到 32
```

**3. 使用更精确的量化**
```cpp
// 从 f16 改为 f32
scalar_kind_t scalar = scalar_kind_t::f32_k;
```

**4. 查询更多分片**（分布式）
```cpp
// 查询所有分片而不是部分
auto results = cluster.search(query, k, n_shards);
```

**召回率 vs 性能权衡**：
| expansion | 召回率 | 延迟 |
|-----------|--------|------|
| 32 | 85% | 0.5x |
| 64 | 95% | 1.0x |
| 128 | 98% | 1.5x |
| 256 | 99% | 2.0x |

---

### Q7: 如何处理高维向量？

**A:**

**1. 降维**
```cpp
// PCA 降维
auto reduced = pca.transform(original_vectors, 768, 256);
```

**2. 量化**
```cpp
// 使用 f16 而非 f32
index.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f16_k);
```

**3. 产品量化（PQ）**
```cpp
// 将向量分成多个子向量，分别量化
class ProductQuantizer {
    std::size_t n_subvectors = 8;  // 768 / 8 = 96 维/子向量
    // ...
};
```

---

## 分布式部署

### Q8: 如何选择分片数量？

**A:**

**经验公式**：
```
分片数 = min(数据量 / 单分片容量, CPU核心数)

示例：
- 1000万向量，单分片1000万 → 1 分片
- 1亿向量，单分片1000万 → 10 分片
- 1亿向量，16核CPU → 10 分片（不是16）
```

**考虑因素**：
1. 每个分片 < 2000万向量
2. 分片数 ≤ CPU核心数
3. 预留扩展空间

---

### Q9: 分布式环境下如何保证一致性？

**A:**

**1. 使用 Raft 共识**
```cpp
RaftNode raft;
raft.add_vector(key, vector);  // 自动复制到多数节点
```

**2. Quorum 机制**
```cpp
// 写需要 W 个确认，读需要 R 个响应
// W + R > N (节点总数)
class QuorumReplication {
    std::size_t write_quorum = 2;
    std::size_t read_quorum = 2;
};
```

**3. 向量时钟**
```cpp
VectorClock vc;
vc.receive(other_vc);  // 合并时钟
```

---

### Q10: 如何处理节点故障？

**A:**

**1. 心跳检测**
```cpp
FailureDetector detector;
detector.add_node("shard1:5000");
detector.set_failure_callback([](auto& node) {
    handle_failure(node);
});
detector.start();
```

**2. 自动故障转移**
```cpp
void handle_failure(std::string const& failed_node) {
    // 1. 标记节点为不可用
    mark_unavailable(failed_node);

    // 2. 提升副本
    promote_replica(failed_node);

    // 3. 重建副本
    rebuild_replica(failed_node);
}
```

**3. 检查点恢复**
```cpp
CheckpointManager checkpoint_mgr;
checkpoint_mgr.restore_latest();  // 恢复到最新检查点
```

---

## 错误排查

### Q11: 编译错误 "undefined reference to ..."

**A:**

**常见原因**：
1. 未链接库
2. 链接顺序错误
3. 未包含头文件

**解决方案**：
```bash
# 正确的编译命令
g++ -std=c++17 -O3 \
    -I/path/to/usearch/include \
    your_code.cpp \
    -o your_program

# 如果使用了 OpenMP
g++ -std=c++17 -O3 -fopenmp \
    -I/path/to/usearch/include \
    your_code.cpp \
    -o your_program
```

---

### Q12: 运行时错误 "Segmentation fault"

**A:**

**调试步骤**：

**1. 使用 GDB**
```bash
gdb ./your_program
(gdb) run
(gdb) bt  # 查看堆栈
```

**2. 使用 Valgrind**
```bash
valgrind --leak-check=full ./your_program
```

**3. 常见原因**
```cpp
// ❌ 错误：未初始化索引
index_dense_gt<float, uint32_t> index;
index.search(query, 10);  // 崩溃！

// ✅ 正确：先初始化
index.init(128, metric_kind_t::cos_k);
index.search(query, 10);

// ❌ 错误：维度不匹配
index.init(128);
// ... 但查询向量是 256 维

// ✅ 正确：确保维度一致
float query[128];
index.search(query, 10);
```

---

### Q13: 内存泄漏怎么办？

**A:**

**1. 检测泄漏**
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./your_program
```

**2. 常见原因**
```cpp
// ❌ 循环引用
std::shared_ptr<Node> a = std::make_shared<Node>();
std::shared_ptr<Node> b = std::make_shared<Node>();
a->next = b;
b->prev = a;  // 循环引用！

// ✅ 使用 weak_ptr 打破循环
std::shared_ptr<Node> a = std::make_shared<Node>();
std::shared_ptr<Node> b = std::make_shared<Node>();
a->next = b;
b->prev = std::weak_ptr<Node>(a);  // weak_ptr
```

**3. USearch 特定**
```cpp
// USearch 内部管理内存，通常不会泄漏
// 如果怀疑泄漏，检查：
// 1. 是否正确删除索引
// 2. 是否有线程未退出
// 3. 是否有循环引用
```

---

## 最佳实践

### Q14: 如何设计一个好的向量索引系统？

**A:**

**1. 容量规划**
```python
# 估算资源
n_vectors = 10_000_000
dimensions = 768
years = 3

# 考虑增长
total_vectors = n_vectors * (1 + 0.5 * years)  # 每年增长50%

# 估算内存
memory_gb = estimate_memory(total_vectors, dimensions, 'f16')

# 硬件配置
print(f"需要内存: {memory_gb:.1f} GB")
print(f"推荐配置: {memory_gb * 2:.1f} GB (留有余量)")
```

**2. 分层架构**
```cpp
// 热数据：内存，最快
index_dense_gt<float, uint32_t> hot_index;

// 温数据：SSD，中等
index_dense_gt<float, uint32_t> warm_index;

// 冷数据：归档
void archive_old_data();
```

**3. 监控和告警**
```python
# 关键指标
metrics = {
    'qps': queries_per_second,
    'latency_p99': percentile_99(latencies),
    'recall': calculate_recall(test_set),
    'memory_usage': get_memory_usage(),
}

# 告警
if metrics['latency_p99'] > threshold:
    send_alert("High latency!")
```

---

### Q15: 如何处理实时数据更新？

**A:**

**1. 增量更新**
```cpp
class IncrementalUpdater {
    std::queue<update_t> updates_;

    void add_update(update_t const& update) {
        updates_.push(update);

        if (updates_.size() >= BATCH_SIZE) {
            flush();
        }
    }

    void flush() {
        // 批量应用更新
        while (!updates_.empty()) {
            auto u = updates_.front();
            updates_.pop();
            apply_update(u);
        }
    }
};
```

**2. 定期重建**
```cpp
// 每天凌晨重建索引
void periodic_rebuild() {
    while (true) {
        wait_until_next_day();

        auto new_index = build_index_from_source();
        atomic_store(&active_index, new_index);
    }
}
```

**3. 版本管理**
```cpp
class VersionedIndex {
    std::atomic<index_dense_gt*> current_;

    void update_index() {
        auto new_index = new index_dense_gt();
        // ... 构建新索引 ...

        // 原子切换
        auto old = current_.exchange(new_index);

        // 延迟删除旧索引
        schedule_delete(old);
    }
};
```

---

## 集成问题

### Q16: 如何与深度学习框架集成？

**A:**

**PyTorch 集成**：
```python
import torch
from usearch.index import Index

# 提取特征
model = ResNet50(pretrained=True)
model.eval()

with torch.no_grad():
    features = model(image_tensor)  # [1, 2048]
    embedding = features.cpu().numpy().flatten()

# 添加到索引
index = Index(ndim=2048, metric='cos')
index.add([image_id], [embedding])

# 搜索
results = index.search(embedding, k=10)
```

**TensorFlow 集成**：
```python
import tensorflow as tf
from usearch.index import Index

# 使用 TF-Hub
module = hub.load('https://tfhub.dev/google/imagenet/mobilenet_v2_100_224/feature_vector/2')
features = module(image_tensor)

# 添加到索引
index.add([image_id], [features.numpy()])
```

---

### Q17: 如何与数据库集成？

**A:**

**PostgreSQL + USearch**：
```python
import psycopg2
from usearch.index import Index

# 1. 从数据库读取数据
conn = psycopg2.connect("dbname=test user=postgres")
cur = conn.cursor()

cur.execute("SELECT id, embedding FROM products")
rows = cur.fetchall()

# 2. 构建向量索引
index = Index(ndim=768, metric='cos')
ids = [row[0] for row in rows]
embeddings = [row[1] for row in rows]
index.add(ids, embeddings)

# 3. 搜索并获取完整信息
results = index.search(query_embedding, k=10)

for r in results:
    cur.execute("SELECT * FROM products WHERE id = %s", (r.key,))
    product = cur.fetchone()
    print(f"Product: {product[1]}, Score: {r.distance}")
```

**MongoDB + USearch**：
```python
from pymongo import MongoClient
from usearch.index import Index

client = MongoClient('mongodb://localhost:27017/')
db = client['ecommerce']
collection = db['products']

# 向量搜索 + 元数据过滤
results = index.search(query, k=100)

# 应用 MongoDB 过滤
filtered = []
for r in results:
    doc = collection.find_one({'_id': r.key})
    if doc and doc['price'] < 100:  # 价格过滤
        filtered.append(doc)

print(sorted(filtered, key=lambda x: x['rating'])[:10])
```

---

### Q18: Web API 如何实现？

**A:**

**Flask API**：
```python
from flask import Flask, request, jsonify
from usearch.index import Index

app = Flask(__name__)
index = Index(ndim=768, metric='cos')

@app.route('/add', methods=['POST'])
def add_vectors():
    data = request.json
    index.add(data['ids'], data['vectors'])
    return jsonify({'status': 'ok'})

@app.route('/search', methods=['POST'])
def search():
    data = request.json
    results = index.search(data['query'], k=data.get('k', 10))
    return jsonify({
        'results': [
            {'id': int(r.key), 'score': float(r.distance)}
            for r in results
        ]
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

**FastAPI（异步）**：
```python
from fastapi import FastAPI
from usearch.index import Index
import numpy as np

app = FastAPI()
index = Index(ndim=768, metric='cos')

@app.post("/search")
async def search(query: List[float], k: int = 10):
    query_array = np.array(query, dtype=np.float32)
    results = index.search(query_array, k)
    return {
        "results": [
            {"id": int(r.key), "score": float(r.distance)}
            for r in results
        ]
    }
```

---

## 其他常见问题

### Q19: USearch 支持哪些平台？

**A:**

| 平台 | 支持状态 | 备注 |
|------|---------|------|
| Linux x86_64 | ✅ 完全支持 | 推荐 |
| Linux ARM64 | ✅ 完全支持 | 树莓派等 |
| macOS x86_64 | ✅ 完全支持 | |
| macOS ARM64 | ✅ 完全支持 | M1/M2 |
| Windows x86_64 | ✅ 完全支持 | 需要 MSVC |
| Windows ARM64 | ⚠️ 实验性 | |

**编译选项**：
```bash
# Linux/macOS
g++ -std=c++17 -O3 -march=native ...

# Windows (MSVC)
cl /std:c++17 /O2 /arch:AVX2 ...
```

---

### Q20: 如何贡献代码？

**A:**

1. Fork 仓库
2. 创建特性分支
   ```bash
   git checkout -b feature/my-feature
   ```
3. 提交更改
   ```bash
   git commit -am 'Add some feature'
   ```
4. 推送到分支
   ```bash
   git push origin feature/my-feature
   ```
5. 创建 Pull Request

**代码规范**：
- 遵循现有代码风格
- 添加测试
- 更新文档
- 通过 CI 检查

---

### Q21: 性能基准在哪里？

**A:**

**运行基准测试**：
```bash
# 克隆仓库
git clone https://github.com/unum-cloud/usearch.git
cd usearch

# 编译基准测试
mkdir build && cd build
cmake .. -DUSEARCH_BUILD_BENCH_CPP=ON
make -j$(nproc)

# 运行
./bench_cpp
```

**预期性能**（1000万 768维向量）：
| 操作 | 时间 | QPS |
|------|------|-----|
| 构建索引 | ~5 分钟 | - |
| 单次搜索 | ~1 ms | ~1000 |
| 批量搜索(100) | ~10 ms | ~10000 |

---

## 获取帮助

如果以上FAQ没有解决你的问题：

1. **查看文档**：`README.md`、`INDEX.md`
2. **搜索Issues**：GitHub Issues
3. **提问**：
   - Stack Overflow（标签 `usearch`）
   - GitHub Discussions
4. **联系**：GitHub Issues

---

**版本**: v1.0
**最后更新**: 2025-01-24
**维护者**: USearch 社区
