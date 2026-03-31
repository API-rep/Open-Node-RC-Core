/*!****************************************************************************
 * @file  statusLED.cpp
 * @brief LED / motor PWM output controller — implementation.
 *
 * @details Fork of TheDIYGuy999/statusLED.  All direct LEDC hardware calls
 *   replaced by PwmControl::setDuty() via the private _write() helper.
 *   Original: https://github.com/TheDIYGuy999/statusLED  (public domain)
 *****************************************************************************/
#include "statusLED.h"


// =============================================================================
// CONSTRUCTOR
// =============================================================================

statusLED::statusLED(bool inverse) {
    _state          = 0;
    _previousMillis = 0;
    _inverse        = inverse;
}


// =============================================================================
// BEGIN
// =============================================================================

void statusLED::begin(PwmControl* ctrl) {
    _ctrl    = ctrl;
    _maxDuty = ctrl ? ctrl->getMaxDuty() : 255u;
    _write(0);
}


// =============================================================================
// INTERNAL WRITE HELPER
// =============================================================================

void statusLED::_write(int brightness) {
    if (!_ctrl) return;
    uint32_t duty = static_cast<uint32_t>(brightness) * _maxDuty / 255u;
    if (_inverse) duty = _maxDuty - duty;
    _ctrl->setDuty(duty);
}


// =============================================================================
// FLASH  (non-blocking state machine — call every loop iteration)
// =============================================================================

bool statusLED::flash(unsigned long onDuration,
                      unsigned long offDuration,
                      unsigned long pauseDuration,
                      int           pulses,
                      int           delay,
                      int           bulbSimRamp,
                      int           flashOffBrightness) {
    _onDuration         = onDuration;
    _offDuration        = offDuration;
    _pauseDuration      = pauseDuration;
    _bulbSimRamp        = bulbSimRamp;
    _pulses             = pulses;
    _delay              = delay;
    _flashOffBrightness = flashOffBrightness;

    unsigned long currentMillis = millis();

    switch (_state) {
        case 0:
            _previousMillis = currentMillis;
            _state = 1;
            break;

        case 1:
            if (currentMillis - _previousMillis >= _delay) _state = 2;
            break;

        case 2:
            _up   = true;
            _down = false;
            _pulseCnt++;
            _previousMillis = currentMillis;
            _state = 3;
            _start = true;
            break;

        case 3:
            if (currentMillis - _previousMillis >= _onDuration) {
                _state = 4;
                _start = false;
            }
            break;

        case 4:
            _down = true;
            _up   = false;
            _previousMillis = currentMillis;
            _state = 5;
            break;

        case 5:
            if (currentMillis - _previousMillis >= _offDuration) _state = 6;
            break;

        case 6:
            if (_pulseCnt >= _pulses) {
                _pulseCnt = 0;
                _previousMillis = currentMillis;
                _state = 7;
            } else {
                _state = 2;
            }
            break;

        case 7:
            if (currentMillis - _previousMillis >= _pauseDuration) _state = 2;
            break;
    }

    if (bulbSimRamp > 0) {
        if (micros() - _previousFlashRampMillis >= static_cast<unsigned long>(_bulbSimRamp)) {
            _previousFlashRampMillis = micros();
            if (_up   && _flashBrightness < 255)                  _flashBrightness++;
            if (_flashBrightness == 255 || _down)                 _up   = false;
            if (_down && _flashBrightness > _flashOffBrightness)  _flashBrightness--;
            if (_flashBrightness == _flashOffBrightness)          _down = false;
        }
    } else {
        if (_up)   { _flashBrightness = 255;                 _up   = false; }
        if (_down) { _flashBrightness = _flashOffBrightness; _down = false; }
    }

    _write(_flashBrightness);
    return _start;
}


// =============================================================================
// ON / OFF / PWM
// =============================================================================

void statusLED::on() {
    _state    = 0;
    _pulseCnt = 0;
    _write(255);
}

void statusLED::off(int bulbSimRamp, int offOffBrightness) {
    _state            = 0;
    _pulseCnt         = 0;
    _offBulbSimRamp   = bulbSimRamp;
    _offOffBrightness = offOffBrightness;  // fix: original param shadowed private member

    if (_offBulbSimRamp > 0) {
        if (micros() - _previousOffRampMillis >= static_cast<unsigned long>(_offBulbSimRamp)) {
            _previousOffRampMillis = micros();
            if (_offBrightness > _offOffBrightness) _offBrightness--;
        }
    } else {
        _offBrightness = _offOffBrightness;
    }

    _write(_offBrightness);
}

void statusLED::pwm(int brightness) {
    _state      = 0;
    _pulseCnt   = 0;
    _brightness = brightness;
    _write(_brightness);
}

// EOF statusLED.cpp
