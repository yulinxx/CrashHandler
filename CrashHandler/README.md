# Crash Handler Library (CrashHandler.dll)

## 功能描述

崩溃处理库，基于 Google Breakpad 实现，提供跨平台（Windows / macOS / Linux）异常捕获和 minidump 生成能力。当应用发生崩溃（如访问违规、空指针、栈溢出等）时，CrashHandler 能够自动捕获异常、生成标准 minidump 文件（`.dmp`），供事后调试和崩溃分析使用。

## 使用方法

### 初始化

在应用 `main()` 函数中**尽量靠前**调用，确保初始化阶段的崩溃也能被捕获：

```cpp
#include "CrashHandler/CrashHandlerDLL.h"

static const char kDumpPath[] = "C:/Temp/MyApp/crashes";
static const char kAppName[] = "MyApp";
static const char kAppVersion[] = "1.0.0";

CrashHandlerConfig config;
CrashHandler_ConfigInit(&config);
config.dumpPath = kDumpPath;
config.appName = kAppName;
config.appVersion = kAppVersion;
config.maxDumpFiles = 10;
config.dumpType = CRASHHANDLER_DUMP_NORMAL;

const int result = CrashHandler_Initialize(&config);
if (result != CRASHHANDLER_OK)
{
    char err[256] = {};
    CrashHandler_GetLastErrorMessage(err, sizeof(err));
    fprintf(stderr, "CrashHandler init failed: %s\n", err);
}
```

### 关闭

在应用退出前调用，释放 Breakpad 资源：

```cpp
CrashHandler_Shutdown();
```

### 写入 minidump

在需要的地方（如异常捕获、断言失败）手动生成 dump 文件：

```cpp
CrashHandler_WriteMinidump();
```

### 设置崩溃回调

崩溃发生时通知用户自定义逻辑：

```cpp
int onCrash(const char* dumpPath, int succeeded, void* userData)
{
    if (succeeded && dumpPath && dumpPath[0] != '\0')
    {
        fprintf(stderr, "Crash dump saved to: %s\n", dumpPath);
    }
    return succeeded;
}

CrashHandler_SetCrashCallback(onCrash, nullptr);
```

### 查询 dump 文件

```cpp
char path[512];
if (CrashHandler_GetLastDumpPath(path, sizeof(path)) == CRASHHANDLER_OK)
{
    // path 为最近一次 dump 路径
}

const int count = CrashHandler_GetDumpFileCount();
for (int i = 0; i < count; ++i)
{
    char dumpPath[512];
    if (CrashHandler_GetDumpFilePath(i, dumpPath, sizeof(dumpPath)) == CRASHHANDLER_OK)
    {
        // dumpPath 为第 i 个 dump 文件
    }
}
```

## 设计框架

### 整体架构

```
┌─────────────────────────────────────────────────┐
│        CrashHandlerDLL.h (标准 C ABI)            │
│  extern "C" + POD，可跨编译器 / 跨 IDE 使用       │
├─────────────────────────────────────────────────┤
│        CrashHandlerEngine (内部 C++ 单例)         │
│  Pimpl 包装，STL 仅在 DLL 内部使用                │
├─────────────────────────────────────────────────┤
│           CrashHandlerImpl (基类)                │
│  公共逻辑：dump 文件管理、回调调度、工具函数       │
├─────────────────────────────────────────────────┤
│  Windows  │   Linux   │   macOS   │  ... 更多平台  │
│  Breakpad │  Breakpad │  Breakpad │              │
└─────────────────────────────────────────────────┘
```

### 核心设计模式

1. **全局单例模式**：`CrashHandlerEngine` 使用单例模式，确保全局仅存在一个崩溃处理器实例
2. **C API Facade**：对外仅暴露 `CrashHandlerDLL.h` 标准 C 接口，禁止 STL 跨 DLL 边界
3. **Pimpl 模式**：平台实现隐藏在 `CrashHandlerImpl` 子类中，公共逻辑与平台逻辑分离
4. **工厂模式**：`createPlatformImpl()` 根据当前平台创建对应的实现实例
5. **回调机制**：C 函数指针回调，崩溃时触发用户自定义逻辑
6. **线程安全设计**：内部使用 `std::mutex` 保证线程安全，采用 `*_nolock` 两层方法模式

### 目录结构

