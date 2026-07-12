# CrashHandler - 跨平台崩溃捕获模块

基于 Google Breakpad 的跨平台崩溃捕获模块，支持 Windows / Linux / macOS，
自动生成 minidump 文件，用于事后崩溃分析。

---

## 目录结构

```
CrashHandler/
├── CMakeLists.txt                          # 顶层 CMake
├── README.md                               # 本文档
└── CrashHandler/
    ├── CMakeLists.txt                      # 库构建配置
    ├── Include/
    │   ├── CrashHandlerAPI.h               # DLL 导出宏
    │   └── CrashHandler/
    │       └── CrashHandlerDLL.h           # 对外标准 C ABI 接口
    └── Src/
        ├── CrashHandlerInternal.h          # 内部 C++ 类型（不导出）
        ├── CrashHandlerImpl.h              # 平台实现基类（内部）
        ├── CrashHandler.cpp                # 公共逻辑（dump 管理、文件操作）
        ├── CrashHandlerWrapper.cpp         # 内部 Engine 单例
        ├── CrashHandlerDLL.cpp             # 标准 C ABI 导出实现
        └── Platform/
            ├── CrashHandlerWin.cpp         # Windows 平台实现
            ├── CrashHandlerLinux.cpp       # Linux 平台实现
            └── CrashHandlerMac.cpp         # macOS 平台实现
```

---

## 设计架构

### 整体分层

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

1. **C ABI 边界**：对外仅暴露 `CrashHandlerDLL.h`，禁止 STL 跨 DLL 边界
2. **内部 Engine 单例**：`CrashHandlerEngine` 仅在 DLL 内部使用
3. **Pimpl 模式**：平台实现隐藏在 `CrashHandlerImpl` 子类中
4. **工厂模式**：`createPlatformImpl()` 根据平台创建对应实现
5. **回调机制**：C 函数指针回调，崩溃时触发用户逻辑

### 线程安全设计

CrashHandler 内部使用 `std::mutex` 保证线程安全，采用 **`_nolock` 两层方法** 的设计模式：

- **公有方法**：加锁 → 调用 `_nolock` 内部版本
- **保护方法 (`*_nolock`)**：不加锁，供已持有锁的内部代码调用

这样做的好处：
- 避免同一线程重复加锁导致的崩溃（`std::mutex` 不是递归锁）
- 减少不必要的锁开销（内部调用直接走 `_nolock` 版本）
- 代码层次清晰，调用者一眼能看出"这个方法假设已持有锁"

### 为什么用 Google Breakpad

| 特性 | 说明 |
|------|------|
| **跨平台** | Windows / Linux / macOS / Android / iOS 全覆盖 |
| **统一格式** | 所有平台生成标准 minidump (`.dmp`)，分析工具统一 |
| **成熟稳定** | 被 Firefox、Chrome（旧版）、Thunderbird 等大型软件验证 |
| **体积小** | minidump 通常几十 KB ~ 几 MB，方便上传和存储 |
| **符号化** | 配合符号文件 (`.pdb` / `.dSYM` / `.debug`) 可还原行号 |
| **vcpkg 集成** | vcpkg 已有 breakpad port，集成方便 |

### 与其他模块的关系

- **SanYiCAD Main**：通过 `CrashHandlerBootstrap` 在 `QApplication` 创建后立即初始化
- **AppPathManager**：提供跨平台可写的 `crashDumpsDir()` 路径
- **Log**：启动后扫描历史 dump 并写入日志；崩溃回调中仅使用 `fprintf(stderr)`（崩溃安全）
- **Qt**：SanYiCAD 在设置 `organizationName` / `applicationName` 后初始化，确保 `QStandardPaths` 路径正确
- **第三方集成**：只需链接 `CrashHandler.dll` 并包含 `CrashHandlerDLL.h`，无需依赖 Qt / Utility

---

## SanYiCAD 集成说明

SanYiCAD 已完成 CrashHandler 接入，启动流程如下：

```
main()
  └─ CADApplicationRuntime 构造
       ├─ 创建 QApplication，设置 org/app 元信息
       ├─ CrashHandlerBootstrap::initialize()   ← 崩溃捕获启用
       └─ ...
  └─ CADApplicationRuntime::run()
       ├─ AppInitializer::initialize()
       │    └─ CrashHandlerBootstrap::logPendingDumps()  ← 记录历史 dump
       └─ ...
  └─ 析构
       └─ CrashHandlerBootstrap::shutdown()
```

相关源码：

