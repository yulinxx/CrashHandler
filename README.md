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
    │       └── CrashHandler.h              # 对外公共接口
    └── Src/
        ├── CrashHandlerImpl.h              # 平台实现基类（内部）
        ├── CrashHandler.cpp                # 公共逻辑（dump 管理、文件操作）
        ├── CrashHandlerWrapper.cpp         # 对外类包装（Pimpl 模式）
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
│           CrashHandler (对外 API)                │
│  单例模式 + Pimpl，提供统一的跨平台接口           │
├─────────────────────────────────────────────────┤
│           CrashHandlerImpl (基类)                │
│  公共逻辑：dump 文件管理、回调调度、工具函数       │
├─────────────────────────────────────────────────┤
│  Windows  │   Linux   │   macOS   │  ... 更多平台  │
│  Breakpad │  Breakpad │  Breakpad │              │
└─────────────────────────────────────────────────┘
```

### 核心设计模式

1. **单例模式 (Singleton)**：全局唯一实例，确保只有一个崩溃处理器
2. **Pimpl 模式**：对外接口与内部实现分离，减少头文件依赖
3. **工厂模式**：`createPlatformImpl()` 根据平台创建对应实现
4. **回调机制**：崩溃发生时触发用户回调，可做上报、清理等操作

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

- **Utility**：依赖 `AppPathManager` 获取崩溃 dump 存储路径
- **Log**：崩溃回调中可关联日志信息，辅助问题定位
- **Qt**：无直接依赖，可在 QApplication 创建前初始化

---

## 使用说明

### 1. 快速开始

在 `main()` 函数**最开头**初始化（确保初始化阶段崩溃也能捕获）：

```cpp
#include "CrashHandler/CrashHandler.h"
#include "Ut/AppPathManager.h"

int main(int argc, char* argv[])
{
    // 0. 最先初始化路径管理
    const std::string appName = "SanYiCAD";
    Ut::AppPathManager::instance().initialize(appName);

    // 0.5 初始化崩溃捕获（越早越好）
    CrashHandler::CrashHandlerConfig config;
    config.dumpPath = Ut::AppPathManager::instance().getCrashDumpsPath();
    config.appName = appName;
    config.appVersion = "1.0.0";
    config.maxDumpFiles = 10;
    config.dumpType = CrashHandler::DumpType::Normal;

    // 设置崩溃回调
    CrashHandler::CrashHandler::instance().setCrashCallback(
        [](const std::string& dumpPath, bool succeeded) -> bool {
            if (succeeded) {
                fprintf(stderr, "Crash dump saved to: %s\n", dumpPath.c_str());
            }
            return succeeded;
        }
    );

    CrashHandler::CrashHandler::instance().initialize(config);

    // ... 后续业务逻辑
}
```

### 2. 配置项详解

`CrashHandlerConfig` 结构体：

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `dumpPath` | `std::string` | `""` | dump 文件存储目录（必须设置） |
| `appName` | `std::string` | `""` | 应用名称，用于 dump 文件名前缀 |
| `appVersion` | `std::string` | `""` | 应用版本号，辅助分析 |
| `appBuildId` | `std::string` | `""` | 构建 ID / Git commit hash |
| `maxDumpFiles` | `int` | `10` | 最大保留 dump 文件数，超出自动清理最旧的 |
| `dumpType` | `DumpType` | `Normal` | dump 详细程度（见下文） |
| `enableCrashUpload` | `bool` | `false` | 是否启用崩溃上报（预留） |
| `uploadServerUrl` | `std::string` | `""` | 上报服务器地址（预留） |

### 3. DumpType 枚举

不同 dump 类型的详细程度和体积对比：

| 类型 | 说明 | 体积 | 适用场景 |
|------|------|------|---------|
| `Normal` | 基本信息（线程、栈、模块） | 小 | 日常使用，上报场景 |
| `WithDataSegs` | 包含数据段 | 中 | 需要全局变量数据时 |
| `WithFullMemory` | 完整进程内存 | 大 | 本地深度调试 |
| `WithHandleData` | 包含句柄信息 | 小 | 句柄泄漏分析 |
| `WithThreadInfo` | 详细线程信息 | 小 | 线程状态分析 |
| `WithFullMemoryInfo` | 内存状态信息 | 中 | 内存问题分析 |

> **建议**：默认用 `Normal`，需要深度分析时切到 `WithFullMemory`。

### 4. 手动触发 Dump

在需要的地方（如异常捕获、断言失败）手动生成 dump：

```cpp
#include "CrashHandler/CrashHandler.h"

void someFunction()
{
    try {
        // ... 可能崩溃的代码
    } catch (...) {
        // 手动写一份 dump 再继续
        CrashHandler::CrashHandler::instance().writeMinidump();
        throw;
    }
}
```

### 5. 获取 Dump 文件列表

```cpp
auto dumps = CrashHandler::CrashHandler::instance().getDumpFiles();
for (const auto& path : dumps) {
    qDebug() << "Dump file:" << QString::fromStdString(path);
}

std::string lastDump = CrashHandler::CrashHandler::instance().getLastDumpPath();
```

### 6. 清理旧 Dump

```cpp
// 手动清理，保留最新 N 个（N = maxDumpFiles）
int removed = CrashHandler::CrashHandler::instance().cleanOldDumps();
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
```

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
// 应用正常启动后，检查是否有未上传的 dump
void checkAndUploadPendingDumps()
{
    auto dumps = CrashHandler::CrashHandler::instance().getDumpFiles();
    for (const auto& dumpPath : dumps) {
        if (!isUploaded(dumpPath)) {
            uploadCrashDump(dumpPath);
            markAsUploaded(dumpPath);
        }
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
1. 确认 `CrashHandler::initialize()` 是否成功调用
2. 检查 `dumpPath` 目录是否存在且有写入权限
3. 确认是在主线程崩溃（多线程崩溃也能捕获，但某些极端情况可能失败）
4. 检查是否被其他异常处理器拦截（如第三方库、调试器）

### Q2: dump 文件很大，能不能减小？

- 降低 `dumpType` 级别（从 `WithFullMemory` 改为 `Normal`）
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

是的。所有公有方法都使用 `std::mutex` 加锁保护。

内部实现采用 **`_nolock` 模式**：
- 对外 API 统一加锁，确保线程安全
- 内部 `_nolock` 后缀方法不加锁，供已持有锁的代码调用
- 避免递归锁问题，同时减少锁开销

> 注意：崩溃回调 (`minidumpCallback`) 运行在**崩溃线程**上，调用的是公有方法（如 `setLastDumpPath`），会正常加锁，是安全的。

---

## 参考资料

- [Google Breakpad 官方文档](https://chromium.googlesource.com/breakpad/breakpad/)
- [Chromium Crashpad 文档](https://chromium.googlesource.com/crashpad/crashpad/)
- [Microsoft Debugging Tools](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/)
- [vcpkg breakpad port](https://github.com/microsoft/vcpkg/tree/master/ports/breakpad)
