#include "MotionProtocol.h"
#include "motion_codec.h"
#include "view_math.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
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

bool Near(double actual, double expected, double tolerance = 1.0e-9) {
    return std::abs(actual - expected) <= tolerance;
}

void Identity(double matrix[4][4]) {
    std::fill(&matrix[0][0], &matrix[0][0] + 16, 0.0);
    matrix[0][0] = 1.0;
    matrix[1][1] = 1.0;
    matrix[2][2] = 1.0;
    matrix[3][3] = 1.0;
}

void WriteU16(std::uint8_t* data, std::uint16_t value) {
    data[0] = static_cast<std::uint8_t>(value);
    data[1] = static_cast<std::uint8_t>(value >> 8U);
}

void WriteU32(std::uint8_t* data, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        data[i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

void WriteU64(std::uint8_t* data, std::uint64_t value) {
    WriteU32(data, static_cast<std::uint32_t>(value));
    WriteU32(data + 4, static_cast<std::uint32_t>(value >> 32U));
}

void WriteFloat(std::uint8_t* data, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(data, bits);
}

std::array<std::uint8_t, ccv::motion::kPacketSize> MakePacket() {
    std::array<std::uint8_t, ccv::motion::kPacketSize> data = {};
    WriteU32(data.data() + ccv::motion::kMagicOffset, ccv::motion::kMagic);
    WriteU16(data.data() + ccv::motion::kVersionOffset, ccv::motion::kVersion);
    WriteU16(data.data() + ccv::motion::kFlagsOffset,
             ccv::motion::kFlagEnable | ccv::motion::kFlagGrab);
    WriteU32(data.data() + ccv::motion::kSequenceOffset, 0xFFFFFFFEU);
    WriteU64(data.data() + ccv::motion::kTimestampOffset, 123456);
    WriteFloat(data.data() + ccv::motion::kQuaternionOffset + 12, 1.0F);
    WriteFloat(data.data() + ccv::motion::kPanOffset, 0.1F);
    WriteFloat(data.data() + ccv::motion::kPanOffset + 4, 0.2F);
    WriteFloat(data.data() + ccv::motion::kZoomOffset, -1.0F);
    WriteU32(data.data() + ccv::motion::kGrabIdOffset, 42);
    WriteU32(data.data() + ccv::motion::kSessionIdOffset, 99);
    return data;
}

void TestOrientation() {
    double anchor[4][4] = {};
    double output[4][4] = {};
    Identity(anchor);
    anchor[3][0] = 5.0;
    anchor[3][1] = 6.0;
    anchor[3][2] = 7.0;
    float identity[4] = {0.0F, 0.0F, 0.0F, 2.0F};
    ccv::viewmath::BuildOrientation(anchor, identity, output);
    Check(Near(output[0][0], 1.0) && Near(output[1][1], 1.0) &&
          Near(output[2][2], 1.0), "identity quaternion");
    Check(Near(output[3][0], 0.0) && Near(output[3][1], 0.0) &&
          Near(output[3][2], 0.0) && Near(output[3][3], 1.0),
          "orientation removes translation");

    float halfTurn = static_cast<float>(std::sqrt(0.5));
    float yaw90[4] = {0.0F, 0.0F, halfTurn, halfTurn};
    ccv::viewmath::BuildOrientation(anchor, yaw90, output);
    double x = output[0][0];
    double y = output[0][1];
    Check(Near(x, 0.0) && Near(y, 1.0), "row-vector yaw +90 maps X to Y");

    float pitch90[4] = {halfTurn, 0.0F, 0.0F, halfTurn};
    ccv::viewmath::BuildOrientation(anchor, pitch90, output);
    Check(Near(output[1][0], 0.0) && Near(output[1][2], 1.0),
          "row-vector pitch +90 maps Y to Z");

    for (int row = 0; row < 3; ++row) {
        double length = std::sqrt(output[row][0] * output[row][0] +
                                  output[row][1] * output[row][1] +
                                  output[row][2] * output[row][2]);
        Check(Near(length, 1.0), "orientation row normalized");
    }
}

void TestPanZoom() {
    double anchor[4][4] = {};
    double output[4][4] = {};
    Identity(anchor);
    anchor[3][0] = 0.2;
    anchor[3][1] = 0.3;
    ccv::viewmath::BuildPanZoom(anchor, 0.1, -0.2, 2.0, output);
    Check(Near(output[0][0], 2.0) && Near(output[1][1], 2.0) &&
          Near(output[2][2], 2.0), "uniform zoom");
    Check(Near(output[3][0], 0.3) && Near(output[3][1], 0.1) &&
          Near(output[3][3], 1.0), "pan offsets");
}

void TestMotionCodec() {
    auto data = MakePacket();
    ccv::MotionFrame frame;
    Check(ccv::DecodeMotionFrame(data.data(), data.size(), frame), "valid motion packet");
    Check(frame.sequence == 0xFFFFFFFEU && frame.hostMs == 123456 &&
          frame.grabId == 42 && frame.sessionId == 99, "motion integer fields");
    Check(Near(frame.quaternion[3], 1.0) && Near(frame.pan[0], 0.1, 1.0e-6) &&
          Near(frame.zoom, -1.0), "motion float fields");
    Check(!ccv::DecodeMotionFrame(data.data(), data.size() - 1, frame),
          "reject short packet");
    Check(!ccv::DecodeMotionFrame(nullptr, data.size(), frame), "reject null packet");

    auto invalid = data;
    invalid[0] ^= 0xFFU;
    Check(!ccv::DecodeMotionFrame(invalid.data(), invalid.size(), frame), "reject bad magic");
    invalid = data;
    WriteU16(invalid.data() + ccv::motion::kVersionOffset, 2);
    Check(!ccv::DecodeMotionFrame(invalid.data(), invalid.size(), frame), "reject bad version");
    invalid = data;
    WriteFloat(invalid.data() + ccv::motion::kQuaternionOffset + 12, 0.0F);
    Check(!ccv::DecodeMotionFrame(invalid.data(), invalid.size(), frame), "reject zero quaternion");
    invalid = data;
    WriteFloat(invalid.data() + ccv::motion::kPanOffset,
               std::numeric_limits<float>::quiet_NaN());
    Check(!ccv::DecodeMotionFrame(invalid.data(), invalid.size(), frame), "reject NaN pan");
    invalid = data;
    WriteFloat(invalid.data() + ccv::motion::kZoomOffset,
               std::numeric_limits<float>::infinity());
    Check(!ccv::DecodeMotionFrame(invalid.data(), invalid.size(), frame), "reject infinite zoom");

    Check(ccv::IsSequenceNewer(0, 0xFFFFFFFFU), "sequence wrap accepts new zero");
    Check(!ccv::IsSequenceNewer(0xFFFFFFFFU, 0), "sequence wrap rejects old value");
    Check(!ccv::IsSequenceNewer(7, 7), "sequence rejects duplicate");
    Check(ccv::IsSequenceNewer(8, 7), "sequence accepts increment");
}

void TestLatestSample() {
    ccv::LatestSample sample;
    ccv::MotionFrame frame;
    std::uint64_t localMs = 0;
    Check(!sample.Read(frame, localMs), "latest sample starts empty");
    frame.sequence = 1;
    sample.Store(frame, 10);
    frame.sequence = 2;
    sample.Store(frame, 20);
    ccv::MotionFrame read;
    Check(sample.Read(read, localMs) && read.sequence == 2 && localMs == 20,
          "latest sample keeps newest frame");
    sample.Clear();
    Check(!sample.Read(read, localMs), "latest sample clears");
}

}

int main() {
    TestOrientation();
    TestPanZoom();
    TestMotionCodec();
    TestLatestSample();
    std::cout << "Checks: " << checks << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
