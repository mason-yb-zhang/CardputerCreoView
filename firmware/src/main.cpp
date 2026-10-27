#include <M5Cardputer.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "SpaceMouseHID.h"

namespace {

using ccv::spacemouse::kMotionReportSize;
using ccv::spacemouse::kButtonReportSize;

// Device presents itself as a 3Dconnexion SpaceMouse Wireless BT (VID 0x256F,
// PID 0xC63A) so the official Windows driver (3dxhid) and 3DxCreo plugin load.
constexpr char kDeviceName[] = "SpaceMouse Wireless";

constexpr uint32_t kSamplePeriodUs = 1000000UL / 60UL;
constexpr uint32_t kDisplayPeriodMs = 200UL;
constexpr uint32_t kRequiredImuData =
        static_cast<uint32_t>(m5::IMU_Class::sensor_mask_accel) |
        static_cast<uint32_t>(m5::IMU_Class::sensor_mask_gyro);

// Tilt-to-axis gain: radians of tilt -> full scale axis deflection.
constexpr float kTiltFullScale = 0.35F;  // ~20 degrees = max deflection
// Gyro-to-rotation gain for rotation axes while held flat: dps -> full scale.
constexpr float kRotationFullScaleDps = 180.0F;

NimBLEHIDDevice* g_hid = nullptr;
NimBLECharacteristic* g_motionReport = nullptr;
NimBLECharacteristic* g_buttonReport = nullptr;
std::atomic<bool> g_bleReady{false};
std::atomic<bool> g_advertising{false};
std::atomic<bool> g_connected{false};
std::atomic<bool> g_notifyEnabled{false};

bool g_imuFault = false;
const char* g_imuStatus = "IMU: OK";
uint32_t g_nextSampleUs = 0;
uint32_t g_nextDisplayMs = 0;
uint32_t g_lastImuUpdateMs = 0;
bool g_button1 = false;
bool g_button2 = false;
uint32_t g_sentMotion = 0;
uint32_t g_notifyFail = 0;
int16_t g_lastTx = 0;
int16_t g_lastRx = 0;

class ServerCallbacks final : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
        g_advertising.store(false);
        g_notifyEnabled.store(false);
        g_connected.store(true);
    }

    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        g_connected.store(false);
        g_notifyEnabled.store(false);
        g_advertising.store(NimBLEDevice::startAdvertising());
    }

    uint32_t onPassKeyDisplay() override {
        return 0;  // Just Works: no passkey prompt
    }

    void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) override {
        // Just Works flow never reaches this; nothing to do if it does.
        (void)connInfo;
        (void)pin;
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        // Bonding finished (success or failure); keep advertising on failure.
        if (connInfo.isBonded() || connInfo.isEncrypted()) {
            Serial.println("Pairing complete");
        }
    }
};

class CharacteristicCallbacks final : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
        g_notifyEnabled.store((subValue & 1U) != 0U);
    }
};

ServerCallbacks g_serverCallbacks;
CharacteristicCallbacks g_motionCallbacks;
CharacteristicCallbacks g_buttonCallbacks;

