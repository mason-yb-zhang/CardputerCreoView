#pragma once

#include <cstddef>
#include <cstdint>

namespace ccv::telemetry {

inline constexpr char kDeviceName[] = "CardputerCreo";
inline constexpr char kServiceUuid[] = "9e5f7c20-7e6b-4f6c-9a1e-7a1d4f4b3001";
inline constexpr char kCharacteristicUuid[] = "9e5f7c20-7e6b-4f6c-9a1e-7a1d4f4b3002";

inline constexpr std::size_t kPacketSize = 20;
inline constexpr std::size_t kSequenceOffset = 0;
inline constexpr std::size_t kTimestampOffset = 2;
inline constexpr std::size_t kGyroOffset = 6;
inline constexpr std::size_t kAccelOffset = 12;
inline constexpr std::size_t kButtonsOffset = 18;

inline constexpr float kGyroUnitsPerDps = 10.0F;
inline constexpr float kAccelUnitsPerG = 1000.0F;

inline constexpr std::uint16_t kButtonGrab = 1U << 0;
inline constexpr std::uint16_t kButtonTranslate = 1U << 1;
inline constexpr std::uint16_t kButtonZoomIn = 1U << 2;
inline constexpr std::uint16_t kButtonZoomOut = 1U << 3;
inline constexpr std::uint16_t kButtonCalibrate = 1U << 4;
inline constexpr std::uint16_t kButtonRecenter = 1U << 5;

}  // namespace ccv::telemetry
