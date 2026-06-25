#include "CrashHandler/CrashHandler.h"
#include "CrashHandlerImpl.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

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

#if defined(_WIN32) || defined(_WIN64)
        std::string searchPath = dumpPath + "\\*.dmp";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE)
            return files;

        do
        {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                files.push_back(dumpPath + "\\" + findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
#else
        DIR* dir = opendir(dumpPath.c_str());
        if (!dir)
            return files;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name = entry->d_name;
            if (name.size() >= 4 && name.substr(name.size() - 4) == ".dmp")
            {
                files.push_back(dumpPath + "/" + name);
            }
        }
        closedir(dir);
#endif

        std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
#if defined(_WIN32) || defined(_WIN64)
            struct _stat64 stA, stB;
            _stat64(a.c_str(), &stA);
            _stat64(b.c_str(), &stB);
            return stA.st_mtime > stB.st_mtime;
#else
            struct stat stA, stB;
            stat(a.c_str(), &stA);
            stat(b.c_str(), &stB);
            return stA.st_mtime > stB.st_mtime;
#endif
            });

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
#if defined(_WIN32) || defined(_WIN64)
            if (DeleteFileA(files[i].c_str()))
                ++removed;
#else
            if (unlink(files[i].c_str()) == 0)
                ++removed;
#endif
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

#if defined(_WIN32) || defined(_WIN64)
        DWORD attrs = GetFileAttributesA(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
            return true;
        return CreateDirectoryA(path.c_str(), nullptr) == TRUE || GetLastError() == ERROR_ALREADY_EXISTS;
#else
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            return true;
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
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