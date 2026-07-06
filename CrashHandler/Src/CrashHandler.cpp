#include "CrashHandler/CrashHandler.h"
#include "CrashHandlerImpl.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

static std::string pathToUtf8(const fs::path& p)
{
#ifdef _WIN32
    if (p.empty())
        return {};
    const std::wstring w = p.native();
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), len, nullptr, nullptr);
    return s;
#else
    return p.string();
#endif
}

namespace CrashHandler
{
    // ============================================================
    // 公有方法（加锁包装）
    // ============================================================

    void CrashHandlerImpl::setCrashCallback(CrashCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setCrashCallback_nolock(callback);
    }

    std::string CrashHandlerImpl::getLastDumpPath() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return getLastDumpPath_nolock();
    }

    void CrashHandlerImpl::setLastDumpPath(const std::string& path)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        setLastDumpPath_nolock(path);
    }

    std::vector<std::string> CrashHandlerImpl::getDumpFiles() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return getDumpFiles_nolock();
    }

    int CrashHandlerImpl::cleanOldDumps()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return cleanOldDumps_nolock();
    }

    const CrashHandlerConfig& CrashHandlerImpl::config() const
    {
        return m_config;
    }

    // ============================================================
    // 内部方法（不加锁，供已持有锁的代码调用）
    // ============================================================

    void CrashHandlerImpl::setCrashCallback_nolock(CrashCallback callback)
    {
        m_crashCallback = callback;
    }

    std::string CrashHandlerImpl::getLastDumpPath_nolock() const
    {
        return m_lastDumpPath;
    }

    void CrashHandlerImpl::setLastDumpPath_nolock(const std::string& path)
    {
        m_lastDumpPath = path;
    }

    std::vector<std::string> CrashHandlerImpl::getDumpFiles_nolock() const
    {
        std::vector<std::string> files;
        const std::string& dumpPath = m_config.dumpPath;

        if (dumpPath.empty())
            return files;

        try
        {
            fs::path dumpFsPath = fs::u8path(dumpPath);
            if (!fs::exists(dumpFsPath))
                return files;

            std::vector<fs::path> dumpPaths;
            for (const auto& entry : fs::directory_iterator(dumpFsPath))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".dmp")
                {
                    dumpPaths.push_back(entry.path());
                }
            }

            std::sort(dumpPaths.begin(), dumpPaths.end(),
                [](const fs::path& a, const fs::path& b) {
                    return fs::last_write_time(a) > fs::last_write_time(b);
                });

            for (const auto& p : dumpPaths)
                files.push_back(pathToUtf8(p));
        }
        catch (...)
        {
        }

        return files;
    }

    int CrashHandlerImpl::cleanOldDumps_nolock()
    {
        if (m_config.maxDumpFiles <= 0)
            return 0;

        std::vector<std::string> files = getDumpFiles_nolock();
        int removed = 0;

        for (size_t i = static_cast<size_t>(m_config.maxDumpFiles); i < files.size(); ++i)
        {
            try
            {
                if (fs::remove(fs::u8path(files[i])))
                    ++removed;
            }
            catch (...)
            {
            }
        }

        return removed;
    }

    // ============================================================
    // 静态工具方法
    // ============================================================

    bool CrashHandlerImpl::ensureDirectoryExists(const std::string& path)
    {
        if (path.empty())
            return false;

        try
        {
            fs::path dir = fs::u8path(path);
            if (fs::exists(dir))
                return fs::is_directory(dir);
            return fs::create_directories(dir);
        }
        catch (...)
        {
            return false;
        }
    }

    std::string CrashHandlerImpl::generateDumpFileName(const std::string& appName)
    {
        std::time_t now = std::time(nullptr);
        std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif

        std::ostringstream oss;
        oss << appName << "_"
            << std::setfill('0')
            << std::setw(4) << (tm_buf.tm_year + 1900)
            << std::setw(2) << (tm_buf.tm_mon + 1)
            << std::setw(2) << tm_buf.tm_mday
            << "_"
            << std::setw(2) << tm_buf.tm_hour
            << std::setw(2) << tm_buf.tm_min
            << std::setw(2) << tm_buf.tm_sec
            << ".dmp";

        return oss.str();
    }
}