| 文件 | 职责 |
|------|------|
| `Main/Src/Common/CrashHandlerBootstrap.h/.cpp` | 封装 C API，管理配置字符串生命周期 |
| `Main/Src/Common/AppPathManager.cpp` | `crashDumpsDir()` 跨平台路径 |
| `Main/Src/Runtime/CADApplicationRuntime.cpp` | 初始化 / 关闭 CrashHandler |

### 各平台 dump 存储路径

路径由 `QStandardPaths::AppLocalDataLocation + "/crashes"` 决定（需先设置 Qt 组织名与应用名）：

| 平台 | 典型路径 |
|------|---------|
| **Windows** | `C:/Users/<user>/AppData/Local/SanYi/SanYiCAD/crashes/` |
| **macOS** | `~/Library/Application Support/SanYi/SanYiCAD/crashes/` |
| **Linux** | `~/.local/share/SanYi/SanYiCAD/crashes/` |

> 若 `QStandardPaths` 不可用，回退到 `<exe目录>/crashes/`。

---

## 使用说明

### 1. 第三方 / 跨 IDE 集成（标准 C API）

在 `main()` 函数**尽量靠前**初始化（确保初始化阶段崩溃也能捕获）：

```cpp
#include "CrashHandler/CrashHandlerDLL.h"

static int onCrash(const char* dumpPath, int succeeded, void* /*userData*/)
{
    if (succeeded && dumpPath && dumpPath[0] != '\0')
    {
        fprintf(stderr, "Crash dump saved to: %s\n", dumpPath);
    }
    return succeeded;
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // 配置必须使用持久化的字符串（生命周期覆盖 Initialize 调用）
    static const char kDumpPath[] = "C:/Temp/MyApp/crashes";  // 或各平台对应路径
    static const char kAppName[] = "MyApp";
    static const char kAppVersion[] = "1.0.0";

    CrashHandlerConfig config;
    CrashHandler_ConfigInit(&config);
    config.dumpPath = kDumpPath;
    config.appName = kAppName;
    config.appVersion = kAppVersion;
    config.maxDumpFiles = 10;
    config.dumpType = CRASHHANDLER_DUMP_NORMAL;

    CrashHandler_SetCrashCallback(onCrash, nullptr);
    const int initResult = CrashHandler_Initialize(&config);
    if (initResult != CRASHHANDLER_OK)
    {
        char err[256] = {};
        CrashHandler_GetLastErrorMessage(err, sizeof(err));
        fprintf(stderr, "CrashHandler init failed: %s\n", err);
    }

    // ... 后续业务逻辑

    CrashHandler_Shutdown();
    return 0;
}
```

**CMake 链接：**

```cmake
target_include_directories(your_target PRIVATE path/to/CrashHandler/CrashHandler/Include)
target_link_libraries(your_target PRIVATE CrashHandler)
```

**版本检查（推荐）：**

```cpp
if (CrashHandler_GetVersion() != CRASHHANDLER_VERSION)
{
    // ABI 版本不匹配，拒绝加载
}
```

### 2. SanYiCAD 内部用法

SanYiCAD 内部无需直接调用 C API，使用 `CrashHandlerBootstrap` 即可：

```cpp
#include "CrashHandlerBootstrap.h"

// 已在 CADApplicationRuntime 中自动调用，一般无需重复初始化
CrashHandlerBootstrap::initialize(appName, appVersion);
CrashHandlerBootstrap::logPendingDumps();  // 日志初始化后
CrashHandlerBootstrap::shutdown();
```

手动触发 dump（任意模块均可）：

```cpp
#include "CrashHandler/CrashHandlerDLL.h"
CrashHandler_WriteMinidump();
```

### 3. 配置项详解

`CrashHandlerConfig` 结构体（C POD，见 `CrashHandlerDLL.h`）：

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `structSize` | `uint32_t` | `sizeof(CrashHandlerConfig)` | 结构体大小校验，ABI 前向兼容 |
| `dumpPath` | `const char*` | `nullptr` | dump 文件存储目录（必须设置） |
| `appName` | `const char*` | `nullptr` | 应用名称，用于 dump 文件名前缀 |
| `appVersion` | `const char*` | `nullptr` | 应用版本号，辅助分析 |
| `appBuildId` | `const char*` | `nullptr` | 构建 ID / Git commit hash |
| `maxDumpFiles` | `int32_t` | `10` | 最大保留 dump 文件数，超出自动清理最旧的 |
| `dumpType` | `CrashHandlerDumpType` | `CRASHHANDLER_DUMP_NORMAL` | dump 详细程度（见下文） |
| `enableCrashUpload` | `int32_t` | `0` | 是否启用崩溃上报（预留） |
| `uploadServerUrl` | `const char*` | `nullptr` | 上报服务器地址（预留） |

