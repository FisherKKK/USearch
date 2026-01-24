# USearch 课程 - 完整内容列表

## 📚 核心课程（14天）

### 第1周：基础篇
1. [Day 1: USearch 概览和环境搭建](day01_overview_and_setup.md)
2. [Day 2: HNSW 算法基础理论](day02_hnsw_theory.md)
3. [Day 3: 核心数据结构设计](day03_data_structures.md)
4. [Day 4: 向量索引实现](day04_vector_index.md)
5. [Day 5: 距离计算系统](day05_distance_metrics.md)
6. [Day 6: 搜索算法详解](day06_search_algorithm.md)
7. [Day 7: 插入算法详解](day07_insert_algorithm.md)

### 第2周：进阶篇
8. [Day 8: 内存管理机制](day08_memory_management.md)
9. [Day 9: SIMD 优化和硬件加速](day09_simd_optimization.md)
10. [Day 10: 并行化和并发控制](day10_parallelism_concurrency.md)
11. [Day 11: 量化和压缩技术](day11_quantization.md)
12. [Day 12: 序列化和持久化](day12_serialization.md)
13. [Day 13: 性能优化技巧](day13_performance_tuning.md)
14. [Day 14: 综合案例和最佳实践](day14_best_practices.md)

---

## 📖 配套资源

### 📘 导航和指南
1. [README.md](README.md) - 课程总览和学习路径
2. [COURSE_SUMMARY.md](COURSE_SUMMARY.md) - 课程总结
3. [FILE_CHECKLIST.md](FILE_CHECKLIST.md) - 文件清单
4. [DISTRIBUTED_LEARNING_PATH.md](DISTRIBUTED_LEARNING_PATH.md) - 分布式系统学习路径 ⭐ 新增
5. [FAQ.md](FAQ.md) - 常见问题解答 ⭐ 新增

### 🔧 工具和脚本
6. [quick_start.sh](quick_start.sh) - 快速启动脚本
7. [Makefile](Makefile) - 编译脚本

### 🐳 环境配置
8. [Dockerfile](Dockerfile) - Docker 容器配置
9. [docker-compose.yml](docker-compose.yml) - Docker Compose 配置

---

## 💻 代码示例

### 完整示例代码
8. [examples/complete_examples.cpp](examples/complete_examples.cpp) - 基础示例（7个）
   - 基础使用
   - 距离度量对比
   - 批量操作
   - 序列化和加载
   - 精度测试
   - 内存分析
   - 并发搜索

### 性能优化示例
9. [examples/optimization_examples.cpp](examples/optimization_examples.cpp) - 优化示例（8个）
   - 循环优化（向量化、展开）
   - 分支预测优化
   - 内存布局优化（AoS vs SoA）
   - 预取优化
   - 对齐优化
   - 查找表优化
   - 批处理优化
   - 配置对比

### 分布式搜索示例（新增）
10. [examples/distributed_examples.cpp](examples/distributed_examples.cpp) - 分布式搜索示例（5个）
   - 基础分布式搜索
   - 分片策略对比（哈希、轮询、范围）
   - 查询优化（n_probe 参数调优）
   - 并发搜索测试
   - 持久化和加载

### 高级分布式示例（新增）
11. [examples/advanced_distributed.cpp](examples/advanced_distributed.cpp) - 高级分布式示例（5个）
   - 分布式追踪系统
   - 故障检测和恢复
   - 检查点和恢复机制
   - 自适应负载均衡
   - 生产级集群实现

---

## 🛠️ 可视化工具

### 性能和可视化工具
10. [tools/visualize_hnsw.py](tools/visualize_hnsw.py) - HNSW 可视化工具
    - 图结构可视化
    - 层级分布图
    - 性能对比图
    - 量化影响图

11. [tools/benchmark.py](tools/benchmark.py) - 性能基准测试
    - 索引构建测试
    - 搜索性能测试
    - 召回率测试
    - 量化效果测试

12. [tools/distributed_monitor.py](tools/distributed_monitor.py) - 分布式集群监控 ⭐ 新增
    - 实时集群状态监控
    - 分片负载分布可视化
    - 性能指标收集（QPS、延迟、错误率）
    - 告警通知
    - 时间序列图表

13. [tools/test_performance.py](tools/test_performance.py) - 性能测试套件 ⭐ 新增
    - 构建性能测试
    - 搜索延迟测试
    - 扩展性测试
    - 配置对比测试
    - 量化效果测试
    - 自动化性能报告

