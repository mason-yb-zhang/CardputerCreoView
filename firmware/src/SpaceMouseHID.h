#pragma once

#include <cstdint>
#include <cstddef>

// HID Report Map for 3Dconnexion SpaceMouse Wireless Bluetooth Edition
// (VID 0x256F / PID 0xC63A). 270 bytes, captured from real hardware by the
// open-source spacemouse community (github.com/AndunHH/spacemouse, CC BY-NC-SA).
// Report 1: 6 axes x 16-bit signed (-350..350), logical collection "Multi-axis
// Controller". Report 3: 2 buttons. Report 4: LED output. Report 0x17: battery.
// Reports 5..26: vendor feature (declared but unused by us).
namespace ccv::spacemouse {

inline constexpr uint16_t kVendorId = 0x256F;
inline constexpr uint16_t kProductId = 0xC63A;
inline constexpr uint16_t kVersion = 0x0100;

inline constexpr uint8_t kReportIdMotion = 0x01;
inline constexpr uint8_t kReportIdButtons = 0x03;
inline constexpr uint8_t kReportIdLed = 0x04;
inline constexpr uint8_t kReportIdBattery = 0x17;

inline constexpr int16_t kAxisMin = -350;
inline constexpr int16_t kAxisMax = 350;

inline constexpr size_t kMotionReportSize = 12;  // 6 x int16, no report-ID prefix (HOGP)
inline constexpr size_t kButtonReportSize = 2;   // 2 data bits + 14 const bits

inline constexpr uint8_t kReportMap[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x08,        // Usage (Multi-axis Controller)
    0xA1, 0x01,        // Collection (Application)
    0xA1, 0x00,        //   Collection (Physical)
    0x85, 0x01,        //     Report ID (1)
    0x16, 0xA2, 0xFE,  //     Logical Minimum (-350)
    0x26, 0x5E, 0x01,  //     Logical Maximum (350)
    0x36, 0x88, 0xFA,  //     Physical Minimum (-1400)
    0x46, 0x78, 0x05,  //     Physical Maximum (1400)
    0x55, 0x0C,        //     Unit Exponent (-4)
    0x65, 0x11,        //     Unit (SI Linear, Length: Centimeter)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x33,        //     Usage (Rx)
    0x09, 0x34,        //     Usage (Ry)
    0x09, 0x35,        //     Usage (Rz)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x06,        //     Report Count (6)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x85, 0x03,        //     Report ID (3)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x02,        //     Usage Maximum (0x02)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x01,        //     Physical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x95, 0x0E,        //     Report Count (14)
    0x81, 0x03,        //     Input (Const,Var,Abs)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x85, 0x04,        //     Report ID (4)
    0x05, 0x08,        //     Usage Page (LEDs)
    0x09, 0x4B,        //     Usage (Generic Indicator)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x01,        //     Report Size (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,Non-volatile)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x07,        //     Report Size (7)
    0x91, 0x03,        //     Output (Const,Var,Abs,Non-volatile)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x85, 0x17,        //     Report ID (23)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x64,        //     Logical Maximum (100)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x05, 0x06,        //     Usage Page (Generic Dev Ctrls)
    0x09, 0x20,        //     Usage (Battery Strength)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
    0x09, 0x27,        //     Usage (0x27)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x75, 0x07,        //     Report Size (7)
    0x81, 0x03,        //     Input (Const,Var,Abs)
    0xC0,              //   End Collection
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,        //   Usage (0x01)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x80,        //     Logical Minimum (-128)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x09, 0x3A,        //     Usage (0x3A)
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x05,        //       Report ID (5)
    0x09, 0x20,        //       Usage (0x20)
    0x95, 0x01,        //       Report Count (1)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x06,        //       Report ID (6)
    0x09, 0x21,        //       Usage (0x21)
    0x95, 0x01,        //       Report Count (1)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x07,        //       Report ID (7)
    0x09, 0x22,        //       Usage (0x22)
    0x95, 0x01,        //       Report Count (1)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x08,        //       Report ID (8)
    0x09, 0x23,        //       Usage (0x23)
    0x95, 0x07,        //       Report Count (7)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x09,        //       Report ID (9)
    0x09, 0x24,        //       Usage (0x24)
    0x95, 0x07,        //       Report Count (7)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x0A,        //       Report ID (10)
    0x09, 0x25,        //       Usage (0x25)
    0x95, 0x07,        //       Report Count (7)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x0B,        //       Report ID (11)
    0x09, 0x26,        //       Usage (0x26)
    0x95, 0x01,        //       Report Count (1)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x13,        //       Report ID (19)
    0x09, 0x2E,        //       Usage (0x2E)
    0x95, 0x01,        //       Report Count (1)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x19,        //       Report ID (25)
    0x09, 0x31,        //       Usage (0x31)
    0x95, 0x04,        //       Report Count (4)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xA1, 0x02,        //     Collection (Logical)
    0x85, 0x1A,        //       Report ID (26)
    0x09, 0x32,        //       Usage (0x32)
    0x95, 0x07,        //       Report Count (7)
    0xB1, 0x02,        //       Feature (Data,Var,Abs,Non-volatile)
    0xC0,              //     End Collection
    0xC0,              //   End Collection
    0xC0,              // End Collection
};
inline constexpr uint16_t kReportMapSize = sizeof(kReportMap);
static_assert(kReportMapSize == 270, "SpaceMouse report map must be 270 bytes");

// Build the 12-byte motion payload: tx,ty,tz,rx,ry,rz little-endian int16.
// HOGP: the Report ID is declared by the 0x2908 Report Reference descriptor,
// so the characteristic VALUE carries no report-ID prefix.
inline void BuildMotionReport(int16_t tx, int16_t ty, int16_t tz,
                              int16_t rx, int16_t ry, int16_t rz,
                              uint8_t* out) {
    const int16_t axes[6] = {tx, ty, tz, rx, ry, rz};
    for (int i = 0; i < 6; ++i) {
        out[i * 2] = static_cast<uint8_t>(axes[i] & 0xFF);
        out[i * 2 + 1] = static_cast<uint8_t>((axes[i] >> 8) & 0xFF);
    }
}

inline void BuildButtonReport(bool button1, bool button2, uint8_t* out) {
    out[0] = static_cast<uint8_t>((button1 ? 1 : 0) | (button2 ? 2 : 0));
    out[1] = 0;
}

inline int16_t ClampAxis(float value) {
    if (value <= static_cast<float>(kAxisMin)) return kAxisMin;
    if (value >= static_cast<float>(kAxisMax)) return kAxisMax;
    return static_cast<int16_t>(value);
}

}  // namespace ccv::spacemouse
