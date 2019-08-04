#include "GitUtil.h"

#include <atomic>
#include <cstdio>
#include <cerrno>
#include <memory>
#include <algorithm>
#include <cctype>
#include <locale>
#include <iostream>
#include <mutex>

#include <Windows.h>

#include "GitCommands.h"
#include "GitThreadHelper.h"

#ifdef WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace StrUtil
{
    inline void LeftTrim(std::string& s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
            {
                return !std::isspace(ch);
            }));
    }

    inline void RightTrim(std::string& s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
            {
                return !std::isspace(ch);
            }).base(), s.end());
    }

    inline void Trim(std::string& s)
    {
        LeftTrim(s);
        RightTrim(s);
    }

    inline std::string LeftTrimCopy(std::string s)
    {
        LeftTrim(s);
        return s;
    }

    inline std::string RightTrimCopy(std::string s)
    {
        RightTrim(s);
        return s;
    }

    inline std::string TrimCopy(std::string s)
    {
        Trim(s);
        return s;
    }

    std::string replace_all(const std::string& text, const std::string& pattern, const std::string& replace)
    {
        using namespace std;

        string result = text;

        using strsize_t = string::size_type;
        strsize_t pos = 0;
        strsize_t offset = 0;

        const auto replaceSize = replace.size();

        while ((pos = result.find(pattern, offset)) != string::npos)
        {
            auto cursor = result.begin() + pos;
            result.replace(cursor, cursor + pattern.size(), replace);
            offset = pos + replaceSize;
        }

        return result;
    }

    bool starts_with(const std::string& text, const std::string& pattern)
    {
        using strsize_t = std::string::size_type;
        const strsize_t len = pattern.length();

        if (text.length() < len)
            return false;

        for (strsize_t i = 0; i < len; ++i)
        {
            if (text[i] != pattern[i])
                return false;
        }

        return true;
    }

    inline void PathTrim(std::string& s)
    {
        s = replace_all(s, "\\", "/");
        s = replace_all(s, "//", "/");
        Trim(s);
    }

    inline GitUtil::LockedFileStatus ParseLockedFileResult(std::string& s)
    {
        GitUtil::LockedFileStatus status;

        std::string::size_type offset = 0;
        std::string::size_type newOffset = 0;


        {
            newOffset = s.find('\t', offset + 1);

            if (newOffset != std::string::npos)
            {
                auto item = s.substr(offset, newOffset - offset);
                Trim(item);

                status.filePath = item;
            }
            else
            {
                return GitUtil::LockedFileStatus();
            }

            offset = newOffset;
        }

        {
            newOffset = s.find('\t', offset + 1);

            if (newOffset != std::string::npos)
            {
                auto item = s.substr(offset, newOffset - offset);
                Trim(item);

                status.owner = item;
            }
            else
            {
                return GitUtil::LockedFileStatus();
            }

            offset = newOffset;
        }

        {
            auto item = s.substr(offset);
            Trim(item);

            status.id = item;
        }

        return std::move(status);
    }
}

namespace OSUtil
{
    std::string ExecuteCommand(const char* command)
    {
        constexpr size_t MAX_BUFFER = 1024 * 1024;
        thread_local char resultBuffer[MAX_BUFFER];

        auto fp = popen(command, "r");
        if (!fp)
        {
            return std::string();
        }

        auto readSize = fread((void*)resultBuffer, sizeof(char), sizeof(resultBuffer) - 1, fp);
        if (readSize < MAX_BUFFER)
        {
            resultBuffer[readSize] = '\0';
        }
        else
        {
            resultBuffer[MAX_BUFFER - 1] = '\0';
        }

        std::string result(static_cast<const char*>(resultBuffer));
        pclose(fp);

        return std::move(result);
    }

    std::vector<std::string> ExecuteCommandMultiLines(const char* command)
    {
        constexpr size_t MAX_BUFFER = 1024 * 4;
        thread_local char resultBuffer[MAX_BUFFER];

        std::vector<std::string> lines;

        auto fp = popen(command, "r");
        if (!fp)
        {
            return lines;
        }

        while (auto p = fgets(resultBuffer, MAX_BUFFER, fp))
        {
            lines.emplace_back(p);
        }

        pclose(fp);

        return std::move(lines);
    }
}

