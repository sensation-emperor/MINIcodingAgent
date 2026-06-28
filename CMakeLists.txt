cmake_minimum_required(VERSION 3.20)
project(MINIcodingAgent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_GUI "Build Qt GUI" OFF)

# Find required packages
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

# Optional: Find CPR for HTTP
find_package(CPR QUIET)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Core library sources
set(CORE_SOURCES
    src/kernel/Kernel.cpp
    src/scheduler/scheduler.cpp
    src/planner/planner.cpp
    src/memory/memory.cpp
    src/context/ContextEngine.cpp
    src/retrieval/RetrievalEngine.cpp
    src/prompt/PromptCompiler.cpp
    src/agents/agents.cpp
    src/tools/ToolRegistry.cpp
    src/execution/ExecutionEngine.cpp
    src/sandbox/SandboxManager.cpp
    src/verification/VerificationEngine.cpp
    src/reflection/ReflectionEngine.cpp
    src/repository/RepositoryIndex.cpp
    src/parser/ParserManager.cpp
    src/lsp/LSPClient.cpp
    src/git/GitManager.cpp
    src/filesystem/FileSystemWatcher.cpp
    src/terminal/TerminalExecutor.cpp
    src/events/EventBus.cpp
    src/logging/Logger.cpp
    src/metrics/MetricsCollector.cpp
    src/settings/SettingsManager.cpp
    src/providers/ModelProvider.cpp
    src/providers/AnthropicProvider.cpp
    src/providers/OpenAIProvider.cpp
    src/providers/OllamaProvider.cpp
    src/models/Message.cpp
    src/embeddings/EmbeddingEngine.cpp
    src/vector/VectorDatabase.cpp
    src/database/SQLiteDB.cpp
    src/cache/CacheManager.cpp
    src/taskgraph/TaskGraph.cpp
    src/workflow/WorkflowEngine.cpp
    src/knowledge/KnowledgeGraph.cpp
    src/network/HTTPClient.cpp
    src/config/ConfigManager.cpp
)

# Create core library
add_library(aios_core STATIC ${CORE_SOURCES})

target_link_libraries(aios_core PUBLIC
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Boost::system
    Boost::thread
)

if(CPR_FOUND)
    target_link_libraries(aios_core PUBLIC cpr::cpr)
    target_compile_definitions(aios_core PRIVATE HAS_CPR)
endif()

# Main executable
add_executable(mini_coding_agent src/main.cpp)
target_link_libraries(mini_coding_agent PRIVATE aios_core)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    
    if(GTest_FOUND)
        add_executable(aios_tests tests/test_main.cpp)
        target_link_libraries(aios_tests PRIVATE aios_core GTest::gtest_main)
        add_test(NAME AIOS_Tests COMMAND aios_tests)
    endif()
endif()

# Installation
install(TARGETS mini_coding_agent DESTINATION bin)
install(DIRECTORY src/ DESTINATION include/aios FILES_MATCHING PATTERN "*.h")