bool initBle() {
    if (!NimBLEDevice::init(kDeviceName)) {
        return false;
    }

    // HID hosts pair with Just Works bonding: no IO, encrypted, bonded.
    NimBLEDevice::setSecurityAuth(true, false, true);  // bonding, no MITM, secure conn
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    NimBLEServer* server = NimBLEDevice::createServer();
    if (server == nullptr) {
        return false;
    }
    server->setCallbacks(&g_serverCallbacks);

    g_hid = new NimBLEHIDDevice(server);
    if (g_hid == nullptr) {
        return false;
    }
    g_hid->setReportMap(
            const_cast<uint8_t*>(ccv::spacemouse::kReportMap),
            ccv::spacemouse::kReportMapSize);
    // PnP ID: 0x02 = Bluetooth SIG vendor-ID source, so Windows builds the
    // instance id "...Dev_VID&02256F_PID&C63A" that 3dxhid.inf matches for
    // SpaceMouse Wireless Bluetooth Edition. (0x01 USB-IF yields VID&01256F
    // and the official 3dxhid driver never binds.)
    g_hid->setPnp(0x02, ccv::spacemouse::kVendorId, ccv::spacemouse::kProductId,
                  ccv::spacemouse::kVersion);
    g_hid->setHidInfo(0x00, 0x01);  // country 0, normally-connectable
    g_hid->setManufacturer("3Dconnexion");

    g_motionReport = g_hid->getInputReport(ccv::spacemouse::kReportIdMotion);
    g_buttonReport = g_hid->getInputReport(ccv::spacemouse::kReportIdButtons);
    if (g_motionReport == nullptr || g_buttonReport == nullptr) {
        return false;
    }
    g_motionReport->setCallbacks(&g_motionCallbacks);
    g_buttonReport->setCallbacks(&g_buttonCallbacks);

    g_hid->startServices();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (advertising == nullptr) {
        return false;
    }
    advertising->enableScanResponse(true);
    advertising->setName(kDeviceName);
    // HID service UUID in the advertisement so Windows offers pairing.
    advertising->addServiceUUID(NimBLEUUID(static_cast<uint16_t>(0x1812)));
    // Appearance: Generic Device category HID (0x03C0 = Generic HID).
    advertising->setAppearance(0x03C0);
    advertising->addTxPower();
    // Fast interval ~20ms so Windows' periodic scan window always catches us.
    advertising->setAdvertisingInterval(32);  // units of 0.625ms -> 20ms

    const bool started = advertising->start();
    g_bleReady.store(started);
    g_advertising.store(started);
    return started;
}

bool finiteVector(const float* values) {
    return std::isfinite(values[0]) && std::isfinite(values[1]) &&
           std::isfinite(values[2]);
}

int16_t AccelerationToAxis(float g, float fullScale) {
    // Flat device = center; tilting produces 0..1g which maps to deflection.
    return ccv::spacemouse::ClampAxis((g / fullScale) *
                                      static_cast<float>(ccv::spacemouse::kAxisMax));
}

int16_t GyroToAxis(float dps, float fullScaleDps) {
    return ccv::spacemouse::ClampAxis((dps / fullScaleDps) *
                                      static_cast<float>(ccv::spacemouse::kAxisMax));
}

