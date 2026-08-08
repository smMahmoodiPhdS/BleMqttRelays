#include "relayManager.h"
#include "boardConfig.h"
#include <Preferences.h>

#define MAX_RELAY_LISTENERS 4

// G2RL-1-E operate time is ~15 ms and release ~10 ms. Sample well above that so
// we never report the transient, and require a couple of agreeing samples so a
// switching spike on the 5 V rail cannot flip the reported state.
#define RELAY_FB_POLL_MS        50
#define RELAY_FB_STABLE_SAMPLES 3

// A mismatch between commanded and actual is only meaningful once the relay has
// had time to move. Below this it is just the coil settling.
#define RELAY_OVERRIDE_CONFIRM_MS 400

static RelayStatus relayStatus[RELAY_COUNT];
static Preferences relayPrefs;
static RelayStateListener listeners[MAX_RELAY_LISTENERS];
static uint8_t listenerCount = 0;

// Per-channel debounce bookkeeping for the feedback input.
static bool          fbCandidate[RELAY_COUNT];
static uint8_t       fbAgreeCount[RELAY_COUNT];
static unsigned long mismatchSince[RELAY_COUNT];
static unsigned long lastPoll = 0;

static void notifyListeners(uint8_t relayIndex) {
    for (uint8_t i = 0; i < listenerCount; i++) {
        listeners[i](relayIndex, relayStatus[relayIndex]);
    }
}

void relay_addListener(RelayStateListener listener) {
    if (listenerCount < MAX_RELAY_LISTENERS) {
        listeners[listenerCount++] = listener;
    }
}

const char* relay_modeName(RelayMode mode) {
    switch (mode) {
        case RELAY_SYNC_OK:      return "auto";
        case RELAY_OVERRIDE_ON:  return "override_on";
        case RELAY_OVERRIDE_OFF: return "override_off";
        default:                 return "unknown";
    }
}

// Reads SENn and converts to coil state.
//
// Both paths honour the same inversion: a LOW level on the FET drain means the
// coil is pulling current, i.e. the relay is ENERGIZED.
//
// Analog path (ADC1 boards): returns false and leaves *state untouched when the
// divider sits between the thresholds - a disconnected sense resistor, a
// half-driven gate, or a reading caught mid-transition. That ambiguity is
// reported as RELAY_FB_UNKNOWN rather than guessed at.
//
// Digital path (boards with no free ADC1 pin): the comparator is the pad's own
// input buffer, so there is no in-between reading to detect. Margins are wide
// (~850 mV at VIH, ~780 mV at VIL), so this is a loss of diagnostics, not of
// reliability.
static bool readFeedback(uint8_t i, bool* state, uint16_t* mv) {
    if (!boardPins.relayFeedbackAnalog) {
        *state = (digitalRead(boardPins.relayFeedback[i]) == LOW);
        *mv = 0;   // not measured on this board
        return true;
    }

    uint32_t sum = 0;
    for (uint8_t s = 0; s < 4; s++) {
        sum += analogReadMilliVolts(boardPins.relayFeedback[i]);
    }
    uint16_t reading = (uint16_t)(sum / 4);
    *mv = reading;

    if (reading <= RELAY_FB_MV_ON_MAX)  { *state = true;  return true; }
    if (reading >= RELAY_FB_MV_OFF_MIN) { *state = false; return true; }
    return false;
}

