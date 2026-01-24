# 补充阅读材料

## 核心论文

### 必读论文

1. **HNSW 原论文**
   - **标题**: Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs
   - **作者**: Y. A. Malkov, D. A. Yashunin
   - **年份**: 2018
   - **会议**: arXiv preprint arXiv:1603.09320
   - **链接**: https://arxiv.org/abs/1603.09320
   - **核心贡献**: 提出了 HNSW 算法，结合了 NSW 的可导航性和层级结构的效率

2. **NSW 前身**
   - **标题**: Small world graphs for efficient approximate nearest neighbor search
   - **作者**: Y. A. Malkov, A. V. Ponomarenko, A. A. Krylov, D. A. Yashunin
   - **年份**: 2014
   - **会议**: SPIE 2014
   - **链接**: https://doi.org/10.1117/12.2063664
   - **核心贡献**: 提出了 NSW 图，引入了可导航小世界概念

3. **乘积量化**
   - **标题**: Product Quantization for Nearest Neighbor Search
   - **作者**: H. Jegou, M. Douze, C. Schmid
   - **年份**: 2011
   - **会议**: IEEE TPAMI 2011
   - **链接**: https://lear.inrialpes.fr/pubs/2011/JDS11/jegou_searching_with_pq_2011.pdf
   - **核心贡献**: 提出了乘积量化（PQ），大幅降低内存使用

### 推荐论文

4. **IVF-PQ**
   - **标题**: Searching in one billion vectors: re-rank with source coding
   - **作者**: H. Jegou, R. Tavenard, M. Douze, L. Amsaleg
   - **年份**: 2011
   - **会议**: ICASSP 2011
   - **核心贡献**: 结合倒排文件和乘积量化

5. **Annoy**
   - **作者**: Erik Bernhardsson
   - **项目**: Spotify Annoy
   - **链接**: https://github.com/spotify/annoy
   - **核心贡献**: 基于森林的近似最近邻搜索

6. **Faiss**
   - **作者**: Johnson et al., Facebook Research
   - **年份**: 2019
   - **会议**: arXiv:1902.09143
   - **链接**: https://arxiv.org/abs/1902.09143
   - **核心贡献**: 综合性的相似度搜索库

## 书籍推荐

### C++ 高级编程

1. **"Effective C++"** by Scott Meyers
   - 第3版
   - ISBN: 978-0321334879
   - 重点章节: 资源管理、继承与面向对象设计

2. **"Effective Modern C++"** by Scott Meyers
   - ISBN: 978-1491903995
   - 重点章节: 智能指针、移动语义、lambda 表达式

3. **"C++ Concurrency in Action"** by Anthony Williams
   - 第2版
   - ISBN: 978-1617294693
   - 重点章节: 线程管理、并发设计、无锁编程

### 算法与数据结构

4. **"Introduction to Algorithms"** by Cormen et al.
   - 第3版（CLRS）
   - ISBN: 978-0262033848
   - 重点章节: 图算法、哈希表、堆

5. **"Algorithm Design Manual"** by Steven Skiena
   - 第2版
   - ISBN: 978-1848000698
   - 重点章节: 数据结构、搜索算法

### 机器学习与向量空间

6. **"Mining of Massive Datasets"** by Leskovec et al.
   - 在线免费版本: http://www.mmds.org/
   - 重点章节: 相似度搜索、聚类

7. **"Information Retrieval"** by Manning et al.
   - ISBN: 978-0521865715
   - 重点章节: 向量空间模型

## 在线资源

### 教程和博客

1. **HNSW 可视化教程**
   - 链接: https://towardsdatascience.com/hnsw-hierarchical-navigable-small-world-explained-edd13233a9d
   - 内容: 图文并茂的 HNSW 算法解释

2. **SimSIMD 文档**
   - 链接: https://github.com/ashvardanian/simsimd
   - 内容: SIMD 优化的详细说明

3. **ANN-Benchmarks**
   - 链接: https://github.com/erikbern/ann-benchmarks
   - 内容: 各种 ANN 算法的性能对比

4. **USearch GitHub Discussions**
   - 链接: https://github.com/unum-cloud/usearch/discussions
   - 内容: 社区问答和讨论

### 视频资源

5. **近似最近邻搜索概述**
   - 链接: YouTube "Approximate Nearest Neighbor Search" by Piotr Indyk
   - 时长: ~45 分钟
   - 内容: LSH、随机投影等算法

6. **向量数据库技术**
   - 链接: YouTube "Vector Databases Explained"
   - 时长: ~30 分钟
   - 内容: 向量数据库的核心技术

### 交互式教程