### 4. DumpType 枚举

C API 使用 `CrashHandlerDumpType`（底层类型 `int32_t`）：

| C 枚举值 | 说明 | 体积 | 适用场景 |
|---------|------|------|---------|
| `CRASHHANDLER_DUMP_NORMAL` | 基本信息（线程、栈、模块） | 小 | 日常使用，上报场景 |
| `CRASHHANDLER_DUMP_WITH_DATA_SEGS` | 包含数据段 | 中 | 需要全局变量数据时 |
| `CRASHHANDLER_DUMP_WITH_FULL_MEMORY` | 完整进程内存 | 大 | 本地深度调试 |
| `CRASHHANDLER_DUMP_WITH_HANDLE_DATA` | 包含句柄信息 | 小 | 句柄泄漏分析 |
| `CRASHHANDLER_DUMP_WITH_THREAD_INFO` | 详细线程信息 | 小 | 线程状态分析 |
| `CRASHHANDLER_DUMP_WITH_FULL_MEMORY_INFO` | 内存状态信息 | 中 | 内存问题分析 |

> **建议**：默认用 `CRASHHANDLER_DUMP_NORMAL`，需要深度分析时切到 `CRASHHANDLER_DUMP_WITH_FULL_MEMORY`。

### 5. 手动触发 Dump

在需要的地方（如异常捕获、断言失败）手动生成 dump：

```cpp
#include "CrashHandler/CrashHandlerDLL.h"

void someFunction()
{
    try {
        // ... 可能崩溃的代码
    } catch (...) {
        CrashHandler_WriteMinidump();
        throw;
    }
}
```

### 6. 获取 Dump 文件列表

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

### 7. 清理旧 Dump

```cpp
const int removed = CrashHandler_CleanOldDumps();
```

---

## 编译要求

### 依赖

| 依赖 | 说明 | 获取方式 |
|------|------|---------|
| **Google Breakpad** | 核心崩溃捕获库 | vcpkg: `breakpad` |
| **C++17** | 语言标准 | 编译器支持 |
| **CMake 3.20+** | 构建系统 | - |

### vcpkg 安装

```powershell
# Windows
vcpkg install breakpad:x64-windows

# Linux
vcpkg install breakpad:x64-linux

# macOS
vcpkg install breakpad:x64-osx
```

### CMake 集成

本项目已在根 `CMakeLists.txt` 中添加：

```cmake
add_subdirectory(CrashHandler/CrashHandler)
```

使用方链接：

```cmake
target_link_libraries(your_target PRIVATE CrashHandler)
target_compile_definitions(your_target PRIVATE ...)  # 不要定义 CRASHHANDLER_EXPORTS
```

> 构建 CrashHandler 本身时 CMake 会自动定义 `CRASHHANDLER_EXPORTS`；消费方只需 `dllimport`（由 `CrashHandlerAPI.h` 自动处理）。

### 多平台编译

| 平台 | vcpkg triplet | 输出 |
|------|--------------|------|
| Windows x64 | `x64-windows` | `CrashHandler.dll` / `CrashHandler_d.dll` |
| Linux x64 | `x64-linux` | `libCrashHandler.so` |
| macOS x64/ARM | `x64-osx` / `arm64-osx` | `libCrashHandler.dylib` |

CMake 会根据 `WIN32` / `APPLE` / `UNIX` 自动选择对应平台源文件（`CrashHandlerWin.cpp` / `CrashHandlerMac.cpp` / `CrashHandlerLinux.cpp`）。

### 符号文件生成

项目提供了 `sanyi_add_debug_symbols()` CMake 函数，自动处理各平台的符号文件：

```cmake
# 在目标上调用
sanyi_add_debug_symbols(${app_name})
```

各平台符号文件输出：

| 平台 | 符号格式 | 输出位置 |
|------|---------|---------|
| **Windows** | `.pdb` | `bin_Qt6/<Config>/symbols/` |
| **Linux** | `.debug` (分离) | `<binary_dir>/symbols/` |
| **macOS** | `.dSYM` | `<binary_dir>/symbols/` |

> **Release 模式**：Windows 下也会生成 PDB（`/Zi + /DEBUG`），同时保留代码优化。

---

## Dump 分析指南

### Windows 平台

#### 方法一：Visual Studio（推荐）

