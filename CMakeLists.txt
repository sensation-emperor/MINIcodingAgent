cmake_minimum_required(VERSION 3.20)
project(MINIcodingAgent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable native CPU optimizations for SIMD instructions (AVX2, etc.)
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    add_compile_options(-march=native)
else()
    # Fallback to SSE4.2 if march=native is not available
    check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
    if(COMPILER_SUPPORTS_SSE42)
        add_compile_options(-msse4.2)
    endif()
endif()

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

# Optional: Find Qt6/Qt5 for GUI
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
    endif()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Core library sources
set(CORE_SOURCES
    src/kernel/Kernel.cpp
    src/scheduler/scheduler.cpp
    src/planner/planner.cpp
    src/taskgraph/taskgraph.cpp
    src/parser/ASTParser.cpp
    src/repository/RepositoryIndex.cpp
    src/database/DatabaseEngine.cpp
    src/vector/VectorStore.cpp
    src/knowledge/KnowledgeGraph.cpp
    src/memory/memory.cpp
    src/context/ContextEngine.cpp
    src/tools/ToolRegistry.cpp
    src/agents/agents.cpp
    src/agents/SpecializedAgents.cpp
    src/agents/Orchestrator.cpp
    src/agents/AgentToolParser.cpp
    src/events/EventBus.cpp
    src/logging/Logger.cpp
    src/providers/ModelProvider.cpp
    src/providers/ModelRouter.cpp
    src/network/HttpClient.cpp
    src/gui/Theme.cpp
    src/config/ConfigManager.cpp
    src/cli/ReplClient.cpp
    src/cli/Repl.cpp
    src/cli/CliSession.cpp
    src/cli/CommandRegistry.cpp
    src/cli/LineReader.cpp
    src/cli/NonInteractiveRunner.cpp
    src/cli/SlashCommand.cpp
    src/cli/TerminalRenderer.cpp
    src/workspace/WorkspaceManager.cpp
    src/workspace/WorkspaceTools.cpp
    src/workspace/BranchSandbox.cpp
    src/workspace/GitWorktree.cpp
    src/workspace/PathContainment.cpp
    src/workspace/SnapshotManager.cpp
    src/testing/TestGenerator.cpp
    src/testing/TestRunner.cpp
    src/testing/TestingManager.cpp
    src/testing/TestingTools.cpp
    src/testing/CoverageAnalyzer.cpp
    src/testing/DiagnosticsEngine.cpp
)

# GUI-specific core modules (only included when BUILD_GUI is ON)
if(BUILD_GUI)
    list(APPEND CORE_SOURCES
        src/core/ConversationManager.cpp
        src/core/MetricsCollector.cpp
        src/core/ModelLibraryManager.cpp
        src/core/ModelRouter.cpp
        src/core/TaskPlanner.cpp
        src/core/WorkspaceProjectManager.cpp
    )
endif()

if(BUILD_GUI)
    list(APPEND CORE_SOURCES
        src/gui/FloatingIslandNavBar.cpp
        src/gui/CommandPillWidget.cpp
        src/gui/ChatView.cpp
        src/gui/DiffViewer.cpp
        src/gui/TaskGraphView.cpp
        src/gui/TerminalWidget.cpp
        src/gui/MainWindow.cpp
    )
endif()

# Create core library
add_library(aios_core STATIC ${CORE_SOURCES})

target_link_libraries(aios_core PUBLIC
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Boost::system
    Boost::thread
)

if(WIN32)
    target_link_libraries(aios_core PUBLIC ws2_32)
endif()

if(CPR_FOUND)
    target_link_libraries(aios_core PUBLIC cpr::cpr)
    target_compile_definitions(aios_core PRIVATE HAS_CPR)
endif()

if(BUILD_GUI)
    if(Qt6_FOUND)
        target_link_libraries(aios_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
    else()
        target_link_libraries(aios_core PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets)
    endif()
    target_compile_definitions(aios_core PUBLIC BUILD_GUI)
endif()

# Main executable
add_executable(mini_coding_agent src/main.cpp)
target_link_libraries(mini_coding_agent PRIVATE aios_core)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    
    if(GTest_FOUND)
        add_executable(aios_tests 
            tests/test_main.cpp 
            tests/test_agents.cpp 
            tests/test_providers.cpp
            tests/test_taskgraph.cpp
            tests/test_gui.cpp
            tests/test_repository.cpp
            tests/test_memory.cpp
            tests/benchmark_aios.cpp
            tests/test_developer_suite.cpp
        )
        target_link_libraries(aios_tests PRIVATE aios_core GTest::gtest_main GTest::gtest)
        add_test(NAME AIOS_Tests COMMAND aios_tests)
    endif()
endif()

# Installation
install(TARGETS mini_coding_agent DESTINATION bin)
install(DIRECTORY src/ DESTINATION include/aios FILES_MATCHING PATTERN "*.h")