```
CrashHandler/CrashHandler/
├── CMakeLists.txt                      # 库构建配置
├── Include/
│   ├── CrashHandlerAPI.h               # DLL 导出宏定义
│   └── CrashHandler/
│       └── CrashHandlerDLL.h           # 对外标准 C ABI 接口
└── Src/
    ├── CrashHandlerInternal.h          # 内部 C++ 类型（不导出）
    ├── CrashHandlerImpl.h              # 平台实现基类
    ├── CrashHandler.cpp                # 公共逻辑（dump 管理、文件操作）
    ├── CrashHandlerWrapper.cpp         # 内部 Engine 单例
    ├── CrashHandlerDLL.cpp             # 标准 C ABI 导出实现
    └── Platform/
        ├── CrashHandlerWin.cpp         # Windows 平台实现
        ├── CrashHandlerLinux.cpp       # Linux 平台实现
        └── CrashHandlerMac.cpp         # macOS 平台实现
```

## 依赖库

| 依赖 | 说明 | 获取方式 |
|------|------|---------|
| **unofficial-breakpad** | 核心崩溃捕获库（Google Breakpad 的 vcpkg 移植） | 通过 vcpkg 安装 |
| **C++17** | 语言标准 | 编译器支持（MSVC 2017+ / GCC 7+ / Clang 5+） |
| **CMake 3.20+** | 构建系统 | - |

## 依赖库安装方法

### Windows

```powershell
vcpkg install breakpad:x64-windows
```

### Linux

```powershell
vcpkg install breakpad:x64-linux
```

### macOS

```powershell
vcpkg install breakpad:x64-osx
```

## 构建配置

### CMake 集成

在项目根 `CMakeLists.txt` 中添加：

```cmake
add_subdirectory(CrashHandler/CrashHandler)
```

使用方链接：

```cmake
target_include_directories(your_target PRIVATE path/to/CrashHandler/CrashHandler/Include)
target_link_libraries(your_target PRIVATE CrashHandler)
```

> 构建 CrashHandler 本身时 CMake 会自动定义 `CRASHHANDLER_EXPORTS`；消费方只需 `dllimport`（由 `CrashHandlerAPI.h` 自动处理）。

### 平台输出

| 平台 | vcpkg triplet | 输出文件 |
|------|--------------|---------|
| Windows x64 | `x64-windows` | `CrashHandler.dll` / `CrashHandler_d.dll` |
| Linux x64 | `x64-linux` | `libCrashHandler.so` |
| macOS x64/ARM | `x64-osx` / `arm64-osx` | `libCrashHandler.dylib` |

CMake 会根据 `WIN32` / `APPLE` / `UNIX` 自动选择对应平台源文件。

### 符号文件生成

项目提供了 `sanyi_add_debug_symbols()` CMake 函数，自动处理各平台的符号文件：

```cmake
sanyi_add_debug_symbols(${LIB_NAME})
```

| 平台 | 符号格式 | 输出位置 |
|------|---------|---------|
| **Windows** | `.pdb` | `bin_Qt6/<Config>/symbols/` |
| **Linux** | `.debug` (分离) | `<binary_dir>/symbols/` |
| **macOS** | `.dSYM` | `<binary_dir>/symbols/` |

## API 概要

### 版本查询

| 函数 | 说明 |
|------|------|
| `CrashHandler_GetVersion()` | 获取 ABI 版本号（`uint32_t`，主版本号变化表示不兼容） |
| `CrashHandler_GetVersionString()` | 获取版本字符串（如 `"1.0.0"`） |

### 配置初始化

| 函数 | 说明 |
|------|------|
| `CrashHandler_ConfigInit(config)` | 将 `CrashHandlerConfig` 结构体初始化为默认值 |

`CrashHandlerConfig` 关键字段：

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `structSize` | `uint32_t` | `sizeof(CrashHandlerConfig)` | 结构体大小校验，ABI 前向兼容 |
| `dumpPath` | `const char*` | `nullptr` | dump 文件存储目录（**必须设置**） |
| `appName` | `const char*` | `nullptr` | 应用名称，用于 dump 文件名前缀 |
| `appVersion` | `const char*` | `nullptr` | 应用版本号，辅助分析 |
| `appBuildId` | `const char*` | `nullptr` | 构建 ID / Git commit hash |
| `maxDumpFiles` | `int32_t` | `10` | 最大保留 dump 文件数，超出自动清理最旧的 |
| `dumpType` | `CrashHandlerDumpType` | `CRASHHANDLER_DUMP_NORMAL` | dump 详细程度 |
| `enableCrashUpload` | `int32_t` | `0` | 是否启用崩溃上报（预留） |
| `uploadServerUrl` | `const char*` | `nullptr` | 上报服务器地址（预留） |

