# 实战项目指南

## 项目概述

本指南提供三个不同难度的实战项目，帮助你将14天课程中学到的知识应用到实际场景中。

---

## 🥉 项目 1：基础向量搜索引擎（初级）

### 目标
实现一个简化版的向量搜索引擎，支持基本的添加和搜索功能。

### 功能要求

#### 必须实现
- [x] 支持添加向量（f32）
- [x] 支持搜索最近邻（暴力搜索）
- [x] 支持至少两种距离度量（L2、Cosine）
- [x] 提供命令行界面

#### 加分项
- [ ] 使用 HNSW 算法（而非暴力搜索）
- [ ] 支持从文件加载数据
- [ ] 提供性能基准测试
- [ ] 支持删除向量

### 技术栈
- C++17
- STL（标准库）
- 可选：USearch（参考实现）

### 实现步骤

#### 第1步：设计数据结构
```cpp
class SimpleVectorSearch {
    std::vector<std::vector<float>> vectors_;
    std::vector<std::size_t> keys_;

public:
    void add(std::size_t key, const std::vector<float>& vector);
    std::vector<std::pair<std::size_t, float>> search(
        const std::vector<float>& query,
        std::size_t k,
        const std::string& metric = "l2"
    );
};
```

#### 第2步：实现距离计算
```cpp
float l2_distance(const float* a, const float* b, std::size_t dims);
float cosine_distance(const float* a, const float* b, std::size_t dims);
```

#### 第3步：实现搜索
```cpp
std::vector<std::pair<std::size_t, float>> SimpleVectorSearch::search(
    const std::vector<float>& query,
    std::size_t k,
    const std::string& metric
) {
    std::vector<std::pair<std::size_t, float>> distances;

    // 计算所有距离
    for (std::size_t i = 0; i < vectors_.size(); ++i) {
        float dist;
        if (metric == "l2") {
            dist = l2_distance(query.data(), vectors_[i].data(), query.size());
        } else if (metric == "cosine") {
            dist = cosine_distance(query.data(), vectors_[i].data(), query.size());
        }
        distances.emplace_back(keys_[i], dist);
    }

    // 排序并返回 top-k
    std::partial_sort(distances.begin(), distances.begin() + k, distances.end(),
        [](auto& a, auto& b) { return a.second < b.second; });

    return std::vector<std::pair<std::size_t, float>>(
        distances.begin(), distances.begin() + k
    );
}
```

#### 第4步：命令行界面
```cpp
int main() {
    SimpleVectorSearch engine;

    // 示例：添加向量
    for (int i = 0; i < 1000; ++i) {
        std::vector<float> vec(128);
        // ... 填充随机数据 ...
        engine.add(i, vec);
    }

    // 搜索
    std::vector<float> query(128);
    // ... 填充查询 ...

    auto results = engine.search(query, 10, "l2");

    for (auto [key, dist] : results) {
        std::cout << key << ": " << dist << "\n";
    }

    return 0;
}
```

### 验收标准
- [ ] 能正确添加和搜索 1000 个 128 维向量
- [ ] 搜索结果准确（与暴力搜索对比）
- [ ] 提供 Makefile 或 CMakeLists.txt
- [ ] 包含 README 和使用说明

---

## 🥈 项目 2：图像相似度搜索系统（中级）

### 目标
构建一个完整的图像搜索系统，支持上传图片并返回相似图片。

### 功能要求

#### 必须实现
- [x] 图像特征提取（使用预训练模型）
- [x] 向量索引构建（使用 USearch）
- [x] 相似图片搜索
- [x] Web 界面（Flask/FastAPI）

#### 加分项
- [ ] 支持多种特征提取方法
- [ ] 图像预处理和增强
- [ ] 结果可视化
- [ ] 性能优化（缓存、批处理）

### 技术栈
- Python 3.8+
- PyTorch/TensorFlow（特征提取）
- USearch（向量搜索）
- Flask/FastAPI（Web框架）
- HTML/CSS/JavaScript（前端）

### 实现步骤

