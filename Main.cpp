#include <iostream>
#include <string>

#include "GitUtil.h"


using namespace std;

std::string GetFileFullPath(const char* path)
{
    auto fullPath = GitUtil::GetFullPath(path);

    if (GitUtil::IsDirectory(fullPath.c_str()))
    {
        cout << "Directory: [" << fullPath << ']' << endl;
    }
    else
    {
        cout << "File: [" << fullPath << ']' << endl;
    }

    return std::move(fullPath);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cout << "Usage: " << argv[0] << " <command>" << endl;
        cout << "Commands:" << endl;

        cout << " lock <path>" << endl;
        cout << " lock-force <path>" << endl;
        cout << " unlock <path>" << endl;
        cout << " unlock-force <path>" << endl;

        cout << " unlock-all" << endl;
        cout << " unlock-force-all" << endl;
        cout << " unlock-all-owner <owner>" << endl;
        cout << " unlock-force-all-owner <owner>" << endl;

        return 0;
    }

    auto CurPath = GitUtil::GetCurrentPath();
    cout << "Current Path: [" << CurPath << ']' << endl;

    auto rootPath = GitUtil::GetRepoRoot(CurPath);
    cout << "Root Path: [" << rootPath << ']' << endl;

    auto originUrl = GitUtil::GetOriginUrl(rootPath);
    cout << "Origin URL: [" << originUrl << ']' << endl;

    string command = argv[1];

    if (command == "lock")
    {
        if (argc != 3)
        {
            cout << "Usage: " << argv[0] << " lock <path>" << endl;
            return -1;
        }

        auto fullPath = GetFileFullPath(argv[2]);

        GitUtil::Lock(rootPath, false, fullPath);
    }
    else if (command == "lock-force")
    {
        if (argc != 3)
        {
            cout << "Usage: " << argv[0] << " " << command << " <path>" << endl;
            return -1;
        }

        auto fullPath = GetFileFullPath(argv[2]);
        string errorMsg;

        if (GitUtil::Lock(rootPath, true, fullPath))
        {
            cout << "Locked: " << fullPath << endl;
        }
        else
        {
            cout << "Lock Failed: " << fullPath << endl;
        }
    }
    else if (command == "unlock")
    {
        if (argc != 3)
        {
            cout << "Usage: " << argv[0] << " " << command << " <path>" << endl;
            return -1;
        }

        auto fullPath = GetFileFullPath(argv[2]);

        if (GitUtil::Unlock(rootPath, false, fullPath))
        {
            cout << "Unlocked: " << fullPath << endl;
        }
        else
        {
            cout << "Unlock Failed: " << fullPath << endl;
        }
    }
    else if (command == "unlock-force")
    {
        if (argc != 3)
        {
            cout << "Usage: " << argv[0] << " " << command << " <path>" << endl;
            return -1;
        }

        auto fullPath = GetFileFullPath(argv[2]);

        if (GitUtil::Unlock(rootPath, true, fullPath))
        {
            cout << "Unlocked: " << fullPath << endl;
        }
        else
        {
            cout << "Unlock Failed: " << fullPath << endl;
        }
    }
    else if (command == "unlock-all")
    {
        auto count = GitUtil::UnlockAll(rootPath, false);
        cout << "Total Unlocked: " << count << endl;
    }
    else if (command == "unlock-force-all")
    {
        auto count = GitUtil::UnlockAll(rootPath, true);
        cout << "Total Unlocked: " << count << endl;
    }
    else if (command == "unlock-all-owner")
    {
        if (argc != 3)
        {
            cout << "Usage: " << argv[0] << " " << command << " <owner>" << endl;
            return -1;
        }

        auto count = GitUtil::UnlockAll(rootPath, false, argv[2]);
        cout << "Total Unlocked: " << count << endl;
    }
    else if (command == "unlock-force-all-owner")
    {
        if (argc != 3)
        {
            cout << "Usage: " << argv[0] << " " << command << " <owner>" << endl;
            return -1;
        }

        auto count = GitUtil::UnlockAll(rootPath, true, argv[2]);
        cout << "Total Unlocked: " << count << endl;
    }
    else
    {
        cerr << "Unsupported command: " << command << endl;
        return -1;
    }

    return 0;
}
