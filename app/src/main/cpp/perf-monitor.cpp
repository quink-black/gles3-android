#include "perf-monitor.h"

#include <algorithm>
#include <numeric>

namespace quink {

PerfMonitor::PerfMonitor(int averageOver,
        std::function<void(long long)> outputResult) :
    mData(averageOver),
    mCallback(outputResult),
    mIndex(0)
{
}

void PerfMonitor::Update(const std::chrono::steady_clock::duration &d)
{
    mData[mIndex++] = d;
    if (mIndex == mData.size()) {
        mIndex = 0;
        auto dura = std::accumulate(mData.begin(), mData.end(), std::chrono::steady_clock::duration()) / mData.size();
        long long result = std::chrono::duration_cast<std::chrono::microseconds>(dura).count();
        mCallback(result);
    }
}

void PerfMonitor::Update(const std::chrono::steady_clock::time_point &t)
{
    if (mLastTime == std::chrono::steady_clock::time_point()) {
        mLastTime = t;
        return;
    }
    auto d = t - mLastTime;
    mLastTime = t;
    Update(d);
}

}

#ifdef TEST_PERF
#include <stdio.h>
#include <unistd.h>

int main()
{
    quink::PerfMonitor perfA(100, [](long long dura) { printf("perfA %lld\n", dura); });
    quink::PerfMonitor perfB(100, [](long long dura) { printf("perfB %lld\n", dura); });

    for (int i = 0; i < 1000; i++) {
        auto a = std::chrono::steady_clock::now();
        usleep(10000);
        auto b = std::chrono::steady_clock::now();
        perfA.Update(b - a);
        perfB.Update(b);
    }

    return 0;
}

#endif
