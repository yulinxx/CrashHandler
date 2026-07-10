#ifndef CRASHHANDLER_CRASHHANDLER_H
#define CRASHHANDLER_CRASHHANDLER_H

#include "CrashHandlerAPI.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace CrashHandler
{
    enum class DumpType
    {
        Normal = 0,
        WithDataSegs,
        WithFullMemory,
        WithHandleData,
        FilterMemory,
        ScanMemory,
        WithUnloadedModules,
        WithIndirectlyReferencedMemory,
        FilterModulePaths,
        WithProcessThreadData,
        WithPrivateReadWriteMemory,
        WithoutOptionalData,
        WithFullMemoryInfo,
        WithThreadInfo,
        WithAllMemory
    };

    struct CRASHHANDLER_API CrashHandlerConfig
    {
        std::string dumpPath;
        std::string appName;
        std::string appVersion;
        std::string appBuildId;
        int maxDumpFiles = 10;
        DumpType dumpType = DumpType::Normal;
        bool enableCrashUpload = false;
        std::string uploadServerUrl;
    };

    using CrashCallback = std::function<bool(const std::string& dumpPath, bool succeeded)>;

    class CrashHandlerImpl;

    class CRASHHANDLER_API CrashHandler
    {
    public:
        static CrashHandler& instance();

        bool initialize(const CrashHandlerConfig& config);
        void shutdown();

        bool isInitialized() const;

        bool writeMinidump();

        void setCrashCallback(CrashCallback callback);

        std::string getLastDumpPath() const;

        std::vector<std::string> getDumpFiles() const;

        int cleanOldDumps();

        CrashHandlerConfig config() const;

    private:
        CrashHandler();
        ~CrashHandler();
        CrashHandler(const CrashHandler&) = delete;
        CrashHandler& operator=(const CrashHandler&) = delete;

        std::unique_ptr<CrashHandlerImpl> m_impl;
    };
}

#endif
