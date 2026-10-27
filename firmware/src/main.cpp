#include <M5Cardputer.h>
#include <NimBLEDevice.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "TelemetryCodec.h"

namespace {

using namespace ccv::telemetry;

constexpr uint32_t kSamplePeriodUs = 1000000UL / 60UL;
constexpr uint32_t kDisplayPeriodMs = 200UL;
constexpr uint32_t kRequiredImuData =
        static_cast<uint32_t>(m5::IMU_Class::sensor_mask_accel) |
        static_cast<uint32_t>(m5::IMU_Class::sensor_mask_gyro);

NimBLECharacteristic* g_characteristic = nullptr;
std::atomic<bool> g_bleReady{false};
std::atomic<bool> g_advertising{false};
std::atomic<bool> g_connected{false};
std::atomic<bool> g_notifyEnabled{false};

bool g_imuFault = false;
const char* g_imuStatus = "IMU: OK";
uint16_t g_sequence = 0;
uint16_t g_buttons = 0;
uint32_t g_nextSampleUs = 0;
uint32_t g_nextDisplayMs = 0;
uint32_t g_lastImuUpdateMs = 0;

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
};

class CharacteristicCallbacks final : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
        g_notifyEnabled.store((subValue & 1U) != 0U);
    }
};

ServerCallbacks g_serverCallbacks;
CharacteristicCallbacks g_characteristicCallbacks;

bool initBle() {
    if (!NimBLEDevice::init(kDeviceName)) {
        return false;
    }

    NimBLEServer* server = NimBLEDevice::createServer();
    if (server == nullptr) {
        return false;
    }
    server->setCallbacks(&g_serverCallbacks);

    NimBLEService* service = server->createService(kServiceUuid);
    if (service == nullptr) {
        return false;
    }

    g_characteristic = service->createCharacteristic(
            kCharacteristicUuid, NIMBLE_PROPERTY::NOTIFY);
    if (g_characteristic == nullptr) {
        return false;
    }
    g_characteristic->setCallbacks(&g_characteristicCallbacks);

    if (!service->start()) {
        return false;
    }

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (advertising == nullptr) {
        return false;
    }

    advertising->enableScanResponse(true);
    if (!advertising->setName(kDeviceName) ||
        !advertising->addServiceUUID(kServiceUuid)) {
        return false;
    }

    const bool started = advertising->start();
    g_bleReady.store(started);
    g_advertising.store(started);
    return started;
}

bool finiteVector(const float* values) {
    return std::isfinite(values[0]) && std::isfinite(values[1]) &&
           std::isfinite(values[2]);
}

uint16_t readButtons() {
    uint16_t buttons = 0;
    if (M5Cardputer.Keyboard.isKeyPressed(' ')) {
        buttons |= kButtonGrab;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('t') ||
        M5Cardputer.Keyboard.isKeyPressed('T')) {
        buttons |= kButtonTranslate;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('+')) {
        buttons |= kButtonZoomIn;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('-')) {
        buttons |= kButtonZoomOut;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('c') ||
        M5Cardputer.Keyboard.isKeyPressed('C')) {
        buttons |= kButtonCalibrate;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('r') ||
        M5Cardputer.Keyboard.isKeyPressed('R')) {
        buttons |= kButtonRecenter;
    }
    return buttons;
}

void setImuFault(const char* status) {
    g_imuFault = true;
    g_imuStatus = status;
}

void sampleTelemetry() {
    M5Cardputer.update();
    g_buttons = readButtons();

    if (g_imuFault) {
        return;
    }

    const uint32_t updated = static_cast<uint32_t>(M5.Imu.update());
    const uint32_t nowMs = millis();
    if ((updated & kRequiredImuData) != kRequiredImuData) {
        // BMI270 also returns an empty mask when no new sample is ready.
        if (static_cast<int32_t>(nowMs - g_lastImuUpdateMs) > 500) {
            setImuFault("IMU READ TIMEOUT");
        }
        return;
    }
    g_lastImuUpdateMs = nowMs;

    const auto& data = M5.Imu.getImuData();
    if (!finiteVector(data.gyro.value) || !finiteVector(data.accel.value)) {
        setImuFault("IMU DATA INVALID");
        return;
    }

    auto packet = EncodePacket(g_sequence, millis(), data.gyro.value,
                               data.accel.value, g_buttons);

    if (g_bleReady.load() && g_connected.load() && g_notifyEnabled.load()) {
        g_characteristic->notify(packet.data(), packet.size());
    }
    ++g_sequence;
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
        bleStatus = "BLE: NOTIFY";
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
    display.drawString("CreoView", 4, 2);
    display.setTextSize(1);
    display.drawString("60Hz", 208, 6);
    display.drawString(bleStatus, 4, 24);

    display.setCursor(4, 40);
    display.printf("GRAB:%s  PAN:%s",
                   (g_buttons & kButtonGrab) ? "ON" : "OFF",
                   (g_buttons & kButtonTranslate) ? "ON" : "OFF");
    display.setCursor(4, 56);
    display.printf("SEQ:%u", static_cast<unsigned>(g_sequence));

    display.setTextColor(g_imuFault ? TFT_RED : TFT_GREEN, TFT_BLACK);
    display.drawString(g_imuStatus, 4, 72);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString("Space:grab  T:pan", 4, 94);
    display.drawString("+/-:zoom  C:cal  R:recenter", 4, 110);

    if (g_imuFault) {
        display.setTextColor(TFT_RED, TFT_BLACK);
        display.drawString("NOTIFY STOPPED", 4, 126);
    } else {
        display.drawString("20 byte telemetry", 4, 126);
    }
    display.endWrite();
}

}

void setup() {
    Serial.begin(115200);
    Serial.println("CardputerCreo boot");
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
        setImuFault("IMU INIT FAILED");
    }
    Serial.printf("CardputerCreo board=%d imu=%d\n",
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
