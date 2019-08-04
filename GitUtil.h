#pragma once

#include <string>
#include <vector>

namespace GitUtil
{
    struct LockedFileStatus
    {
        std::string filePath;
        std::string owner;
        std::string id;
    };

    bool IsDirectory(const char* path);
    std::vector<std::string> ListFiles(const char* path);
    void ListFilesRecursive(std::vector<std::string>& outList, const char* path);

    std::string GetFullPath(const char* path);
    std::string GetCurrentPath();
    std::string GetRepoRoot(const std::string& path);
    std::string GetOriginUrl(const std::string& rootPath);

    std::vector<LockedFileStatus> GetLockedFiles(const std::string& rootPath);

    bool IsLocked(const std::string& rootPath, const std::string& fileFullPath);
    bool Lock(const std::string& rootPath, bool isForced, const std::vector<std::string>& fileFullPathList);
    bool Unlock(const std::string& rootPath, bool isForced, const std::vector<std::string>& fileFullPathList);
    bool Lock(const std::string& rootPath, bool isForced, const std::string& fileFullPath);
    bool Unlock(const std::string& rootPath, bool isForced, const std::string& fileFullPath);

    size_t UnlockAll(const std::string& rootPath, bool isForced);
    size_t UnlockAll(const std::string& rootPath, bool isForced, const std::string& owner);
}
