#pragma once

#include <cstdint>
#include <mutex>

namespace ccv {

struct MotionFrame {
    std::uint16_t flags = 0;
    std::uint32_t sequence = 0;
    std::uint64_t hostMs = 0;
    float quaternion[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    float pan[3] = {};
    float zoom = 0.0F;
    std::uint32_t grabId = 0;
    std::uint32_t sessionId = 0;
};

class LatestSample {
public:
    void Store(const MotionFrame& frame, std::uint64_t localMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_ = frame;
        localMs_ = localMs;
        valid_ = true;
    }

    bool Read(MotionFrame& frame, std::uint64_t& localMs) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!valid_) {
            return false;
        }
        frame = frame_;
        localMs = localMs_;
        return true;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        valid_ = false;
        frame_ = MotionFrame{};
        localMs_ = 0;
    }

private:
    mutable std::mutex mutex_;
    MotionFrame frame_;
    std::uint64_t localMs_ = 0;
    bool valid_ = false;
};

}
