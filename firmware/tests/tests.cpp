#include "TelemetryCodec.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace {

int failures = 0;
int checks = 0;

void Check(bool condition, const std::string& name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

uint16_t ReadU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8U);
}

int16_t ReadI16(const uint8_t* data) {
    return static_cast<int16_t>(ReadU16(data));
}

}

int main() {
    using namespace ccv::telemetry;
    const float gyro[3] = {1.0F, -2.0F, 4000.0F};
    const float accel[3] = {1.0F, -0.5F, -40.0F};
    auto packet = EncodePacket(0x1234, 0x89ABCDEF, gyro, accel, 0x003F);

    Check(packet.size() == 20, "packet size");
    Check(packet[kSequenceOffset] == 0x34 && packet[kSequenceOffset + 1] == 0x12,
          "sequence little endian");
    Check(packet[kTimestampOffset] == 0xEF && packet[kTimestampOffset + 1] == 0xCD &&
          packet[kTimestampOffset + 2] == 0xAB && packet[kTimestampOffset + 3] == 0x89,
          "timestamp little endian");
    Check(ReadI16(packet.data() + kGyroOffset) == 10, "gyro X scale");
    Check(ReadI16(packet.data() + kGyroOffset + 2) == -20, "gyro Y signed scale");
    Check(ReadI16(packet.data() + kGyroOffset + 4) == INT16_MAX, "gyro saturation");
    Check(ReadI16(packet.data() + kAccelOffset) == 1000, "accel X scale");
    Check(ReadI16(packet.data() + kAccelOffset + 2) == -500, "accel Y signed scale");
    Check(ReadI16(packet.data() + kAccelOffset + 4) == INT16_MIN, "accel negative saturation");
    Check(ReadU16(packet.data() + kButtonsOffset) == 0x003F, "button bit field");
    Check(ScaledI16(-4000.0F, 10.0F) == INT16_MIN, "signed lower saturation");
    Check(ScaledI16(4000.0F, 10.0F) == INT16_MAX, "signed upper saturation");

    std::cout << "Checks: " << checks << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
