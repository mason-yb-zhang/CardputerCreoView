#pragma once

#include <cstddef>
#include <cstdint>

namespace ccv::motion {

inline constexpr std::uint32_t kMagic = 0x4D564343U;
inline constexpr std::uint16_t kVersion = 1;
inline constexpr std::uint16_t kPort = 49250;
inline constexpr std::size_t kPacketSize = 60;

inline constexpr std::size_t kMagicOffset = 0;
inline constexpr std::size_t kVersionOffset = 4;
inline constexpr std::size_t kFlagsOffset = 6;
inline constexpr std::size_t kSequenceOffset = 8;
inline constexpr std::size_t kTimestampOffset = 12;
inline constexpr std::size_t kQuaternionOffset = 20;
inline constexpr std::size_t kPanOffset = 36;
inline constexpr std::size_t kZoomOffset = 48;
inline constexpr std::size_t kGrabIdOffset = 52;
inline constexpr std::size_t kSessionIdOffset = 56;

inline constexpr std::uint16_t kFlagEnable = 1U << 0;
inline constexpr std::uint16_t kFlagGrab = 1U << 1;
inline constexpr std::uint16_t kFlagTranslate = 1U << 2;

}  // namespace ccv::motion
