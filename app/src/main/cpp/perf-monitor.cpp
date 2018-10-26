#include "perf-monitor.h"

#include <algorithm>
#include <numeric>

PerfMonitor::PerfMonitor(int averageOver,
        std::function<void(long long)> outputResult) :
    mData(averageOver),
    mCallback(outputResult),
    mIndex(0) {
}

void PerfMonitor::Update(const std::chrono::steady_clock::duration &d) {
    mData[mIndex++] = d;
    if (mIndex == mData.size()) {
        mIndex = 0;
        auto dura = std::accumulate(mData.begin(), mData.end(), std::chrono::steady_clock::duration()) / mData.size();
        long long result = std::chrono::duration_cast<std::chrono::microseconds>(dura).count();
        mCallback(result);
    }
}

void PerfMonitor::Update(const std::chrono::steady_clock::time_point &t) {
    if (mLastTime == std::chrono::steady_clock::time_point()) {
        mLastTime = t;
        return;
    }
    auto d = t - mLastTime;
    mLastTime = t;
    Update(d);
}
