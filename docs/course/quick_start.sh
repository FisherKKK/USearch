#!/bin/bash
#
# USearch 课程快速启动脚本
# 用于快速设置学习环境并启动相关工具
#

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示标题
show_banner() {
    echo -e "${BLUE}"
    echo "======================================"
    echo "  USearch 课程环境快速启动"
    echo "======================================"
    echo -e "${NC}"
}

# 检查依赖
check_dependencies() {
    print_info "检查依赖..."

    # 检查 GCC
    if command -v gcc &> /dev/null; then
        GCC_VERSION=$(gcc --version | head -n1)
        print_success "GCC: $GCC_VERSION"
    else
        print_error "GCC 未安装"
        print_info "安装命令: sudo apt-get install build-essential"
        exit 1
    fi

    # 检查 CMake
    if command -v cmake &> /dev/null; then
        CMAKE_VERSION=$(cmake --version | head -n1)
        print_success "CMake: $CMAKE_VERSION"
    else
        print_error "CMake 未安装"
        print_info "安装命令: sudo apt-get install cmake"
        exit 1
    fi

    # 检查 Python
    if command -v python3 &> /dev/null; then
        PYTHON_VERSION=$(python3 --version)
        print_success "Python: $PYTHON_VERSION"
    else
        print_warning "Python3 未安装（可选）"
    fi

    # 检查 Docker
    if command -v docker &> /dev/null; then
        DOCKER_VERSION=$(docker --version)
        print_success "Docker: $DOCKER_VERSION"
    else
        print_warning "Docker 未安装（可选）"
    fi
}

# 设置 USearch
setup_usearch() {
    print_info "设置 USearch..."

    if [ -d "usearch" ]; then
        print_warning "usearch 目录已存在，跳过克隆"
    else
        print_info "克隆 USearch 仓库..."
        git clone https://github.com/unum-cloud/usearch.git
        print_success "克隆完成"
    fi

    cd usearch

    # 初始化子模块
    print_info "初始化子模块..."
    git submodule update --init --recursive
    print_success "子模块初始化完成"

    # 创建构建目录
    mkdir -p build
    cd build

    # 配置
    print_info "配置 CMake..."
    cmake -D CMAKE_BUILD_TYPE=Release \
          -D USEARCH_USE_OPENMP=1 \
          -D USEARCH_USE_SIMSIMD=1 \
          -D USEARCH_USE_JEMALLOC=1 \
          -D USEARCH_BUILD_TEST_CPP=1 \
          -D USEARCH_BUILD_BENCH_CPP=1 \
          ..
    print_success "配置完成"

    # 编译
    print_info "编译 USearch（这可能需要几分钟）..."
    make -j$(nproc)
    print_success "编译完成"

    # 运行测试
    print_info "运行测试..."
    if ./test_cpp; then
        print_success "所有测试通过！"
    else
        print_error "测试失败"
        exit 1
    fi

    cd ../..
}

# 设置 Python 环境
setup_python() {
    print_info "设置 Python 环境..."

    if ! command -v python3 &> /dev/null; then
        print_warning "Python3 未安装，跳过"
        return
    fi

    # 安装依赖
    print_info "安装 Python 包..."
    pip3 install --user numpy usearch matplotlib seaborn jupyter 2>/dev/null || {
        print_warning "pip 安装失败，尝试使用虚拟环境..."
        python3 -m venv .venv
        source .venv/bin/activate
        pip install numpy usearch matplotlib seaborn jupyter
    }

    print_success "Python 环境设置完成"
}

# 运行示例
run_examples() {
    print_info "编译并运行示例..."

    if [ ! -d "usearch/build" ]; then
        print_error "USearch 未构建，请先运行: $0 setup"
        exit 1
    fi

    cd examples

    # 编译示例
    print_info "编译示例..."
    g++ -std=c++17 -O3 -march=native \
        -I../../usearch/include \
        complete_examples.cpp \
        -o complete_examples \
        -lpthread -lm

    # 运行示例
    print_info "运行示例..."
    ./complete_examples

    cd ..
}

# 启动 Jupyter Notebook
start_jupyter() {
    print_info "启动 Jupyter Notebook..."

    if ! command -v jupyter &> /dev/null; then
        print_error "Jupyter 未安装"
        print_info "安装命令: pip3 install jupyter"
        exit 1
    fi

    jupyter notebook --no-browser --ip=0.0.0.0 --port=8888
}

# 使用 Docker
use_docker() {
    print_info "使用 Docker 启动环境..."

    if ! command -v docker &> /dev/null; then
        print_error "Docker 未安装"
        print_info "安装命令: curl -fsSL https://get.docker.com | sh"
        exit 1
    fi

    # 构建镜像
    print_info "构建 Docker 镜像..."
    docker-compose build

    # 启动容器
    print_info "启动容器..."
    docker-compose run --rm usearch-course /bin/bash
}

# 性能基准测试
run_benchmark() {
    print_info "运行性能基准测试..."

    if [ ! -f "usearch/build/bench_cpp" ]; then
        print_error "基准测试程序未找到"
        exit 1
    fi

    print_info "运行基准测试（这将需要几分钟）..."
    usearch/build/bench_cpp

    print_success "基准测试完成"
}

# 清理
clean() {
    print_info "清理构建文件..."

    if [ -d "usearch/build" ]; then
        rm -rf usearch/build
        print_success "清理完成"
    else
        print_warning "没有需要清理的文件"
    fi
}

# 显示帮助
show_help() {
    cat << EOF
USearch 课程快速启动脚本

用法: $0 [命令]

命令:
  setup       - 设置 USearch 和依赖
  python      - 设置 Python 环境
  examples    - 编译并运行示例
  jupyter     - 启动 Jupyter Notebook
  docker      - 使用 Docker 环境
  benchmark   - 运行性能基准测试
  clean       - 清理构建文件
  check       - 检查依赖
  help        - 显示此帮助信息

示例:
  $0 setup          # 设置环境
  $0 examples       # 运行示例
  $0 jupyter        # 启动 Jupyter
  $0 docker         # 使用 Docker

完整学习流程:
  1. $0 setup           # 设置环境
  2. $0 python          # 设置 Python（可选）
  3. $0 examples        # 运行示例
  4. $0 jupyter         # 启动 Jupyter（可选）

更多信息请参考: README.md
EOF
}

# 主函数
main() {
    show_banner

    case "${1:-help}" in
        setup)
            check_dependencies
            setup_usearch
            setup_python
            print_success "环境设置完成！"
            print_info "下一步: $0 examples"
            ;;
        python)
            setup_python
            ;;
        examples)
            run_examples
            ;;
        jupyter)
            start_jupyter
            ;;
        docker)
            use_docker
            ;;
        benchmark)
            run_benchmark
            ;;
        clean)
            clean
            ;;
        check)
            check_dependencies
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "未知命令: $1"
            show_help
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"
