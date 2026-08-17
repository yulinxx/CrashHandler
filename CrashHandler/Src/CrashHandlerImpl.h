#pragma once

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
        CrashHandlerImpl() = default;
        virtual ~CrashHandlerImpl() = default;

    public:
        virtual bool initialize(const CrashHandlerConfigData& config) = 0;
        virtual void shutdown() = 0;

        virtual bool writeMinidump() = 0;

        void setCrashCallback(CrashCallback callback);

        std::string getLastDumpPath() const;
        void setLastDumpPath(const std::string& path);

        std::vector<std::string> getDumpFiles() const;
        int cleanOldDumps();

        const CrashHandlerConfigData& config() const;

        bool isInitialized() const
        {
            return m_initialized;
        }

        static bool ensureDirectoryExists(const std::string& path);
        static std::string generateDumpFileName(const std::string& appName);

        /// 将 Breakpad 生成的 GUID 文件名重命名为自定义名（appName_时间戳.dmp）。
        /// 成功时 outUtf8Path 输出新路径；失败（源不存在等）保持原路径。
        static bool renameDumpFile(const std::string& srcUtf8Path,
            const std::string& appName,
            std::string& outUtf8Path);

    protected:
        void setCrashCallback_nolock(CrashCallback callback);
        std::string getLastDumpPath_nolock() const;
        void setLastDumpPath_nolock(const std::string& path);
        std::vector<std::string> getDumpFiles_nolock() const;
        int cleanOldDumps_nolock();

    protected:
        CrashHandlerConfigData m_config;
        CrashCallback m_crashCallback;
        std::string m_lastDumpPath;
        mutable std::mutex m_mutex;
        bool m_initialized = false;
    };

    std::unique_ptr<CrashHandlerImpl> createPlatformImpl();

}  // namespace CrashHandler