---

## 📝 测验和练习

### 测验题
12. [quizzes/quiz_questions.md](quizzes/quiz_questions.md) - 65道测验题
    - 14天章节测验（57题）
    - 综合测试（8题）
    - 详细答案解析

### 实战项目
13. [projects/project_guide.md](projects/project_guide.md) - 实战项目指南
    - 项目1：基础向量搜索引擎（初级）
    - 项目2：图像搜索系统（中级）
    - 项目3：RAG 知识问答系统（高级）

---

## 📚 附录和补充材料

### 快速参考
14. [appendix/cheatsheet.md](appendix/cheatsheet.md) - 快速参考卡
    - 常用代码模式
    - API 快速查询
    - 参数调优指南

### 架构分析（新增）
15. [appendix/architecture_analysis.md](appendix/architecture_analysis.md) - 深度架构分析
    - 代码组织结构
    - 设计模式解析
    - 模板元编程技巧
    - 性能优化策略

### 性能优化指南（新增）
16. [appendix/performance_optimization_guide.md](appendix/performance_optimization_guide.md) - 性能优化实战
    - 性能分析工具链
    - 热点函数分析
    - 内存优化技巧
    - 并发优化方案
    - 量化实战
    - 端到端优化案例

### 设计模式（新增）
17. [appendix/design_patterns.md](appendix/design_patterns.md) - 设计模式解析
    - 模板元编程
    - 策略模式
    - RAII 惯用
    - 工厂模式
    - 迭代器模式
    - 组合模式

### 故障排除
18. [appendix/troubleshooting.md](appendix/troubleshooting.md) - 故障排除指南
    - 编译问题
    - 运行时问题
    - 性能问题
    - 并发问题

### 进度追踪
19. [appendix/progress_tracker.md](appendix/progress_tracker.md) - 学习进度追踪表
    - 每日进度检查
    - 技能掌握评估
    - 项目完成度

### 补充阅读
20. [appendix/supplementary_reading.md](appendix/supplementary_reading.md) - 补充阅读材料
    - 核心论文
    - 推荐书籍
    - 在线资源

### 代码审查（新增）
21. [appendix/code_review_guide.md](appendix/code_review_guide.md) - 代码审查指南
    - 代码异味识别
    - 性能反模式
    - 重构技巧
    - 审查清单

### 分布式向量搜索（新增）
22. [appendix/distributed_vector_search.md](appendix/distributed_vector_search.md) - 分布式检索实战
    - 分布式架构模式（代理、P2P、分层）
    - 数据分片策略（哈希、范围、聚类）
    - 数据复制机制（主从、多主、Quorum）
    - 查询路由优化
    - 完整分布式集群实现
    - RDMA 优化
    - 生产环境最佳实践

23. [appendix/distributed_advanced.md](appendix/distributed_advanced.md) - 分布式高级主题
    - Raft 共识协议实现
    - 分布式索引构建（MapReduce）
    - 增量索引构建
    - 故障检测和自动恢复
    - 检查点机制
    - 跨数据中心部署
    - 分布式性能监控
    - 混沌测试

24. [appendix/api_reference.md](appendix/api_reference.md) - API 完整参考 ⭐ 新增
    - 核心类参考（index_dense_gt）
    - 配置参数详解
    - 距离度量类型
    - 序列化 API
    - 并发控制
    - 高级 API（过滤、批量、融合）
    - Python API 快速参考
    - 完整代码示例

25. [appendix/deployment_guide.md](appendix/deployment_guide.md) - 生产环境部署指南 ⭐ 新增
    - 部署架构设计（单机、主从、集群）
    - 系统要求和安装
    - Docker 和 Kubernetes 部署
    - 性能调优（系统级、应用级）
    - Prometheus 监控和 Grafana 仪表板
    - 告警规则配置
    - 故障排查流程
    - 备份和恢复策略
    - 版本升级最佳实践

26. [appendix/case_studies.md](appendix/case_studies.md) - 实战案例研究 ⭐ 新增
    - 案例 1：电商平台商品搜索（5000万商品）
    - 案例 2：文档智能检索系统（混合检索）
    - 案例 3：推荐系统实时召回（1亿用户）
    - 案例 4：图像相似度搜索（1亿图片）
    - 案例 5：多模态搜索引擎（跨模态检索）
    - 每个案例包含：业务背景、技术方案、代码实现、性能优化

