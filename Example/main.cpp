#include <CrashHandler/CrashHandlerDLL.h>

#include <cstdio>
#include <string>

// ============================================================
//  CrashHandler 使用示例
//  编译后执行 SyCrashHandlerExample.exe，在当前目录生成 crash_dumps\
//
//  标准调用顺序（见 CrashHandlerDLL.h 头注释）：
//     ConfigInit -> SetCrashCallback（可选） -> Initialize
//        -> ... 运行期（崩溃时 Breakpad 自动写 dump）...
//        -> Shutdown
//
//  本示例不真的崩溃，而是用 WriteMinidump 手动写一个诊断快照，
//  演示 dump 的写入、查询与清理接口。
// ============================================================

namespace
{
    // 崩溃回调：dump 写完后调用，可在此做上报 / 本地留痕。
    // 返回非 0 = 沿用 Breakpad 默认行为。
    int onCrash(const char* dumpPath, int succeeded, void* /*userData*/)
    {
        std::printf("[callback] dump=\"%s\" succeeded=%d\n", dumpPath ? dumpPath : "(null)", succeeded);
        return 1;
    }

    void printResult(const char* what, int result)
    {
        std::printf("  %-26s -> %s (%d)\n", what, CrashHandler_StatusText(result), result);
    }
}  // namespace

int main()
{
    std::puts("=== CrashHandler Example ===");

    // ---- 1. 版本查询（ABI 兼容性防线） ----
    std::printf("Version: %s (0x%08X)\n\n", CrashHandler_GetVersionString(), CrashHandler_GetVersion());

    // ---- 2. 配置 ----
    CrashHandlerConfig config;
    CrashHandler_ConfigInit(&config);  // 清零 + 默认值（maxDumpFiles=10 等）
    const std::string dumpPath = "crash_dumps";
    config.dumpPath = dumpPath.c_str();  // 必填：dump 输出目录（不存在会自动创建）
    config.appName = "SyCrashHandlerExample";
    config.appVersion = "1.0.0";
    config.maxDumpFiles = 5;  // 只保留最近 5 个，超出由 CleanOldDumps 清理

    // ---- 3. 可选：崩溃回调（初始化前后设置都可以，会暂存并在初始化时生效） ----
    CrashHandler_SetCrashCallback(onCrash, nullptr);

    // ---- 4. 初始化：安装 Breakpad 崩溃接管 ----
    printResult("Initialize", CrashHandler_Initialize(&config));
    std::printf("  IsInitialized: %d\n\n", CrashHandler_IsInitialized());

    // ---- 5. 手动写一个 minidump（运行期自我诊断快照） ----
    printResult("WriteMinidump", CrashHandler_WriteMinidump());

    char path[1024] = {};
    if (CrashHandler_GetLastDumpPath(path, sizeof(path)) == CRASHHANDLER_OK)
    {
        std::printf("  Last dump: %s\n", path);
    }
    std::printf("  Dump count: %d\n", CrashHandler_GetDumpFileCount());

    // ---- 6. 清理过期 dump（超出 maxDumpFiles 的最老文件） ----
    std::printf("  CleanOldDumps removed: %d\n\n", CrashHandler_CleanOldDumps());

    // ---- 7. 关闭 ----
    CrashHandler_Shutdown();
    std::printf("After Shutdown, IsInitialized: %d\n", CrashHandler_IsInitialized());
    return 0;
}
