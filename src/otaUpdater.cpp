#include "otaUpdater.h"
#include "mqttFunctions.h"
#include "boardConfig.h"
#include <Preferences.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

// Cap the random pre-update delay so a fleet of these devices doesn't all
// hit the update server at once when a new version is published.
#define OTA_MAX_RANDOM_DELAY_S 300

static Preferences otaPrefs;
static String currentVersion = "NONE";
static String remoteVersion = "NULL";
static unsigned long randomDelayMs = 0;
static unsigned long updateRequestedAtMs = 0;
static bool updateErrorLatched = false;

String ota_versionTopic() {
    return String("configs/ts/") + OTA_MODEL_CODE + "/" + OTA_HARDWARE_VERSION + "/firmVer";
}

void ota_setRemoteVersion(const String& version) {
    remoteVersion = version;
}

void setup_ota() {
    otaPrefs.begin("ota", true);
    currentVersion = otaPrefs.getString("firmVer", "NONE");
    otaPrefs.end();

    Serial.print("OTA current firmware version: ");
    Serial.println(currentVersion);
}

static void onOtaStart() {
    Serial.println("OTA update started");
}

static void onOtaEnd() {
    Serial.println("OTA update finished");
    currentVersion = remoteVersion;
    otaPrefs.begin("ota", false);
    otaPrefs.putString("firmVer", currentVersion);
    otaPrefs.end();
}

static void onOtaProgress(int cur, int total) {
    esp_task_wdt_reset();
    Serial.printf("OTA progress: %d / %d\n", cur, total);
}

static void onOtaError(int err) {
    Serial.printf("OTA update error: %d\n", err);
}

void loop_ota() {
    if (remoteVersion == "NULL" || remoteVersion == currentVersion) return;
    if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) return;
    if (updateErrorLatched) return;

    if (randomDelayMs == 0) {
        randomDelayMs = (unsigned long)random(1, OTA_MAX_RANDOM_DELAY_S) * 1000UL;
        updateRequestedAtMs = millis();
        Serial.print("New firmware detected, scheduling update in (ms): ");
        Serial.println(randomDelayMs);
        return;
    }

    if (millis() - updateRequestedAtMs < randomDelayMs) return;

    String binPath = String("http://asanautomation.ir/uploads/firmware/BleMqttRelays.ino_") +
                      OTA_MODEL_CODE + "_" + OTA_HARDWARE_VERSION + "." + remoteVersion + ".bin";

    httpUpdate.onStart(onOtaStart);
    httpUpdate.onEnd(onOtaEnd);
    httpUpdate.onProgress(onOtaProgress);
    httpUpdate.onError(onOtaError);

    Serial.print("Starting OTA from: ");
    Serial.println(binPath);

    t_httpUpdate_return ret = httpUpdate.update(wifiClient, binPath);
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("OTA failed (%d): %s\n", httpUpdate.getLastError(),
                           httpUpdate.getLastErrorString().c_str());
            updateErrorLatched = true;
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("OTA: no updates available");
            updateErrorLatched = true;
            break;
        case HTTP_UPDATE_OK:
            Serial.println("OTA update OK, rebooting");
            break;
    }
}