bool GitUtil::IsDirectory(const char* path)
{
    DWORD fileType = GetFileAttributesA(path);

    if (fileType == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    if ((fileType & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return false;
    }

    return true;
}

std::vector<std::string> GitUtil::ListFiles(const char* path)
{
    std::vector<std::string> fileList;

    WIN32_FIND_DATAA fileData;
    memset(&fileData, 0, sizeof(WIN32_FIND_DATAA));

    std::string filesPath(path);
    StrUtil::PathTrim(filesPath);
    filesPath.append("/*");

    auto handle = FindFirstFileA(filesPath.c_str(), &fileData);
    bool findSelf = false;
    bool findParent = false;

    while (handle != INVALID_HANDLE_VALUE)
    {
        if (findSelf || strcmp(fileData.cFileName, ".") != 0)
        {
            if (findParent || strcmp(fileData.cFileName, "..") != 0)
            {
                std::string file(fileData.cFileName);
                fileList.push_back(file);
            }
            else
            {
                findParent = true;
            }
        }
        else
        {
            findSelf = true;
        }

        if (FindNextFileA(handle, &fileData) == false)
            break;
    }

    FindClose(handle);

    return std::move(fileList);
}

namespace
{
    using namespace GitUtil;

    void ListFilesRecursiveInternal(std::vector<std::string>& outList, const char* root, const char* path)
    {
        std::string fullPath(root);
        if (fullPath.empty())
        {
            fullPath.append(path);
        }
        else
        {
            fullPath.append("/").append(path);
        }

        if (IsDirectory(fullPath.c_str()))
        {
            auto files = ListFiles(fullPath.c_str());

            for (auto& file : files)
            {
                ListFilesRecursiveInternal(outList, fullPath.c_str(), file.c_str());
            }
        }
        else
        {
            outList.push_back(fullPath);
        }
    }
}

void GitUtil::ListFilesRecursive(std::vector<std::string>& outList, const char* path)
{
    ListFilesRecursiveInternal(outList, "", path);
}

std::string GitUtil::GetFullPath(const char* path)
{
    char fullPath[MAX_PATH];

    GetFullPathName(path, MAX_PATH, fullPath, nullptr);

    std::string str(fullPath);
    StrUtil::PathTrim(str);

    return std::move(str);
}

std::string GitUtil::GetCurrentPath()
{
    return GetFullPath("./");
}

std::string GitUtil::GetRepoRoot(const std::string& path)
{
    std::string rootPath = OSUtil::ExecuteCommand(Git::GetRepoRootPath);
    StrUtil::PathTrim(rootPath);

    return std::move(rootPath);
}

std::string GitUtil::GetOriginUrl(const std::string& rootPath)
{
    static const std::string command(Git::GetOriginUrl);
    auto modCommand = StrUtil::replace_all(command, "<root path>", rootPath);

    std::string url = OSUtil::ExecuteCommand(modCommand.c_str());
    StrUtil::Trim(url);

    return std::move(url);
}

std::vector<GitUtil::LockedFileStatus> GitUtil::GetLockedFiles(const std::string& rootPath)
{
    static const std::string command(Git::GetLockedList);
    auto modCommand = StrUtil::replace_all(command, "<root path>", rootPath);

    auto lockedFiles = OSUtil::ExecuteCommandMultiLines(modCommand.c_str());

    std::vector<GitUtil::LockedFileStatus> results;

    for (auto& file : lockedFiles)
    {
        StrUtil::Trim(file);

        results.emplace_back(StrUtil::ParseLockedFileResult(file));
    }

    return results;
}

bool GitUtil::IsLocked(const std::string& rootPath, const std::string& fileFullPath)
{
    static const std::string command(Git::IsLocked);
    auto modCommand = StrUtil::replace_all(command, "<root path>", rootPath);
    auto filePath = StrUtil::replace_all(fileFullPath, rootPath + "/", "");

    modCommand = StrUtil::replace_all(modCommand, "<file path>", filePath);
    auto result = OSUtil::ExecuteCommand(modCommand.c_str());
    StrUtil::Trim(result);

    return StrUtil::starts_with(result, filePath);
}

bool GitUtil::Lock(const std::string& rootPath, bool isForced, const std::vector<std::string>& fullPathList)
{
    Git::MultiThreadTask Task(128);

    for (auto& file : fullPathList)
    {
        std::cout << "Lock List: " << file << std::endl;
    }

    auto LockInternal = [rootPath, isForced](const std::string& fileFullPath, std::string& OutMessage)
    {
        const std::string command(!isForced ? Git::LockFile : Git::LockFileForce);

        auto modCommand = StrUtil::replace_all(command, "<root path>", rootPath);
        auto filePath = StrUtil::replace_all(fileFullPath, rootPath + "/", "");

        modCommand = StrUtil::replace_all(modCommand, "<file path>", filePath);
        OutMessage.append(OSUtil::ExecuteCommand(modCommand.c_str()));

        return StrUtil::starts_with(OutMessage, "Locked ");
    };

    std::mutex lockObj;
    std::atomic<size_t> count = 0;

    for (auto& file : fullPathList)
    {
        Task.Add([LockInternal, &Task, &lockObj, &count](const std::string& fullPath) -> bool
            {
                std::string msg;

                if (LockInternal(fullPath, msg))
                {
                    ++count;
                    std::lock_guard<std::mutex> lock(lockObj);
                    std::cout << "Locked File: " << fullPath << std::endl;
                    return true;
                }

                {
                    std::lock_guard<std::mutex> lock(lockObj);
                    std::cout << "Lock Failed: " << fullPath << std::endl;
                    std::cout << msg;
                }

                return false;
            }, file);
    }

    Task.WaitForComplete();

    std::cout << "Lock Result: " << fullPathList.size() << " / " << count << std::endl;

    return fullPathList.size() == count;
}

bool GitUtil::Unlock(const std::string& rootPath, bool isForced, const std::vector<std::string>& fullPathList)
{
    Git::MultiThreadTask Task(128);

    for (auto& file : fullPathList)
    {
        std::cout << "Unlock List: " << file << std::endl;
    }

    auto UnlockInternal = [rootPath, isForced](const std::string& fileFullPath, std::string& OutMessage)
    {
        static const std::string command(!isForced ? Git::UnlockFile : Git::UnlockFileForce);

        auto modCommand = StrUtil::replace_all(command, "<root path>", rootPath);
        auto filePath = StrUtil::replace_all(fileFullPath, rootPath + "/", "");

        modCommand = StrUtil::replace_all(modCommand, "<file path>", filePath);
        OutMessage = OSUtil::ExecuteCommand(modCommand.c_str());

        return StrUtil::starts_with(OutMessage, "Unlocked ");
    };

    std::mutex lockObj;
    std::atomic<size_t> count = 0;

    for (auto& file : fullPathList)
    {
        Task.Add([UnlockInternal, &Task, &lockObj, &count](const std::string& fullPath) -> bool
            {
                std::string msg;

                if (UnlockInternal(fullPath, msg))
                {
                    ++count;
                    std::lock_guard<std::mutex> lock(lockObj);
                    std::cout << "Unlocked File: " << fullPath << std::endl;
                    return true;
                }

                {
                    std::lock_guard<std::mutex> lock(lockObj);
                    std::cout << "Unlock Failed: " << fullPath << std::endl;
                    std::cout << msg;
                }

                return false;
            }, file);
    }

    Task.WaitForComplete();

    std::cout << "Unlock Result: " << fullPathList.size() << " / " << count << std::endl;

    return fullPathList.size() == count;
}

bool GitUtil::Lock(const std::string& rootPath, bool isForced, const std::string& fileFullPath)
{
    std::vector<std::string> fullPathList;
    ListFilesRecursive(fullPathList, fileFullPath.c_str());
    return Lock(rootPath, isForced, fullPathList);
}

bool GitUtil::Unlock(const std::string& rootPath, bool isForced, const std::string& fileFullPath)
{
    std::vector<std::string> fullPathList;
    ListFilesRecursive(fullPathList, fileFullPath.c_str());
    return Unlock(rootPath, isForced, fullPathList);
}

size_t GitUtil::UnlockAll(const std::string& rootPath, bool isForced)
{
    std::vector<std::string> fullPathList;

    auto lockedFiles = GitUtil::GetLockedFiles(rootPath);
    for (auto& status : lockedFiles)
    {
        if (status.filePath.empty())
            continue;

        auto& path = status.filePath;
        fullPathList.push_back(path);
    }

    return Unlock(rootPath, isForced, fullPathList);
}

size_t GitUtil::UnlockAll(const std::string& rootPath, bool isForced, const std::string& owner)
{
    std::vector<std::string> fullPathList;

    auto lockedFiles = GitUtil::GetLockedFiles(rootPath);
    for (auto& status : lockedFiles)
    {
        if (status.filePath.empty())
            continue;

        if (status.owner != owner)
            continue;

        auto& path = status.filePath;
        fullPathList.push_back(path);
    }

    return Unlock(rootPath, isForced, fullPathList);
}
