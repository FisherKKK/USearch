# USearch 课程示例 Makefile
# 用于编译和运行所有课程示例

CXX = g++
CXXFLAGS = -std=c++17 -O3 -march=native -Wall -Wextra
INCLUDES = -I../../../include
LDFLAGS = -lpthread -lm

# 目录
EXAMPLES_DIR = examples
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

# 源文件
EXAMPLES_SRCS = $(wildcard $(EXAMPLES_DIR)/*.cpp)
EXAMPLES_BINS = $(patsubst $(EXAMPLES_DIR)/%.cpp,$(BIN_DIR)/%,$(EXAMPLES_SRCS))

.PHONY: all clean test benchmark help

all: $(BIN_DIR) $(EXAMPLES_BINS)
	@echo "=========================================="
	@echo "所有示例编译完成！"
	@echo "二进制文件位于: $(BIN_DIR)"
	@echo "=========================================="

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.cpp
	@echo "编译 $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

# 完整示例
complete_examples: $(EXAMPLES_DIR)/complete_examples.cpp
	@echo "编译完整示例..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $(BIN_DIR)/$@ $(LDFLAGS)

# 运行所有示例
run: all
	@echo "运行所有示例..."
	$(BIN_DIR)/complete_examples

# 性能基准测试
benchmark: all
	@echo "运行性能基准测试..."
	$(BIN_DIR)/benchmark_example

# 测试
test: all
	@echo "运行测试..."
	$(BIN_DIR)/test_example

# 清理
clean:
	rm -rf $(BUILD_DIR)
	@echo "清理完成"

# 帮助
help:
	@echo "可用目标:"
	@echo "  all              - 编译所有示例"
	@echo "  complete_examples - 编译完整示例"
	@echo "  run              - 运行所有示例"
	@echo "  benchmark        - 运行性能基准测试"
	@echo "  test             - 运行测试"
	@echo "  clean            - 清理构建文件"
	@echo "  help             - 显示此帮助信息"
