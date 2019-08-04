#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>


namespace Git
{
    using namespace std;

    class MultiThreadTask
    {
        mutex lockObj;
        size_t capacity;
        vector<thread> pool;

    public:
        MultiThreadTask(size_t capacity)
            : lockObj()
            , capacity(capacity)
        {
            pool.reserve(capacity);
        }

        void Add(std::function<bool(const string& str)> func, const string& str)
        {
            lock_guard<mutex> lock(lockObj);

            if (pool.size() >= capacity)
            {
                WaitForComplete_NeedLock();
            }


            pool.emplace_back([func](const string& str) { func(str); }, str);
        }

        void WaitForComplete()
        {
            lock_guard<mutex> lock(lockObj);
            WaitForComplete_NeedLock();
        }

    private:
        void WaitForComplete_NeedLock()
        {
            for (auto& thread : pool)
            {
                thread.join();
            }

            pool.clear();
        }
    };
}