7. **Distill.pub**
   - 链接: https://distill.pub/
   - 推荐: "Visualizing MNIST" 等文章
   - 内容: 可视化交互式教程

8. **Jupyter Notebooks**
   - 链接: https://github.com/jupyter/notebooks
   - 内容: 大量的机器学习教程

## 技术文档

### USearch 相关

1. **USearch API 文档**
   - 链接: https://unum-cloud.github.io/usearch/
   - 内容: 完整的 API 参考

2. **USearch Python 教程**
   - 链接: https://github.com/unum-cloud/usearch/tree/main/python
   - 内容: Python 绑定的使用说明

3. **C++ API 参考**
   - 链接: https://github.com/unum-cloud/usearch/blob/main/include/usearch/index.hpp
   - 内容: 源码级文档

### 相关库文档

4. **Faiss 文档**
   - 链接: https://faiss.ai/
   - 内容: Faiss 的使用指南和 API 文档

5. **Annoy 文档**
   - 链接: https://github.com/spotify/annoy
   - 内容: Annoy 的使用说明

6. **Hnswlib 文档**
   - 链接: https://github.com/nmslib/hnswlib
   - 内容: HNSW 的 C++ 实现

## 开发工具

### 性能分析

1. **perf**
   - 文档: https://perf.wiki.kernel.org/
   - 用途: Linux 性能分析

2. **Valgrind**
   - 文档: https://valgrind.org/docs/
   - 用途: 内存泄漏和性能分析

3. **VTune**
   - 文档: https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html
   - 用途: Intel 性能分析器

### 可视化

4. **Flame Graph**
   - 项目: https://github.com/brendangregg/FlameGraph
   - 用途: 性能火焰图生成

5. **matplotlib**
   - 文档: https://matplotlib.org/
   - 用途: Python 数据可视化

### IDE 和编辑器

6. **CLion**
   - 链接: https://www.jetbrains.com/clion/
   - 用途: C++ 开发

7. **VS Code**
   - 链接: https://code.visualstudio.com/
   - 推荐插件: C/C++, Python, Jupyter

## 社区资源

### 论坛和问答

1. **Stack Overflow**
   - 标签: hnsw, vector-search, approximate-nearest-neighbor

2. **Reddit**
   - r/MachineLearning
   - r/algorithms
   - r/Cpp

3. **Discord/Slack**
   - USearch Discord（如果有）
   - 机器学习社区

### 开源项目

1. **向量数据库**
   - Milvus: https://github.com/milvus-io/milvus
   - Qdrant: https://github.com/qdrant/qdrant
   - Weaviate: https://github.com/weaviate/weaviate

2. **嵌入模型**
   - Sentence-Transformers: https://www.sbert.net/
   - OpenAI Embeddings: https://platform.openai.com/docs/guides/embeddings

3. **相关工具**
   - UMAP: https://github.com/lmcinnes/umap
   - t-SNE: https://github.com/DmitryUlyanov/Multicore-TSNE

## 研究前沿

### 最新论文（2023-2024）

1. **DiskANN**
   - 标题: FreshDiskANN: A Fast and Accurate Graph-based ANN Index for Streaming Datasets
   - 链接: https://arxiv.org/abs/2307.01727

2. **学习索引**
   - 标题: Learned Index Structures for Similarity Search
   - 链接: https://arxiv.org/abs/2008.04130

3. **GPU 加速**
   - 标题: GPU-Accelerated Approximate Nearest Neighbor Search
   - 链接: https://arxiv.org/abs/2206.14520

### 会议推荐

1. **NeurIPS**
   - 链接: https://neurips.cc/
   - 关注: 机器学习前沿

2. **ICML**
   - 链接: https://icml.cc/
   - 关注: 机器学习理论

3. **SIGMOD/VLDB**
   - 链接: https://www.vldb.org/
   - 关注: 数据库技术

## 实践建议

### 阅读策略

1. **论文阅读顺序**：
   - 先读 HNSW 原论文
   - 再读 NSW 论文
   - 最后读相关变体

2. **代码阅读顺序**：
   - index.hpp（核心）
   - index_dense.hpp（密集向量）
   - 具体语言绑定

3. **实践结合**：
   - 读论文前先跑代码
   - 读论文时对照代码
   - 读论文后实现变体

### 深入学习路径

**初级**（1-2个月）
- 完成14天课程
- 实现简化版 HNSW
- 阅读核心论文

**中级**（3-6个月）
- 阅读所有推荐论文
- 深入研究源码
- 贡献代码

**高级**（6-12个月）
- 研究前沿论文
- 提出新算法
- 发表研究成果

---

**建议**: 根据自己的基础和兴趣，选择合适的材料开始学习。不要试图一次读完所有内容，循序渐进最重要！