1. 用 Visual Studio 打开 `.dmp` 文件
2. 设置符号路径（工具 -> 选项 -> 调试 -> 符号）
3. 点击"使用 仅限本机 进行调试"
4. 可以看到调用栈、线程、变量等信息

#### 方法二：Breakpad 命令行工具

```bash
# 1. 生成符号文件
dump_syms SanYiCAD.pdb > SanYiCAD.sym

# 2. 创建符号目录结构
mkdir -p symbols/SanYiCAD.pdb/<hash>/
cp SanYiCAD.sym symbols/SanYiCAD.pdb/<hash>/

# 3. 分析 minidump
minidump_stackwalk crash.dmp symbols/ > stacktrace.txt
```

### Linux 平台

```bash
# 1. 生成符号文件
dump_syms SanYiCAD > SanYiCAD.sym

# 2. 分析
minidump_stackwalk crash.dmp symbols/ > stacktrace.txt
```

### macOS 平台

```bash
# 1. 从 dSYM 生成符号
dump_syms -g SanYiCAD.dSYM/Contents/Resources/DWARF/SanYiCAD > SanYiCAD.sym

# 2. 分析
minidump_stackwalk crash.dmp symbols/ > stacktrace.txt
```

---

## 后续扩展建议

### 一、崩溃上报系统

当需要从用户端收集崩溃数据时，可按以下方案扩展。

#### 1. 客户端上报实现

在 `CrashHandler` 模块中添加上报逻辑，建议使用单独的上传进程（避免崩溃进程状态不稳定）。

**方案 A：进程内上传（简单但有风险）**

```cpp
// 在 CrashHandlerWin.cpp 的 minidumpCallback 中
static bool minidumpCallback(...)
{
    // ... 保存 dump 路径

    // 注意：崩溃回调中内存可能已破坏，尽量做简单操作
    // 建议：只保存 dump 路径，由正常启动时检查并上传
    return succeeded;
}
```

**方案 B：启动时检查并上传（推荐，更安全）**

```cpp
#include "CrashHandler/CrashHandlerDLL.h"

void checkAndUploadPendingDumps()
{
    const int count = CrashHandler_GetDumpFileCount();
    for (int i = 0; i < count; ++i)
    {
        char dumpPath[512] = {};
        if (CrashHandler_GetDumpFilePath(i, dumpPath, sizeof(dumpPath)) != CRASHHANDLER_OK)
        {
            continue;
        }
        // uploadCrashDump(dumpPath);
        // markAsUploaded(dumpPath);
    }
}
```

**上传协议示例：**

```http
POST /api/v1/crash-report HTTP/1.1
Host: crash.example.com
Content-Type: multipart/form-data

dump_file: <binary dmp file>
app_name: SanYiCAD
app_version: 1.0.0
os: Windows 10
os_version: 10.0.19045
cpu: AMD64
build_id: abc123def
```

#### 2. 服务端架构

```
                    ┌─────────────────────┐
                    │   Load Balancer     │
                    └─────────┬───────────┘
                              │
                    ┌─────────▼───────────┐
                    │  API Gateway / Nginx │
                    └─────────┬───────────┘
                              │
           ┌──────────────────┼──────────────────┐
           ▼                  ▼                  ▼
    ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
    │ 上传服务    │   │ 上传服务    │   │ 上传服务    │
    │ (接收dump)  │   │ (接收dump)  │   │ (接收dump)  │
    └──────┬──────┘   └──────┬──────┘   └──────┬──────┘
           │                 │                 │
           └─────────────────┼─────────────────┘
                             ▼
                    ┌─────────────────┐
                    │   对象存储      │
                    │  (保存 dmp 文件) │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  消息队列       │
                    │  (Kafka/RabbitMQ)│
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  符号化处理服务 │
                    │ minidump-processor │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │   数据库        │
                    │  (崩溃统计)     │
                    └─────────────────┘
```

#### 3. 符号化分析服务

使用 breakpad 自带的 `minidump-processor` 库构建分析服务：

```cpp
// 伪代码：服务端符号化流程
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"

string analyzeDump(const string& dumpPath, const string& symbolsDir)
{
    google_breakpad::MinidumpProcessor processor(
        &symbolSupplier,  // 符号文件提供器
        &sourceLineResolver
    );

    google_breakpad::ProcessState processState;
    processor.Process(dumpPath, &processState);

    // 输出调用栈
    for (int i = 0; i < processState.threads()->size(); i++) {
        auto* thread = processState.threads()->at(i);
        for (int j = 0; j < thread->frames()->size(); j++) {
            auto* frame = thread->frames()->at(j);
            // function_name, source_file, source_line...
        }
    }
}
```

