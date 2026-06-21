#ifndef CRASHHANDLER_IMPL_H
#define CRASHHANDLER_IMPL_H

#include "CrashHandler/CrashHandler.h"
#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace CrashHandler
{

class CrashHandlerImpl
{
public:
    virtual ~CrashHandlerImpl() = default;

    virtual bool initialize(const CrashHandlerConfig& config) = 0;
    virtual void shutdown() = 0;

    virtual bool writeMinidump() = 0;

    void setCrashCallback(CrashCallback callback);

    std::string getLastDumpPath() const;
    void setLastDumpPath(const std::string& path);

    std::vector<std::string> getDumpFiles() const;
    int cleanOldDumps();

    const CrashHandlerConfig& config() const;

    bool isInitialized() const { return m_initialized; }

    static bool ensureDirectoryExists(const std::string& path);
    static std::string generateDumpFileName(const std::string& appName);

protected:
    void setCrashCallback_nolock(CrashCallback callback);
    std::string getLastDumpPath_nolock() const;
    void setLastDumpPath_nolock(const std::string& path);
    std::vector<std::string> getDumpFiles_nolock() const;
    int cleanOldDumps_nolock();

    CrashHandlerConfig m_config;
    CrashCallback m_crashCallback;
    std::string m_lastDumpPath;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
};

std::unique_ptr<CrashHandlerImpl> createPlatformImpl();

}

#endif
