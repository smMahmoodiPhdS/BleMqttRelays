#include "bleRelayServer.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "wifiManagerTools.h"
#include "boardConfig.h"
#include "relayManager.h"

static BLECharacteristic* relayCharacteristics[RELAY_COUNT];

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

class BleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        Serial.println("BLE client connected");
    }
    void onDisconnect(BLEServer* pServer) override {
        Serial.println("BLE client disconnected, restarting advertising");
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

static void onRelayStateChanged(uint8_t relayIndex, bool state) {
    if (relayIndex >= RELAY_COUNT || !relayCharacteristics[relayIndex]) return;
    uint8_t value = state ? 0x01 : 0x00;
    relayCharacteristics[relayIndex]->setValue(&value, 1);
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
                BLECharacteristic::PROPERTY_WRITE_NR);
        characteristic->setCallbacks(new RelayCharCallbacks(i));

        uint8_t initial = relay_getState(i) ? 0x01 : 0x00;
        characteristic->setValue(&initial, 1);
        relayCharacteristics[i] = characteristic;
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
