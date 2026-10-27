#pragma once

#include "latest_sample.h"

#include <atomic>
#include <string>
#include <thread>

namespace ccv {

class MotionReceiver {
public:
    explicit MotionReceiver(LatestSample& sample);
    ~MotionReceiver();

    MotionReceiver(const MotionReceiver&) = delete;
    MotionReceiver& operator=(const MotionReceiver&) = delete;

    bool Start(std::string& error);
    void Stop();
    bool IsRunning() const;

private:
    void Run();

    LatestSample& sample_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::uintptr_t socketHandle_ = ~static_cast<std::uintptr_t>(0);
    bool wsaStarted_ = false;
    bool hasSequence_ = false;
    std::uint32_t sessionId_ = 0;
    std::uint32_t lastSequence_ = 0;
};

}
