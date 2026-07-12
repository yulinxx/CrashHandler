#ifndef CRASHHANDLER_IMPL_H
#define CRASHHANDLER_IMPL_H

#include "CrashHandlerInternal.h"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace CrashHandler
{

class CrashHandlerImpl
{
public:
    virtual ~CrashHandlerImpl() = default;

    virtual bool initialize(const CrashHandlerConfigData& config) = 0;
    virtual void shutdown() = 0;

    virtual bool writeMinidump() = 0;

    void setCrashCallback(CrashCallback callback);

    std::string getLastDumpPath() const;
    void setLastDumpPath(const std::string& path);

    std::vector<std::string> getDumpFiles() const;
    int cleanOldDumps();

    const CrashHandlerConfigData& config() const;

    bool isInitialized() const { return m_initialized; }

    static bool ensureDirectoryExists(const std::string& path);
    static std::string generateDumpFileName(const std::string& appName);

protected:
    void setCrashCallback_nolock(CrashCallback callback);
    std::string getLastDumpPath_nolock() const;
    void setLastDumpPath_nolock(const std::string& path);
    std::vector<std::string> getDumpFiles_nolock() const;
    int cleanOldDumps_nolock();

    CrashHandlerConfigData m_config;
    CrashCallback m_crashCallback;
    std::string m_lastDumpPath;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
};

std::unique_ptr<CrashHandlerImpl> createPlatformImpl();

}

#endif
