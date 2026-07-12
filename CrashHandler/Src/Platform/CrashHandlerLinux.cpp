#include "CrashHandlerImpl.h"

#include <string>
#include <memory>
#include <cstring>

#include "client/linux/handler/exception_handler.h"

namespace CrashHandler
{

static google_breakpad::MinidumpType dumpTypeToMinidumpType(DumpType type)
{
    switch (type)
    {
        case DumpType::WithFullMemory:
        case DumpType::WithAllMemory:
            return google_breakpad::MD_FULL_DUMP;
        default:
            return google_breakpad::MD_NORMAL_DUMP;
    }
}

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
            -1
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

    static bool minidumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                                 void* context,
                                 bool succeeded)
    {
        if (!context)
            return succeeded;

        auto* handler = static_cast<CrashHandlerLinux*>(context);

        std::string dumpPath = descriptor.path();
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

}
