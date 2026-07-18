#include "thresholdControl.h"
#include "relayManager.h"

void ThresholdControl::configure(ThresholdDir dir, float hysteresis,
                                 unsigned long minOnMs, unsigned long minOffMs) {
    _dir = dir;
    _hyst = hysteresis;
    _minOnMs = minOnMs;
    _minOffMs = minOffMs;
}

void ThresholdControl::addRelay(uint8_t relayIndex) {
    if (_relayCount < 4) _relays[_relayCount++] = relayIndex;
}

bool ThresholdControl::ready() const {
    // RAISE needs the lower limit; LOWER needs the upper limit; both need current.
    if (!_hasCur) return false;
    return (_dir == DIR_RAISE) ? _hasLower : _hasUpper;
}

bool ThresholdControl::update() {
    if (!ready()) return false;

    unsigned long now = millis();
    bool prev = _on;

    bool want, release;
    if (_dir == DIR_RAISE) {
        want    = _cur <= (_lower - _hyst);
        release = _cur >= (_lower + _hyst);
    } else {  // DIR_LOWER
        want    = _cur >= (_upper + _hyst);
        release = _cur <= (_upper - _hyst);
    }

    if (_on) {
        if (release && (now - _onSince) >= _minOnMs) {
            _on = false;
            _offSince = now;
        }
    } else {
        if (want && (now - _offSince) >= _minOffMs) {
            _on = true;
            _onSince = now;
        }
    }

    if (_on != prev) {
        for (uint8_t i = 0; i < _relayCount; i++) relay_setState(_relays[i], _on);
        return true;
    }
    return false;
}
