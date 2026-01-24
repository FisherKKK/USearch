# USearch 实战案例研究
## Real-World Case Studies

---

## 📚 目录

1. [案例 1：电商平台商品搜索](#案例-1电商平台商品搜索)
2. [案例 2：文档智能检索系统](#案例-2文档智能检索系统)
3. [案例 3：推荐系统实时召回](#案例-3推荐系统实时召回)
4. [案例 4：图像相似度搜索](#案例-4图像相似度搜索)
5. [案例 5：多模态搜索引擎](#案例-5多模态搜索引擎)

---

## 案例 1：电商平台商品搜索

### 业务背景

某大型电商平台拥有：
- 5000 万商品
- 每日 100 万次搜索
- 5000 维商品向量（CLIP embedding）
- 要求：< 50ms 延迟，95% 召回率

### 挑战

1. **规模问题**
   - 单机无法存储 5000 万向量
   - 需要分布式架构

2. **性能要求**
   - 严格的延迟要求
   - 高并发处理

3. **数据更新**
   - 每天新增 10 万商品
   - 需要增量更新

### 解决方案

#### 架构设计

```cpp
/**
 * 电商商品搜索引擎
 */

class EcommerceSearchEngine {
    struct ShardConfig {
        std::size_t shard_id;
        std::string address;
        std::size_t num_vectors;
    };

    std::vector<ShardConfig> shards_;
    std::unique_ptr<ShardingStrategy> strategy_;

public:
    EcommerceSearchEngine(std::vector<std::string> const& shard_addresses) {
        // 初始化分片配置
        for (std::size_t i = 0; i < shard_addresses.size(); ++i) {
            shards_.push_back({
                .shard_id = i,
                .address = shard_addresses[i],
                .num_vectors = 0
            });
        }

        // 使用一致性哈希分片
        strategy_ = std::make_unique<ConsistentHashSharding>(shard_addresses.size());
    }

    // 添加商品
    bool add_product(product_id_t pid, float const* embedding) {
        std::size_t shard_id = strategy_->shard_id(pid);
        return add_to_shard(shard_id, pid, embedding);
    }

    // 搜索相似商品
    std::vector<ProductResult> search_similar_products(
        float const* query_embedding,
        std::size_t k,
        SearchFilter const& filter
    ) {
        // 1. 确定要查询的分片（查询所有以获得最佳召回率）
        std::vector<std::future<std::vector<result_t>>> futures;

        for (auto const& shard : shards_) {
            futures.push_back(std::async(std::launch::async, [&]() {
                return search_shard(shard.address, query_embedding, k * 2);  // 多查询一些
            }));
        }

        // 2. 聚合结果
        std::vector<result_t> all_results;
        for (auto& future : futures) {
            auto results = future.get();
            all_results.insert(all_results.end(), results.begin(), results.end());
        }

        // 3. 应用过滤条件
        auto filtered = apply_filters(all_results, filter);

        // 4. 重排序（考虑业务规则）
        auto reranked = rerank_products(query_embedding, filtered);

        // 5. 返回 Top-K
        if (reranked.size() > k) {
            reranked.resize(k);
        }

        return reranked;
    }

private:
    // 重排序：结合向量相似度和业务规则
    std::vector<ProductResult> rerank_products(
        float const* query,
        std::vector<result_t> const& vector_results
    ) {
        std::vector<ProductResult> results;

        for (auto const& r : vector_results) {
            ProductResult pr;
            pr.product_id = r.key;
            pr.vector_distance = r.distance;

            // 获取商品元数据
            auto metadata = get_product_metadata(r.key);

            // 综合评分
            pr.final_score =
                0.7 * (1.0 - r.distance) +           // 向量相似度
                0.2 * metadata.popularity_score +    // 热度
                0.1 * metadata.rating;               // 评分

            results.push_back(pr);
        }

        // 按最终评分排序
        std::sort(results.begin(), results.end(),
                 [](auto const& a, auto const& b) {
                     return a.final_score > b.final_score;
                 });

        return results;
    }
};
```

#### 性能优化

**1. 查询缓存**

```cpp
class QueryCache {
    struct CacheEntry {
        std::vector<float> query_hash;  // 查询指纹
        std::vector<ProductResult> results;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<CacheEntry> cache_;
    std::size_t max_size_;

public:
    std::optional<std::vector<ProductResult>> get(float const* query, std::size_t dims) {
        auto hash = hash_query(query, dims);

        for (auto const& entry : cache_) {
            if (entry.query_hash == hash) {
                auto age = std::chrono::system_clock::now() - entry.timestamp;
                if (age < std::chrono::minutes(5)) {
                    return entry.results;
                }
            }
        }

        return std::nullopt;
    }

    void put(float const* query, std::size_t dims,
             std::vector<ProductResult> const& results) {
        // LRU 淘汰
        if (cache_.size() >= max_size_) {
            auto oldest = std::min_element(cache_.begin(), cache_.end(),
                                         [](auto const& a, auto const& b) {
                                             return a.timestamp < b.timestamp;
                                         });
            cache_.erase(oldest);
        }

        cache_.push_back({
            hash_query(query, dims),
            results,
            std::chrono::system_clock::now()
        });
    }
};
```

**2. 热点数据预热**

```cpp
// 预加载热门商品到缓存
void preload_hot_products() {
    auto hot_pids = get_hot_product_ids();  // 从数据库获取

    for (auto pid : hot_pids) {
        auto embedding = get_product_embedding(pid);
        search_shard(pid, embedding.data(), 10);  // 预热
    }
}
```

#### 实施效果

| 指标 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| P50 延迟 | 120 ms | 25 ms | 4.8x |
| P99 延迟 | 500 ms | 45 ms | 11x |
| QPS | 2000 | 15000 | 7.5x |
| 召回率 | 85% | 96% | +11% |

---

## 案例 2：文档智能检索系统

### 业务背景

企业级文档管理系统：
- 1000 万文档
- 多语言支持（中文、英文、日文）
- 语义搜索 + 关键词搜索
- 实时索引更新

### 技术方案

#### 混合检索架构

```cpp
/**
 * 混合检索：向量 + 关键词
 */

class HybridDocumentSearch {
    index_dense_gt<float, std::uint32_t> vector_index_;
    std::unordered_map<doc_id_t, InvertedIndex> keyword_index_;

public:
    // 添加文档
    void add_document(Document const& doc) {
        // 1. 生成向量嵌入
        auto embedding = embed_text(doc.content);

        // 2. 添加到向量索引
        vector_index_.add(doc.id, embedding.data());

        // 3. 添加到关键词索引
        auto keywords = extract_keywords(doc.content);
        for (auto kw : keywords) {
            keyword_index_[kw].add(doc.id);
        }
    }

    // 混合搜索
    std::vector<SearchResult> hybrid_search(
        std::string const& query_text,
        std::size_t k
    ) {
        // 1. 向量搜索
        auto query_embedding = embed_text(query_text);
        auto vector_results = vector_index_.search(query_embedding.data(), k * 2);

        // 2. 关键词搜索
        auto query_keywords = extract_keywords(query_text);
        std::set<doc_id_t> keyword_matches;
        for (auto kw : query_keywords) {
            auto it = keyword_index_.find(kw);
            if (it != keyword_index_.end()) {
                auto docs = it->second.search();
                keyword_matches.insert(docs.begin(), docs.end());
            }
        }

        // 3. 结果融合（Reciprocal Rank Fusion）
        std::map<doc_id_t, double> scores;

        // 向量得分
        for (std::size_t i = 0; i < vector_results.size(); ++i) {
            double score = 1.0 / (k + i + 1);
            scores[vector_results[i].key] += 0.6 * score;  // 权重 60%
        }

        // 关键词得分
        std::size_t rank = 0;
        for (auto doc_id : keyword_matches) {
            double score = 1.0 / (k + rank + 1);
            scores[doc_id] += 0.4 * score;  // 权重 40%
            rank++;
        }

        // 4. 排序并返回 Top-K
        std::vector<std::pair<doc_id_t, double>> sorted_scores(
            scores.begin(), scores.end()
        );
        std::partial_sort(sorted_scores.begin(), sorted_scores.begin() + k,
                         sorted_scores.end(),
                         [](auto const& a, auto const& b) {
                             return a.second > b.second;
                         });

        // 5. 构建结果（包含高亮片段）
        std::vector<SearchResult> results;
        for (std::size_t i = 0; i < std::min(k, sorted_scores.size()); ++i) {
            SearchResult sr;
            sr.doc_id = sorted_scores[i].first;
            sr.score = sorted_scores[i].second;

            // 获取文档并高亮
            auto doc = get_document(sr.doc_id);
            sr.highlight = highlight_keywords(doc.content, query_keywords);

            results.push_back(sr);
        }

        return results;
    }
};
```

#### 实时索引更新

```cpp
/**
 * 增量索引构建器
 */

class IncrementalIndexBuilder {
    index_dense_gt<float, std::uint32_t>& index_;
    std::queue<index_operation_t> operation_queue_;
    std::mutex queue_mutex_;
    std::thread builder_thread_;
    std::atomic<bool> running_{true};

public:
    IncrementalIndexBuilder(index_dense_gt<float, std::uint32_t>& index)
        : index_(index) {

        builder_thread_ = std::thread([this] { build_loop(); });
    }

    ~IncrementalIndexBuilder() {
        running_ = false;
        if (builder_thread_.joinable()) {
            builder_thread_.join();
        }
    }

    // 添加操作到队列
    void add_operation(index_operation_t const& op) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        operation_queue_.push(op);

        // 如果队列达到阈值，触发批量构建
        if (operation_queue_.size() >= 1000) {
            cv_.notify_one();
        }
    }

private:
    void build_loop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // 等待足够的数据或停止信号
            cv_.wait(lock, [this] {
                return operation_queue_.size() >= 1000 || !running_;
            });

            if (!running_ && operation_queue_.empty()) {
                break;
            }

            // 收集批次
            std::vector<doc_id_t> ids;
            std::vector<float> embeddings;

            while (!operation_queue_.empty() && ids.size() < 1000) {
                auto op = operation_queue_.front();
                operation_queue_.pop();

                ids.push_back(op.doc_id);
                embeddings.insert(embeddings.end(),
                               op.embedding.begin(),
                               op.embedding.end());
            }

            lock.unlock();

            // 批量添加到索引
            if (!ids.empty()) {
                index_.add(ids.data(), embeddings.data(), ids.size());

                // 持久化检查点
                if (need_checkpoint()) {
                    index_.save("index.usearch");
                }
            }
        }
    }

    std::condition_variable cv_;
};
```

---

## 案例 3：推荐系统实时召回

### 业务场景

视频推荐系统：
- 1 亿用户
- 5000 万视频
- 需要实时个性化推荐
- 延迟要求：< 20ms

### 解决方案

#### 用户-物品双塔架构

```cpp
/**
 * 推荐系统实时召回
 */

class RecommendationSystem {
    // 用户向量塔
    index_dense_gt<float, user_id_t> user_index_;

    // 物品向量塔
    index_dense_gt<float, item_id_t> item_index_;

public:
    // 实时推荐
    std::vector<item_id_t> recommend(user_id_t uid, std::size_t k) {
        // 1. 获取用户向量
        auto user_embedding = get_user_embedding(uid);

        // 2. 从物品库召回
        auto candidates = item_index_.search(user_embedding.data(), k * 10);

        // 3. 过滤（已看、不喜欢等）
        auto filtered = filter_candidates(uid, candidates);

        // 4. 排序（使用更精细的模型）
        auto ranked = rank_items(uid, user_embedding, filtered);

        // 5. 多样性处理
        auto diversified = diversify_results(ranked, k);

        return diversified;
    }

private:
    // 结果多样性
    std::vector<item_id_t> diversify_results(
        std::vector<item_id_t> const& items,
        std::size_t k
    ) {
        std::vector<item_id_t> result;
        std::set<std::string> used_categories;

        for (auto item_id : items) {
            if (result.size() >= k) break;

            auto category = get_item_category(item_id);

            // 限制每个类别的数量
            if (used_categories.count(category) < 3) {
                result.push_back(item_id);
                used_categories.insert(category);
            }
        }

        // 如果结果不足，补充其他项目
        while (result.size() < k && result.size() < items.size()) {
            result.push_back(items[result.size()]);
        }

        return result;
    }
};
```

#### 用户向量实时更新

```cpp
/**
 * 用户兴趣实时更新
 */

class UserEmbeddingUpdater {
    struct UserState {
        std::vector<float> embedding;
        std::deque<item_id_t> recent_items;  // 最近观看
        std::size_t update_count;
    };

    std::unordered_map<user_id_t, UserState> user_states_;
    std::mutex state_mutex_;

public:
    // 用户行为反馈
    void on_user_action(user_id_t uid, item_id_t iid, action_type_t action) {
        std::lock_guard<std::mutex> lock(state_mutex_);

        auto& state = user_states_[uid];

        // 更新最近行为
        state.recent_items.push_back(iid);
        if (state.recent_items.size() > 100) {
            state.recent_items.pop_front();
        }

        // 每 10 次行为更新一次用户向量
        state.update_count++;
        if (state.update_count >= 10) {
            update_user_embedding(uid);
            state.update_count = 0;
        }
    }

private:
    void update_user_embedding(user_id_t uid) {
        auto& state = user_states_[uid];

        // 获取最近观看的物品向量
        std::vector<float> item_embeddings;
        for (auto iid : state.recent_items) {
            auto item_emb = get_item_embedding(iid);
            item_embeddings.insert(item_embeddings.end(),
                                  item_emb.begin(), item_emb.end());
        }

        // 聚合（平均）
        std::size_t dim = item_embeddings.size() / state.recent_items.size();
        std::vector<float> aggregated(dim, 0.0f);

        for (std::size_t i = 0; i < state.recent_items.size(); ++i) {
            for (std::size_t d = 0; d < dim; ++d) {
                aggregated[d] += item_embeddings[i * dim + d];
            }
        }

        for (auto& v : aggregated) {
            v /= state.recent_items.size();
        }

        // 更新用户向量（加权平均，保留长期兴趣）
        float alpha = 0.3;  // 新兴趣权重
        for (std::size_t d = 0; d < dim; ++d) {
            state.embedding[d] = (1 - alpha) * state.embedding[d] +
                                alpha * aggregated[d];
        }

        // 归一化
        float norm = std::sqrt(std::inner_product(
            state.embedding.begin(), state.embedding.end(),
            state.embedding.begin(), 0.0f
        ));
        for (auto& v : state.embedding) {
            v /= norm;
        }
    }
};
```

---

## 案例 4：图像相似度搜索

### 应用场景

以图搜图系统：
- 1 亿图片
- 4096 维 ResNet 特征
- 支持 10 万 QPS
- P99 延迟 < 100ms

### 技术实现

#### 特征提取 + 向量搜索

```cpp
/**
 * 图像搜索系统
 */

class ImageSearchSystem {
    // 图像特征提取器
    CNNFeatureExtractor extractor_;

    // 向量索引
    index_dense_gt<float, image_id_t> index_;

public:
    // 添加图片
    bool add_image(image_id_t iid, std::string const& image_path) {
        // 1. 加载图片
        auto image = load_image(image_path);

        // 2. 提取特征
        auto feature = extractor_.extract(image);

        // 3. 添加到索引
        return index_.add(iid, feature.data());
    }

    // 以图搜图
    std::vector<SimilarImage> search_similar_images(
        std::string const& query_image_path,
        std::size_t k
    ) {
        // 1. 提取查询图片特征
        auto query_image = load_image(query_image_path);
        auto query_feature = extractor_.extract(query_image);

        // 2. 向量搜索
        auto results = index_.search(query_feature.data(), k);

        // 3. 构建结果
        std::vector<SimilarImage> similar_images;
        for (auto const& r : results) {
            SimilarImage si;
            si.image_id = r.key;
            si.similarity = 1.0 - r.distance;  // 转换为相似度
            si.url = get_image_url(r.key);
            si.metadata = get_image_metadata(r.key);
            similar_images.push_back(si);
        }

        return similar_images;
    }
};
```

#### 量化压缩

```cpp
/**
 * 产品量化（PQ）压缩
 */

class ProductQuantizer {
    std::size_t n_subvectors_;
    std::size_t n_subdims_;
    std::vector<codebook_t> codebooks_;

public:
    // 训练量化器
    void train(float const* vectors, std::size_t n, std::size_t dim) {
        n_subdims_ = dim / n_subvectors_;  // 每个子向量维度

        // 训练每个子空间
        for (std::size_t i = 0; i < n_subvectors_; ++i) {
            std::vector<float> subvectors(n * n_subdims_);

            // 提取子向量
            for (std::size_t j = 0; j < n; ++j) {
                std::copy(vectors + j * dim + i * n_subdims_,
                         vectors + j * dim + (i + 1) * n_subdims_,
                         subvectors.data() + j * n_subdims_);
            }

            // K-Means 聚类
            codebooks_[i] = kmeans(subvectors, n, n_subdims_, 256);  // 256 个中心
        }
    }

    // 编码向量
    std::vector<uint8_t> encode(float const* vector, std::size_t dim) {
        std::vector<uint8_t> codes(n_subvectors_);

        for (std::size_t i = 0; i < n_subvectors_; ++i) {
            // 找到最近的中心
            float min_dist = std::numeric_limits<float>::max();
            uint8_t best_code = 0;

            for (std::size_t k = 0; k < 256; ++k) {
                float dist = distance(
                    vector + i * n_subdims_,
                    codebooks_[i][k].data(),
                    n_subdims_
                );

                if (dist < min_dist) {
                    min_dist = dist;
                    best_code = k;
                }
            }

            codes[i] = best_code;
        }

        return codes;
    }

    // 压缩后的距离计算（近似）
    float asymmetric_distance(
        float const* query,
        std::vector<uint8_t> const& codes,
        std::size_t dim
    ) const {
        float total_dist = 0.0f;

        // 预计算查询到所有中心的距离
        std::vector<std::vector<float>> tables(n_subvectors_,
                                              std::vector<float>(256));

        for (std::size_t i = 0; i < n_subvectors_; ++i) {
            for (std::size_t k = 0; k < 256; ++k) {
                tables[i][k] = distance(
                    query + i * n_subdims_,
                    codebooks_[i][k].data(),
                    n_subdims_
                );
            }
        }

        // 表查找
        for (std::size_t i = 0; i < n_subvectors_; ++i) {
            total_dist += tables[i][codes[i]];
        }

        return total_dist;
    }
};
```

---

## 案例 5：多模态搜索引擎

### 业务需求

支持多种输入类型的搜索：
- 文本 → 图像
- 图像 → 文本
- 图像 → 图像
- 文本 → 文本

### 跨模态检索

```cpp
/**
 * 多模态搜索引擎
 */

class MultiModalSearchEngine {
    // 统一的特征空间（CLIP）
    index_dense_gt<float, content_id_t> unified_index_;

    // 模态到内容的映射
    std::unordered_map<content_id_t, ContentMetadata> metadata_;

public:
    // 添加内容
    void add_content(content_id_t cid, Content const& content) {
        // 1. 根据类型提取特征
        std::vector<float> embedding;
        switch (content.type) {
            case ContentType::Image:
                embedding = encode_image(content.data);
                break;
            case ContentType::Text:
                embedding = encode_text(content.data);
                break;
            case ContentType::Video:
                embedding = encode_video(content.data);
                break;
        }

        // 2. 添加到统一索引
        unified_index_.add(cid, embedding.data());

        // 3. 保存元数据
        metadata_[cid] = content.metadata;
    }

    // 跨模态搜索
    template <typename QueryType>
    std::vector<SearchResult> search(QueryType const& query, std::size_t k) {
        // 1. 编码查询到统一空间
        auto query_embedding = encode_query(query);

        // 2. 向量搜索
        auto results = unified_index_.search(query_embedding.data(), k);

        // 3. 构建结果
        std::vector<SearchResult> search_results;
        for (auto const& r : results) {
            SearchResult sr;
            sr.content_id = r.key;
            sr.similarity = 1.0 - r.distance;
            sr.metadata = metadata_[r.key];
            search_results.push_back(sr);
        }

        return search_results;
    }

private:
    // 统一的查询编码
    template <typename T>
    std::vector<float> encode_query(T const& query) {
        if constexpr (std::is_same_v<T, std::string>) {
            // 文本查询
            return encode_text(query);
        } else if constexpr (std::is_same_v<T, Image>) {
            // 图像查询
            return encode_image(query);
        }
        // 其他类型...
    }
};
```

---

## 总结

### 关键经验

1. **架构选择**
   - 小规模（< 1000万）：单机 + 多线程
   - 中等规模（1000万-1亿）：分布式集群
   - 大规模（> 1亿）：分层架构 + 缓存

2. **性能优化**
   - 批量操作是关键
   - 合理使用量化
   - 预热热点数据

3. **运维实践**
   - 完善的监控
   - 定期备份
   - 容量规划

4. **业务集成**
   - 向量搜索 + 业务规则
   - 重排序提升效果
   - 多样性提升体验

---

**版本**: v1.0
**最后更新**: 2025-01-24
