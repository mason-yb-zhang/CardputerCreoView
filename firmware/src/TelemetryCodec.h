#pragma once

#include "TelemetryProtocol.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ccv::telemetry {

inline void PutU16(uint8_t* packet, std::size_t offset, uint16_t value) {
    packet[offset] = static_cast<uint8_t>(value & 0xFFU);
    packet[offset + 1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

inline void PutI16(uint8_t* packet, std::size_t offset, int16_t value) {
    PutU16(packet, offset, static_cast<uint16_t>(value));
}

inline void PutU32(uint8_t* packet, std::size_t offset, uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        packet[offset + i] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
    }
}

inline int16_t ScaledI16(float value, float scale) {
    const float scaled = value * scale;
    if (scaled >= 32767.0F) {
        return INT16_MAX;
    }
    if (scaled <= -32768.0F) {
        return INT16_MIN;
    }
    return static_cast<int16_t>(std::lround(scaled));
}

inline std::array<uint8_t, kPacketSize> EncodePacket(
        uint16_t sequence, uint32_t timestampMs, const float gyro[3],
        const float accel[3], uint16_t buttons) {
    std::array<uint8_t, kPacketSize> packet{};
    PutU16(packet.data(), kSequenceOffset, sequence);
    PutU32(packet.data(), kTimestampOffset, timestampMs);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        PutI16(packet.data(), kGyroOffset + axis * 2,
               ScaledI16(gyro[axis], kGyroUnitsPerDps));
        PutI16(packet.data(), kAccelOffset + axis * 2,
               ScaledI16(accel[axis], kAccelUnitsPerG));
    }
    PutU16(packet.data(), kButtonsOffset, buttons);
    return packet;
}

}
