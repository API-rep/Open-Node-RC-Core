/*!****************************************************************************
 * @file  statusLED.h
 * @brief LED / motor PWM output controller — blinking, flash, pwm, on, off.
 *
 * @details Fork of TheDIYGuy999/statusLED adapted for Open Node RC Core.
 *   The original library managed its own LEDC channel via direct
 *   ledcSetup() / ledcAttachPin() / ledcWrite() calls.  This fork delegates
 *   channel allocation to PwmBroker: callers request a PwmControl lease
 *   from PwmBroker::getInstance().requestResource() and pass the raw pointer
 *   to begin().  Ownership of the lease stays with the caller.
 *
 *   Changes vs original:
 *     - begin(pin, channel, freq, resolution) → begin(PwmControl* ctrl)
 *     - All ledcWrite calls replaced by _write() helper via PwmControl
 *     - AVR platform path removed (ESP32-only project)
 *     - Bug fix: off() parameter shadowed private member _offOffBrightness
 *
 *   State machine (flash), bulbSimRamp logic and inverse polarity handling
 *   are preserved unchanged from the original.
 *
 *   Original: https://github.com/TheDIYGuy999/statusLED  (public domain)
 *****************************************************************************/
#pragma once

#include <Arduino.h>
#include <PwmControl.h>


class statusLED {
public:
    explicit statusLED(bool inverse);

    /// Bind this channel to a PwmControl lease obtained from PwmBroker.
    /// @param ctrl  Non-owning raw pointer — caller retains ownership and
    ///              must ensure the PwmControl outlives this object.
    void begin(PwmControl* ctrl);

    bool flash(unsigned long onDuration,
               unsigned long offDuration,
               unsigned long pauseDuration,
               int           pulses,
               int           delay              = 0,
               int           bulbSimRamp        = 0,
               int           flashOffBrightness = 0);

    void on();
    void off(int bulbSimRamp = 0, int offOffBrightness = 0);
    void pwm(int brightness);

private:
    /// Scale brightness (0–255) → duty (0–_maxDuty), apply _inverse, write.
    void _write(int brightness);

    PwmControl* _ctrl     = nullptr;
    uint32_t    _maxDuty  = 255u;

    unsigned long _onDuration;
    unsigned long _offDuration;
    unsigned long _pauseDuration;
    unsigned long _delay;
    int _pulses;
    int _pulseCnt           = 0;
    int _brightness;
    int _flashBrightness    = 0;
    int _flashOffBrightness = 0;
    int _offBrightness      = 0;
    int _offOffBrightness   = 0;
    int _bulbSimRamp;
    int _offBulbSimRamp;
    unsigned long _previousMillis          = 0;
    unsigned long _previousFlashRampMillis = 0;
    unsigned long _previousOffRampMillis   = 0;
    byte  _state   = 0;
    bool  _inverse;
    bool  _start;
    bool  _up;
    bool  _down;
};

// EOF statusLED.h
