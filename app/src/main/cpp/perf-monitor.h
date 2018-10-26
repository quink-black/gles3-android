#ifndef TONEMAP_PERF_MONITOR_H
#define TONEMAP_PERF_MONITOR_H

#include <chrono>
#include <functional>
#include <vector>

class PerfMonitor {
public:
    PerfMonitor(int averageOver, std::function<void(long long)> callback);

    void Update(const std::chrono::steady_clock::duration &d);
    void Update(const std::chrono::steady_clock::time_point &t);

private:
    std::vector<std::chrono::steady_clock::duration> mData;
    std::function<void(long long)> mCallback;
    std::chrono::steady_clock::time_point mLastTime;
    std::size_t mIndex;
};

#endif
