#include "CrashHandler/CrashHandlerDLL.h"
#include "CrashHandlerInternal.h"

#include <cstring>
#include <string>

#define CRASHHANDLER_STRINGIFY_IMPL(x) #x
#define CRASHHANDLER_STRINGIFY(x) CRASHHANDLER_STRINGIFY_IMPL(x)

namespace
{
    thread_local char g_lastError[512] = {};

    void setLastError(const char* message)
    {
        if (!message)
        {
            g_lastError[0] = '\0';
            return;
        }

        std::strncpy(g_lastError, message, sizeof(g_lastError) - 1);
        g_lastError[sizeof(g_lastError) - 1] = '\0';
    }

    CrashHandler::CrashHandlerConfigData configFromC(const CrashHandlerConfig* config)
    {
        CrashHandler::CrashHandlerConfigData data;
        if (!config)
        {
            return data;
        }

        if (config->dumpPath)
        {
            data.dumpPath = config->dumpPath;
        }
        if (config->appName)
        {
            data.appName = config->appName;
        }
        if (config->appVersion)
        {
            data.appVersion = config->appVersion;
        }
        if (config->appBuildId)
        {
            data.appBuildId = config->appBuildId;
        }
        if (config->uploadServerUrl)
        {
            data.uploadServerUrl = config->uploadServerUrl;
        }

        data.maxDumpFiles = config->maxDumpFiles > 0 ? config->maxDumpFiles : 10;
        data.dumpType = CrashHandler::dumpTypeFromC(static_cast<int32_t>(config->dumpType));
        data.enableCrashUpload = config->enableCrashUpload != 0;
        return data;
    }

    int copyStringToBuffer(const std::string& value, char* buffer, size_t bufferSize)
    {
        if (!buffer || bufferSize == 0)
        {
            setLastError("Output buffer is null or too small");
            return CRASHHANDLER_ERR_NULL_POINTER;
        }

        if (value.size() + 1 > bufferSize)
        {
            setLastError("Output buffer is too small");
            return CRASHHANDLER_ERR_BUFFER_TOO_SMALL;
        }

        std::memcpy(buffer, value.c_str(), value.size() + 1);
        return CRASHHANDLER_OK;
    }
}

