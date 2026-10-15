cmake_minimum_required(VERSION 4.3)

# ============================================================================
# CrashHandler —— 独立构建入口
# ============================================================================
# 集成构建：根 CMakeLists.txt → add_subdirectory(CrashHandler/CrashHandler)（内层）。
# 独立构建：本文件提供 project() 与环境配置，把 CrashHandler 单独拉出来开发 / 跑测试。
#
# 三条设计约束同时成立：
#
#   1) 复用仓库统一配置，不自己手搓一套，避免与主工程漂移：
#        ../Config.cmake               —— vcpkg / Qt 路径、MSVC 运行库、模块开关
#        ../CMake/StandaloneInit.cmake —— SANYI_ROOT、编译选项、GTest 查找、兄弟模块拉入
#      与 FileIO / Log / License / Utility / Nesting 的独立入口保持同一套写法。
#
#   2) 可脱离主程序单独编译：唯一依赖是 vcpkg 的 breakpad（unofficial-breakpad），
#      没有源码级兄弟 DLL 需要拉入。breakpad 缺失时内层只告警并跳过建 target。
#
#   3) 测试由内层 CrashHandler/CrashHandler/CMakeLists.txt 在
#      BUILD_CRASHHANDLER_TESTS=ON 时挂载，本文件不得重复 add_subdirectory(Test)。
#
# 用法：
#   cmake -S CrashHandler -B CrashHandler/build
#   cmake --build CrashHandler/build --config Release
#   ctest --test-dir CrashHandler/build -C Release
#   cmake -S CrashHandler -B CrashHandler/build -DBUILD_CRASHHANDLER_TESTS=OFF  # 只编库
#   cmake -S CrashHandler -B CrashHandler/build -DBUILD_CRASHHANDLER_EXAMPLE=OFF # 不编示例
# ============================================================================

include(${CMAKE_CURRENT_LIST_DIR}/../Config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../CMake/StandaloneInit.cmake)

project(SanYiCrashHandler
    VERSION 1.0.0
    DESCRIPTION "SanYi CAD Crash Handler Library (Breakpad minidump)"
    LANGUAGES CXX
)

# MSVC 的 /W4 等选项必须在 project() 之后才能正确判定编译器
sanyi_setup_compiler_options()

# 示例目标仅存在于独立构建（内层不负责 Example）
option(BUILD_CRASHHANDLER_EXAMPLE "Build CrashHandler usage example" ON)

# --- 测试 -------------------------------------------------------------------
sanyi_only_own_tests(BUILD_CRASHHANDLER_TESTS)
if(BUILD_CRASHHANDLER_TESTS)
    sanyi_find_gtest()
endif()

# 内层：创建 CrashHandler DLL、挂载测试（BUILD_CRASHHANDLER_TESTS 时）、输出状态
add_subdirectory(CrashHandler)

# breakpad 缺失时内层直接 return()（只告警、不建 target），示例随之跳过
if(BUILD_CRASHHANDLER_EXAMPLE AND TARGET CrashHandler)
    add_subdirectory(Example)
endif()

# --- 独立构建摘要 -----------------------------------------------------------
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  [Standalone: ${PROJECT_NAME}]")
message(STATUS "========================================")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build Tests:    ${BUILD_CRASHHANDLER_TESTS}")
message(STATUS "  Build Example:  ${BUILD_CRASHHANDLER_EXAMPLE}")
if(TARGET CrashHandler)
    message(STATUS "  Breakpad:       FOUND")
else()
    message(STATUS "  Breakpad:       NOT FOUND (module skipped)")
endif()
message(STATUS "  Output:         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
message(STATUS "")
