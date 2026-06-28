# Getting Started with MINI Coding Agent

## Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+, or MSVC 2022+)
- CMake 3.20 or higher
- vcpkg or Conan package manager

## Building the Project

### Using vcpkg

1. Install vcpkg if you haven't already:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

2. Build the project:
```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

### Using Conan

1. Create a `conanfile.txt`:
```
[requires]
fmt/10.2.1
spdlog/1.13.0
nlohmann_json/3.11.3
boost/1.84.0

[generators]
CMakeDeps
CMakeToolchain
```

2. Build:
```bash
conan install . --output-folder=build --build=missing
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
```

## Running

```bash
./mini_coding_agent --config config.json
```

## Project Structure

```
AIOS/
├── src/
│   ├── kernel/          # Core kernel orchestration
│   ├── scheduler/       # Task scheduling
│   ├── planner/         # Task planning engine
│   ├── memory/          # Memory management
│   ├── context/         # Context building
│   ├── retrieval/       # Information retrieval
│   ├── prompt/          # Prompt compilation
│   ├── agents/          # Agent management
│   ├── tools/           # Tool registry
│   ├── execution/       # Code execution
│   ├── sandbox/         # Sandboxing
│   ├── verification/    # Verification engine
│   ├── reflection/      # Self-reflection
│   ├── repository/      # Repository indexing
│   ├── parser/          # Code parsing
│   ├── providers/       # AI model providers
│   └── ...
├── tests/
├── docs/
├── CMakeLists.txt
└── README.md
```

## Next Steps

1. Configure your AI provider in `config.json`
2. Implement additional tools in `src/tools/`
3. Extend the agent system in `src/agents/`
4. Add custom planners in `src/planner/`
