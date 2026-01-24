# USearch 生产环境部署指南
## Production Deployment Guide

---

## 📚 目录

1. [部署架构](#部署架构)
2. [单机部署](#单机部署)
3. [分布式部署](#分布式部署)
4. [云原生部署](#云原生部署)
5. [性能调优](#性能调优)
6. [监控和告警](#监控和告警)
7. [故障排查](#故障排查)
8. [最佳实践](#最佳实践)

---

## 1. 部署架构

### 1.1 单机架构

**适用场景**：
- 向量数量 < 1000 万
- QPS < 1000
- 快速原型开发

```
┌─────────────────────────┐
│   Application Server    │
├─────────────────────────┤
│                         │
│  ┌───────────────────┐  │
│  │  USearch Index    │  │
│  │  (Single Process) │  │
│  └───────────────────┘  │
│                         │
└─────────────────────────┘
```

### 1.2 主从架构

**适用场景**：
- 需要高可用
- 读写分离

```
┌──────────┐
│  Client  │
└────┬─────┘
     │
     ├─────► ┌──────────────┐ (Read)
     │       │   Slave 1    │
     │       └──────────────┘
     │
     ├─────► ┌──────────────┐ (Read)
     │       │   Slave 2    │
     │       └──────────────┘
     │
     └─────► ┌──────────────┐ (Write)
             │    Master    │
             └──────────────┘
```

### 1.3 分布式集群架构

**适用场景**：
- 大规模数据（> 1 亿向量）
- 高吞吐量（> 10,000 QPS）
- 需要水平扩展

```
                    ┌─────────────┐
                    │   Load      │
                    │  Balancer   │
                    └──────┬──────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
    ┌────▼────┐      ┌─────▼─────┐     ┌────▼────┐
    │ Shard 1 │      │  Shard 2  │     │ Shard 3 │
    ├────────┤      ├───────────┤     ├─────────┤
    │ Node A  │      │ Node B    │     │ Node C  │
    │ Node A' │      │ Node B'   │     │ Node C' │
    └─────────┘      └───────────┘     └─────────┘
```

---

## 2. 单机部署

### 2.1 系统要求

**最低配置**：
- CPU: 4 核
- 内存: 8 GB
- 存储: 100 GB SSD
- OS: Linux (Ubuntu 20.04+, CentOS 8+)

**推荐配置**：
- CPU: 16 核（支持 AVX2）
- 内存: 64 GB
- 存储: 1 TB NVMe SSD
- OS: Linux (Ubuntu 22.04 LTS)

### 2.2 安装步骤

#### 从源码编译

```bash
# 1. 安装依赖
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libgomp1

# 2. 克隆仓库
git clone https://github.com/unum-cloud/usearch.git
cd usearch

# 3. 编译
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSEARCH_USE_OPENMP=ON \
    -DUSEARCH_USE_SIMSIMD=ON
make -j$(nproc)

# 4. 安装
sudo make install
```

#### Docker 部署

```dockerfile
FROM ubuntu:22.04

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libgomp1

# 复制源码
COPY . /usearch
WORKDIR /usearch

# 编译
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DUSEARCH_USE_OPENMP=ON && \
    make -j$(nproc) && \
    make install

# 运行
CMD ["./usearch_server"]
```

构建和运行：
```bash
docker build -t usearch:latest .
docker run -p 8080:8080 -v /data:/data usearch:latest
```

### 2.3 配置文件

创建配置文件 `config.json`：

```json
{
  "index": {
    "dimensions": 768,
    "metric": "cos",
    "dtype": "f32",
    "connectivity": 16,
    "expansion": 64
  },
  "server": {
    "host": "0.0.0.0",
    "port": 8080,
    "threads": 8
  },
  "storage": {
    "index_path": "/data/index.usearch",
    "auto_save": true,
    "save_interval": 300
  }
}
```

### 2.4 服务管理

#### Systemd 服务

创建 `/etc/systemd/system/usearch.service`：

```ini
[Unit]
Description=USearch Vector Search Service
After=network.target

[Service]
Type=simple
User=usearch
WorkingDirectory=/opt/usearch
ExecStart=/opt/usearch/usearch_server --config /etc/usearch/config.json
Restart=always
RestartSec=10

# 资源限制
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
```

启用服务：
```bash
sudo systemctl daemon-reload
sudo systemctl enable usearch
sudo systemctl start usearch
sudo systemctl status usearch
```

---

## 3. 分布式部署

### 3.1 部署架构设计

**示例：4 节点集群**

```yaml
# docker-compose.yml
version: '3.8'

services:
  # 负载均衡器
  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
    depends_on:
      - shard1
      - shard2
      - shard3
      - shard4

  # 分片节点
  shard1:
    image: usearch:latest
    environment:
      - SHARD_ID=0
      - SHARD_COUNT=4
    volumes:
      - shard1_data:/data

  shard2:
    image: usearch:latest
    environment:
      - SHARD_ID=1
      - SHARD_COUNT=4
    volumes:
      - shard2_data:/data

  shard3:
    image = usearch:latest
    environment:
      - SHARD_ID=2
      - SHARD_COUNT=4
    volumes:
      - shard3_data:/data

  shard4:
    image: usearch:latest
    environment:
      - SHARD_ID=3
      - SHARD_COUNT=4
    volumes:
      - shard4_data:/data

volumes:
  shard1_data:
  shard2_data:
  shard3_data:
  shard4_data:
```

### 3.2 Nginx 配置

```nginx
# nginx.conf
events {
    worker_connections 1024;
}

http {
    upstream usearch_cluster {
        least_conn;

        server shard1:8080 weight=1;
        server shard2:8080 weight=1;
        server shard3:8080 weight=1;
        server shard4:8080 weight=1;

        # 健康检查
        check interval=3000 rise=2 fall=3 timeout=1000;
    }

    server {
        listen 80;

        location / {
            proxy_pass http://usearch_cluster;
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;

            # 超时设置
            proxy_connect_timeout 5s;
            proxy_send_timeout 60s;
            proxy_read_timeout 60s;
        }

        # 健康检查端点
        location /health {
            access_log off;
            return 200 "OK\n";
            add_header Content-Type text/plain;
        }
    }
}
```

### 3.3 启动集群

```bash
# 启动所有节点
docker-compose up -d

# 检查状态
docker-compose ps

# 查看日志
docker-compose logs -f

# 扩展分片
docker-compose up -d --scale shard=8
```

---

## 4. 云原生部署

### 4.1 Kubernetes 部署

#### Deployment 配置

```yaml
# usearch-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: usearch-shard
  labels:
    app: usearch
spec:
  replicas: 4
  selector:
    matchLabels:
      app: usearch
  template:
    metadata:
      labels:
        app: usearch
    spec:
      containers:
      - name: usearch
        image: usearch:latest
        ports:
        - containerPort: 8080
        env:
        - name: SHARD_ID
          valueFrom:
            fieldRef:
              fieldPath: metadata.annotations['shard-id']
        - name: SHARD_COUNT
          value: "4"
        resources:
          requests:
            memory: "4Gi"
            cpu: "2"
          limits:
            memory: "8Gi"
            cpu: "4"
        volumeMounts:
        - name: data
          mountPath: /data
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 10
          periodSeconds: 5
      volumes:
      - name: data
        emptyDir: {}
---
apiVersion: v1
kind: Service
metadata:
  name: usearch-service
spec:
  selector:
    app: usearch
  ports:
  - protocol: TCP
    port: 80
    targetPort: 8080
  type: LoadBalancer
```

应用配置：
```bash
kubectl apply -f usearch-deployment.yaml
kubectl get pods
kubectl get svc
```

### 4.2 Helm Chart

创建 Chart 结构：
```
usearch-chart/
├── Chart.yaml
├── values.yaml
├── templates/
│   ├── deployment.yaml
│   ├── service.yaml
│   ├── configmap.yaml
│   └── ingress.yaml
```

**values.yaml**：
```yaml
replicaCount: 4

image:
  repository: usearch
  tag: latest
  pullPolicy: IfNotPresent

service:
  type: LoadBalancer
  port: 80

resources:
  limits:
    cpu: 4
    memory: 8Gi
  requests:
    cpu: 2
    memory: 4Gi

config:
  dimensions: 768
  metric: cos
  connectivity: 16
  expansion: 64
```

部署：
```bash
helm install usearch ./usearch-chart
helm upgrade usearch ./usearch-chart
```

---

## 5. 性能调优

### 5.1 系统级优化

#### 内核参数调优

```bash
# /etc/sysctl.conf

# 网络优化
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_tw_reuse = 1
net.ipv4.ip_local_port_range = 10000 65535

# 内存优化
vm.swappiness = 1
vm.dirty_ratio = 15
vm.dirty_background_ratio = 5

# 文件描述符
fs.file-max = 2097152
```

应用：
```bash
sudo sysctl -p
```

#### 文件描述符限制

```bash
# /etc/security/limits.conf
usearch soft nofile 65536
usearch hard nofile 65536
usearch soft nproc 4096
usearch hard nproc 4096
```

### 5.2 应用级优化

#### 配置调优

```cpp
// 高性能配置
index_dense_config_t config;

// 连接性
config.connectivity = 16;      // 平衡精度和速度
config.expansion = 64;         // 搜索范围

// 并发
config.multi = true;           // 启用多线程

// 对齐
config.alignment = 64;         // 缓存行对齐

// 初始化
index.init(dimensions, metric_kind_t::cos_k, scalar_kind_t::f32_k, config);
```

#### 内存优化

```cpp
// 预分配
index.reserve(10000000);  // 1000万向量

// 使用量化
index.init(dimensions,
           metric_kind_t::cos_k,
           scalar_kind_t::f16_k);  // 节省50%内存
```

### 5.3 批处理优化

```cpp
// ❌ 逐个处理
for (std::size_t i = 0; i < n; ++i) {
    index.add(keys[i], vectors + i * d);
}

// ✅ 批处理
index.add(keys, vectors, n);

// ✅ 并行批处理
#pragma omp parallel for schedule(dynamic)
for (std::size_t batch = 0; batch < n_batches; ++batch) {
    std::size_t start = batch * batch_size;
    std::size_t end = std::min(start + batch_size, n);
    index.add(keys + start, vectors + start * d, end - start);
}
```

---

## 6. 监控和告警

### 6.1 Prometheus 监控

#### 导出指标

```cpp
#include <prometheus/registry.h>
#include <prometheus/exposer.h>

class MonitoredIndex {
    index_dense_gt<float, std::uint32_t> index_;

    // Prometheus 指标
    prometheus::Counter& query_counter_;
    prometheus::Histogram& query_latency_;
    prometheus::Gauge& index_size_;

public:
    MonitoredIndex()
        : query_counter_(prometheus::BuildCounter()
            .Name("usearch_queries_total")
            .Register(prometheus::DefaultRegistry())
            .Add({})),
          query_latency_(prometheus::BuildHistogram()
            .Name("usearch_query_latency_seconds")
            .Register(prometheus::DefaultRegistry())
            .Add({})),
          index_size_(prometheus::BuildGauge()
            .Name("usearch_index_size")
            .Register(prometheus::DefaultRegistry())
            .Add({}))
    {}

    std::vector<result_t> search(float const* query, std::size_t k) {
        auto start = std::chrono::system_clock::now();

        auto results = index_.search(query, k);

        auto end = std::chrono::system_clock::now();
        auto duration = std::chrono::duration<double>(end - start).count();

        // 记录指标
        query_counter_.Increment();
        query_latency_.Observe(duration);
        index_size_.Set(index_.size());

        return results;
    }
};
```

#### Prometheus 配置

```yaml
# prometheus.yml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'usearch'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
```

### 6.2 Grafana 仪表板

#### 关键指标

**查询指标**：
- `rate(usearch_queries_total[5m])` - QPS
- `histogram_quantile(0.99, usearch_query_latency_seconds)` - P99 延迟
- `histogram_quantile(0.95, usearch_query_latency_seconds)` - P95 延迟

**索引指标**：
- `usearch_index_size` - 索引大小
- `usearch_memory_usage_bytes` - 内存使用
- `usearch_disk_usage_bytes` - 磁盘使用

**系统指标**：
- `rate(process_cpu_seconds_total[5m])` - CPU 使用率
- `process_resident_memory_bytes` - 内存使用

### 6.3 告警规则

```yaml
# alerting.yml
groups:
  - name: usearch_alerts
    interval: 30s
    rules:
      # 高延迟告警
      - alert: HighSearchLatency
        expr: histogram_quantile(0.99, usearch_query_latency_seconds) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High search latency detected"
          description: "P99 latency is {{ $value }}s"

      # 低 QPS 告警
      - alert: LowQueryRate
        expr: rate(usearch_queries_total[5m]) < 100
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "Low query rate"
          description: "QPS is {{ $value }}"

      # 内存使用告警
      - alert: HighMemoryUsage
        expr: usearch_memory_usage_bytes / node_memory_MemTotal_bytes > 0.9
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High memory usage"
          description: "Memory usage is {{ $value }}%"
```

---

## 7. 故障排查

### 7.1 常见问题

#### 问题 1：搜索慢

**症状**：P99 延迟 > 100ms

**诊断**：
```bash
# 检查 CPU 使用
top -p $(pidof usearch_server)

# 检查线程数
ps -eLf | grep usearch_server | wc -l

# 检查索引大小
curl http://localhost:8080/stats
```

**解决方案**：
1. 增加 expansion 参数
2. 启用多线程
3. 使用量化（f16/i8）
4. 添加缓存

#### 问题 2：内存不足

**症状**：OOM 错误

**诊断**：
```bash
# 检查内存使用
free -h

# 检查进程内存
cat /proc/$(pidof usearch_server)/status | grep VmRSS

# 使用 valgrind 检查泄漏
valgrind --leak-check=full ./usearch_server
```

**解决方案**：
1. 使用 f16 量化
2. 启用 swap
3. 分片索引
4. 增加物理内存

#### 问题 3：构建索引慢

**症状**：构建 1000 万向量需要 > 1 小时

**解决方案**：
```cpp
// 批量添加（关键）
index.add(keys, vectors, n);

// 并行构建
#pragma omp parallel for
for (std::size_t i = 0; i < n_batches; ++i) {
    // 批量添加
}

// 降低 ef_construction
config.expansion = 32;  // 降低以提高速度
```

### 7.2 调试工具

#### 性能分析

```bash
# CPU 性能分析
perf record -g -p $(pidof usearch_server)
perf report

# 火焰图
git clone https://github.com/brendangregg/FlameGraph
perf script | FlameGraph/stackcollapse-perf.pl | \
    FlameGraph/flamegraph.pl > flamegraph.svg
```

#### 内存分析

```bash
# 内存映射
pmap -x $(pidof usearch_server)

# 堆分析
jmap -heap $(pidof usearch_server)

# 内存泄漏检测
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./usearch_server
```

---

## 8. 最佳实践

### 8.1 容量规划

**估算公式**：

```
内存需求 = 向量数量 × 维度 × 字节数 × (1 + overhead)

示例：1000万 768维向量
- f32: 10M × 768 × 4 × 1.5 = ~46 GB
- f16: 10M × 768 × 2 × 1.5 = ~23 GB
- i8:  10M × 768 × 1 × 1.5 = ~11 GB
```

**QPS 估算**：

```
单节点 QPS = 核心数 / 平均延迟(秒)

示例：16 核，平均延迟 5ms
QPS = 16 / 0.005 = 3200
```

### 8.2 备份策略

#### 增量备份

```bash
#!/bin/bash
# backup.sh

BACKUP_DIR="/backup/usearch"
INDEX_PATH="/data/index.usearch"
DATE=$(date +%Y%m%d_%H%M%S)

# 创建快照
cp $INDEX_PATH $BACKUP_DIR/index_$DATE.usearch

# 压缩
gzip $BACKUP_DIR/index_$DATE.usearch

# 上传到 S3
aws s3 cp $BACKUP_DIR/index_$DATE.usearch.gz \
    s3://backups/usearch/

# 清理旧备份（保留最近7天）
find $BACKUP_DIR -name "index_*.usearch.gz" -mtime +7 -delete
```

#### 定时任务

```bash
# 添加到 crontab
0 */6 * * * /opt/scripts/backup.sh
```

### 8.3 安全配置

#### 网络隔离

```yaml
# 仅允许内网访问
# iptables
-A INPUT -p tcp --dport 8080 -s 10.0.0.0/8 -j ACCEPT
-A INPUT -p tcp --dport 8080 -j DROP
```

#### 认证

```cpp
// 添加 API 密钥验证
bool authenticate(std::string const& api_key) {
    static const std::set<std::string> valid_keys = {
        "key1", "key2", "key3"
    };
    return valid_keys.count(api_key) > 0;
}
```

### 8.4 版本升级

**滚动升级**：

```bash
# 1. 拉取新镜像
docker pull usearch:v2.0

# 2. 逐个升级节点
for shard in shard1 shard2 shard3 shard4; do
    docker-compose up -d --no-deps --force-recreate $shard
    sleep 30  # 等待节点就绪
done

# 3. 验证
curl http://localhost/health
```

---

## 9. 检查清单

### 部署前检查

- [ ] 硬件资源满足要求
- [ ] 操作系统版本兼容
- [ ] 依赖库已安装
- [ ] 网络配置正确
- [ ] 防火墙规则已设置
- [ ] 存储空间充足
- [ ] 备份策略已制定

### 部署后检查

- [ ] 服务正常启动
- [ ] 健康检查通过
- [ ] 性能基准测试完成
- [ ] 监控配置完成
- [ ] 告警规则设置
- [ ] 日志正常输出
- [ ] 备份任务运行

### 运维检查

- [ ] 每日：检查系统日志
- [ ] 每周：检查性能指标
- [ ] 每月：检查备份完整性
- [ ] 每季度：容量规划评估
- [ ] 每年：灾难恢复演练

---

**版本**: v1.0
**最后更新**: 2025-01-24
