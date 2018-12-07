#pragma once

#include <chrono>
#include <functional>
#include <vector>

namespace quink {

class PerfMonitor {
public:
    PerfMonitor(int averageOver, std::function<void(long long)> callback);

    void Update(const std::chrono::high_resolution_clock::duration &d);
    void Update(const std::chrono::high_resolution_clock::time_point &t);

private:
    std::vector<std::chrono::high_resolution_clock::duration> mData;
    std::function<void(long long)> mCallback;
    std::chrono::high_resolution_clock::time_point mLastTime;
    std::size_t mIndex;
};

}