extern "C"
{
    uint32_t CrashHandler_GetVersion(void)
    {
        return CRASHHANDLER_VERSION;
    }

    const char* CrashHandler_GetVersionString(void)
    {
        static const char kVersionString[] =
            CRASHHANDLER_STRINGIFY(CRASHHANDLER_VERSION_MAJOR) "."
            CRASHHANDLER_STRINGIFY(CRASHHANDLER_VERSION_MINOR) "."
            CRASHHANDLER_STRINGIFY(CRASHHANDLER_VERSION_PATCH);
        return kVersionString;
    }

    void CrashHandler_ConfigInit(CrashHandlerConfig* config)
    {
        if (!config)
        {
            return;
        }

        std::memset(config, 0, sizeof(CrashHandlerConfig));
        config->structSize = sizeof(CrashHandlerConfig);
        config->maxDumpFiles = 10;
        config->dumpType = CRASHHANDLER_DUMP_NORMAL;
        config->enableCrashUpload = 0;
    }

    int CrashHandler_Initialize(const CrashHandlerConfig* config)
    {
        try
        {
            if (!config)
            {
                setLastError("config is null");
                return CRASHHANDLER_ERR_NULL_POINTER;
            }

            if (config->structSize != sizeof(CrashHandlerConfig))
            {
                setLastError("CrashHandlerConfig struct size mismatch");
                return CRASHHANDLER_ERR_VERSION_MISMATCH;
            }

            if (!config->dumpPath || config->dumpPath[0] == '\0')
            {
                setLastError("dumpPath is required");
                return CRASHHANDLER_ERR_INVALID_ARG;
            }

            auto& engine = CrashHandler::CrashHandlerEngine::instance();
            if (engine.isInitialized())
            {
                setLastError(nullptr);
                return CRASHHANDLER_OK;
            }

            const CrashHandler::CrashHandlerConfigData data = configFromC(config);
            if (!engine.initialize(data))
            {
                setLastError("Failed to initialize CrashHandler");
                return CRASHHANDLER_ERR_IO;
            }

            setLastError(nullptr);
            return CRASHHANDLER_OK;
        }
        catch (const std::exception& ex)
        {
            setLastError(ex.what());
            return CRASHHANDLER_ERR_INTERNAL;
        }
        catch (...)
        {
            setLastError("Unknown internal error");
            return CRASHHANDLER_ERR_INTERNAL;
        }
    }

    void CrashHandler_Shutdown(void)
    {
        try
        {
            CrashHandler::CrashHandlerEngine::instance().shutdown();
            setLastError(nullptr);
        }
        catch (...)
        {
            setLastError("Unknown internal error during shutdown");
        }
    }

    int CrashHandler_IsInitialized(void)
    {
        try
        {
            return CrashHandler::CrashHandlerEngine::instance().isInitialized() ? 1 : 0;
        }
        catch (...)
        {
            setLastError("Unknown internal error");
            return 0;
        }
    }

    int CrashHandler_WriteMinidump(void)
    {
        try
        {
            auto& engine = CrashHandler::CrashHandlerEngine::instance();
            if (!engine.isInitialized())
            {
                setLastError("CrashHandler is not initialized");
                return CRASHHANDLER_ERR_NOT_INITIALIZED;
            }

            if (!engine.writeMinidump())
            {
                setLastError("Failed to write minidump");
                return CRASHHANDLER_ERR_IO;
            }

            setLastError(nullptr);
            return CRASHHANDLER_OK;
        }
        catch (const std::exception& ex)
        {
            setLastError(ex.what());
            return CRASHHANDLER_ERR_INTERNAL;
        }
        catch (...)
        {
            setLastError("Unknown internal error");
            return CRASHHANDLER_ERR_INTERNAL;
        }
    }

    void CrashHandler_SetCrashCallback(CrashHandlerCallback callback, void* userData)
    {
        try
        {
            auto& engine = CrashHandler::CrashHandlerEngine::instance();
            if (!callback)
            {
                engine.setCrashCallback(nullptr);
                setLastError(nullptr);
                return;
            }

            engine.setCrashCallback([callback, userData](const std::string& dumpPath, bool succeeded) {
                return callback(dumpPath.c_str(), succeeded ? 1 : 0, userData) != 0;
                });
            setLastError(nullptr);
        }
        catch (...)
        {
            setLastError("Unknown internal error while setting callback");
        }
    }

    int CrashHandler_GetLastDumpPath(char* buffer, size_t bufferSize)
    {
        try
        {
            auto& engine = CrashHandler::CrashHandlerEngine::instance();
            if (!engine.isInitialized())
            {
                setLastError("CrashHandler is not initialized");
                return CRASHHANDLER_ERR_NOT_INITIALIZED;
            }

            return copyStringToBuffer(engine.getLastDumpPath(), buffer, bufferSize);
        }
        catch (const std::exception& ex)
        {
            setLastError(ex.what());
            return CRASHHANDLER_ERR_INTERNAL;
        }
        catch (...)
        {
            setLastError("Unknown internal error");
            return CRASHHANDLER_ERR_INTERNAL;
        }
    }

    int CrashHandler_GetDumpFileCount(void)
    {
        try
        {
            auto& engine = CrashHandler::CrashHandlerEngine::instance();
            if (!engine.isInitialized())
            {
                setLastError("CrashHandler is not initialized");
                return CRASHHANDLER_ERR_NOT_INITIALIZED;
            }

            const auto files = engine.getDumpFiles();
            setLastError(nullptr);
            return static_cast<int>(files.size());
        }
        catch (const std::exception& ex)
        {
            setLastError(ex.what());
            return CRASHHANDLER_ERR_INTERNAL;
        }
        catch (...)
        {
            setLastError("Unknown internal error");
            return CRASHHANDLER_ERR_INTERNAL;
        }
    }

    int CrashHandler_GetDumpFilePath(int index, char* buffer, size_t bufferSize)
    {
        try
        {
            if (index < 0)
            {
                setLastError("index must be >= 0");
                return CRASHHANDLER_ERR_INVALID_ARG;
            }

            auto& engine = CrashHandler::CrashHandlerEngine::instance();
            if (!engine.isInitialized())
            {
                setLastError("CrashHandler is not initialized");
                return CRASHHANDLER_ERR_NOT_INITIALIZED;
            }

            const auto files = engine.getDumpFiles();
            if (index >= static_cast<int>(files.size()))
            {
                setLastError("dump file index out of range");
                return CRASHHANDLER_ERR_OUT_OF_RANGE;
            }

            return copyStringToBuffer(files[static_cast<size_t>(index)], buffer, bufferSize);
        }
        catch (const std::exception& ex)
        {
            setLastError(ex.what());
            return CRASHHANDLER_ERR_INTERNAL;
        }
        catch (...)
        {
            setLastError("Unknown internal error");
            return CRASHHANDLER_ERR_INTERNAL;
        }
    }

    int CrashHandler_CleanOldDumps(void)
    {
        try
        {
            auto& engine = CrashHandler::CrashHandlerEngine::instance();
            if (!engine.isInitialized())
            {
                setLastError("CrashHandler is not initialized");
                return CRASHHANDLER_ERR_NOT_INITIALIZED;
            }

            const int removed = engine.cleanOldDumps();
            setLastError(nullptr);
            return removed;
        }
        catch (const std::exception& ex)
        {
            setLastError(ex.what());
            return CRASHHANDLER_ERR_INTERNAL;
        }
        catch (...)
        {
            setLastError("Unknown internal error");
            return CRASHHANDLER_ERR_INTERNAL;
        }
    }

    int CrashHandler_GetLastErrorMessage(char* buffer, size_t bufferSize)
    {
        if (!buffer || bufferSize == 0)
        {
            return CRASHHANDLER_ERR_NULL_POINTER;
        }

        std::strncpy(buffer, g_lastError, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return CRASHHANDLER_OK;
    }
}