# 🐍 Python Binding完全教程 - C++到Python的桥梁

> 基于USearch项目的PyBind11实战指南
>
> 从零开始学习如何将C++库封装成Python模块

---

## 目录

1. [Python Binding基础](#1-python-binding基础)
2. [环境搭建与工具链](#2-环境搭建与工具链)
3. [PyBind11入门](#3-pybind11入门)
4. [类型转换系统](#4-类型转换系统)
5. [类与对象绑定](#5-类与对象绑定)
6. [NumPy集成](#6-numpy集成)
7. [内存管理与生命周期](#7-内存管理与生命周期)
8. [多线程与GIL](#8-多线程与gil)
9. [错误处理与异常](#9-错误处理与异常)
10. [性能优化技巧](#10-性能优化技巧)
11. [项目实战](#11-项目实战)
12. [调试与测试](#12-调试与测试)

---

## 1. Python Binding基础

### 1.1 什么是Python Binding?

**定义:** Python Binding是将C/C++库暴露给Python的技术，让Python代码能够调用C/C++函数。

**为什么需要Binding?**

```
Python优势:          C++优势:
├─ 易用性高          ├─ 执行速度快
├─ 开发迅速          ├─ 内存控制精确
├─ 生态丰富          ├─ 底层控制能力
└─ 原型快速          └─ 高性能计算

        ↓
   Python Binding
        ↓
  结合两者优势!
```

**常见方案对比:**

| 方案 | 难度 | 性能 | 类型安全 | 推荐度 |
|------|------|------|---------|-------|
| **PyBind11** | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ✅ 强烈推荐 |
| ctypes | ⭐ | ⭐⭐⭐⭐ | ⭐⭐ | ⚠️ 简单场景 |
| SWIG | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⚠️ 多语言 |
| Cython | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⚠️ Python优化 |
| Boost.Python | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ❌ 已过时 |

### 1.2 PyBind11优势

**USearch为什么选择PyBind11?**

```cpp
// python/lib.cpp:21-24
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
```

**优势:**
1. ✅ **Header-Only** - 无需编译依赖库
2. ✅ **现代C++** - 支持C++11/14/17/20
3. ✅ **自动类型转换** - STL容器、NumPy数组
4. ✅ **轻量级** - 编译后体积小
5. ✅ **文档完善** - 官方文档清晰

---

## 2. 环境搭建与工具链

### 2.1 安装依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    python3-dev \
    python3-pip \
    cmake \
    build-essential

# 安装PyBind11
pip install pybind11

# 或通过git submodule (推荐)
git submodule add https://github.com/pybind/pybind11.git extern/pybind11
git submodule update --init --recursive
```

### 2.2 项目结构

**USearch的Python绑定结构:**

```
python/
├── lib.cpp                 # C++绑定代码 (核心)
├── setup.py                # 构建配置 (已废弃,使用pyproject.toml)
├── pyproject.toml          # 现代构建配置
├── usearch/
│   ├── __init__.py        # 包入口
│   ├── index.py           # Python包装层
│   └── compiled.pyi       # 类型存根 (Type stub)
└── scripts/
    └── test_index.py      # 单元测试
```

### 2.3 最小构建配置

**pyproject.toml示例:**

```toml
[build-system]
requires = ["setuptools>=42", "wheel", "pybind11>=2.10.0"]
build-backend = "setuptools.build_meta"

[project]
name = "usearch"
version = "2.23.0"
description = "Smaller & Faster Single-File Vector Search Engine"
requires-python = ">=3.7"

[tool.setuptools.packages.find]
where = ["."]
include = ["usearch*"]
```

**CMakeLists.txt (可选,用于开发):**

```cmake
cmake_minimum_required(VERSION 3.14)
project(usearch_python)

# 查找Python和PyBind11
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# 创建Python模块
pybind11_add_module(compiled python/lib.cpp)

# 链接头文件
target_include_directories(compiled PRIVATE include/)

# 设置输出目录
set_target_properties(compiled PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python/usearch
)
```

---

## 3. PyBind11入门

### 3.1 Hello World

**最简单的Python扩展:**

```cpp
// hello.cpp
#include <pybind11/pybind11.h>

namespace py = pybind11;

// 普通函数
int add(int a, int b) {
    return a + b;
}

// 模块定义
PYBIND11_MODULE(example, m) {
    m.doc() = "Hello World module";  // 模块文档

    // 绑定函数
    m.def("add", &add,
          "Add two numbers",
          py::arg("a"), py::arg("b"));
}
```

**编译:**

```bash
# 方法1: 使用c++直接编译
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    hello.cpp -o example$(python3-config --extension-suffix)

# 方法2: 使用setup.py
# setup.py
from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup

ext_modules = [
    Pybind11Extension("example", ["hello.cpp"]),
]

setup(
    name="example",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)

python setup.py build_ext --inplace
```

**Python使用:**

```python
import example

result = example.add(1, 2)
print(result)  # 输出: 3

# 支持关键字参数
result = example.add(a=10, b=20)
print(result)  # 输出: 30

# 查看文档
help(example.add)
# add(a: int, b: int) -> int
#     Add two numbers
```

### 3.2 函数重载

**C++函数重载:**

```cpp
#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

// 重载函数
int add(int a, int b) {
    return a + b;
}

double add(double a, double b) {
    return a + b;
}

std::string add(const std::string& a, const std::string& b) {
    return a + b;
}

PYBIND11_MODULE(overload_example, m) {
    // 需要显式指定函数签名
    m.def("add", static_cast<int(*)(int, int)>(&add), "Add integers");
    m.def("add", static_cast<double(*)(double, double)>(&add), "Add doubles");
    m.def("add", static_cast<std::string(*)(const std::string&, const std::string&)>(&add),
          "Concatenate strings");
}
```

**Python调用:**

```python
import overload_example

print(overload_example.add(1, 2))          # 3 (int)
print(overload_example.add(1.5, 2.5))      # 4.0 (double)
print(overload_example.add("Hello ", "World"))  # "Hello World" (str)
```

### 3.3 默认参数

```cpp
#include <pybind11/pybind11.h>

namespace py = pybind11;

int power(int base, int exponent = 2) {
    int result = 1;
    for (int i = 0; i < exponent; i++)
        result *= base;
    return result;
}

PYBIND11_MODULE(default_args, m) {
    m.def("power", &power,
          "Raise base to exponent",
          py::arg("base"),
          py::arg("exponent") = 2);  // 默认参数
}
```

**Python使用:**

```python
import default_args

print(default_args.power(3))      # 9 (3^2)
print(default_args.power(3, 3))   # 27 (3^3)
print(default_args.power(base=2, exponent=10))  # 1024
```

---

## 4. 类型转换系统

### 4.1 自动类型转换

**PyBind11支持的自动转换:**

| C++类型 | Python类型 | 示例 |
|---------|-----------|------|
| `int, long` | `int` | 42 |
| `float, double` | `float` | 3.14 |
| `bool` | `bool` | True |
| `std::string` | `str` | "hello" |
| `std::vector<T>` | `list` | [1, 2, 3] |
| `std::map<K,V>` | `dict` | {"a": 1} |
| `std::pair<T1,T2>` | `tuple` | (1, 2) |
| `std::optional<T>` | `Optional[T]` | None or value |

**示例:**

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // 启用STL转换
#include <vector>
#include <map>

namespace py = pybind11;

std::vector<int> get_numbers() {
    return {1, 2, 3, 4, 5};
}

std::map<std::string, int> get_scores() {
    return {{"Alice", 95}, {"Bob", 87}, {"Charlie", 92}};
}

PYBIND11_MODULE(types_example, m) {
    m.def("get_numbers", &get_numbers);
    m.def("get_scores", &get_scores);
}
```

```python
import types_example

numbers = types_example.get_numbers()
print(numbers)  # [1, 2, 3, 4, 5] - Python list

scores = types_example.get_scores()
print(scores)   # {'Alice': 95, 'Bob': 87, 'Charlie': 92} - Python dict
```

### 4.2 自定义类型转换

**USearch的标量类型转换 (python/lib.cpp:139-153):**

```cpp
scalar_kind_t numpy_string_to_kind(std::string const& name) {
    // NumPy dtype字符串 → USearch内部类型
    if (name == "B" || name == "<B" || name == "u1" || name == "|u1")
        return scalar_kind_t::b1x8_k;
    else if (name == "b" || name == "<b" || name == "i1" || name == "|i1")
        return scalar_kind_t::i8_k;
    else if (name == "e" || name == "<e" || name == "f2" || name == "<f2")
        return scalar_kind_t::f16_k;  // Half precision
    else if (name == "f" || name == "<f" || name == "f4" || name == "<f4")
        return scalar_kind_t::f32_k;
    else if (name == "d" || name == "<d" || name == "i8" || name == "<i8")
        return scalar_kind_t::f64_k;
    else
        return scalar_kind_t::unknown_k;
}
```

**为什么需要这个函数?**
```
NumPy数组: dtype='f4' (字符串)
     ↓ 转换
USearch: scalar_kind_t::f32_k (枚举)
     ↓ 使用
模板特化: 选择float版本的distance函数
```

### 4.3 枚举类型绑定

**USearch的枚举绑定:**

```cpp
// python/lib.cpp (简化)
PYBIND11_MODULE(compiled, m) {
    // 绑定MetricKind枚举
    py::enum_<metric_kind_t>(m, "MetricKind", py::arithmetic())
        .value("IP", metric_kind_t::ip_k, "Inner Product")
        .value("Cos", metric_kind_t::cos_k, "Cosine Similarity")
        .value("L2sq", metric_kind_t::l2sq_k, "Squared Euclidean")
        .value("Haversine", metric_kind_t::haversine_k, "Haversine (geographic)")
        .value("Hamming", metric_kind_t::hamming_k, "Hamming (binary)")
        .export_values();  // 导出到模块级别

    // 绑定ScalarKind枚举
    py::enum_<scalar_kind_t>(m, "ScalarKind", py::arithmetic())
        .value("F64", scalar_kind_t::f64_k, "64-bit float")
        .value("F32", scalar_kind_t::f32_k, "32-bit float")
        .value("F16", scalar_kind_t::f16_k, "16-bit float")
        .value("I8", scalar_kind_t::i8_k, "8-bit integer")
        .value("B1", scalar_kind_t::b1x8_k, "Binary")
        .export_values();
}
```

**Python使用:**

```python
from usearch.compiled import MetricKind, ScalarKind

# 使用枚举
metric = MetricKind.Cos
print(metric)  # MetricKind.Cos

# 比较
if metric == MetricKind.Cos:
    print("Using cosine similarity")

# 枚举值
print(int(metric))  # 99 (对应'c'的ASCII码)

# 所有可能值
print(list(MetricKind.__members__.keys()))
# ['IP', 'Cos', 'L2sq', 'Haversine', 'Hamming', ...]
```

---

## 5. 类与对象绑定

### 5.1 基础类绑定

**简单类示例:**

```cpp
#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

class Person {
public:
    Person(const std::string& name, int age)
        : name_(name), age_(age) {}

    std::string get_name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    int get_age() const { return age_; }
    void set_age(int age) { age_ = age; }

    std::string greet() const {
        return "Hello, I'm " + name_ + "!";
    }

private:
    std::string name_;
    int age_;
};

PYBIND11_MODULE(person_module, m) {
    py::class_<Person>(m, "Person")
        // 构造函数
        .def(py::init<const std::string&, int>(),
             py::arg("name"), py::arg("age"))

        // 方法
        .def("get_name", &Person::get_name)
        .def("set_name", &Person::set_name)
        .def("get_age", &Person::get_age)
        .def("set_age", &Person::set_age)
        .def("greet", &Person::greet);
}
```

**Python使用:**

```python
import person_module

# 创建对象
person = person_module.Person("Alice", 30)

# 调用方法
print(person.get_name())  # "Alice"
print(person.greet())     # "Hello, I'm Alice!"

# 修改属性
person.set_age(31)
print(person.get_age())   # 31
```

### 5.2 属性绑定

**使用`def_readwrite`和`def_readonly`:**

```cpp
class Rectangle {
public:
    double width;
    double height;

    Rectangle(double w, double h) : width(w), height(h) {}

    double area() const { return width * height; }
};

PYBIND11_MODULE(rectangle, m) {
    py::class_<Rectangle>(m, "Rectangle")
        .def(py::init<double, double>())

        // 读写属性 (public成员)
        .def_readwrite("width", &Rectangle::width)
        .def_readwrite("height", &Rectangle::height)

        // 方法
        .def("area", &Rectangle::area);
}
```

**使用`def_property`和`def_property_readonly`:**

```cpp
class Circle {
private:
    double radius_;

public:
    Circle(double r) : radius_(r) {}

    double get_radius() const { return radius_; }
    void set_radius(double r) {
        if (r < 0) throw std::invalid_argument("Radius must be positive");
        radius_ = r;
    }

    double area() const { return 3.14159 * radius_ * radius_; }
};

PYBIND11_MODULE(circle, m) {
    py::class_<Circle>(m, "Circle")
        .def(py::init<double>())

        // 属性 (通过getter/setter)
        .def_property("radius",
                     &Circle::get_radius,
                     &Circle::set_radius)

        // 只读属性
        .def_property_readonly("area", &Circle::area);
}
```

**Python使用:**

```python
import circle

c = circle.Circle(5.0)

# 属性访问 (调用getter/setter)
print(c.radius)  # 5.0
c.radius = 10.0  # 调用set_radius
print(c.radius)  # 10.0

# 只读属性
print(c.area)    # 314.159
# c.area = 100   # 错误! 只读属性
```

### 5.3 USearch的Index类绑定

**核心Index类绑定 (python/lib.cpp:1229-1265,简化):**

```cpp
// python/lib.cpp
PYBIND11_MODULE(compiled, m) {
    py::class_<dense_index_py_t> i(m, "Index");

    // 构造函数 (通过工厂函数)
    i.def(py::init([](std::size_t dimensions,
                      std::string const& metric,
                      std::string const& dtype,
                      std::size_t connectivity,
                      std::size_t expansion_add,
                      std::size_t expansion_search,
                      bool multi) {
        // 转换参数类型
        metric_kind_t metric_kind = /* 解析metric字符串 */;
        scalar_kind_t scalar_kind = /* 解析dtype字符串 */;

        // 调用C++工厂函数
        return make_index(dimensions, scalar_kind, connectivity,
                         expansion_add, expansion_search,
                         metric_kind, /* ... */);
    }),
    py::arg("ndim"),
    py::arg("metric") = "cos",
    py::arg("dtype") = "f32",
    py::arg("connectivity") = default_connectivity(),
    py::arg("expansion_add") = default_expansion_add(),
    py::arg("expansion_search") = default_expansion_search(),
    py::arg("multi") = false);

    // 只读属性
    i.def_property_readonly("size", &dense_index_py_t::size);
    i.def_property_readonly("capacity", &dense_index_py_t::capacity);
    i.def_property_readonly("connectivity", &dense_index_py_t::connectivity);
    i.def_property_readonly("ndim",
        [](dense_index_py_t const& index) -> std::size_t {
            return index.metric().dimensions();
        });
    i.def_property_readonly("dtype",
        [](dense_index_py_t const& index) -> scalar_kind_t {
            return index.scalar_kind();
        });

    // 读写属性
    i.def_property("expansion_add",
                   &dense_index_py_t::expansion_add,
                   &dense_index_py_t::change_expansion_add);
    i.def_property("expansion_search",
                   &dense_index_py_t::expansion_search,
                   &dense_index_py_t::change_expansion_search);

    // 特殊方法
    i.def("__len__", &dense_index_py_t::size);
}
```

**Python使用:**

```python
from usearch.compiled import Index

# 创建索引
index = Index(
    ndim=768,
    metric='cos',
    dtype='f16',
    connectivity=16,
    expansion_add=128,
    expansion_search=64
)

# 只读属性
print(f"Size: {index.size}")
print(f"Dimensions: {index.ndim}")
print(f"Data type: {index.dtype}")

# 读写属性
print(f"Expansion (search): {index.expansion_search}")
index.expansion_search = 128  # 修改
print(f"New expansion: {index.expansion_search}")

# 特殊方法
print(f"Length: {len(index)}")  # 调用 __len__
```

---

## 6. NumPy集成

### 6.1 NumPy数组作为参数

**基础NumPy绑定:**

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

// 接受NumPy数组,计算和
double sum_array(py::array_t<double> array) {
    // 请求数组信息
    py::buffer_info buf = array.request();

    // 获取指针和大小
    double* ptr = static_cast<double*>(buf.ptr);
    size_t size = buf.size;

    // 计算和
    double result = 0.0;
    for (size_t i = 0; i < size; i++) {
        result += ptr[i];
    }

    return result;
}

PYBIND11_MODULE(numpy_example, m) {
    m.def("sum_array", &sum_array);
}
```

**Python使用:**

```python
import numpy as np
import numpy_example

arr = np.array([1.0, 2.0, 3.0, 4.0, 5.0])
result = numpy_example.sum_array(arr)
print(result)  # 15.0

# 支持不同dtype (自动转换)
arr_int = np.array([1, 2, 3], dtype=np.int32)
result = numpy_example.sum_array(arr_int)  # 自动转换为double
```

### 6.2 检查数组形状和维度

**USearch的add_many函数 (python/lib.cpp:214-259):**

```cpp
template <typename index_at>
static void add_many_to_index(
    index_at& index,
    py::buffer keys,      // NumPy数组: keys
    py::buffer vectors,   // NumPy数组: vectors
    bool force_copy,
    std::size_t threads,
    progress_func_t const& progress
) {
    // 获取数组信息
    py::buffer_info keys_info = keys.request();
    py::buffer_info vectors_info = vectors.request();

    // ===== 验证1: 数据类型 =====
    if (keys_info.itemsize != sizeof(dense_key_t))
        throw std::invalid_argument("Incompatible key type!");

    // ===== 验证2: 内存布局 (C-contiguous) =====
    if (keys_info.strides[0] != static_cast<Py_ssize_t>(keys_info.itemsize))
        throw std::invalid_argument("Keys array must be C-contiguous.");

    if (vectors_info.strides[1] != static_cast<Py_ssize_t>(vectors_info.itemsize))
        throw std::invalid_argument("Matrix rows must be contiguous, "
                                   "try `ascontiguousarray`.");

    // ===== 验证3: 数组维度 =====
    if (keys_info.ndim != 1)
        throw std::invalid_argument("Keys must be placed in a "
                                   "single-dimensional array!");

    if (vectors_info.ndim != 2)
        throw std::invalid_argument("Expects a matrix of vectors to add!");

    // ===== 验证4: 形状匹配 =====
    Py_ssize_t keys_count = keys_info.shape[0];
    Py_ssize_t vectors_count = vectors_info.shape[0];
    Py_ssize_t vectors_dimensions = vectors_info.shape[1];

    if (vectors_dimensions != static_cast<Py_ssize_t>(index.scalar_words()))
        throw std::invalid_argument("The number of vector dimensions doesn't match!");

    if (keys_count != vectors_count)
        throw std::invalid_argument("Number of keys and vectors must match!");

    // ===== 执行添加操作 =====
    // ...
}
```

**关键概念:**

```python
import numpy as np

# C-contiguous (行优先,连续存储)
arr_c = np.array([[1, 2, 3],
                  [4, 5, 6]], order='C')
print(arr_c.flags['C_CONTIGUOUS'])  # True
print(arr_c.strides)  # (12, 4) - 每行12字节,每元素4字节

# F-contiguous (列优先,Fortran风格)
arr_f = np.array([[1, 2, 3],
                  [4, 5, 6]], order='F')
print(arr_f.flags['C_CONTIGUOUS'])  # False
print(arr_f.strides)  # (4, 8) - 需要转换!

# 转换为C-contiguous
arr_fixed = np.ascontiguousarray(arr_f)
print(arr_fixed.flags['C_CONTIGUOUS'])  # True
```

### 6.3 返回NumPy数组

**创建NumPy数组返回给Python:**

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

py::array_t<double> create_matrix(size_t rows, size_t cols) {
    // 分配内存
    auto result = py::array_t<double>({rows, cols});

    // 获取可变访问器
    auto result_2d = result.mutable_unchecked<2>();

    // 填充数据
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            result_2d(i, j) = i * cols + j;
        }
    }

    return result;
}

PYBIND11_MODULE(matrix_example, m) {
    m.def("create_matrix", &create_matrix);
}
```

**Python使用:**

```python
import matrix_example

matrix = matrix_example.create_matrix(3, 4)
print(matrix)
# [[ 0.  1.  2.  3.]
#  [ 4.  5.  6.  7.]
#  [ 8.  9. 10. 11.]]
```

### 6.4 USearch的search函数

**返回复杂NumPy结果 (python/lib.cpp:261-319):**

```cpp
template <typename scalar_at>
static void search_typed(
    dense_index_py_t& index,
    py::buffer_info& vectors_info,
    std::size_t wanted,
    bool exact,
    std::size_t threads,
    // 输出参数: 预分配的NumPy数组
    py::array_t<dense_key_t>& keys_py,
    py::array_t<distance_t>& distances_py,
    py::array_t<Py_ssize_t>& counts_py,
    // 统计信息
    std::atomic<std::size_t>& stats_visited_members,
    std::atomic<std::size_t>& stats_computed_distances,
    progress_func_t const& progress
) {
    // 获取可变访问器 (无边界检查,高性能)
    auto keys_py2d = keys_py.template mutable_unchecked<2>();
    auto distances_py2d = distances_py.template mutable_unchecked<2>();
    auto counts_py1d = counts_py.template mutable_unchecked<1>();

    Py_ssize_t vectors_count = vectors_info.shape[0];
    byte_t const* vectors_data = reinterpret_cast<byte_t const*>(vectors_info.ptr);

    // 多线程搜索
    executor_default_t{threads}.dynamic(vectors_count,
        [&](std::size_t thread_idx, std::size_t task_idx) {

        // 获取查询向量
        scalar_at const* vector = (scalar_at const*)(
            vectors_data + task_idx * vectors_info.strides[0]
        );

        // 执行搜索
        dense_search_result_t result = index.search(vector, wanted, thread_idx, exact);
        if (!result) {
            // 错误处理
            return false;
        }

        // 将结果写入NumPy数组
        counts_py1d(task_idx) = static_cast<Py_ssize_t>(
            result.dump_to(
                &keys_py2d(task_idx, 0),      // 输出keys
                &distances_py2d(task_idx, 0),  // 输出distances
                wanted
            )
        );

        // 更新统计
        stats_visited_members += result.visited_members;
        stats_computed_distances += result.computed_distances;

        return true;
    });
}
```

**调用层 (在Python中预分配数组):**

```python
def search(self, vectors, count=10, exact=False, threads=0):
    """搜索最近邻"""
    vectors = np.asarray(vectors, dtype=self.dtype)

    # 预分配结果数组
    if vectors.ndim == 1:
        keys = np.empty((count,), dtype=np.uint64)
        distances = np.empty((count,), dtype=np.float32)
        counts = np.empty((1,), dtype=np.intp)
    else:
        batch_size = len(vectors)
        keys = np.empty((batch_size, count), dtype=np.uint64)
        distances = np.empty((batch_size, count), dtype=np.float32)
        counts = np.empty((batch_size,), dtype=np.intp)

    # 调用C++函数 (直接填充到预分配的数组)
    self._compiled.search(vectors, keys, distances, counts,
                         count, exact, threads)

    return keys, distances, counts
```

---

## 7. 内存管理与生命周期

### 7.1 对象所有权

**PyBind11的所有权策略:**

| 策略 | 说明 | 使用场景 |
|------|------|---------|
| `py::return_value_policy::take_ownership` | Python接管C++对象 | 工厂函数 |
| `py::return_value_policy::copy` | 复制返回 | 小对象 |
| `py::return_value_policy::move` | 移动返回 | 大对象 |
| `py::return_value_policy::reference` | 返回引用 | C++对象仍存在 |
| `py::return_value_policy::reference_internal` | 引用,生命周期绑定到父对象 | 成员访问 |
| `py::return_value_policy::automatic` | 自动选择 (默认) | 大多数情况 |

**示例:**

```cpp
class DataHolder {
    std::vector<double> data_;
public:
    DataHolder(size_t size) : data_(size) {}

    // 返回引用 (不安全,除非DataHolder一直存在)
    std::vector<double>& get_data_ref() {
        return data_;
    }

    // 返回拷贝 (安全,但可能慢)
    std::vector<double> get_data_copy() const {
        return data_;
    }

    // 返回const引用 (安全,生命周期绑定)
    const std::vector<double>& get_data_const_ref() const {
        return data_;
    }
};

PYBIND11_MODULE(ownership, m) {
    py::class_<DataHolder>(m, "DataHolder")
        .def(py::init<size_t>())

        // 危险: 返回非const引用
        .def("get_data_ref", &DataHolder::get_data_ref,
             py::return_value_policy::reference_internal)

        // 安全: 返回拷贝
        .def("get_data_copy", &DataHolder::get_data_copy)

        // 最优: const引用,生命周期绑定
        .def("get_data_const_ref", &DataHolder::get_data_const_ref,
             py::return_value_policy::reference_internal);
}
```

### 7.2 共享指针

**使用`std::shared_ptr`管理生命周期:**

```cpp
#include <pybind11/pybind11.h>
#include <memory>

namespace py = pybind11;

class Resource {
public:
    Resource() { std::cout << "Resource created\n"; }
    ~Resource() { std::cout << "Resource destroyed\n"; }

    void use() { std::cout << "Using resource\n"; }
};

// 工厂函数返回shared_ptr
std::shared_ptr<Resource> create_resource() {
    return std::make_shared<Resource>();
}

PYBIND11_MODULE(shared_ptr_example, m) {
    py::class_<Resource, std::shared_ptr<Resource>>(m, "Resource")
        //                 ↑ 指定使用shared_ptr管理
        .def("use", &Resource::use);

    m.def("create_resource", &create_resource);
}
```

**Python使用:**

```python
import shared_ptr_example

# 创建资源
res1 = shared_ptr_example.create_resource()  # "Resource created"
res1.use()  # "Using resource"

# Python和C++共享所有权
res2 = res1  # 引用计数+1

del res1  # 引用计数-1
res2.use()  # 仍然有效

del res2  # 引用计数=0, "Resource destroyed"
```

### 7.3 USearch的共享索引

**USearch Indexes类使用shared_ptr (python/lib.cpp:73-76):**

```cpp
struct dense_indexes_py_t {
    // 每个分片使用shared_ptr管理
    std::vector<std::shared_ptr<dense_index_py_t>> shards_;

    void merge(std::shared_ptr<dense_index_py_t> shard) {
        shards_.push_back(shard);  // 增加引用计数
    }

    // ... 其他方法
};
```

**为什么使用shared_ptr?**
- ✅ Python和C++共享所有权
- ✅ 线程安全的引用计数
- ✅ 自动清理,无内存泄漏
- ✅ 可以安全地在多个Indexes之间共享分片

---

## 8. 多线程与GIL

### 8.1 GIL (Global Interpreter Lock)

**GIL是什么?**

```
Python的全局解释器锁:
┌─────────────────────────────────┐
│         Python进程              │
│  ┌──────────────────────────┐   │
│  │        GIL (锁)          │   │
│  └──────────────────────────┘   │
│    ↓                    ↓        │
│  Thread 1             Thread 2   │
│  (执行Python)        (等待GIL) │
└─────────────────────────────────┘

结果: 多线程Python代码无法并行!
```

**如何绕过GIL?**

使用`py::gil_scoped_release`释放GIL:

```cpp
#include <pybind11/pybind11.h>
#include <thread>
#include <chrono>

namespace py = pybind11;

void slow_computation() {
    // ===== 释放GIL =====
    py::gil_scoped_release release;

    // 现在可以真正并行执行
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 自动重新获取GIL (析构时)
}

PYBIND11_MODULE(gil_example, m) {
    m.def("slow_computation", &slow_computation);
}
```

**Python测试:**

```python
import gil_example
import threading
import time

def run():
    gil_example.slow_computation()

# 启动多个线程
threads = [threading.Thread(target=run) for _ in range(4)]

start = time.time()
for t in threads:
    t.start()
for t in threads:
    t.join()
end = time.time()

print(f"Time: {end - start:.2f}s")
# 如果释放了GIL: ~5秒 (并行)
# 如果没释放GIL: ~20秒 (串行)
```

### 8.2 USearch的多线程实现

**add_many的多线程 (python/lib.cpp:167-212):**

```cpp
template <typename scalar_at>
static void add_typed_to_index(
    dense_index_py_t& index,
    py::buffer_info const& keys_info,
    py::buffer_info const& vectors_info,
    bool force_copy,
    std::size_t threads,
    progress_func_t const& progress
) {
    Py_ssize_t vectors_count = vectors_info.shape[0];
    byte_t const* vectors_data = reinterpret_cast<byte_t const*>(vectors_info.ptr);
    byte_t const* keys_data = reinterpret_cast<byte_t const*>(keys_info.ptr);

    std::atomic<std::size_t> processed{0};
    atomic_error_t atomic_error{nullptr};

    // ===== 核心: executor_default_t会在内部释放GIL! =====
    executor_default_t{threads}.dynamic(
        vectors_count,
        [&](std::size_t thread_idx, std::size_t task_idx) {
            // 这里可以真正并行执行
            dense_key_t key = *reinterpret_cast<dense_key_t const*>(
                keys_data + task_idx * keys_info.strides[0]
            );
            scalar_at const* vector = reinterpret_cast<scalar_at const*>(
                vectors_data + task_idx * vectors_info.strides[0]
            );

            dense_add_result_t result = index.add(key, vector, thread_idx, force_copy);
            if (!result) {
                atomic_error = result.error.release();
                return false;
            }

            // ===== 检查Python信号 (需要GIL) =====
            ++processed;
            if (thread_idx == 0) {
                // 临时获取GIL
                py::gil_scoped_acquire acquire;

                // 检查Ctrl+C等信号
                if (PyErr_CheckSignals() != 0 ||
                    !progress(processed.load(), vectors_count)) {
                    atomic_error.store("Operation has been terminated");
                    return false;
                }
            }

            return true;
        }
    );

    // 错误处理
    auto error = atomic_error.load();
    if (error) {
        PyErr_SetString(PyExc_RuntimeError, error);
        throw py::error_already_set();
    }
}
```

**关键要点:**

1. **自动GIL管理:** executor内部自动释放/获取GIL
2. **信号检查:** 定期检查Python信号(Ctrl+C)
3. **原子操作:** 使用`std::atomic`避免数据竞争
4. **错误传播:** 从C++线程安全地传递错误到Python

---

## 9. 错误处理与异常

### 9.1 C++异常到Python异常

**自动转换:**

```cpp
#include <pybind11/pybind11.h>
#include <stdexcept>

namespace py = pybind11;

void may_throw(int value) {
    if (value < 0) {
        throw std::invalid_argument("Value must be non-negative");
    }
    if (value > 100) {
        throw std::out_of_range("Value must be <= 100");
    }
    // ...
}

PYBIND11_MODULE(exception_example, m) {
    m.def("may_throw", &may_throw);
}
```

**Python捕获:**

```python
import exception_example

try:
    exception_example.may_throw(-1)
except ValueError as e:  # std::invalid_argument → ValueError
    print(f"Error: {e}")
    # 输出: Error: Value must be non-negative

try:
    exception_example.may_throw(101)
except IndexError as e:  # std::out_of_range → IndexError
    print(f"Error: {e}")
    # 输出: Error: Value must be <= 100
```

**异常映射表:**

| C++异常 | Python异常 |
|---------|-----------|
| `std::exception` | `RuntimeError` |
| `std::invalid_argument` | `ValueError` |
| `std::out_of_range` | `IndexError` |
| `std::bad_alloc` | `MemoryError` |
| `std::domain_error` | `ValueError` |
| `std::runtime_error` | `RuntimeError` |

### 9.2 自定义异常

```cpp
#include <pybind11/pybind11.h>

namespace py = pybind11;

// 自定义C++异常
class CustomError : public std::exception {
    std::string message_;
public:
    CustomError(const std::string& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

void risky_operation() {
    throw CustomError("Something went wrong!");
}

PYBIND11_MODULE(custom_exception, m) {
    // 注册自定义异常
    py::register_exception<CustomError>(m, "CustomError");

    m.def("risky_operation", &risky_operation);
}
```

**Python使用:**

```python
import custom_exception

try:
    custom_exception.risky_operation()
except custom_exception.CustomError as e:
    print(f"Caught custom error: {e}")
```

### 9.3 USearch的错误处理

**forward_error辅助函数 (python/lib.cpp:155-163):**

```cpp
template <typename result_at>
void forward_error(result_at&& result) {
    // 检查C++结果对象
    if (!result)
        throw std::invalid_argument(result.error.release());

    // 检查Python信号 (Ctrl+C等)
    int signals = PyErr_CheckSignals();
    if (signals != 0)
        throw py::error_already_set();  // 传播Python异常
}
```

**使用示例:**

```cpp
i.def("add_one",
    [](dense_index_py_t& index, dense_key_t key, py::buffer vector) {
        // ... 准备数据

        // 执行操作
        dense_add_result_t result = index.add(key, vector_ptr);

        // 检查错误并转发
        forward_error(result);

        return result.new_size;
    });
```

**错误处理流程:**

```
C++层:
index.add() → 返回result对象 (带error字段)
     ↓
forward_error() → 检查result.error
     ↓ (如果有错误)
throw std::invalid_argument(error_message)
     ↓
PyBind11层:
转换为Python的ValueError
     ↓
Python层:
try:
    index.add_one(key, vector)
except ValueError as e:
    print(e)
```

---

## 10. 性能优化技巧

### 10.1 避免不必要的拷贝

**❌ 低效版本:**

```cpp
void process_large_array(py::array_t<double> array) {
    std::vector<double> vec = array.cast<std::vector<double>>();  // 拷贝!
    // 处理vec...
}
```

**✅ 高效版本:**

```cpp
void process_large_array(py::array_t<double> array) {
    py::buffer_info buf = array.request();
    double* ptr = static_cast<double*>(buf.ptr);  // 直接访问
    size_t size = buf.size;
    // 直接处理ptr...
}
```

### 10.2 使用unchecked访问器

**checked vs unchecked:**

```cpp
// ❌ 慢: 每次访问都检查边界
void sum_checked(py::array_t<double> arr) {
    auto a = arr.mutable_unchecked<1>();  // 获取访问器

    for (ssize_t i = 0; i < arr.size(); i++) {
        double val = a[i];  // ← 边界检查
        // ...
    }
}

// ✅ 快: 无边界检查
void sum_unchecked(py::array_t<double> arr) {
    auto a = arr.unchecked<1>();  // unchecked访问器

    for (ssize_t i = 0; i < arr.size(); i++) {
        double val = a(i);  // ← 无边界检查,使用()而不是[]
        // ...
    }
}
```

**性能差异:** 10-30% 提升 (取决于操作复杂度)

**USearch使用 (python/lib.cpp:269-270):**

```cpp
// 使用unchecked访问器获取最佳性能
auto keys_py2d = keys_py.template mutable_unchecked<2>();
auto distances_py2d = distances_py.template mutable_unchecked<2>();
auto counts_py1d = counts_py.template mutable_unchecked<1>();
```

### 10.3 预分配输出数组

**❌ 低效: 在C++中分配:**

```cpp
py::array_t<double> compute_results(size_t n) {
    auto result = py::array_t<double>(n);  // C++分配
    auto r = result.mutable_unchecked<1>();

    for (size_t i = 0; i < n; i++) {
        r(i) = expensive_computation(i);
    }

    return result;
}
```

**✅ 高效: 在Python预分配:**

```cpp
void compute_results(py::array_t<double> output) {
    auto r = output.mutable_unchecked<1>();
    size_t n = output.size();

    for (size_t i = 0; i < n; i++) {
        r(i) = expensive_computation(i);  // 直接写入
    }
}
```

```python
# Python端
result = np.empty(1000000, dtype=np.float64)  # 预分配
compute_results(result)  # 直接填充
```

**优势:**
- ✅ 减少跨语言边界的数据传输
- ✅ Python可以控制内存分配策略
- ✅ 避免不必要的引用计数操作

### 10.4 批量操作

**❌ 逐个调用:**

```python
for key, vector in zip(keys, vectors):
    index.add_one(key, vector)  # N次Python→C++调用
```

**✅ 批量调用:**

```python
index.add_many(keys, vectors)  # 1次Python→C++调用
```

**性能差异:**
- 100万向量: 逐个调用 ~20秒, 批量调用 ~2秒 (10倍提升!)

---

## 11. 项目实战: 完整示例

### 11.1 简化版USearch

**C++头文件 (simple_index.hpp):**

```cpp
#pragma once
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>

class SimpleIndex {
public:
    using key_t = uint64_t;
    using distance_t = float;

    SimpleIndex(size_t dimensions)
        : dimensions_(dimensions) {}

    // 添加向量
    void add(key_t key, const float* vector) {
        std::vector<float> vec(vector, vector + dimensions_);
        vectors_[key] = std::move(vec);
    }

    // 搜索最近邻
    std::vector<std::pair<key_t, distance_t>> search(
        const float* query,
        size_t k
    ) const {
        std::vector<std::pair<key_t, distance_t>> results;

        // 计算所有距离
        for (const auto& [key, vec] : vectors_) {
            float dist = cosine_distance(query, vec.data());
            results.emplace_back(key, dist);
        }

        // 排序并返回top-k
        std::partial_sort(
            results.begin(),
            results.begin() + std::min(k, results.size()),
            results.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            }
        );

        results.resize(std::min(k, results.size()));
        return results;
    }

    size_t size() const { return vectors_.size(); }
    size_t dimensions() const { return dimensions_; }

private:
    size_t dimensions_;
    std::unordered_map<key_t, std::vector<float>> vectors_;

    float cosine_distance(const float* a, const float* b) const {
        float dot = 0, norm_a = 0, norm_b = 0;
        for (size_t i = 0; i < dimensions_; i++) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        return 1.0f - dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }
};
```

**Python绑定 (bindings.cpp):**

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "simple_index.hpp"

namespace py = pybind11;

PYBIND11_MODULE(simple_index, m) {
    m.doc() = "Simple vector search index";

    py::class_<SimpleIndex>(m, "Index")
        // 构造函数
        .def(py::init<size_t>(), py::arg("dimensions"))

        // 添加单个向量
        .def("add",
            [](SimpleIndex& self, uint64_t key, py::array_t<float> vector) {
                py::buffer_info buf = vector.request();
                if (buf.ndim != 1)
                    throw std::runtime_error("Expected 1D array");
                if (buf.shape[0] != static_cast<ssize_t>(self.dimensions()))
                    throw std::runtime_error("Vector dimension mismatch");

                self.add(key, static_cast<float*>(buf.ptr));
            },
            py::arg("key"), py::arg("vector"))

        // 批量添加
        .def("add_many",
            [](SimpleIndex& self, py::array_t<uint64_t> keys,
               py::array_t<float> vectors) {
                py::buffer_info keys_buf = keys.request();
                py::buffer_info vectors_buf = vectors.request();

                if (keys_buf.ndim != 1)
                    throw std::runtime_error("Keys must be 1D");
                if (vectors_buf.ndim != 2)
                    throw std::runtime_error("Vectors must be 2D");
                if (keys_buf.shape[0] != vectors_buf.shape[0])
                    throw std::runtime_error("Keys and vectors count mismatch");

                uint64_t* keys_ptr = static_cast<uint64_t*>(keys_buf.ptr);
                float* vectors_ptr = static_cast<float*>(vectors_buf.ptr);
                size_t count = keys_buf.shape[0];
                size_t stride = vectors_buf.strides[0] / sizeof(float);

                for (size_t i = 0; i < count; i++) {
                    self.add(keys_ptr[i], vectors_ptr + i * stride);
                }
            },
            py::arg("keys"), py::arg("vectors"))

        // 搜索
        .def("search",
            [](const SimpleIndex& self, py::array_t<float> query, size_t k) {
                py::buffer_info buf = query.request();
                if (buf.ndim != 1)
                    throw std::runtime_error("Query must be 1D");

                auto results = self.search(static_cast<float*>(buf.ptr), k);

                // 转换为NumPy数组
                py::array_t<uint64_t> keys(results.size());
                py::array_t<float> distances(results.size());

                auto keys_ptr = keys.mutable_unchecked<1>();
                auto dist_ptr = distances.mutable_unchecked<1>();

                for (size_t i = 0; i < results.size(); i++) {
                    keys_ptr(i) = results[i].first;
                    dist_ptr(i) = results[i].second;
                }

                return py::make_tuple(keys, distances);
            },
            py::arg("query"), py::arg("k") = 10)

        // 属性
        .def_property_readonly("size", &SimpleIndex::size)
        .def_property_readonly("dimensions", &SimpleIndex::dimensions)

        // 特殊方法
        .def("__len__", &SimpleIndex::size)
        .def("__repr__",
            [](const SimpleIndex& self) {
                return "<SimpleIndex dimensions=" +
                       std::to_string(self.dimensions()) +
                       " size=" + std::to_string(self.size()) + ">";
            });
}
```

**setup.py:**

```python
from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup

ext_modules = [
    Pybind11Extension(
        "simple_index",
        ["bindings.cpp"],
        include_dirs=["include"],
        cxx_std=11,
    ),
]

setup(
    name="simple_index",
    version="0.1.0",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)
```

**编译和使用:**

```bash
# 编译
python setup.py build_ext --inplace

# 测试
python
```

```python
import numpy as np
import simple_index

# 创建索引
index = simple_index.Index(dimensions=128)

# 添加向量
vectors = np.random.randn(1000, 128).astype(np.float32)
keys = np.arange(1000, dtype=np.uint64)
index.add_many(keys, vectors)

# 搜索
query = np.random.randn(128).astype(np.float32)
keys, distances = index.search(query, k=10)

print(f"Found {len(keys)} neighbors")
print(f"Keys: {keys}")
print(f"Distances: {distances}")

# 特殊方法
print(len(index))  # 1000
print(repr(index))  # <SimpleIndex dimensions=128 size=1000>
```

---

## 12. 调试与测试

### 12.1 调试技巧

**1. 启用调试符号:**

```bash
# setup.py中添加
ext_modules = [
    Pybind11Extension(
        "mymodule",
        ["src/bindings.cpp"],
        extra_compile_args=['-g', '-O0'],  # 调试模式
    ),
]
```

**2. 使用GDB/LLDB:**

```bash
# 在GDB中运行Python
gdb python
(gdb) run script.py

# 设置断点
(gdb) break bindings.cpp:42
(gdb) continue

# 查看Python栈和C++栈
(gdb) py-bt  # Python栈
(gdb) bt     # C++栈
```

**3. 打印调试信息:**

```cpp
#include <iostream>

void debug_function(py::array_t<float> arr) {
    py::buffer_info buf = arr.request();

    std::cerr << "Array info:\n";
    std::cerr << "  ndim: " << buf.ndim << "\n";
    std::cerr << "  shape: [";
    for (size_t i = 0; i < buf.shape.size(); i++) {
        std::cerr << buf.shape[i];
        if (i < buf.shape.size() - 1) std::cerr << ", ";
    }
    std::cerr << "]\n";
    std::cerr << "  strides: [";
    for (size_t i = 0; i < buf.strides.size(); i++) {
        std::cerr << buf.strides[i];
        if (i < buf.strides.size() - 1) std::cerr << ", ";
    }
    std::cerr << "]\n";
}
```

### 12.2 单元测试

**Python端测试 (test_simple_index.py):**

```python
import pytest
import numpy as np
import simple_index

def test_construction():
    """测试构造"""
    index = simple_index.Index(dimensions=10)
    assert index.dimensions == 10
    assert index.size == 0
    assert len(index) == 0

def test_add_one():
    """测试添加单个向量"""
    index = simple_index.Index(dimensions=5)
    vector = np.array([1, 2, 3, 4, 5], dtype=np.float32)

    index.add(0, vector)
    assert index.size == 1

def test_add_many():
    """测试批量添加"""
    index = simple_index.Index(dimensions=3)

    keys = np.array([0, 1, 2], dtype=np.uint64)
    vectors = np.array([
        [1, 0, 0],
        [0, 1, 0],
        [0, 0, 1]
    ], dtype=np.float32)

    index.add_many(keys, vectors)
    assert index.size == 3

def test_search():
    """测试搜索"""
    index = simple_index.Index(dimensions=3)

    # 添加3个正交向量
    keys = np.array([0, 1, 2], dtype=np.uint64)
    vectors = np.array([
        [1, 0, 0],
        [0, 1, 0],
        [0, 0, 1]
    ], dtype=np.float32)
    index.add_many(keys, vectors)

    # 查询应该找到最近的
    query = np.array([0.9, 0.1, 0.1], dtype=np.float32)
    result_keys, result_dists = index.search(query, k=2)

    assert len(result_keys) == 2
    assert result_keys[0] == 0  # [1,0,0]最近

def test_dimension_mismatch():
    """测试维度不匹配错误"""
    index = simple_index.Index(dimensions=5)

    with pytest.raises(RuntimeError, match="dimension mismatch"):
        wrong_vector = np.array([1, 2, 3], dtype=np.float32)
        index.add(0, wrong_vector)

def test_invalid_input():
    """测试无效输入"""
    index = simple_index.Index(dimensions=3)

    with pytest.raises(RuntimeError, match="Expected 1D"):
        matrix = np.array([[1, 2, 3], [4, 5, 6]], dtype=np.float32)
        index.add(0, matrix)

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
```

**运行测试:**

```bash
pytest test_simple_index.py -v

# 输出:
# test_simple_index.py::test_construction PASSED
# test_simple_index.py::test_add_one PASSED
# test_simple_index.py::test_add_many PASSED
# test_simple_index.py::test_search PASSED
# test_simple_index.py::test_dimension_mismatch PASSED
# test_simple_index.py::test_invalid_input PASSED
```

---

## 13. 总结与最佳实践

### 13.1 核心要点回顾

**1. 类型系统:**
- ✅ 利用PyBind11的自动类型转换
- ✅ 使用`py::array_t<T>`处理NumPy数组
- ✅ 枚举用`py::enum_<>`绑定

**2. 性能:**
- ✅ 批量操作优于逐个操作
- ✅ 使用`unchecked()`访问器
- ✅ 预分配输出数组
- ✅ 释放GIL实现真正并行

**3. 内存:**
- ✅ 使用`std::shared_ptr`管理复杂对象
- ✅ 注意返回值策略
- ✅ 避免不必要的拷贝

**4. 错误处理:**
- ✅ C++异常自动转换为Python异常
- ✅ 定期检查Python信号
- ✅ 使用`atomic`变量传递多线程错误

### 13.2 检查清单

**开始新项目时:**
- [ ] 安装PyBind11 (`pip install pybind11`)
- [ ] 创建项目结构 (C++源码 + Python包)
- [ ] 编写CMakeLists.txt或setup.py
- [ ] 设置持续集成 (GitHub Actions)

**编写绑定代码时:**
- [ ] 包含必要的头文件 (`<pybind11/numpy.h>`, `<pybind11/stl.h>`)
- [ ] 使用`namespace py = pybind11;`
- [ ] 为函数参数命名 (`py::arg("name")`)
- [ ] 添加文档字符串
- [ ] 处理异常

**优化性能时:**
- [ ] Profile找出瓶颈
- [ ] 批量操作
- [ ] 释放GIL
- [ ] 使用unchecked访问器
- [ ] 预分配输出

**发布前:**
- [ ] 编写单元测试
- [ ] 添加类型存根 (`.pyi`文件)
- [ ] 编写文档
- [ ] 测试多Python版本 (3.7-3.12)
- [ ] 测试多平台 (Linux/macOS/Windows)

### 13.3 学习资源

**官方文档:**
- PyBind11文档: https://pybind11.readthedocs.io
- NumPy C API: https://numpy.org/doc/stable/reference/c-api/

**示例项目:**
- USearch: https://github.com/unum-cloud/usearch
- pybind11项目列表: https://github.com/pybind/pybind11/blob/master/docs/advanced/cast/overview.rst

**进阶主题:**
- Custom type casters
- Embedding Python in C++
- NumPy dtype handling
- Multi-module projects

---

**恭喜你完成Python Binding完全教程!** 🎉

现在你已经掌握了:
- ✅ PyBind11基础和高级特性
- ✅ NumPy数组处理
- ✅ 多线程和GIL管理
- ✅ 性能优化技巧
- ✅ 错误处理最佳实践
- ✅ 完整项目实战经验

**下一步:** 尝试将你自己的C++库绑定到Python! 💪
