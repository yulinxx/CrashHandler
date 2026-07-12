#ifndef CRASHHANDLER_INTERNAL_H
#define CRASHHANDLER_INTERNAL_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace CrashHandler
{

enum class DumpType : int32_t
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

struct CrashHandlerConfigData
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

class CrashHandlerEngine
{
public:
    static CrashHandlerEngine& instance();

    bool initialize(const CrashHandlerConfigData& config);
    void shutdown();

    bool isInitialized() const;

    bool writeMinidump();

    void setCrashCallback(CrashCallback callback);

    std::string getLastDumpPath() const;

    std::vector<std::string> getDumpFiles() const;

    int cleanOldDumps();

    const CrashHandlerConfigData& config() const;

    void applyPendingCallback();

private:
    CrashHandlerEngine();
    ~CrashHandlerEngine();
    CrashHandlerEngine(const CrashHandlerEngine&) = delete;
    CrashHandlerEngine& operator=(const CrashHandlerEngine&) = delete;

    std::unique_ptr<CrashHandlerImpl> m_impl;
    CrashCallback m_pendingCallback;
};

DumpType dumpTypeFromC(int32_t value);

}

#endif
