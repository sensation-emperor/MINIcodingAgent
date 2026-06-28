# Build Instructions for MINI Coding Agent

## Quick Start

### Prerequisites
- CMake 3.20+
- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- vcpkg package manager

### Step 1: Install Dependencies via vcpkg

```bash
# Clone and bootstrap vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # On Windows: .\bootstrap-vcpkg.bat

# Install required packages
./vcpkg install fmt spdlog nlohmann-json boost-system boost-thread
```

### Step 2: Build the Project

```bash
cd /workspace
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . -j$(nproc)
```

### Step 3: Run

```bash
./mini_coding_agent
```

## Alternative: Using System Packages (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    g++ \
    libfmt-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libboost-all-dev \
    libgtest-dev

mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

## Current Status

The project skeleton is now in place with:

✅ Core kernel infrastructure
✅ Event bus system
✅ Logging system (using spdlog)
✅ Configuration management
✅ Basic provider interfaces (Anthropic, OpenAI, Ollama)
✅ Stub implementations for all major subsystems
✅ CMake build system
✅ vcpkg manifest for dependencies

## Next Implementation Priorities

1. **Model Provider Integration** - Connect to actual AI backends
2. **Tool System** - Implement file operations, terminal execution, etc.
3. **Agent Framework** - Build the agent orchestration layer
4. **Planner Engine** - Implement task graph generation
5. **Context Engine** - Build context assembly and compression
6. **Memory System** - Implement short and long-term memory
7. **Repository Indexing** - AST parsing and symbol tracking

## Architecture Overview

See README.md for the complete architecture documentation including:
- High-level system design
- Component interactions
- Execution flow
- Tool system design
- Memory architecture
