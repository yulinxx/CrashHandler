// CrashHandler 模块单元测试
// 覆盖 C ABI（CrashHandlerDLL.h）的参数校验、生命周期与 dump 查询接口。
// 这里全部使用「不真的崩溃」的调用路径：手动 WriteMinidump 写快照，
// 避免测试进程真的异常退出。

#include <CrashHandler/CrashHandlerDLL.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace
{
    // 每个用例独立的 dump 目录，先清干净再用，保证 dump 计数可断言
    std::string makeFreshDumpDir(const char* caseName)
    {
        const fs::path dir = fs::temp_directory_path() / "sanyi_crashhandler_tests" / caseName;
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        return dir.u8string();  // C++17: 返回 std::string
    }
}  // namespace

// ============================================================================
// 版本与静态信息
// ============================================================================

TEST(CrashHandlerApi, VersionMatchesHeaderMacros)
{
    EXPECT_EQ(CrashHandler_GetVersion(), CRASHHANDLER_VERSION);
    EXPECT_STREQ(CrashHandler_GetVersionString(), "1.0.0");
}

TEST(CrashHandlerApi, ConfigInitFillsDefaults)
{
    CrashHandlerConfig config;
    CrashHandler_ConfigInit(&config);

    EXPECT_EQ(config.structSize, sizeof(CrashHandlerConfig));
    EXPECT_EQ(config.maxDumpFiles, 10);
    EXPECT_EQ(config.dumpType, CRASHHANDLER_DUMP_NORMAL);
    EXPECT_EQ(config.enableCrashUpload, 0);
    EXPECT_EQ(config.dumpPath, nullptr);
    EXPECT_EQ(config.appName, nullptr);
    EXPECT_EQ(config.uploadServerUrl, nullptr);
}

TEST(CrashHandlerApi, ConfigInitNullIsSafe)
{
    CrashHandler_ConfigInit(nullptr);  // 不应崩溃
    SUCCEED();
}

TEST(CrashHandlerApi, StatusTextCoversKnownCodes)
{
    EXPECT_STREQ(CrashHandler_StatusText(CRASHHANDLER_OK), "OK");
    EXPECT_STREQ(CrashHandler_StatusText(CRASHHANDLER_ERR_NOT_INITIALIZED), "Not initialized");
    EXPECT_STREQ(CrashHandler_StatusText(CRASHHANDLER_ERR_BUFFER_TOO_SMALL), "Buffer too small");
    EXPECT_STREQ(CrashHandler_StatusText(CRASHHANDLER_ERR_VERSION_MISMATCH), "Version mismatch");
    EXPECT_STREQ(CrashHandler_StatusText(-12345), "Unknown error");
}

TEST(CrashHandlerApi, LastErrorMessageRejectsNull)
{
    EXPECT_EQ(CrashHandler_GetLastErrorMessage(nullptr, 0), CRASHHANDLER_ERR_NULL_POINTER);
}

// ============================================================================
// 生命周期：未初始化防护 → 参数校验 → 初始化 → 写 dump → 查询 → 关闭
// 单个用例内顺序执行，避免 gtest 打乱初始化状态相关的先后依赖。
// ============================================================================

TEST(CrashHandlerLifecycle, GuardBeforeInitialize)
{
    EXPECT_EQ(CrashHandler_IsInitialized(), 0);
    EXPECT_EQ(CrashHandler_WriteMinidump(), CRASHHANDLER_ERR_NOT_INITIALIZED);
    EXPECT_EQ(CrashHandler_GetDumpFileCount(), CRASHHANDLER_ERR_NOT_INITIALIZED);
    EXPECT_EQ(CrashHandler_CleanOldDumps(), CRASHHANDLER_ERR_NOT_INITIALIZED);
}

TEST(CrashHandlerLifecycle, InitializeRejectsBadConfig)
{
    EXPECT_EQ(CrashHandler_Initialize(nullptr), CRASHHANDLER_ERR_NULL_POINTER);

    CrashHandlerConfig config;
    CrashHandler_ConfigInit(&config);

    // 缺 dumpPath
    EXPECT_EQ(CrashHandler_Initialize(&config), CRASHHANDLER_ERR_INVALID_ARG);

    // structSize 与头文件不一致 —— 跨版本 ABI 防线
    config.structSize = 1;
    EXPECT_EQ(CrashHandler_Initialize(&config), CRASHHANDLER_ERR_VERSION_MISMATCH);

    // 错误信息可查询
    char message[256] = {};
    EXPECT_EQ(CrashHandler_GetLastErrorMessage(message, sizeof(message)), CRASHHANDLER_OK);
    EXPECT_STRNE(message, "");

    EXPECT_EQ(CrashHandler_IsInitialized(), 0);
}

TEST(CrashHandlerLifecycle, InitWriteDumpQueryShutdown)
{
    const std::string dumpDir = makeFreshDumpDir("lifecycle");

    CrashHandlerConfig config;
    CrashHandler_ConfigInit(&config);
    config.dumpPath = dumpDir.c_str();
    config.appName = "CrashHandlerTests";
    config.appVersion = "1.0.0";
    config.maxDumpFiles = 5;

    ASSERT_EQ(CrashHandler_Initialize(&config), CRASHHANDLER_OK);
    EXPECT_EQ(CrashHandler_IsInitialized(), 1);

    // 重复初始化幂等：已初始化时直接返回 OK（见 CrashHandler_Initialize）
    EXPECT_EQ(CrashHandler_Initialize(&config), CRASHHANDLER_OK);

    // 手动写一个 minidump（Breakpad 同步写入并回调）
    ASSERT_EQ(CrashHandler_WriteMinidump(), CRASHHANDLER_OK);
    EXPECT_GE(CrashHandler_GetDumpFileCount(), 1);

    // 最近一次 dump 路径可查询，且落在配置的目录里
    char path[1024] = {};
    ASSERT_EQ(CrashHandler_GetLastDumpPath(path, sizeof(path)), CRASHHANDLER_OK);
    EXPECT_FALSE(std::string(path).empty());
    EXPECT_NE(std::string(path).find(dumpDir), std::string::npos)
        << "dump path '" << path << "' should be under '" << dumpDir << "'";

    // 缓冲区过小必须报错而不是截断
    char tiny[4] = {};
    EXPECT_EQ(CrashHandler_GetLastDumpPath(tiny, sizeof(tiny)), CRASHHANDLER_ERR_BUFFER_TOO_SMALL);

    // dump 文件列表查询：越界 / 负索引的边界条件
    const int count = CrashHandler_GetDumpFileCount();
    ASSERT_GT(count, 0);
    char entry[1024] = {};
    EXPECT_EQ(CrashHandler_GetDumpFilePath(0, entry, sizeof(entry)), CRASHHANDLER_OK);
    EXPECT_EQ(CrashHandler_GetDumpFilePath(count, entry, sizeof(entry)), CRASHHANDLER_ERR_OUT_OF_RANGE);
    EXPECT_EQ(CrashHandler_GetDumpFilePath(-1, entry, sizeof(entry)), CRASHHANDLER_ERR_INVALID_ARG);

    // 清理过期 dump：当前只有 1 个、上限 5 个，不应删任何文件
    EXPECT_EQ(CrashHandler_CleanOldDumps(), 0);

    // 关闭后状态复位，dump 查询接口回到「未初始化」防护
    CrashHandler_Shutdown();
    EXPECT_EQ(CrashHandler_IsInitialized(), 0);
    EXPECT_EQ(CrashHandler_GetDumpFileCount(), CRASHHANDLER_ERR_NOT_INITIALIZED);
}