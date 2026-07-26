#include "bleRelayServer.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "wifiManagerTools.h"
#include "boardConfig.h"
#include "relayManager.h"
#include "statusLeds.h"

static BLECharacteristic* relayCharacteristics[RELAY_COUNT];
static BLECharacteristic* statusCharacteristic = nullptr;

// Encodes up to 6 chars of the farm owner name as hex, padded to 12 hex
// digits (6 bytes), so different farm owners produce different UUIDs.
static String hexEncodeOwner(const char* owner) {
    String ownerStr(owner);
    String truncated = ownerStr.substring(0, min((size_t)ownerStr.length(), (size_t)6));

    String encoded;
    char buf[3];
    for (size_t i = 0; i < truncated.length(); i++) {
        snprintf(buf, sizeof(buf), "%02X", (unsigned char)truncated[i]);
        encoded += buf;
    }
    while (encoded.length() < 12) encoded += "00";
    return encoded;
}

static String makeServiceUuid() {
    char dipHex[3];
    snprintf(dipHex, sizeof(dipHex), "%02X", dipValue);
    return "4fafc201-1fb5-" + String(dipHex) + "9e-8fcc-" + hexEncodeOwner(farmOwner);
}

static String makeCharacteristicUuid(uint8_t relayNumOneBased) {
    char dipHex[3];
    snprintf(dipHex, sizeof(dipHex), "%02X", dipValue);
    String segment = String(dipHex) + "8" + String(relayNumOneBased);
    return "beb5483e-36e1-" + segment + "-b7f5-" + hexEncodeOwner(farmOwner);
}

// Packed status for all four channels in one read/notify, so the app can learn
// the whole board's reality in a single round trip instead of four. Relay N is
// slot N-1; the per-relay UUID above keeps digit 4 of segment 3 as the relay
// number, and this one uses '0' there, outside the 1..4 range the app scans for.
static String makeStatusCharacteristicUuid() {
    char dipHex[3];
    snprintf(dipHex, sizeof(dipHex), "%02X", dipValue);
    return "beb5483e-36e1-" + String(dipHex) + "80-b7f5-" + hexEncodeOwner(farmOwner);
}

// One byte per relay:
//   bit0 = actual (measured coil state)
//   bit1 = commanded
//   bit2..3 = RelayMode (0 auto, 1 override_on, 2 override_off, 3 unknown)
static uint8_t packRelayByte(const RelayStatus& s) {
    return (uint8_t)((s.actual ? 0x01 : 0x00) |
                     (s.commanded ? 0x02 : 0x00) |
                     ((uint8_t)(s.mode & 0x03) << 2));
}

class BleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        Serial.println("BLE client connected");
        statusLeds_setBleConnected(true);
    }
    void onDisconnect(BLEServer* pServer) override {
        Serial.println("BLE client disconnected, restarting advertising");
        statusLeds_setBleConnected(false);
        BLEDevice::startAdvertising();
    }
};

class RelayCharCallbacks : public BLECharacteristicCallbacks {
    uint8_t relayIndex;

public:
    explicit RelayCharCallbacks(uint8_t idx) : relayIndex(idx) {}

    void onWrite(BLECharacteristic* characteristic) override {
        std::string value = characteristic->getValue();
        if (value.length() == 0) return;

        Serial.print("[TMP Relay Debug] BLE Write received for Relay Index ");
        Serial.print(relayIndex);
        Serial.print(", value: 0x");
        Serial.println((uint8_t)value[0], HEX);

        relay_setState(relayIndex, value[0] == 0x01);
    }
};

// Fired by relayManager whenever the *observed* state changes - MQTT, BLE, the
// control role, or the on-board slider. Push it out so a connected app never
// has to poll and never shows a switch that disagrees with the coil.
static void onRelayStateChanged(uint8_t relayIndex, const RelayStatus& status) {
    if (relayIndex >= RELAY_COUNT || !relayCharacteristics[relayIndex]) return;

    // Per-relay characteristic keeps its simple 1-byte contract, but the byte
    // is now the measured state rather than the last write we accepted.
    uint8_t value = status.actual ? 0x01 : 0x00;
    relayCharacteristics[relayIndex]->setValue(&value, 1);
    relayCharacteristics[relayIndex]->notify();

    if (statusCharacteristic) {
        uint8_t packed[RELAY_COUNT];
        for (uint8_t i = 0; i < RELAY_COUNT; i++) {
            packed[i] = packRelayByte(relay_getStatus(i));
        }
        statusCharacteristic->setValue(packed, RELAY_COUNT);
        statusCharacteristic->notify();
    }
}

void setup_bleRelayServer() {
    String serviceUuid = makeServiceUuid();

    String deviceName = String(DEVICE_NAME_PREFIX) + "-" + farmOwner;
    BLEDevice::init(deviceName.c_str());

    BLEServer* bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new BleServerCallbacks());

    BLEService* service = bleServer->createService(serviceUuid.c_str());

    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        String charUuid = makeCharacteristicUuid(i + 1);
        BLECharacteristic* characteristic = service->createCharacteristic(
            charUuid.c_str(),
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_NOTIFY);
        characteristic->setCallbacks(new RelayCharCallbacks(i));
        // NOTIFY needs a CCCD (0x2902) or the client has nothing to subscribe to.
        characteristic->addDescriptor(new BLE2902());

        uint8_t initial = relay_getState(i) ? 0x01 : 0x00;
        characteristic->setValue(&initial, 1);
        relayCharacteristics[i] = characteristic;
    }

    // Packed 4-byte status: actual + commanded + override mode for every channel.
    statusCharacteristic = service->createCharacteristic(
        makeStatusCharacteristicUuid().c_str(),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    statusCharacteristic->addDescriptor(new BLE2902());
    {
        uint8_t packed[RELAY_COUNT];
        for (uint8_t i = 0; i < RELAY_COUNT; i++) packed[i] = packRelayByte(relay_getStatus(i));
        statusCharacteristic->setValue(packed, RELAY_COUNT);
    }

    // TODO (BLE full control, planned): expose additional BLE characteristics so
    // the board can be fully managed even when the MQTT broker is down —
    // temperature/humidity limits, light schedules, and on/off thresholds. These
    // must mirror the MQTT-side settings so the app's UI cards can drive either
    // transport interchangeably (see reconciliation doc §4.2). Not yet implemented.

    relay_addListener(onRelayStateChanged);

    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(serviceUuid.c_str());
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.print("BLE service UUID: ");
    Serial.println(serviceUuid);
}
