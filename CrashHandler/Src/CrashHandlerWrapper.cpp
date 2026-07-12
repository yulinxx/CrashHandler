#include "CrashHandlerInternal.h"
#include "CrashHandlerImpl.h"

namespace CrashHandler
{
    DumpType dumpTypeFromC(int32_t value)
    {
        switch (value)
        {
            case static_cast<int32_t>(DumpType::Normal):
                return DumpType::Normal;
            case static_cast<int32_t>(DumpType::WithDataSegs):
                return DumpType::WithDataSegs;
            case static_cast<int32_t>(DumpType::WithFullMemory):
                return DumpType::WithFullMemory;
            case static_cast<int32_t>(DumpType::WithHandleData):
                return DumpType::WithHandleData;
            case static_cast<int32_t>(DumpType::FilterMemory):
                return DumpType::FilterMemory;
            case static_cast<int32_t>(DumpType::ScanMemory):
                return DumpType::ScanMemory;
            case static_cast<int32_t>(DumpType::WithUnloadedModules):
                return DumpType::WithUnloadedModules;
            case static_cast<int32_t>(DumpType::WithIndirectlyReferencedMemory):
                return DumpType::WithIndirectlyReferencedMemory;
            case static_cast<int32_t>(DumpType::FilterModulePaths):
                return DumpType::FilterModulePaths;
            case static_cast<int32_t>(DumpType::WithProcessThreadData):
                return DumpType::WithProcessThreadData;
            case static_cast<int32_t>(DumpType::WithPrivateReadWriteMemory):
                return DumpType::WithPrivateReadWriteMemory;
            case static_cast<int32_t>(DumpType::WithoutOptionalData):
                return DumpType::WithoutOptionalData;
            case static_cast<int32_t>(DumpType::WithFullMemoryInfo):
                return DumpType::WithFullMemoryInfo;
            case static_cast<int32_t>(DumpType::WithThreadInfo):
                return DumpType::WithThreadInfo;
            case static_cast<int32_t>(DumpType::WithAllMemory):
                return DumpType::WithAllMemory;
            default:
                return DumpType::Normal;
        }
    }

    CrashHandlerEngine& CrashHandlerEngine::instance()
    {
        static CrashHandlerEngine s_instance;
        return s_instance;
    }

    CrashHandlerEngine::CrashHandlerEngine()
        : m_impl(nullptr)
    {
    }

    CrashHandlerEngine::~CrashHandlerEngine()
    {
        if (m_impl)
        {
            m_impl->shutdown();
        }
    }

    bool CrashHandlerEngine::initialize(const CrashHandlerConfigData& config)
    {
        if (!m_impl)
        {
            m_impl = createPlatformImpl();
        }

        const bool ok = m_impl ? m_impl->initialize(config) : false;
        if (ok)
        {
            applyPendingCallback();
        }
        return ok;
    }

    void CrashHandlerEngine::shutdown()
    {
        if (m_impl)
        {
            m_impl->shutdown();
        }
    }

    bool CrashHandlerEngine::isInitialized() const
    {
        return m_impl && m_impl->isInitialized();
    }

    bool CrashHandlerEngine::writeMinidump()
    {
        return m_impl ? m_impl->writeMinidump() : false;
    }

    void CrashHandlerEngine::setCrashCallback(CrashCallback callback)
    {
        m_pendingCallback = std::move(callback);
        if (m_impl)
        {
            m_impl->setCrashCallback(m_pendingCallback);
        }
    }

    std::string CrashHandlerEngine::getLastDumpPath() const
    {
        return m_impl ? m_impl->getLastDumpPath() : std::string();
    }

    std::vector<std::string> CrashHandlerEngine::getDumpFiles() const
    {
        return m_impl ? m_impl->getDumpFiles() : std::vector<std::string>();
    }

    int CrashHandlerEngine::cleanOldDumps()
    {
        return m_impl ? m_impl->cleanOldDumps() : 0;
    }

    const CrashHandlerConfigData& CrashHandlerEngine::config() const
    {
        static const CrashHandlerConfigData kEmptyConfig;
        return m_impl ? m_impl->config() : kEmptyConfig;
    }

    void CrashHandlerEngine::applyPendingCallback()
    {
        if (m_impl && m_pendingCallback)
        {
            m_impl->setCrashCallback(m_pendingCallback);
        }
    }
}