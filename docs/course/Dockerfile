# USearch 课程环境 Dockerfile
# 用于快速搭建统一的学习和开发环境

FROM ubuntu:22.04

# 设置非交互模式
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 更新并安装基础依赖
RUN apt-get update && apt-get install -y \
    # 构建工具
    build-essential \
    cmake \
    git \
    wget \
    curl \
    vim \
    # Python 相关
    python3 \
    python3-pip \
    python3-dev \
    # 性能分析工具
    valgrind \
    perf \
    # 文档工具
    doxygen \
    graphviz \
    # 可视化工具
    python3-matplotlib \
    python3-seaborn \
    # 其他工具
    htop \
    tmux \
    && rm -rf /var/lib/apt/lists/*

# 安装新版 GCC/G++
RUN apt-get update && apt-get install -y \
    software-properties-common \
    && add-apt-repository ppa:ubuntu-toolchain-r/test \
    && apt-get update && apt-get install -y \
    gcc-12 \
    g++-12 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100

# 安装 jemalloc
RUN apt-get update && apt-get install -y \
    libjemalloc-dev \
    && rm -rf /var/lib/apt/lists/*

# 安装 Python 包
RUN pip3 install --upgrade pip && \
    pip3 install --no-cache-dir \
    numpy \
    usearch \
    jupyter \
    notebook \
    matplotlib \
    seaborn \
    scipy \
    scikit-learn \
    torch \
    torchvision \
    transformers \
    flask \
    fastapi \
    uvicorn \
    opencv-python \
    pillow

# 设置工作目录
WORKDIR /workspace

# 克隆 USearch 仓库
RUN cd /opt && \
    git clone https://github.com/unum-cloud/usearch.git && \
    cd usearch && \
    git submodule update --init --recursive

# 创建构建脚本
RUN echo '#!/bin/bash\n\
cd /opt/usearch && \
mkdir -p build && \
cd build && \
cmake -D CMAKE_BUILD_TYPE=Release \
      -D USEARCH_USE_OPENMP=1 \
      -D USEARCH_USE_SIMSIMD=1 \
      -D USEARCH_USE_JEMALLOC=1 \
      -D USEARCH_BUILD_TEST_CPP=1 \
      -D USEARCH_BUILD_BENCH_CPP=1 \
      .. && \
make -j$(nproc)\n\
' > /usr/local/bin/build-usearch && \
chmod +x /usr/local/bin/build-usearch

# 运行测试脚本
RUN echo '#!/bin/bash\n\
cd /opt/usearch/build && \
./test_cpp\n\
' > /usr/local/bin/test-usearch && \
chmod +x /usr/local/bin/test-usearch

# 运行基准测试脚本
RUN echo '#!/bin/bash\n\
cd /opt/usearch/build && \
./bench_cpp\n\
' > /usr/local/bin/benchmark-usearch && \
chmod +x /usr/local/bin/benchmark-usearch

# 构建 USearch
RUN build-usearch

# 设置环境变量
ENV USEARCH_ROOT=/opt/usearch
ENV USEARCH_BUILD=/opt/usearch/build
ENV PYTHONPATH=/workspace:$PYTHONPATH
ENV LD_LIBRARY_PATH=/opt/usearch/build:$LD_LIBRARY_PATH

# 复制课程文件（如果有的话）
COPY . /workspace/course/

# 创建 Jupyter 配置
RUN mkdir -p /root/.jupyter && \
    echo "c.NotebookApp.ip = '0.0.0.0'" > /root/.jupyter/jupyter_notebook_config.py && \
    echo "c.NotebookApp.notebook_dir = '/workspace'" >> /root/.jupyter/jupyter_notebook_config.py && \
    echo "c.NotebookApp.allow_root = True" >> /root/.jupyter/jupyter_notebook_config.py

# 暴露 Jupyter 端口
EXPOSE 8888

# 默认命令
CMD ["bash"]
