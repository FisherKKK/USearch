# USearch 性能调优检查清单
## Performance Tuning Checklist

---

## 📋 使用说明

本文档提供完整的性能调优检查清单，涵盖从开发到部署的全流程。

**检查等级**：
- [ ] 必须检查
- [-] 建议检查
- [o] 可选检查

---

## 1. 开发阶段

### 1.1 代码质量

- [ ] **使用批量操作**
  ```cpp
  // ❌ 避免
  for (std::size_t i = 0; i < n; ++i) {
      index.add(keys[i], vectors + i * d);
  }

  // ✅ 推荐
  index.add(keys, vectors, n);
  ```

- [ ] **预分配内存**
  ```cpp
  index.reserve(estimated_size);
  ```

- [ ] **避免不必要的拷贝**
  ```cpp
  // ❌ 避免
  void process(std::vector<float> vector);

  // ✅ 推荐
  void process(std::vector<float> const& vector);
  ```

- [-] **使用移动语义**
  ```cpp
  std::vector<float> create_embedding();
  auto emb = create_embedding();  // RVO
  index.add(id, emb.data());
  ```

### 1.2 编译选项

- [ ] **启用优化**
  ```bash
  g++ -O3 -march=native ...
  ```

- [ ] **启用 SIMD**
  ```bash
  -DUSEARCH_USE_SIMSIMD=ON
  ```

- [ ] **启用 OpenMP**（多线程）
  ```bash
  -fopenmp
  -DUSEARCH_USE_OPENMP=ON
  ```

- [-] **启用 LTO**（链接时优化）
  ```bash
  -flto
  ```

---

## 2. 配置优化

### 2.1 索引参数

- [ ] **选择合适的 connectivity**
  ```cpp
  // 小规模/快速
  config.connectivity = 8;

  // 通用
  config.connectivity = 16;

  // 高精度
  config.connectivity = 32;
  ```

- [ ] **选择合适的 expansion**
  ```cpp
  // 构建时
  config.expansion = 64;  // ef_construction

  // 搜索时
  index.search(query, k, 64);  // ef_search
  ```

- [-] **量化选择**
  ```cpp
  // 高精度
  scalar_kind_t scalar = scalar_kind_t::f32_k;

  // 平衡
  scalar_kind_t scalar = scalar_kind_t::f16_k;

  // 大规模
  scalar_kind_t scalar = scalar_kind_t::i8_k;
  ```

### 2.2 距离度量

- [ ] **选择合适的度量**
  ```cpp
  // 归一化向量
  metric_kind_t metric = metric_kind_t::cos_k;

  // 推荐系统
  metric_kind_t metric = metric_kind_t::ip_k;

  // 坐标数据
  metric_kind_t metric = metric_kind_t::l2sq_k;
  ```

---

## 3. 运行时优化

### 3.1 并发控制

- [ ] **启用多线程**
  ```cpp
  config.multi = true;

  // 或使用 OpenMP
  #pragma omp parallel for
  for (std::size_t i = 0; i < n_queries; ++i) {
      results[i] = index.search(queries[i], k);
  }
  ```

- [-] **调整线程数**
  ```bash
  export OMP_NUM_THREADS=8
  ```

### 3.2 内存优化

- [ ] **使用内存对齐**
  ```cpp
  config.alignment = 64;  // 缓存行对齐
  ```

- [-] **启用大页内存**（Linux）
  ```bash
  echo 100 > /proc/sys/vm/nr_hugepages
  ```

- [-] **NUMA 优化**（多CPU服务器）
  ```bash
  numactl --cpunodebind=0 --membind=0 ./your_program
  ```

---

## 4. 系统级优化

### 4.1 操作系统

- [ ] **调整文件描述符限制**
  ```bash
  # /etc/security/limits.conf
  * soft nofile 65536
  * hard nofile 65536
  ```

- [ ] **禁用 swap**
  ```bash
  sudo swapoff -a
  ```

- [-] **CPU 调频设置**
  ```bash
  # 性能模式
  sudo cpupower frequency-set -g performance
  ```

### 4.2 网络优化**（分布式）

- [ ] **增加 TCP 缓冲区**
  ```bash
  # /etc/sysctl.conf
  net.core.rmem_max = 134217728
  net.core.wmem_max = 134217728
  net.ipv4.tcp_rmem = 4096 87380 67108864
  net.ipv4.tcp_wmem = 4096 65536 67108864
  ```

- [-] **启用 TCP Fast Open**
  ```bash
  net.ipv4.tcp_fastopen = 3
  ```

---

## 5. 监控和分析

### 5.1 性能分析

- [ ] **使用 perf 分析**
  ```bash
  perf record -g -p $(pidof your_program)
  perf report
  ```

