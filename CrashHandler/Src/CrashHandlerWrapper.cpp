#include "CrashHandler/CrashHandler.h"
#include "CrashHandlerImpl.h"

namespace CrashHandler
{
    CrashHandler& CrashHandler::instance()
    {
        static CrashHandler s_instance;
        return s_instance;
    }

    CrashHandler::CrashHandler()
        : m_impl(nullptr)
    {
    }

    CrashHandler::~CrashHandler()
    {
        if (m_impl)
        {
            m_impl->shutdown();
        }
    }

    bool CrashHandler::initialize(const CrashHandlerConfig& config)
    {
        if (!m_impl)
        {
            m_impl = createPlatformImpl();
        }
        return m_impl ? m_impl->initialize(config) : false;
    }

    void CrashHandler::shutdown()
    {
        if (m_impl)
        {
            m_impl->shutdown();
        }
    }

    bool CrashHandler::isInitialized() const
    {
        return m_impl && m_impl->isInitialized();
    }

    bool CrashHandler::writeMinidump()
    {
        return m_impl ? m_impl->writeMinidump() : false;
    }

    void CrashHandler::setCrashCallback(CrashCallback callback)
    {
        if (m_impl)
        {
            m_impl->setCrashCallback(std::move(callback));
        }
    }

    std::string CrashHandler::getLastDumpPath() const
    {
        return m_impl ? m_impl->getLastDumpPath() : std::string();
    }

    std::vector<std::string> CrashHandler::getDumpFiles() const
    {
        return m_impl ? m_impl->getDumpFiles() : std::vector<std::string>();
    }

    int CrashHandler::cleanOldDumps()
    {
        return m_impl ? m_impl->cleanOldDumps() : 0;
    }

    CrashHandlerConfig CrashHandler::config() const
    {
        return m_impl ? m_impl->config() : CrashHandlerConfig();
    }
}