void setup_relays() {
    relayPrefs.begin("relays", false);

    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        pinMode(boardPins.relayPin[i], OUTPUT);

        if (boardPins.relayFeedbackPresent) {
            // No internal pull either way: the external divider always defines
            // the level, and a pull-up would fight it.
            pinMode(boardPins.relayFeedback[i], INPUT);
            if (boardPins.relayFeedbackAnalog) {
                // 11 dB attenuation puts the ~3.33 V "released" level inside
                // range (it saturates the top of the curve, which is fine - we
                // only need to clear RELAY_FB_MV_OFF_MIN).
                analogSetPinAttenuation(boardPins.relayFeedback[i], ADC_11db);
            }
        }

        bool restored = relayPrefs.getBool(String("r" + String(i)).c_str(), false);
        relayStatus[i].commanded = restored;
        digitalWrite(boardPins.relayPin[i], restored ? HIGH : LOW);

        relayStatus[i].actual  = restored;    // provisional; corrected below
        relayStatus[i].mode    = boardPins.relayFeedbackPresent ? RELAY_FB_UNKNOWN
                                                                : RELAY_SYNC_OK;
        relayStatus[i].senseMv = 0;
        fbCandidate[i]   = restored;
        fbAgreeCount[i]  = 0;
        mismatchSince[i] = 0;
    }
    relayPrefs.end();

    if (!boardPins.relayFeedbackPresent) {
        // No sense dividers on this board. Do NOT read the pins: they are
        // unwired inputs and would return noise that looks like a relay
        // flipping itself. Report commanded state as measured state and never
        // claim an override - the honest answer is "we cannot tell", and
        // pretending otherwise would put false alarms on the dashboard.
        Serial.println("[relay] coil feedback NOT present on this board — "
                       "reported state is the commanded state, not a measurement");
        for (uint8_t i = 0; i < RELAY_COUNT; i++) {
            Serial.printf("[relay] ch%u gpio=%u cmd=%s\n", i + 1,
                          boardPins.relayPin[i],
                          relayStatus[i].commanded ? "ON" : "OFF");
        }
        return;
    }

    // Let the coils settle, then take a first real reading so the very first
    // MQTT/BLE publish already reflects the slider positions.
    delay(60);
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        bool measured;
        uint16_t mv;
        if (readFeedback(i, &measured, &mv)) {
            relayStatus[i].actual  = measured;
            relayStatus[i].senseMv = mv;
            relayStatus[i].mode    = (measured == relayStatus[i].commanded)
                                       ? RELAY_SYNC_OK
                                       : (measured ? RELAY_OVERRIDE_ON : RELAY_OVERRIDE_OFF);
            fbCandidate[i] = measured;
        }
        Serial.printf("[relay] ch%u gpio=%u fb=%u(%s) cmd=%s actual=%s (%u mV) mode=%s\n",
                      i + 1, boardPins.relayPin[i], boardPins.relayFeedback[i],
                      boardPins.relayFeedbackAnalog ? "adc" : "digital",
                      relayStatus[i].commanded ? "ON" : "OFF",
                      relayStatus[i].actual ? "ON" : "OFF",
                      relayStatus[i].senseMv, relay_modeName(relayStatus[i].mode));
    }
}

void relay_setState(uint8_t relayIndex, bool state) {
    if (relayIndex >= RELAY_COUNT) return;

    bool changed = (relayStatus[relayIndex].commanded != state);
    relayStatus[relayIndex].commanded = state;
    digitalWrite(boardPins.relayPin[relayIndex], state ? HIGH : LOW);

    // Only touch NVS when the command actually changed. The role logic can call
    // this every control tick, and flash has a finite erase budget.
    if (changed) {
        relayPrefs.begin("relays", false);
        relayPrefs.putBool(String("r" + String(relayIndex)).c_str(), state);
        relayPrefs.end();
    }

    if (changed) {
        // Give the relay time to physically move before we call a mismatch an
        // override. Only on a real change - re-asserting the same command must
        // not keep resetting this timer, or a slider override would never be
        // confirmed on a channel the control role re-asserts every tick.
        mismatchSince[relayIndex] = millis();
        Serial.printf("[relay] ch%u commanded %s\n", relayIndex + 1, state ? "ON" : "OFF");

        // Without sense hardware there is nothing to observe, so the command is
        // the only thing we can report. Notify here, since relay_loop() will
        // not.
        if (!boardPins.relayFeedbackPresent) {
            relayStatus[relayIndex].actual = state;
            relayStatus[relayIndex].mode   = RELAY_SYNC_OK;
            notifyListeners(relayIndex);
        }
    }

    // Do not fabricate an "actual" here. relay_loop() will observe the result
    // and notify - including the case where the slider means nothing happens.
}