#### 第1步：特征提取
```python
import torch
import torchvision.models as models
from PIL import Image
from torchvision import transforms

class FeatureExtractor:
    def __init__(self):
        # 使用预训练的 ResNet50
        self.model = models.resnet50(pretrained=True)
        self.model.eval()

        # 移除最后的分类层
        self.feature_extractor = torch.nn.Sequential(*list(self.model.children())[:-1])

        self.preprocess = transforms.Compose([
            transforms.Resize(256),
            transforms.CenterCrop(224),
            transforms.ToTensor(),
            transforms.Normalize(
                mean=[0.485, 0.456, 0.406],
                std=[0.229, 0.224, 0.225]
            )
        ])

    def extract(self, image_path):
        image = Image.open(image_path).convert('RGB')
        input_tensor = self.preprocess(image)
        input_batch = input_tensor.unsqueeze(0)

        with torch.no_grad():
            features = self.feature_extractor(input_batch)

        return features.squeeze().numpy()
```

#### 第2步：索引构建
```python
import usearch
import numpy as np
import glob

class ImageIndex:
    def __init__(self, dimension=2048):
        self.index = usearch.Index(
            ndim=dimension,
            metric='cos',
            dtype='f32'
        )
        self.extractor = FeatureExtractor()
        self.image_paths = []

    def build(self, image_dir):
        """从目录构建索引"""
        image_paths = glob.glob(f"{image_dir}/*.jpg")[:10000]

        print(f"索引 {len(image_paths)} 张图片...")

        batch_size = 32
        for i in range(0, len(image_paths), batch_size):
            batch_paths = image_paths[i:i+batch_size]
            features = []

            for path in batch_paths:
                feat = self.extractor.extract(path)
                features.append(feat)

            features_array = np.vstack(features).astype(np.float32)
            keys = np.arange(i, i + len(batch_paths), dtype=np.uint64)

            self.index.add(keys, features_array)
            self.image_paths.extend(batch_paths)

            if (i // batch_size + 1) % 10 == 0:
                print(f"  已处理 {i + len(batch_paths)} 张")

        self.index.save("image_index.usearch")
        print("索引完成")

    def search(self, query_image_path, k=10):
        """搜索相似图片"""
        query_feat = self.extractor.extract(query_image_path)
        results = self.index.search(query_feat.astype(np.float32), k)

        similar_images = []
        for key, distance in results:
            similar_images.append({
                'path': self.image_paths[key],
                'score': 1 - distance  # 转换为相似度
            })

        return similar_images
```

#### 第3步：Web API
```python
from flask import Flask, request, jsonify, send_file
import os

app = Flask(__name__)
index = None

@app.route('/build', methods=['POST'])
def build_index():
    """构建索引"""
    data = request.json
    image_dir = data['image_dir']

    global index
    index = ImageIndex()
    index.build(image_dir)

    return jsonify({'status': 'ok', 'count': len(index.image_paths)})

@app.route('/search', methods=['POST'])
def search():
    """搜索相似图片"""
    if 'image' not in request.files:
        return jsonify({'error': 'No image uploaded'}), 400

    file = request.files['image']
    k = int(request.form.get('k', 10))

    # 保存临时文件
    temp_path = f'/tmp/{file.filename}'
    file.save(temp_path)

    # 搜索
    results = index.search(temp_path, k)

    # 清理
    os.remove(temp_path)

    return jsonify({'results': results})

@app.route('/image/<path:filename>')
def get_image(filename):
    """返回图片"""
    return send_file(filename)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
```

#### 第4步：前端界面
```html
<!DOCTYPE html>
<html>
<head>
    <title>图像搜索</title>
    <style>
        .gallery {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
            gap: 16px;
            margin-top: 20px;
        }
        .image-card {
            border: 1px solid #ddd;
            padding: 8px;
            border-radius: 8px;
        }
        .image-card img {
            width: 100%;
            height: 200px;
            object-fit: cover;
        }
        .score {
            font-weight: bold;
            color: #007bff;
        }
    </style>
</head>
<body>
    <h1>图像相似度搜索</h1>

    <div>
        <input type="file" id="queryImage" accept="image/*">
        <button onclick="search()">搜索</button>
        <input type="number" id="k" value="10" min="1" max="100">
    </div>

    <div class="gallery" id="results"></div>

    <script>
        async function search() {
            const fileInput = document.getElementById('queryImage');
            const k = document.getElementById('k').value;

            if (!fileInput.files[0]) {
                alert('请选择图片');
                return;
            }

            const formData = new FormData();
            formData.append('image', fileInput.files[0]);
            formData.append('k', k);

            const response = await fetch('/search', {
                method: 'POST',
                body: formData
            });

            const data = await response.json();
            displayResults(data.results);
        }

        function displayResults(results) {
            const gallery = document.getElementById('results');
            gallery.innerHTML = '';

            results.forEach(result => {
                const card = document.createElement('div');
                card.className = 'image-card';
                card.innerHTML = `
                    <img src="/image/${result.path}" alt="result">
                    <div>相似度: <span class="score">${result.score.toFixed(3)}</span></div>
                `;
                gallery.appendChild(card);
            });
        }
    </script>
</body>
</html>
```

