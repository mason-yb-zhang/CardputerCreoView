#pragma once

#include "latest_sample.h"

#include <cstddef>
#include <cstdint>

namespace ccv {

bool DecodeMotionFrame(const std::uint8_t* data, std::size_t size, MotionFrame& frame);
bool IsSequenceNewer(std::uint32_t value, std::uint32_t previous);

}