void relay_loop() {
    // Nothing to poll on a board without sense dividers; relay_setState() has
    // already reported everything that can be known.
    if (!boardPins.relayFeedbackPresent) return;

    unsigned long now = millis();
    if (now - lastPoll < RELAY_FB_POLL_MS) return;
    lastPoll = now;

    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        RelayStatus& st = relayStatus[i];
        bool measured;
        uint16_t mv;

        if (!readFeedback(i, &measured, &mv)) {
            st.senseMv = mv;
            fbAgreeCount[i] = 0;
            if (st.mode != RELAY_FB_UNKNOWN) {
                st.mode = RELAY_FB_UNKNOWN;
                Serial.printf("[relay] ch%u feedback out of range (%u mV)\n", i + 1, mv);
                notifyListeners(i);
            }
            continue;
        }

        st.senseMv = mv;

        // Debounce: a new level has to hold for RELAY_FB_STABLE_SAMPLES polls.
        if (measured != fbCandidate[i]) {
            fbCandidate[i] = measured;
            fbAgreeCount[i] = 1;
            continue;
        }
        if (fbAgreeCount[i] < RELAY_FB_STABLE_SAMPLES) {
            fbAgreeCount[i]++;
            if (fbAgreeCount[i] < RELAY_FB_STABLE_SAMPLES) continue;
        }

        RelayMode newMode = st.mode;
        if (measured == st.commanded) {
            newMode = RELAY_SYNC_OK;
            mismatchSince[i] = now;
        } else if (now - mismatchSince[i] >= RELAY_OVERRIDE_CONFIRM_MS) {
            // Sustained disagreement. Most often the slider is parked in
            // ALWAYS-ON or OFF; a dead FET, an open coil or a blown sense
            // resistor look identical from here, which is why the mode is named
            // for the symptom rather than the cause.
            newMode = measured ? RELAY_OVERRIDE_ON : RELAY_OVERRIDE_OFF;
        }

        if (measured != st.actual || newMode != st.mode) {
            bool wasOverridden = (st.mode == RELAY_OVERRIDE_ON || st.mode == RELAY_OVERRIDE_OFF);
            st.actual = measured;
            st.mode   = newMode;
            Serial.printf("[relay] ch%u observed %s mode=%s (%u mV)\n",
                          i + 1, measured ? "ON" : "OFF", relay_modeName(newMode), mv);
            if (!wasOverridden && (newMode == RELAY_OVERRIDE_ON || newMode == RELAY_OVERRIDE_OFF)) {
                Serial.printf("[relay] ch%u is not following commands - check the "
                              "MT-102 toggle (SW_MODE%u)\n", i + 1, i + 1);
            }
            notifyListeners(i);
        }
    }
}

bool relay_getState(uint8_t relayIndex) {
    if (relayIndex >= RELAY_COUNT) return false;
    return relayStatus[relayIndex].actual;
}

bool relay_getCommanded(uint8_t relayIndex) {
    if (relayIndex >= RELAY_COUNT) return false;
    return relayStatus[relayIndex].commanded;
}

RelayMode relay_getMode(uint8_t relayIndex) {
    if (relayIndex >= RELAY_COUNT) return RELAY_FB_UNKNOWN;
    return relayStatus[relayIndex].mode;
}

const RelayStatus& relay_getStatus(uint8_t relayIndex) {
    static const RelayStatus empty = {false, false, RELAY_FB_UNKNOWN, 0};
    if (relayIndex >= RELAY_COUNT) return empty;
    return relayStatus[relayIndex];
}

bool relay_isOverridden(uint8_t relayIndex) {
    RelayMode m = relay_getMode(relayIndex);
    return m == RELAY_OVERRIDE_ON || m == RELAY_OVERRIDE_OFF;
}

void relay_republishAll() {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) notifyListeners(i);
}
