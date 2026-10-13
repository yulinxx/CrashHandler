#include "CrashHandlerImpl.h"

#include <filesystem>
#include <string>
#include <memory>
#include <cstring>

#include "client/linux/handler/exception_handler.h"

namespace CrashHandler
{
    // Linux 版 breakpad 的 ExceptionHandler 不暴露 dump 类型开关
    // （MinidumpDescriptor 只有目录/大小限制），因此 DumpType 在此平台上被忽略。

    class CrashHandlerLinux : public CrashHandlerImpl
    {
    public:
        CrashHandlerLinux()
            : m_exceptionHandler(nullptr)
        {
        }

        ~CrashHandlerLinux() override
        {
            shutdown();
        }

        bool initialize(const CrashHandlerConfigData& config) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_initialized)
            {
                return true;
            }

            if (config.dumpPath.empty())
            {
                return false;
            }

            if (!ensureDirectoryExists(config.dumpPath))
            {
                return false;
            }

            m_config = config;

            // 使用 std::filesystem::path 确保路径格式正确（统一使用 / 分隔符）
            const std::filesystem::path dumpPathFs(config.dumpPath);

            const google_breakpad::MinidumpDescriptor descriptor(dumpPathFs.string());
            m_exceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
                descriptor, &filterCallback, &minidumpCallback, this, true, -1);

            m_initialized = true;

            cleanOldDumps_nolock();

            return true;
        }

        void shutdown() override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_exceptionHandler.reset();
            m_initialized = false;
        }

        bool writeMinidump() override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_initialized || !m_exceptionHandler)
            {
                return false;
            }
            return m_exceptionHandler->WriteMinidump();
        }

        bool isInitialized() const
        {
            return m_initialized;
        }

    private:
        static bool filterCallback(void* context)
        {
            return context != nullptr;
        }

        static bool minidumpCallback(
            const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded)
        {
            if (!context)
            {
                return succeeded;
            }

            auto* handler = static_cast<CrashHandlerLinux*>(context);

            std::string dumpPath = descriptor.path();
            std::string renamedPath;
            if (succeeded && CrashHandlerImpl::renameDumpFile(dumpPath, handler->m_config.appName, renamedPath))
            {
                dumpPath = renamedPath;
            }

            handler->setLastDumpPath(dumpPath);

            if (handler->m_crashCallback)
            {
                return handler->m_crashCallback(dumpPath, succeeded);
            }

            return succeeded;
        }

        std::unique_ptr<google_breakpad::ExceptionHandler> m_exceptionHandler;
    };

    std::unique_ptr<CrashHandlerImpl> createPlatformImpl()
    {
        return std::make_unique<CrashHandlerLinux>();
    }

}  // namespace CrashHandler