- [-] **生成火焰图**
  ```bash
  perf script | FlameGraph/stackcollapse-perf.pl | \
    FlameGraph/flamegraph.pl > flamegraph.svg
  ```

- [-] **使用 Valgrind**
  ```bash
  valgrind --tool=callgrind ./your_program
  kcachegrind callgrind.out.<pid>
  ```

### 5.2 内存分析

- [ ] **检查内存泄漏**
  ```bash
  valgrind --leak-check=full ./your_program
  ```

- [-] **分析内存分布**
  ```bash
  cat /proc/$(pidof your_program)/smaps
  ```

### 5.3 应用监控

- [ ] **记录 QPS**
  ```cpp
  std::atomic<std::uint64_t> query_count{0};

  // 搜索后
  query_count.fetch_add(1);

  // 定期计算
  double qps = query_count / elapsed_seconds;
  ```

- [ ] **记录延迟分布**
  ```cpp
  std::vector<double> latencies;

  auto start = std::chrono::high_resolution_clock::now();
  auto results = index.search(query, k);
  auto end = std::chrono::high_resolution_clock::now();

  double latency = std::chrono::duration<double, std::milli>(end - start).count();
  latencies.push_back(latency);
  ```

- [-] **计算百分位数**
  ```cpp
  std::sort(latencies.begin(), latencies.end());

  double p50 = latencies[latencies.size() * 0.5];
  double p95 = latencies[latencies.size() * 0.95];
  double p99 = latencies[latencies.size() * 0.99];
  ```

---

## 6. 部署优化

### 6.1 容器化

- [ ] **使用多阶段构建**
  ```dockerfile
  # 构建阶段
  FROM ubuntu:22.04 AS builder
  RUN apt-get update && apt-get install -y cmake g++
  COPY . /usearch
  RUN mkdir build && cd build && \
      cmake .. -DCMAKE_BUILD_TYPE=Release && \
      make -j$(nproc)

  # 运行阶段
  FROM ubuntu:22.04
  COPY --from=builder /usearch/build/usearch_server /usr/local/bin/
  ```

- [-] **限制资源**
  ```yaml
  resources:
    limits:
      cpus: '4'
      memory: 8G
  ```

### 6.2 负载均衡

- [ ] **配置健康检查**
  ```nginx
  upstream usearch {
      server shard1:8080 max_fails=3 fail_timeout=30s;
      server shard2:8080 max_fails=3 fail_timeout=30s;
  }
  ```

- [-] **使用 least_conn 负载均衡**
  ```nginx
  upstream usearch {
      least_conn;
      server shard1:8080;
      server shard2:8080;
  }
  ```

---

## 7. 性能基准

### 7.1 建立基准

- [ ] **定义基准场景**
  ```python
  benchmark_scenarios = [
      {"name": "small", "n_vectors": 10000, "dimensions": 128},
      {"name": "medium", "n_vectors": 1000000, "dimensions": 768},
      {"name": "large", "n_vectors": 10000000, "dimensions": 768},
  ]
  ```

- [ ] **记录基准数据**
  ```cpp
  struct BenchmarkResult {
      std::string scenario;
      std::size_t n_vectors;
      double build_time;
      double search_latency;
      double qps;
      double recall;
  };
  ```

### 7.2 性能对比

- [ ] **对比不同配置**
  ```python
  configs = [
      {"connectivity": 8, "expansion": 32},
      {"connectivity": 16, "expansion": 64},
      {"connectivity": 32, "expansion": 128},
  ]

  for config in configs:
      result = run_benchmark(config)
      print(f"Config: {config}")
      print(f"  QPS: {result.qps}")
      print(f"  Recall: {result.recall}")
  ```

- [-] **对比不同量化**
  ```python
  scalars = ["f32", "f16", "i8"]

  for scalar in scalars:
      result = run_benchmark(scalar=scalar)
      print(f"{scalar}: Memory={result.memory_mb}MB, QPS={result.qps}")
  ```

---

## 8. 常见性能问题

### 8.1 搜索慢

**检查项**：
- [ ] expansion 是否太小？
- [ ] 是否启用了多线程？
- [ ] CPU 是否达到了 100%？
- [ ] 是否有锁竞争？
- [ ] 网络延迟是否过高？（分布式）

**解决方案**：
```cpp
// 1. 增加 expansion
config.expansion = 128;

// 2. 启用多线程
config.multi = true;

// 3. 使用缓存
auto cached = cache.get(query);
if (cached) return cached;
```

### 8.2 内存不足

**检查项**：
- [ ] 向量数量是否超出预期？
- [ ] 是否使用了量化？
- [ ] 是否有内存泄漏？
- [ ] 碎片是否严重？