### 生命周期管理

| 函数 | 说明 |
|------|------|
| `CrashHandler_Initialize(config)` | 初始化崩溃处理器，返回 `CRASHHANDLER_OK` 表示成功 |
| `CrashHandler_Shutdown()` | 关闭崩溃处理器，释放资源 |
| `CrashHandler_IsInitialized()` | 查询是否已初始化 |

### minidump 操作

| 函数 | 说明 |
|------|------|
| `CrashHandler_WriteMinidump()` | 手动触发 minidump 写入 |
| `CrashHandler_GetLastDumpPath(buffer, size)` | 获取最近一次 dump 文件路径 |
| `CrashHandler_GetDumpFileCount()` | 获取当前 dump 文件夹中的 dump 文件数量 |
| `CrashHandler_GetDumpFilePath(index, buffer, size)` | 获取指定索引的 dump 文件路径 |
| `CrashHandler_CleanOldDumps()` | 清理超出 `maxDumpFiles` 数量的旧 dump 文件，返回删除数量 |

### 回调设置

| 函数 | 说明 |
|------|------|
| `CrashHandler_SetCrashCallback(callback, userData)` | 设置崩溃回调函数 |

回调函数签名：

```cpp
int CrashHandlerCallback(const char* dumpPath, int succeeded, void* userData);
```

- `dumpPath`：UTF-8 dump 文件路径（可能为空字符串）
- `succeeded`：非 0 表示 dump 写入成功
- `userData`：用户自定义数据
- 返回值：非 0 沿用用户结果，0 沿用 Breakpad 默认行为

### 错误查询

| 函数 | 说明 |
|------|------|
| `CrashHandler_GetLastErrorMessage(buffer, size)` | 获取最近一次错误信息（线程局部存储） |

### 错误码

| 错误码 | 值 | 说明 |
|--------|----|------|
| `CRASHHANDLER_OK` | `0` | 操作成功 |
| `CRASHHANDLER_ERR_INVALID_ARG` | `-1` | 参数无效 |
| `CRASHHANDLER_ERR_NULL_POINTER` | `-2` | 空指针 |
| `CRASHHANDLER_ERR_NOT_INITIALIZED` | `-3` | 未初始化 |
| `CRASHHANDLER_ERR_ALREADY_INITIALIZED` | `-4` | 已初始化 |
| `CRASHHANDLER_ERR_IO` | `-5` | IO 错误 |
| `CRASHHANDLER_ERR_BUFFER_TOO_SMALL` | `-6` | 缓冲区过小 |
| `CRASHHANDLER_ERR_OUT_OF_RANGE` | `-7` | 索引越界 |
| `CRASHHANDLER_ERR_VERSION_MISMATCH` | `-9` | ABI 版本不匹配 |
| `CRASHHANDLER_ERR_INTERNAL` | `-99` | 内部错误 |

### DumpType 枚举

| 枚举值 | 说明 | 适用场景 |
|--------|------|---------|
| `CRASHHANDLER_DUMP_NORMAL` | 基本信息（线程、栈、模块） | 日常使用 |
| `CRASHHANDLER_DUMP_WITH_DATA_SEGS` | 包含数据段 | 需要全局变量数据时 |
| `CRASHHANDLER_DUMP_WITH_FULL_MEMORY` | 完整进程内存 | 本地深度调试 |
| `CRASHHANDLER_DUMP_WITH_HANDLE_DATA` | 包含句柄信息 | 句柄泄漏分析 |
| `CRASHHANDLER_DUMP_WITH_THREAD_INFO` | 详细线程信息 | 线程状态分析 |
| `CRASHHANDLER_DUMP_WITH_FULL_MEMORY_INFO` | 内存状态信息 | 内存问题分析 |

## 版本信息

- **当前版本**：1.0.0
- **ABI 版本宏**：`CRASHHANDLER_VERSION`（值为 `0x00010000`）
- **版本字符串**：`"1.0.0"`