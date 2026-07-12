#ifndef CRASHHANDLER_CRASHHANDLER_DLL_H
#define CRASHHANDLER_CRASHHANDLER_DLL_H

/**
 * @file CrashHandlerDLL.h
 * @brief CrashHandler 模块的标准 C ABI 接口（可跨编译器 / 跨 IDE 使用）
 *
 * 设计要点：
 *   1) 纯 C 接口 —— 参数与返回值均为 POD 类型
 *   2) 禁止 STL 跨边界 —— 字符串通过 char* + length 传递
 *   3) 回调使用函数指针 —— 不使用 std::function
 *   4) 版本可查询 —— 主版本号变化表示 ABI 不兼容
 *
 * 典型调用顺序：
 *   CrashHandler_ConfigInit  ->  CrashHandler_SetCrashCallback (可选)
 *   ->  CrashHandler_Initialize  ->  ...  ->  CrashHandler_Shutdown
 */

#include "CrashHandlerAPI.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRASHHANDLER_VERSION_MAJOR 1
#define CRASHHANDLER_VERSION_MINOR 0
#define CRASHHANDLER_VERSION_PATCH 0

#define CRASHHANDLER_MAKE_VERSION(major, minor, patch) \
    (((uint32_t)(major) << 16) | ((uint32_t)(minor) << 8) | (uint32_t)(patch))

#define CRASHHANDLER_VERSION \
    CRASHHANDLER_MAKE_VERSION( \
        CRASHHANDLER_VERSION_MAJOR, \
        CRASHHANDLER_VERSION_MINOR, \
        CRASHHANDLER_VERSION_PATCH)

    typedef enum CrashHandlerResult
    {
        CRASHHANDLER_OK = 0,
        CRASHHANDLER_ERR_INVALID_ARG = -1,
        CRASHHANDLER_ERR_NULL_POINTER = -2,
        CRASHHANDLER_ERR_NOT_INITIALIZED = -3,
        CRASHHANDLER_ERR_ALREADY_INITIALIZED = -4,
        CRASHHANDLER_ERR_IO = -5,
        CRASHHANDLER_ERR_BUFFER_TOO_SMALL = -6,
        CRASHHANDLER_ERR_OUT_OF_RANGE = -7,
        CRASHHANDLER_ERR_VERSION_MISMATCH = -9,
        CRASHHANDLER_ERR_INTERNAL = -99
    } CrashHandlerResult;

    typedef enum CrashHandlerDumpType : int32_t
    {
        CRASHHANDLER_DUMP_NORMAL = 0,
        CRASHHANDLER_DUMP_WITH_DATA_SEGS = 1,
        CRASHHANDLER_DUMP_WITH_FULL_MEMORY = 2,
        CRASHHANDLER_DUMP_WITH_HANDLE_DATA = 3,
        CRASHHANDLER_DUMP_FILTER_MEMORY = 4,
        CRASHHANDLER_DUMP_SCAN_MEMORY = 5,
        CRASHHANDLER_DUMP_WITH_UNLOADED_MODULES = 6,
        CRASHHANDLER_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY = 7,
        CRASHHANDLER_DUMP_FILTER_MODULE_PATHS = 8,
        CRASHHANDLER_DUMP_WITH_PROCESS_THREAD_DATA = 9,
        CRASHHANDLER_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY = 10,
        CRASHHANDLER_DUMP_WITHOUT_OPTIONAL_DATA = 11,
        CRASHHANDLER_DUMP_WITH_FULL_MEMORY_INFO = 12,
        CRASHHANDLER_DUMP_WITH_THREAD_INFO = 13,
        CRASHHANDLER_DUMP_WITH_ALL_MEMORY = 14
    } CrashHandlerDumpType;

    typedef struct CrashHandlerConfig
    {
        uint32_t structSize;
        const char* dumpPath;
        const char* appName;
        const char* appVersion;
        const char* appBuildId;
        int32_t maxDumpFiles;
        CrashHandlerDumpType dumpType;
        int32_t enableCrashUpload;
        const char* uploadServerUrl;
        uint32_t reserved0;
        uint32_t reserved1;
    } CrashHandlerConfig;

    /**
     * @brief 崩溃回调
     * @param dumpPath UTF-8 dump 文件路径（可能为空字符串）
     * @param succeeded 非 0 表示 dump 写入成功
     * @param userData 用户自定义数据
     * @return 非 0 沿用用户结果，0 沿用 Breakpad 默认行为
     */
    typedef int (*CrashHandlerCallback)(
        const char* dumpPath,
        int succeeded,
        void* userData);

    CRASHHANDLER_C_API CRASHHANDLER_API uint32_t CrashHandler_GetVersion(void);

    CRASHHANDLER_C_API CRASHHANDLER_API const char* CrashHandler_GetVersionString(void);

    CRASHHANDLER_C_API CRASHHANDLER_API void CrashHandler_ConfigInit(CrashHandlerConfig* config);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_Initialize(const CrashHandlerConfig* config);

    CRASHHANDLER_C_API CRASHHANDLER_API void CrashHandler_Shutdown(void);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_IsInitialized(void);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_WriteMinidump(void);

    CRASHHANDLER_C_API CRASHHANDLER_API void CrashHandler_SetCrashCallback(
        CrashHandlerCallback callback,
        void* userData);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_GetLastDumpPath(
        char* buffer,
        size_t bufferSize);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_GetDumpFileCount(void);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_GetDumpFilePath(
        int index,
        char* buffer,
        size_t bufferSize);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_CleanOldDumps(void);

    CRASHHANDLER_C_API CRASHHANDLER_API int CrashHandler_GetLastErrorMessage(
        char* buffer,
        size_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