### 二、符号服务器

为每个发布版本归档对应的符号文件，确保任何历史版本的 dump 都能被分析。

#### 符号目录结构（按 breakpad 约定）

```
symbols/
├── SanYiCAD.pdb/
│   ├── 5F3D4E8A2B9C4D8E9F1A2B3C4D5E6F781/
│   │   └── SanYiCAD.sym
│   └── A1B2C3D4E5F67890ABCDEF12345678901/
│       └── SanYiCAD.sym
├── Qt6Core.dll/
│   └── ...
└── ...
```

> 每个模块（exe/dll/so/dylib）都有对应的符号目录，以调试 ID 为子目录名。

#### 符号服务器实现

- **简单方案**：用 HTTP 文件服务器（Nginx）托管 symbols 目录
- **企业方案**：微软 Symbol Server 协议（支持按需下载）
- **云存储方案**：对象存储 + CDN（适合大规模分发）

### 三、崩溃统计与告警

在服务端数据库中统计崩溃信息，用于版本质量监控：

- **崩溃率**：每千活跃用户崩溃数
- **Top 崩溃**：按调用栈 hash 聚类，找出最频发崩溃
- **版本对比**：新版本 vs 旧版本崩溃率变化
- **告警**：崩溃率突增时自动通知（邮件/钉钉/飞书）

### 四、更多高级特性

| 特性 | 说明 | 实现难度 |
|------|------|---------|
| **崩溃去重** | 按调用栈 hash 聚合，避免重复上报 | 中 |
| **用户标识** | 上报时带上用户 ID，方便定位特定用户问题 | 低 |
| **自定义上下文** | 崩溃时附带操作日志、关键变量等 | 中 |
| **OOM 捕获** | 内存耗尽时也能生成 dump | 高 |
| **GPU 崩溃** | 显卡驱动崩溃的捕获与分析 | 高 |
| **多进程捕获** | 子进程崩溃也能捕获（如渲染进程） | 中 |

---

## 常见问题

### Q1: 崩溃了但没有生成 dump 文件？

排查步骤：
1. 确认 `CrashHandler_Initialize()` 返回 `CRASHHANDLER_OK`
2. 失败时调用 `CrashHandler_GetLastErrorMessage()` 查看原因
3. 检查 `dumpPath` 目录是否存在且有写入权限
4. 确认 Breakpad 平台实现已编译进对应 DLL（Windows/Linux/macOS 各有一套源文件）
5. 检查是否被其他异常处理器拦截（如第三方库、调试器）

### Q2: dump 文件很大，能不能减小？

- 降低 `dumpType` 级别（从 `CRASHHANDLER_DUMP_WITH_FULL_MEMORY` 改为 `CRASHHANDLER_DUMP_NORMAL`）
- 开启 dump 压缩（上传前 zip 一下，通常能压缩 50%+）

### Q3: Release 模式需要符号文件吗？

是的。Release 模式下：
- 可执行文件本身不含调试信息
- 需要对应的 `.pdb` (Windows) / `.dSYM` (macOS) / `.debug` (Linux)
- **发布时务必归档对应版本的符号文件**，否则拿到 dump 也解不出来

### Q4: 能不能在崩溃回调里做复杂操作？

**不建议**。崩溃发生时：
- 堆可能已破坏，`new/malloc` 可能失败
- 全局对象可能处于不一致状态
- 线程可能死锁

**建议**：
- 只做简单、异步安全的操作
- 复杂操作（如网络上传）放在下次启动时做
- 或者使用 Breakpad 的 out-of-process 模式（另起进程生成 dump）

### Q5: CrashHandler 是线程安全的吗？

是的。DLL 内部所有 C++ 逻辑均使用 `std::mutex` 加锁保护。

C API 层面：
- 所有导出函数在入口使用 `try/catch` 捕获异常
- 错误信息通过 `CrashHandler_GetLastErrorMessage()` 获取（线程局部存储）

> 注意：崩溃回调 (`minidumpCallback`) 运行在**崩溃线程**上，回调中只做 `fprintf` 等异步安全操作，不要调用日志、网络、复杂 STL。

---

## 参考资料

- [Google Breakpad 官方文档](https://chromium.googlesource.com/breakpad/breakpad/)
- [Chromium Crashpad 文档](https://chromium.googlesource.com/crashpad/crashpad/)
- [Microsoft Debugging Tools](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/)
- [vcpkg breakpad port](https://github.com/microsoft/vcpkg/tree/master/ports/breakpad)
