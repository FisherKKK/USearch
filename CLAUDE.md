# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

USearch is a single-file vector search engine implementing HNSW (Hierarchical Navigable Small World) algorithm for Approximate Nearest Neighbor (ANN) search. The project is a polyglot library with a C++11 core and bindings for 10+ languages, emphasizing performance, portability, and minimal dependencies.

**Core Philosophy:**
- Single-header C++11 implementation in `include/usearch/index.hpp` (~4.6K lines)
- Language bindings are native (not SWIG-generated) for low latency
- Hardware-agnostic with optional SIMD optimizations via SimSIMD
- Support for multiple quantization levels (f64, f32, f16, bf16, i8, b1)
- Zero required dependencies in C++ core

## Architecture

### Core Components

**Three-tier header structure:**
1. `include/usearch/index.hpp` - Base HNSW graph implementation (`index_gt` class template)
2. `include/usearch/index_dense.hpp` - Dense equi-dimensional vectors (`index_dense_gt` class template)
3. `include/usearch/index_plugins.hpp` - Utilities, memory management, serialization

**Key Classes:**
- `index_gt<...>` - Generic HNSW index with configurable distance metrics and memory management
- `index_dense_gt<...>` - Specialization for fixed-dimensional dense vectors with serialization
- `index_dense_head_t` - Binary format header (64 bytes) containing version, metric type, dimensions, counts

**Metric System:**
- Built-in metrics: cosine, inner product, L2 squared, Haversine, Jaccard, Hamming, Tanimoto, Sorensen
- Custom metric support via function pointers (JIT compilation possible with Numba/Cppyy)
- SimSIMD integration for hardware-accelerated distance calculations

### Language Bindings

Each language binding is independent with its own build system:

- **C99** (`c/`): Exports C API via `libusearch_c.so/.dylib/.dll`, header in `c/usearch.h`
- **Python** (`python/`): PyBind11 bindings, configured via `setup.py` (not CMake)
- **JavaScript** (`javascript/`): NAPI bindings via `binding.gyp`, TypeScript typings
- **Rust** (`rust/`): Native Rust wrapper in `lib.rs`, builds via `build.rs`
- **Go** (`golang/`): CGo bindings that link against C library
- **Java** (`java/`): JNI bindings, Gradle build system, fat-JAR distribution
- **C#** (`csharp/`): P/Invoke to C library, .NET/Mono compatible
- **Swift** (`swift/`): Swift Package Manager, depends on Apple Foundation
- **Objective-C** (`objc/`): Apple platforms only
- **WebAssembly** (`wasm/`): Emscripten-based compilation

## Build Commands

### C++ Core

**Dependencies (Ubuntu):**
```bash
sudo apt-get update && sudo apt-get install cmake build-essential libjemalloc-dev g++-12 gcc-12
```

**Build and test:**
```bash
# Debug build with tests
cmake -D USEARCH_BUILD_TEST_CPP=1 -D CMAKE_BUILD_TYPE=Debug -B build_debug
cmake --build build_debug --config Debug
build_debug/test_cpp

# Release build with all features
cmake -D CMAKE_BUILD_TYPE=Release \
  -D USEARCH_USE_FP16LIB=1 \
  -D USEARCH_USE_OPENMP=1 \
  -D USEARCH_USE_SIMSIMD=1 \
  -D USEARCH_USE_JEMALLOC=1 \
  -D USEARCH_BUILD_TEST_CPP=1 \
  -D USEARCH_BUILD_BENCH_CPP=1 \
  -D USEARCH_BUILD_LIB_C=1 \
  -D USEARCH_BUILD_TEST_C=1 \
  -B build_release
cmake --build build_release --config Release
```

**Run benchmarks:**
```bash
build_release/bench_cpp
```

**Linting:**
```bash
cppcheck --enable=all --force --suppress=cstyleCast --suppress=unusedFunction \
  include/usearch/index.hpp \
  include/usearch/index_dense.hpp \
  include/usearch/index_plugins.hpp
```

### Python

**Setup (requires git submodules):**
```bash
git submodule update --init --recursive
```

**Install locally:**
```bash
# Using uv (recommended)
uv venv --python 3.11
source .venv/bin/activate
uv pip install -e . --force-reinstall

# Using pip
pip install -e . --force-reinstall
```

**Run tests:**
```bash
uv pip install pytest pytest-repeat numpy
python -m pytest python/scripts/ -s -x -p no:warnings
```

**Build wheels for all platforms:**
```bash
pip install cibuildwheel
cibuildwheel --platform linux  # Requires Docker, works on any OS
```

**Linting:**
```bash
pip install ruff
ruff --format=github --select=E9,F63,F7,F82 --target-version=py310 python
```

### JavaScript

**Setup:**
```bash
# Install Node Version Manager if needed
wget -qO- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
nvm install 20
```

**Build and test:**
```bash
npm install -g typescript
npm install          # Compiles javascript/lib.cpp
npm run build-js     # Generates JS from TypeScript
npm test             # Runs test suite
```

**WebAssembly:**
```bash
emcmake cmake -B build -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -s TOTAL_MEMORY=64MB"
emmake make -C build
node build/usearch.test.js
```

### Rust

```bash
cargo test -p usearch -- --nocapture --test-threads=1
cargo clippy --all-targets --all-features
```

### Go

**Build C library first:**
```bash
cmake -B build_release \
  -D USEARCH_BUILD_LIB_C=1 \
  -D USEARCH_USE_OPENMP=1 \
  -D USEARCH_USE_SIMSIMD=1
cmake --build build_release --config Release

cp build_release/libusearch_c.so golang/  # or .dylib on macOS
cp c/usearch.h golang/
```