**解决方案**：
```cpp
// 1. 使用 f16 量化
scalar_kind_t scalar = scalar_kind_t::f16_k;

// 2. 分片
std::vector<index_dense_gt<>> shards;
shards.resize(num_shards);

// 3. 定期压缩索引
index.compact();
```

### 8.3 构建慢

**检查项**：
- [ ] 是否使用批量添加？
- [ ] 是否并行构建？
- [ ] ef_construction 是否过大？

**解决方案**：
```cpp
// 1. 批量添加
index.add(keys, vectors, n);

// 2. 并行构建分片
#pragma omp parallel for
for (std::size_t i = 0; i < num_shards; ++i) {
    shards[i].add(keys[i], vectors[i], n_per_shard);
}

// 3. 降低 ef_construction
config.expansion = 32;  // 构建时
```

---

## 9. 性能目标

### 9.1 定义目标

- [ ] **设定 QPS 目标**
  ```
  单节点: > 1000 QPS
  小集群: > 10000 QPS
  大集群: > 100000 QPS
  ```

- [ ] **设定延迟目标**
  ```
  P50: < 5 ms
  P95: < 20 ms
  P99: < 50 ms
  ```

- [ ] **设定召回率目标**
  ```
  基本场景: > 90%
  高精度: > 95%
  近似: > 85%
  ```

### 9.2 持续优化

- [ ] **定期性能测试**
  ```bash
  # 每周运行
  ./benchmark.py --weekly
  ```

- [-] **记录性能趋势**
  ```python
  def record_metrics():
      metrics = {
          'date': datetime.now(),
          'qps': get_current_qps(),
          'p99_latency': get_p99_latency(),
          'memory': get_memory_usage(),
      }
      save_to_database(metrics)
  ```

---

## 10. 生产环境检查清单

### 10.1 上线前

- [ ] **代码审查完成**
- [ ] **单元测试通过**
- [ ] **性能测试完成**
- [ ] **压力测试完成**
- [ ] **监控配置完成**
- [ ] **告警规则设置**
- [ ] **备份策略制定**
- [ ] **回滚方案准备**

### 10.2 上线后

- [ ] **监控关键指标**
  - QPS
  - 延迟（P50/P95/P99）
  - 错误率
  - 内存使用
  - CPU 使用
  - 磁盘 I/O

- [ ] **定期检查**
  ```bash
  # 每日
  - 检查错误日志
  - 检查性能指标

  # 每周
  - 分析性能趋势
  - 检查容量规划

  # 每月
  - 性能基准测试
  - 灾难恢复演练
  ```

---

## 11. 优化效果跟踪

### 11.1 优化前后对比

| 指标 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| QPS | ___ | ___ | ___ |
| P99 延迟 | ___ | ___ | ___ |
| 内存使用 | ___ | ___ | ___ |
| 召回率 | ___ | ___ | ___ |

### 11.2 ROI 分析

```
优化投入时间：____ 小时
性能提升：____ %
业务价值：____

ROI = 业务价值 / 投入时间
```

---

## 12. 快速诊断脚本

```bash
#!/bin/bash
# quick_diagnose.sh

echo "=== USearch 性能诊断 ==="

# 1. 检查 CPU
echo -e "\n1. CPU 使用："
top -bn1 | grep "Cpu(s)"

# 2. 检查内存
echo -e "\n2. 内存使用："
free -h

# 3. 检查进程
echo -e "\n3. 进程信息："
ps aux | grep usearch

# 4. 检查文件描述符
echo -e "\n4. 文件描述符："
cat /proc/$(pidof usearch)/limits | grep "open files"

# 5. 检查线程数
echo -e "\n5. 线程数："
ps -eLf | grep usearch | wc -l

# 6. 检查网络（分布式）
echo -e "\n6. 网络连接："
netstat -an | grep ESTABLISHED | wc -l

# 7. 检查磁盘 I/O
echo -e "\n7. 磁盘 I/O："
iostat -x 1 1 | grep -v "^$"

echo -e "\n=== 诊断完成 ==="
```

---

## 13. 优化建议优先级

### 高优先级（必须）

1. ✅ 使用批量操作
2. ✅ 启用编译优化（-O3）
3. ✅ 选择合适的配置参数
4. ✅ 预分配内存

### 中优先级（推荐）

5. ➖ 启用多线程
6. ➖ 使用量化
7. ➖ 系统参数调优
8. ➖ 添加缓存

### 低优先级（可选）

9. ⭕ NUMA 优化
10. ⭕ RDMA 优化（分布式）
11. ⭕ 自定义内存分配器
12. ⭕ GPU 加速

---

**版本**: v1.0
**最后更新**: 2025-01-24
**维护者**: USearch 社区