void sampleTelemetry() {
    M5Cardputer.update();
    const bool space = M5Cardputer.Keyboard.isKeyPressed(' ');
    const bool t = M5Cardputer.Keyboard.isKeyPressed('t') ||
                   M5Cardputer.Keyboard.isKeyPressed('T');
    const bool button1Now = space;
    const bool button2Now = t;

    if (g_imuFault || !g_connected.load()) {
        g_button1 = button1Now;
        g_button2 = button2Now;
        return;
    }

    if (button1Now != g_button1 || button2Now != g_button2) {
        g_button1 = button1Now;
        g_button2 = button2Now;
        std::array<uint8_t, kButtonReportSize> report{};
        ccv::spacemouse::BuildButtonReport(g_button1, g_button2, report.data());
        g_buttonReport->setValue(report.data(), report.size());
        g_buttonReport->notify();
    }

    const uint32_t updated = static_cast<uint32_t>(M5.Imu.update());
    const uint32_t nowMs = millis();
    if ((updated & kRequiredImuData) != kRequiredImuData) {
        if (static_cast<int32_t>(nowMs - g_lastImuUpdateMs) > 500) {
            g_imuFault = true;
            g_imuStatus = "IMU READ TIMEOUT";
        }
        return;
    }
    g_lastImuUpdateMs = nowMs;

    const auto& data = M5.Imu.getImuData();
    if (!finiteVector(data.gyro.value) || !finiteVector(data.accel.value)) {
        g_imuFault = true;
        g_imuStatus = "IMU DATA INVALID";
        return;
    }
    const float accel[3] = {data.accel.value[0], data.accel.value[1],
                            data.accel.value[2]};
    const float gyro[3] = {data.gyro.value[0], data.gyro.value[1],
                           data.gyro.value[2]};

    // SpaceMouse semantics: translation axes from tilt (gravity), rotation
    // axes from gyro while buttons held. Held flat = both modes active.
    const int16_t tx = AccelerationToAxis(accel[0], kTiltFullScale);
    const int16_t ty = AccelerationToAxis(accel[1], kTiltFullScale);
    const int16_t tz = AccelerationToAxis(accel[2] - 1.0F, kTiltFullScale);
    const int16_t rx = GyroToAxis(gyro[0], kRotationFullScaleDps);
    const int16_t ry = GyroToAxis(gyro[1], kRotationFullScaleDps);
    const int16_t rz = GyroToAxis(gyro[2], kRotationFullScaleDps);

    std::array<uint8_t, kMotionReportSize> report{};
    ccv::spacemouse::BuildMotionReport(tx, ty, tz, rx, ry, rz, report.data());
    g_motionReport->setValue(report.data(), report.size());
    if (g_motionReport->notify()) {
        g_sentMotion++;
    } else {
        g_notifyFail++;
    }
    g_lastTx = tx;
    g_lastRx = rx;
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

void drawStatus() {
    const bool connected = g_connected.load();
    const bool notifying = g_notifyEnabled.load();
    const char* bleStatus = "BLE: ADVERTISING";

    if (!g_bleReady.load()) {
        bleStatus = "BLE: INIT ERROR";
    } else if (connected && notifying) {
        bleStatus = "BLE HID: READY";
    } else if (connected) {
        bleStatus = "BLE: CONNECTED";
    } else if (!g_advertising.load()) {
        bleStatus = "BLE: ADV ERROR";
    }

    auto& display = M5.Display;
    display.startWrite();
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.drawString("SpaceMouse", 4, 2);
    display.setTextSize(1);
    display.drawString("60Hz", 208, 6);
    display.drawString(bleStatus, 4, 24);
    display.setCursor(4, 40);
    display.printf("B1:%s  B2:%s", g_button1 ? "ON" : "OFF", g_button2 ? "ON" : "OFF");
    display.setCursor(4, 56);
    display.drawString("VID 256F PID C63A", 4, 56);

    display.setTextColor(g_imuFault ? TFT_RED : TFT_GREEN, TFT_BLACK);
    display.drawString(g_imuStatus, 4, 72);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(4, 94);
    display.printf("SENT:%lu FAIL:%lu", (unsigned long)g_sentMotion,
                   (unsigned long)g_notifyFail);
    display.setCursor(4, 110);
    display.printf("TX:%d RX:%d", (int)g_lastTx, (int)g_lastRx);
    if (g_imuFault) {
        display.setTextColor(TFT_RED, TFT_BLACK);
        display.drawString("MOTION STOPPED", 4, 126);
    }
    display.endWrite();
}

}

void setup() {
    Serial.begin(115200);
    Serial.println("Cardputer SpaceMouse HID boot");
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.fallback_board = m5::board_t::board_M5CardputerADV;
    cfg.internal_imu = true;
    M5Cardputer.begin(cfg, true);
    Serial.println("M5 initialized");
    M5.Display.setRotation(1);

    const bool bleReady = initBle();
    Serial.printf("BLE init=%d\n", bleReady ? 1 : 0);
    if (!M5.Imu.isEnabled()) {
        g_imuFault = true;
        g_imuStatus = "IMU INIT FAILED";
    }
    Serial.printf("board=%d imu=%d\n",
                  static_cast<int>(M5.getBoard()),
                  static_cast<int>(M5.Imu.getType()));

    drawStatus();
    g_nextSampleUs = micros() + kSamplePeriodUs;
    g_nextDisplayMs = millis() + kDisplayPeriodMs;
    g_lastImuUpdateMs = millis();
}

void loop() {
    const uint32_t nowUs = micros();
    if (deadlineReached(nowUs, g_nextSampleUs)) {
        g_nextSampleUs += kSamplePeriodUs;
        if (deadlineReached(nowUs, g_nextSampleUs)) {
            g_nextSampleUs = nowUs + kSamplePeriodUs;
        }
        sampleTelemetry();
    }

    const uint32_t nowMs = millis();
    if (deadlineReached(nowMs, g_nextDisplayMs)) {
        g_nextDisplayMs = nowMs + kDisplayPeriodMs;
        drawStatus();
    }

    delay(1);
}
