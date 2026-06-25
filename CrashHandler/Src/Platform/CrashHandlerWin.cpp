#include "CrashHandlerImpl.h"

#include <windows.h>
#include <string>
#include <memory>

#include "client/windows/handler/exception_handler.h"

namespace CrashHandler
{
    static MINIDUMP_TYPE dumpTypeToMinidumpType(DumpType type)
    {
        switch (type)
        {
            case DumpType::Normal:
                return MiniDumpNormal;
            case DumpType::WithDataSegs:
                return MiniDumpWithDataSegs;
            case DumpType::WithFullMemory:
                return MiniDumpWithFullMemory;
            case DumpType::WithHandleData:
                return MiniDumpWithHandleData;
            case DumpType::FilterMemory:
                return MiniDumpFilterMemory;
            case DumpType::ScanMemory:
                return MiniDumpScanMemory;
            case DumpType::WithUnloadedModules:
                return MiniDumpWithUnloadedModules;
            case DumpType::WithIndirectlyReferencedMemory:
                return MiniDumpWithIndirectlyReferencedMemory;
            case DumpType::FilterModulePaths:
                return MiniDumpFilterModulePaths;
            case DumpType::WithProcessThreadData:
                return MiniDumpWithProcessThreadData;
            case DumpType::WithPrivateReadWriteMemory:
                return MiniDumpWithPrivateReadWriteMemory;
            case DumpType::WithoutOptionalData:
                return MiniDumpWithoutOptionalData;
            case DumpType::WithFullMemoryInfo:
                return MiniDumpWithFullMemoryInfo;
            case DumpType::WithThreadInfo:
                return MiniDumpWithThreadInfo;
            case DumpType::WithAllMemory:
                return MiniDumpWithFullMemory;
            default:
                return MiniDumpNormal;
        }
    }

    class CrashHandlerWin : public CrashHandlerImpl
    {
    public:
        CrashHandlerWin()
            : m_exceptionHandler(nullptr)
        {
        }

        ~CrashHandlerWin() override
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

            std::wstring dumpPathW = utf8ToWide(config.dumpPath);

            m_exceptionHandler = std::make_unique<google_breakpad::ExceptionHandler>(
                dumpPathW,
                &filterCallback,
                &minidumpCallback,
                this,
                google_breakpad::ExceptionHandler::HANDLER_ALL,
                dumpTypeToMinidumpType(config.dumpType),
                static_cast<const wchar_t*>(nullptr),
                static_cast<const google_breakpad::CustomClientInfo*>(nullptr)
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
        static bool filterCallback(void* context, EXCEPTION_POINTERS* exinfo,
            MDRawAssertionInfo* assertion)
        {
            (void)exinfo;
            (void)assertion;
            return context != nullptr;
        }

        static bool minidumpCallback(const wchar_t* dump_path,
            const wchar_t* minidump_id,
            void* context,
            EXCEPTION_POINTERS* exinfo,
            MDRawAssertionInfo* assertion,
            bool succeeded)
        {
            (void)exinfo;
            (void)assertion;

            if (!context)
                return succeeded;

            auto* handler = static_cast<CrashHandlerWin*>(context);

            std::string dumpPathStr;
            if (dump_path && minidump_id)
            {
                std::wstring fullPath(dump_path);
                fullPath += L"\\";
                fullPath += minidump_id;
                fullPath += L".dmp";
                dumpPathStr = wideToUtf8(fullPath);
            }

            handler->setLastDumpPath(dumpPathStr);

            if (handler->m_crashCallback)
            {
                return handler->m_crashCallback(dumpPathStr, succeeded);
            }

            return succeeded;
        }

        static std::wstring utf8ToWide(const std::string& utf8)
        {
            if (utf8.empty())
                return L"";

            int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
            if (len <= 0)
                return L"";

            std::wstring wide(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
            wide.resize(len - 1);
            return wide;
        }

        static std::string wideToUtf8(const std::wstring& wide)
        {
            if (wide.empty())
                return "";

            int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len <= 0)
                return "";

            std::string utf8(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], len, nullptr, nullptr);
            utf8.resize(len - 1);
            return utf8;
        }

        std::unique_ptr<google_breakpad::ExceptionHandler> m_exceptionHandler;
    };

    std::unique_ptr<CrashHandlerImpl> createPlatformImpl()
    {
        return std::make_unique<CrashHandlerWin>();
    }
}