### 验收标准
- [ ] 能索引至少 1000 张图片
- [ ] 搜索响应时间 < 100ms
- [ ] Web 界面友好易用
- [ ] 包含完整的部署文档

---

## 🥇 项目 3：RAG 知识问答系统（高级）

### 目标
构建一个基于 RAG 的知识问答系统，结合向量检索和 LLM 生成。

### 功能要求

#### 必须实现
- [x] 文档切片和向量化
- [x] 向量索引和检索
- [x] LLM 集成（生成答案）
- [x] API 接口
- [x] 性能优化

#### 加分项
- [ ] 支持多种文档格式（PDF、DOCX、Markdown）
- [ ] 多轮对话
- [ ] 引用来源标注
- [ ] 评估指标（准确率、召回率）

### 技术栈
- Python 3.8+
- LangChain/LlamaIndex
- USearch 或其他向量数据库
- OpenAI API / 本地 LLM
- FastAPI

### 实现步骤

#### 第1步：文档处理
```python
from langchain.text_splitter import RecursiveCharacterTextSplitter
from sentence_transformers import SentenceTransformer

class DocumentProcessor:
    def __init__(self):
        self.text_splitter = RecursiveCharacterTextSplitter(
            chunk_size=500,
            chunk_overlap=50,
            length=len
        )
        self.encoder = SentenceTransformer('all-MiniLM-L6-v2')

    def process(self, documents):
        """处理文档并返回chunks"""
        all_chunks = []

        for doc in documents:
            chunks = self.text_splitter.split_text(doc['text'])
            for i, chunk in enumerate(chunks):
                all_chunks.append({
                    'text': chunk,
                    'source': doc['source'],
                    'chunk_id': i
                })

        return all_chunks

    def encode(self, chunks):
        """编码chunks"""
        texts = [chunk['text'] for chunk in chunks]
        embeddings = self.encoder.encode(texts)
        return embeddings.astype(np.float32)
```

#### 第2步：向量存储
```python
import usearch
import numpy as np
import pickle

class VectorStore:
    def __init__(self, dimension=384):
        self.index = usearch.Index(
            ndim=dimension,
            metric='cos',
            dtype='f32',
            connectivity=16,
            expansion=64
        )
        self.chunks = []

    def add_documents(self, chunks, embeddings):
        """添加文档chunks"""
        keys = np.arange(len(self.chunks), len(self.chunks) + len(chunks), dtype=np.uint64)

        self.index.add(keys, embeddings)
        self.chunks.extend(chunks)

    def save(self, path):
        """保存索引和chunks"""
        self.index.save(f"{path}.usearch")
        with open(f"{path}.chunks", 'wb') as f:
            pickle.dump(self.chunks, f)

    def load(self, path):
        """加载索引和chunks"""
        self.index.load(f"{path}.usearch")
        with open(f"{path}.chunks", 'rb') as f:
            self.chunks = pickle.load(f)

    def search(self, query_embedding, k=5):
        """搜索相关文档"""
        results = self.index.search(query_embedding.astype(np.float32), k)

        retrieved_chunks = []
        for key, distance in results:
            retrieved_chunks.append({
                'chunk': self.chunks[key],
                'score': 1 - distance
            })

        return retrieved_chunks
```

