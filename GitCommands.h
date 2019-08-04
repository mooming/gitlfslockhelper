#pragma once

namespace Git
{
    constexpr auto GetRepoRootPath = "git rev-parse --show-toplevel";
    constexpr auto GetOriginUrl = "git -C <root path> remote get-url origin";
    constexpr auto GetLockedList = "git -C <root path> lfs locks";
    constexpr auto IsLocked = "git -C <root path> lfs locks --path=<file path>";
    constexpr auto LockFile = "git -C <root path> lfs lock <file path>";
    constexpr auto LockFileForce = "git -C <root path> lfs lock -f <file path>";
    constexpr auto UnlockFile = "git -C <root path> lfs unlock <file path>";
    constexpr auto UnlockFileForce = "git -C <root path> lfs unlock -f <file path>";
}
