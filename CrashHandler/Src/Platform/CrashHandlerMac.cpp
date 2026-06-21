#include "CrashHandlerImpl.h"

#include <string>
#include <memory>

#include "client/mac/handler/exception_handler.h"

namespace CrashHandler
{

class CrashHandlerMac : public CrashHandlerImpl
{
public:
    CrashHandlerMac()
        : m_exceptionHandler(nullptr)
    {
    }

    ~CrashHandlerMac() override
    {
        shutdown();
    }

    bool initialize(const CrashHandlerConfig& config) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_initialized)
            return true;

        if (config.dumpPath.empty())
            return false;

        if (!ensureDirectoryExists(config.dumpPath))
            return false;

        m_config = config;

        m_exceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
            config.dumpPath,
            &filterCallback,
            &minidumpCallback,
            this,
            true,
            nullptr
        );

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
            return false;
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

    static bool minidumpCallback(const char* dump_path,
                               const char* minidump_id,
                               void* context,
                               bool succeeded)
    {
        if (!context)
            return succeeded;

        auto* handler = static_cast<CrashHandlerMac*>(context);

        std::string dumpPathStr;
        if (dump_path && minidump_id)
        {
            dumpPathStr = std::string(dump_path) + "/" + minidump_id + ".dmp";
        }

        handler->setLastDumpPath(dumpPathStr);

        if (handler->m_crashCallback)
        {
            return handler->m_crashCallback(dumpPathStr, succeeded);
        }

        return succeeded;
    }

    std::unique_ptr<google_breakpad::ExceptionHandler> m_exceptionHandler;
};

std::unique_ptr<CrashHandlerImpl> createPlatformImpl()
{
    return std::make_unique<CrashHandlerMac>();
}

}
