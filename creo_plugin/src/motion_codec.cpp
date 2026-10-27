#include "motion_codec.h"

#include "MotionProtocol.h"

#include <cmath>
#include <cstring>

namespace ccv {
namespace {

std::uint16_t ReadU16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
}

std::uint32_t ReadU32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::uint64_t ReadU64(const std::uint8_t* data) {
    return static_cast<std::uint64_t>(ReadU32(data)) |
           (static_cast<std::uint64_t>(ReadU32(data + 4)) << 32U);
}

float ReadFloat(const std::uint8_t* data) {
    std::uint32_t bits = ReadU32(data);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}

bool IsSequenceNewer(std::uint32_t value, std::uint32_t previous) {
    return static_cast<std::int32_t>(value - previous) > 0;
}

bool DecodeMotionFrame(const std::uint8_t* data, std::size_t size, MotionFrame& frame) {
    if (data == nullptr || size != motion::kPacketSize ||
        ReadU32(data + motion::kMagicOffset) != motion::kMagic ||
        ReadU16(data + motion::kVersionOffset) != motion::kVersion) {
        return false;
    }

    frame.flags = ReadU16(data + motion::kFlagsOffset);
    frame.sequence = ReadU32(data + motion::kSequenceOffset);
    frame.hostMs = ReadU64(data + motion::kTimestampOffset);
    for (int i = 0; i < 4; ++i) {
        frame.quaternion[i] = ReadFloat(data + motion::kQuaternionOffset + i * 4);
    }
    for (int i = 0; i < 3; ++i) {
        frame.pan[i] = ReadFloat(data + motion::kPanOffset + i * 4);
    }
    frame.zoom = ReadFloat(data + motion::kZoomOffset);
    frame.grabId = ReadU32(data + motion::kGrabIdOffset);
    frame.sessionId = ReadU32(data + motion::kSessionIdOffset);

    float normSquared = 0.0F;
    for (float value : frame.quaternion) {
        if (!std::isfinite(value)) {
            return false;
        }
        normSquared += value * value;
    }
    if (!std::isfinite(normSquared) || normSquared < 0.000001F || normSquared > 100.0F) {
        return false;
    }
    for (float value : frame.pan) {
        if (!std::isfinite(value) || value < -4.0F || value > 4.0F) {
            return false;
        }
    }
    return std::isfinite(frame.zoom) && frame.zoom >= -4.0F && frame.zoom <= 4.0F;
}

}
