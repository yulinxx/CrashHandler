cmake_minimum_required(VERSION 4.3)

# --- 模块元信息 ---
set(LIB_NAME "CrashHandler")
set(LIB_FOLDER "Core")
set(LIB_DESCRIPTION "Crash Handler Library")
set(LIB_EXPORT_MACRO "CRASHHANDLER_EXPORTS")

# --- 版本（按模块独立）---
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

# --- 路径 ---
if(NOT SANYI_ROOT)
    get_filename_component(SANYI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()
include("${SANYI_ROOT}/CMake/SanYiPaths.cmake")
include("${SANYI_ROOT}/CMake/Utils.cmake")

# --- 源文件 ---
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.c"
)
file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp"
)
list(FILTER LIB_SOURCES EXCLUDE REGEX "${SANYI_SOURCE_EXCLUDE_REGEX}")
list(FILTER LIB_HEADERS EXCLUDE REGEX "${SANYI_HEADER_EXCLUDE_REGEX}")

# --- 第三方依赖 ---
find_package(unofficial-breakpad CONFIG REQUIRED)

# --- 平台相关源文件 ---
list(REMOVE_ITEM LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/Platform/CrashHandlerWin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/Platform/CrashHandlerMac.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/Platform/CrashHandlerLinux.cpp"
)
if(WIN32)
    list(APPEND LIB_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Src/Platform/CrashHandlerWin.cpp"
    )
elseif(APPLE)
    list(APPEND LIB_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Src/Platform/CrashHandlerMac.cpp"
    )
elseif(UNIX)
    list(APPEND LIB_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Src/Platform/CrashHandlerLinux.cpp"
    )
endif()

# 统一入口已无条件生成调试符号（version.rc / PDB-DWARF），
# 该 option 仅为兼容旧构建参数保留，实际始终生效。
option(SANYI_CRASHHANDLER_SYMBOLS "Generate debug symbols for CrashHandler" ON)

# --- 统一入口创建 DLL ---
sanyi_add_shared_library(${LIB_NAME}
    FOLDER "${LIB_FOLDER}"
    DESCRIPTION "${LIB_DESCRIPTION}"
    EXPORT_MACRO "${LIB_EXPORT_MACRO}"
    VERSION_MAJOR ${VERSION_MAJOR}
    VERSION_MINOR ${VERSION_MINOR}
    VERSION_PATCH ${VERSION_PATCH}
    SOURCES ${LIB_SOURCES}
    HEADERS ${LIB_HEADERS}
    PUBLIC_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    PRIVATE_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Src"
    LINK_LIBRARIES
        PRIVATE
            unofficial::breakpad::libbreakpad
            unofficial::breakpad::libbreakpad_client
)

message(STATUS "----------------------------------------")

sanyi_export_imported_target(${LIB_NAME}
    LIB_NAME "${LIB_NAME}"
    PUBLIC_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    INTERFACE_LINK_LIBRARIES
)