**Run tests:**
```bash
cd golang && LD_LIBRARY_PATH=. go test -v ; cd ..
```

**Static checks:**
```bash
cd golang
go vet ./...
staticcheck ./...      # if installed
golangci-lint run      # if installed
```

### Java

```bash
gradle clean build --warning-mode=all  # Build
gradle test --rerun-tasks              # Run tests
gradle spotlessApply                   # Format code
```

### C#

**Build C library first, then:**
```bash
# Linux
mkdir -p "csharp/lib/runtimes/linux-x64/native"
cp "build_release/libusearch_c.so" "csharp/lib/runtimes/linux-x64/native"
cd csharp
dotnet test -c Release

# macOS (ARM)
mkdir -p "csharp/lib/runtimes/osx-arm64/native"
cp "build_release/libusearch_c.dylib" "csharp/lib/runtimes/osx-arm64/native"
cd csharp
dotnet test -c Release
```

### Swift

```bash
swift build && swift test -v
```

**Format:**
```bash
brew install swift-format
swift-format . -i -r
```

## Development Patterns

### Testing Philosophy

**Single test per language binding:** Each binding has one comprehensive test file:
- C++: `cpp/test.cpp`
- Python: `python/scripts/test_index.py` (main), plus specialized tests
- JavaScript: `javascript/usearch.test.js`
- Rust: tests in `rust/lib.rs`
- Go: `golang/lib_test.go`

**Before commits:** Always run the relevant language test suite.

### Cross-Compilation

Use LLVM/Clang for easy cross-compilation:

```bash
# For ARM64 from x86_64
export TARGET_ARCH="aarch64-linux-gnu"
export BUILD_ARCH="arm64"
cmake -D CMAKE_C_COMPILER_TARGET=${TARGET_ARCH} \
  -D CMAKE_CXX_COMPILER_TARGET=${TARGET_ARCH} \
  -D CMAKE_SYSTEM_PROCESSOR=${BUILD_ARCH} \
  -B build_artifacts
cmake --build build_artifacts --config Release
```

### Submodules

USearch depends on submodules for optional features:
- `simsimd/` - SIMD-optimized distance metrics
- `fp16/` - Half-precision floating-point emulation
- `stringzilla/` - Fast string operations (future use)

Always initialize before builds:
```bash
git submodule update --init --recursive
```

### Build System Conventions

**CMake options prefix:** `USEARCH_BUILD_*`, `USEARCH_USE_*`

**Key flags:**
- `USEARCH_USE_OPENMP` - Parallelize with OpenMP (default OFF)
- `USEARCH_USE_SIMSIMD` - Hardware acceleration (default OFF)
- `USEARCH_USE_JEMALLOC` - Better allocator (default OFF)
- `USEARCH_USE_FP16LIB` - Software half-precision (default ON)

**Python build:** Controlled by `setup.py` via environment variables:
- `USEARCH_USE_SIMSIMD` - Enable SimSIMD (default: ON except Windows)
- `USEARCH_USE_FP16LIB` - Enable FP16 library (default: ON)
- `USEARCH_USE_OPENMP` - Enable OpenMP (default: ON for Linux+GCC)

### File Format

Binary index format starts with 64-byte header (`index_dense_head_t`):
- Magic string: "usearch" (7 bytes)
- Version: major, minor, patch (3x 2 bytes)
- Metric kind, scalar kind, key kind, slot kind (4x 4 bytes)
- Counts: present, deleted, dimensions (3x 8 bytes)
- Multi-vector flag (1 byte)

Compatible across platforms, enabling zero-copy memory mapping.

## Common Patterns

### Adding a New Metric

1. Add enum to `metric_kind_t` in `index.hpp`
2. Implement metric function in `metric_punned_t` class
3. Add to C API enum in `c/usearch.h` (`usearch_metric_kind_t`)
4. Update language bindings to expose new metric

### Performance Debugging

**Useful GDB breakpoints:**
- `__asan::ReportGenericError` - Memory access violations
- `__ubsan::ScopedReport::~ScopedReport` - Undefined behavior
- `usearch_raise_runtime_error` - USearch assertions

**Hardware acceleration check:**
```bash
python -c 'from usearch.index import Index; print(Index(ndim=768, metric="cos", dtype="f16").hardware_acceleration)'
```

### Memory Mapping Large Indexes

USearch supports viewing indexes without loading into RAM:

```python
# Don't load into memory, just map the file
view = Index.restore("index.usearch", view=True)
```

This enables serving billion-scale indexes from disk with minimal memory.

## Release Process

Versioning is controlled by `VERSION` file. Releases use semantic-release configured in `.releaserc`. The CI/CD in `.github/` handles:
- Multi-platform wheel builds (cibuildwheel)
- NPM package publishing
- Crate publishing
- Maven/Gradle JAR creation
- NuGet package

## Important Constraints

1. **Never break the binary format** - Indexes must remain compatible across versions
2. **Single-header must stay standalone** - `index.hpp` should compile without dependencies
3. **Language bindings are independent** - Changes to one shouldn't require rebuilding others
4. **Preserve CMake/setup.py independence** - Python builds don't use CMake
5. **Git submodules are optional** - Core should work without SimSIMD/FP16Lib

## Code Style

- **C++**: `.clang-format` enforced, 120-char lines
- **Python**: Ruff linter, Black formatter (120-char lines)
- **JavaScript**: Standard style
- **Java**: Spotless with AOSP style
- **Swift**: `swift-format` with `.swift-format` config
- **Rust**: `cargo fmt`, `cargo clippy`
- **Go**: `go fmt`, `go vet`