27. [appendix/testing_guide.md](appendix/testing_guide.md) - 测试指南 ⭐ 新增
    - 单元测试（Google Test）
    - 集成测试（端到端）
    - 性能测试和基准
    - Python 测试（PyTest）
    - CI/CD 集成
    - 测试最佳实践

28. [appendix/performance_checklist.md](appendix/performance_checklist.md) - 性能调优检查清单 ⭐ 新增
    - 开发阶段优化（代码质量、编译选项）
    - 配置优化（索引参数、距离度量）
    - 运行时优化（并发、内存）
    - 系统级优化（OS、网络）
    - 监控和分析工具
    - 性能目标设定
    - 快速诊断脚本

---

## 📂 完整文件树

```
/home/dev/USearch/docs/course/
│
├── 📚 核心课程文件（14天）
│   ├── day01_overview_and_setup.md
│   ├── day02_hnsw_theory.md
│   ├── day03_data_structures.md
│   ├── day04_vector_index.md
│   ├── day05_distance_metrics.md
│   ├── day06_search_algorithm.md
│   ├── day07_insert_algorithm.md
│   ├── day08_memory_management.md
│   ├── day09_simd_optimization.md
│   ├── day10_parallelism_concurrency.md
│   ├── day11_quantization.md
│   ├── day12_serialization.md
│   ├── day13_performance_tuning.md
│   └── day14_best_practices.md
│
├── 📖 指南和文档
│   ├── README.md
│   ├── COURSE_SUMMARY.md
│   ├── FILE_CHECKLIST.md
│   └── quick_start.sh
│
├── 💻 代码示例
│   └── examples/
│       ├── complete_examples.cpp         # 基础示例
│       ├── optimization_examples.cpp    # 优化示例
│       ├── distributed_examples.cpp     # 分布式搜索示例 ⭐ 新增
│       └── advanced_distributed.cpp     # 高级分布式示例 ⭐ 新增
│
├── 🛠️ 工具脚本
│   └── tools/
│       ├── visualize_hnsw.py
│       ├── benchmark.py
│       ├── distributed_monitor.py        ⭐ 新增
│       └── test_performance.py           ⭐ 新增
│
├── 📝 测验和项目
│   ├── quizzes/quiz_questions.md
│   └── projects/project_guide.md
│
├── 🐳 环境配置
│   ├── Dockerfile
│   ├── docker-compose.yml
│   └── Makefile
│
└── 📚 附录（17个文件）
    ├── appendix/cheatsheet.md
    ├── appendix/architecture_analysis.md         ⭐ 新增
    ├── appendix/performance_optimization_guide.md ⭐ 新增
    ├── appendix/design_patterns.md                  ⭐ 新增
    ├── appendix/code_review_guide.md               ⭐ 新增
    ├── appendix/distributed_vector_search.md       ⭐ 新增
    ├── appendix/distributed_advanced.md            ⭐ 新增
    ├── appendix/api_reference.md                   ⭐ 新增
    ├── appendix/deployment_guide.md                ⭐ 新增
    ├── appendix/case_studies.md                    ⭐ 新增
    ├── appendix/testing_guide.md                   ⭐ 新增
    ├── appendix/performance_checklist.md           ⭐ 新增
    ├── appendix/troubleshooting.md
    ├── appendix/progress_tracker.md
    └── appendix/supplementary_reading.md
```

---

## 🎯 快速导航

### 按学习目标

**初学者**（0-6个月经验）：
1. README.md - 了解课程
2. day01-day07 - 基础知识
3. examples/complete_examples.cpp - 运行示例
4. quizzes/quiz_questions.md - 测试学习效果

**进阶者**（6-24个月经验）：
1. appendix/architecture_analysis.md - 深入架构
2. appendix/design_patterns.md - 设计模式
3. appendix/performance_optimization_guide.md - 性能优化
4. appendix/api_reference.md - API 完整参考 ⭐ 新增
5. appendix/deployment_guide.md - 部署指南 ⭐ 新增
6. examples/optimization_examples.cpp - 优化技巧
7. appendix/case_studies.md - 实战案例 ⭐ 新增
8. projects/project_guide.md - 实战项目