#### 第3步：RAG 系统
```python
import openai
from typing import List, Dict

class RAGSystem:
    def __init__(self, api_key=None):
        self.processor = DocumentProcessor()
        self.vector_store = VectorStore()

        if api_key:
            openai.api_key = api_key

    def index_documents(self, documents: List[Dict]):
        """索引文档"""
        print("处理文档...")
        chunks = self.processor.process(documents)

        print("编码chunks...")
        embeddings = self.processor.encode(chunks)

        print("构建索引...")
        self.vector_store.add_documents(chunks, embeddings)
        self.vector_store.save("knowledge_base")

        print(f"索引完成，共 {len(chunks)} 个chunks")

    def query(self, question: str, k: int = 3) -> Dict:
        """查询知识库"""
        # 编码问题
        question_embedding = self.processor.encoder.encode([question])[0]

        # 检索相关文档
        retrieved_chunks = self.vector_store.search(question_embedding, k)

        # 构建上下文
        context = "\n\n".join([
            f"【来源: {chunk['chunk']['source']}】\n{chunk['chunk']['text']}"
            for chunk in retrieved_chunks
        ])

        # 生成答案
        prompt = f"""基于以下上下文回答问题。如果上下文中没有相关信息，请说"我不知道"。

上下文：
{context}

问题：{question}

答案："""

        response = openai.ChatCompletion.create(
            model="gpt-3.5-turbo",
            messages=[
                {"role": "system", "content": "你是一个有帮助的助手。"},
                {"role": "user", "content": prompt}
            ],
            temperature=0.7
        )

        answer = response.choices[0].message.content

        return {
            'question': question,
            'answer': answer,
            'sources': [
                {
                    'text': chunk['chunk']['text'][:200] + "...",
                    'source': chunk['chunk']['source'],
                    'score': chunk['score']
                }
                for chunk in retrieved_chunks
            ]
        }
```

#### 第4步：FastAPI 接口
```python
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import List

app = FastAPI(title="RAG Knowledge System")
rag = RAGSystem()

class Document(BaseModel):
    text: str
    source: str

class QueryRequest(BaseModel):
    question: str
    k: int = 3

@app.post("/index")
def index_documents(documents: List[Document]):
    """索引文档"""
    try:
        docs = [doc.dict() for doc in documents]
        rag.index_documents(docs)
        return {"status": "ok", "indexed": len(docs)}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/query")
def query(request: QueryRequest):
    """查询知识库"""
    try:
        result = rag.query(request.question, request.k)
        return result
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/health")
def health():
    """健康检查"""
    return {"status": "ok"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
```

### 验收标准
- [ ] 能索引至少 1000 个文档chunks
- [ ] 端到端延迟 < 3 秒
- [ ] 提供清晰的API文档
- [ ] 包含评估指标

---

## 项目评估标准

### 代码质量 (30分)
- 代码结构和组织
- 命名规范
- 注释和文档
- 错误处理

### 功能完整性 (40分)
- 必须功能全部实现
- 加分项实现情况
- 功能的正确性

### 性能 (20分)
- 响应时间
- 内存使用
- 可扩展性

### 文档和演示 (10分)
- README 完整性
- 使用说明
- 演示效果

## 提交要求

1. **源代码**：GitHub 仓库链接
2. **文档**：README.md，包括安装和使用说明
3. **演示**：5-10 分钟演示视频或截图
4. **报告**：简短的技术报告（2-3页）

## 常见问题

### Q: 可以使用其他语言吗？
**A**: 推荐使用 C++ 或 Python，但也可以使用其他语言。

### Q: 可以使用现成的向量数据库吗？
**A**: 项目1需要自己实现，项目2和3可以使用 USearch。

### Q: 如何测试性能？
**A**: 可以使用课程中的性能测试代码，或自行编写基准测试。

### Q: 需要完整的部署吗？
**A**: 不需要，本地运行即可，但提供部署说明会加分。

## 资源

- **USearch GitHub**: https://github.com/unum-cloud/usearch
- **示例代码**: `examples/` 目录
- **数据集**:
  - SIFT-1M: https://corpus-texmex.irisa.fr/
  - ImageNet: https://www.image-net.org/
  - Wikipedia: https://dumps.wikimedia.org/

祝你项目顺利！如有问题，欢迎在课程仓库提 Issue。