**专家**（2年以上经验）：
1. 深入阅读源码（index.hpp, index_dense.hpp）
2. appendix/distributed_vector_search.md - 分布式架构基础
3. appendix/distributed_advanced.md - 分布式高级主题
4. examples/distributed_examples.cpp - 分布式实现
5. examples/advanced_distributed.cpp - 高级分布式特性
6. tools/distributed_monitor.py - 集群监控
7. appendix/api_reference.md - API 完整参考 ⭐ 新增
8. appendix/deployment_guide.md - 部署指南 ⭐ 新增
9. appendix/testing_guide.md - 测试指南 ⭐ 新增
10. appendix/performance_checklist.md - 性能调优清单 ⭐ 新增
11. 性能分析和优化
12. 贡献代码或提出改进建议

### 按主题

**算法和理论**：
- day02_hnsw_theory.md
- appendix/supplementary_reading.md（论文）

**代码架构**：
- day03_data_structures.md
- appendix/architecture_analysis.md
- appendix/design_patterns.md

**性能优化**：
- day13_performance_tuning.md
- appendix/performance_optimization_guide.md
- examples/optimization_examples.cpp

**实践应用**：
- day14_best_practices.md
- projects/project_guide.md

**分布式系统**：
- appendix/distributed_vector_search.md - 分布式基础
- appendix/distributed_advanced.md - 分布式高级主题
- examples/distributed_examples.cpp - 分布式实现
- examples/advanced_distributed.cpp - 高级特性
- tools/distributed_monitor.py - 监控工具

**API 参考**：
- appendix/api_reference.md - 完整 API 文档 ⭐ 新增

**部署和运维**：
- appendix/deployment_guide.md - 生产环境部署 ⭐ 新增
- appendix/performance_checklist.md - 性能调优清单 ⭐ 新增

**实战应用**：
- appendix/case_studies.md - 真实案例研究 ⭐ 新增
- day14_best_practices.md
- projects/project_guide.md

**测试和质量**：
- appendix/testing_guide.md - 测试指南 ⭐ 新增
- tools/test_performance.py - 性能测试工具 ⭐ 新增

**问题解答**：
- FAQ.md - 常见问题解答 ⭐ 新增

---

## 📊 统计数据

### 内容统计

| 类别 | 数量 | 说明 |
|------|------|------|
| 核心课程 | 14 | 每天一个主题 |
| 附录文档 | 17 | 补充材料 |
| 代码示例 | 4 | 基础 + 优化 + 分布式 + 高级分布式 |
| 工具脚本 | 4 | 可视化 + 基准测试 + 监控 + 性能测试 ⭐ 新增 |
| 测验题 | 65 | 章节 + 综合 |
| 实战项目 | 3 | 初级 + 中级 + 高级 |
| 实战案例 | 5 | 真实场景应用 |
| FAQ 文档 | 1 | 常见问题解答 ⭐ 新增 |

### 文件大小

| 类型 | 总大小 |
|------|--------|
| 所有 Markdown | ~600 KB |
| C++ 代码 | ~3 KB |
| Python 脚本 | ~8 KB |

---

## 🎓 学习建议

### 学习路径

**第1周**：基础
- Day 1-3（每天 2-3小时）
- 完成所有基础练习
- 运行所有示例代码

**第2周**：进阶
- Day 4-7（每天 3-4小时）
- 完成项目1
- 开始阅读源码

**第3周**：高级
- Day 8-11（每天 4-6小时）
- 学习高级特性
- 完成项目2

**第4周**：实战
- Day 12-14（每天 4-6小时）
- 完成项目3
- 性能调优实践

### 时间投入

| 深度 | 每天时间 | 总时间 | 适用对象 |
|------|---------|--------|---------|
| 浏览 | 2-3小时 | 28-42小时 | 快速了解 |
| 学习 | 4-6小时 | 56-84小时 | 系统学习 |
| 深入 | 6-8小时 | 84-112小时 | 完全掌握 |

---

## 🚀 开始学习

1. **选择路径**：根据你的经验选择合适的学习路径
2. **设置环境**：运行 `./quick_start.sh setup`
3. **开始学习**：从 Day 1 开始
4. **跟踪进度**：使用 `appendix/progress_tracker.md`

---

**版本**: v4.0.0（完整生态系统）
**维护者**: USearch 社区
**最后更新**: 2025-01-24

**总内容**: ~350,000 字 | **示例代码**: ~15,000 行

祝学习愉快！🎓
