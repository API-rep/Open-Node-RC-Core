/******************************************************************************
 * @file sound_hal.cpp
 * Sound module — ComBus → rc_engine_sound hardware abstraction layer.
 *
 * @details Reads the latest UART snapshot and writes the rc_engine_sound
 * global pulseWidth[] and failSafe variables. Channel assignments follow
 * the Flysky FS-i6X profile convention used as the base remote config:
 *
 *   pulseWidth[1]  STEERING  ← ComBus STEERING_BUS
 *   pulseWidth[3]  THROTTLE  ← ComBus DRIVE_SPEED_BUS
 *   pulseWidth[5]  HORN      ← ComBus HORN (digital → 1000/2000)
 *   pulseWidth[2]  FUNCTION_R← ComBus LIGHTS (digital → 1000/2000)
 *   pulseWidth[6]  GEARBOX   ← ComBus DUMP_BUS
 *
 * Global variables written:
 *   - pulseWidth[]   (extern uint16_t[14] from src.ino)
 *   - pulseZero[]    (extern uint16_t[14] from src.ino) — set at init only
 *   - pulseLimit     (extern uint16_t from src.ino)    — set at init only
 *   - failSafe       (extern volatile bool from src.ino)
 *****************************************************************************/

#include "sound_hal.h"

#include <Arduino.h>

#include "../transport/sound_uart_rx.h"
#include "../config/sound_config.h"
#include <core/transport/combus_transport.h>


// =============================================================================
// 1. CHANNEL INDEX MAPPING
// =============================================================================
//
//  These match the Flysky FS-i6X remote profile in 2_Remote.h.
//  If a different remote profile is selected in the sound env, update these.
//

#define SOUND_CH_STEERING    1u   ///< pulseWidth index for steering input
#define SOUND_CH_FUNCTION_R  2u   ///< pulseWidth index for FUNCTION_R (lights, jake)
#define SOUND_CH_THROTTLE    3u   ///< pulseWidth index for throttle
#define SOUND_CH_FUNCTION_L  4u   ///< pulseWidth index for FUNCTION_L (indicators)
#define SOUND_CH_HORN        5u   ///< pulseWidth index for horn / siren
#define SOUND_CH_GEARBOX     6u   ///< pulseWidth index for gearbox / left stick


// =============================================================================
// 2. EXTERN DECLARATIONS  (rc_engine_sound globals from src.ino)
// =============================================================================
//
//  These externs are only available once the engine source is integrated.
//  Until then (-D SOUND_ENGINE_READY not set), local stubs are used so the
//  module compiles cleanly during the scaffolding phase.
//
#ifdef SOUND_ENGINE_READY
extern uint16_t pulseWidth[SOUND_HAL_PULSE_ARRAY_SIZE];
extern uint16_t pulseZero[SOUND_HAL_PULSE_ARRAY_SIZE];
extern uint16_t pulseLimit;
extern volatile bool failSafe;
#else
  // Scaffolding stubs — removed once SOUND_ENGINE_READY is defined
static uint16_t pulseWidth[SOUND_HAL_PULSE_ARRAY_SIZE];
static uint16_t pulseZero[SOUND_HAL_PULSE_ARRAY_SIZE];
static uint16_t pulseLimit  = 1100u;
static bool     failSafe    = false;
#endif


// =============================================================================
// 3. PRIVATE HELPERS
// =============================================================================

/**
 * Linear map of a ComBus uint16_t value to pulse width in µs.
 *
 * @details ComBus range [0, 65535] → pulse [1000, 2000] µs.
 *
 * @param cbVal  Raw ComBus analog value (0–65535).
 * @return Pulse width in microseconds.
 */
static inline uint16_t cbValToPulse(uint16_t cbVal) {
    return (uint16_t)map((long)cbVal, 0L, 65535L,
                         (long)SOUND_PULSE_MIN_US,
                         (long)SOUND_PULSE_MAX_US);
}

/** Fill all pulse channels with neutral (1500 µs). */
static void applyNeutral() {
    for (uint8_t i = 0u; i < SOUND_HAL_PULSE_ARRAY_SIZE; ++i) {
        pulseWidth[i] = SOUND_PULSE_CTR_US;
    }
}


// =============================================================================
// 4. PUBLIC API
// =============================================================================

/**
 * Initialize the sound HAL — neutral pulse widths, failsafe active.
 */
void sound_hal_init() {
      // --- Set neutral pulse widths ---
    applyNeutral();

      // --- Initialize calibration arrays to match neutral defaults ---
    for (uint8_t i = 0u; i < SOUND_HAL_PULSE_ARRAY_SIZE; ++i) {
        pulseZero[i] = SOUND_PULSE_CTR_US;
    }
    pulseLimit = 1100u;   // rc_engine_sound standard value

      // --- Begin with failsafe active until first valid frame ---
    failSafe = true;
}

/**
 * Update pulseWidth[] from latest ComBus snapshot.
 */
void sound_hal_update() {
    const ComBusSnapshot* snap = sound_uart_rx_snapshot();

      // --- Link-loss failsafe ---
    if (!snap || !sound_uart_rx_is_alive(SOUND_HAL_FAILSAFE_TIMEOUT_MS)) {
        failSafe = true;
        applyNeutral();
        return;
    }

      // --- Machine-side failsafe propagation ---
    bool machineFail = (snap->flags & COMBUS_FLAG_FAILSAFE) != 0u;
    failSafe = machineFail;

    if (machineFail) {
        applyNeutral();
        return;
    }

      // --- Analog channels → pulse width ---
    uint8_t chThrottle = static_cast<uint8_t>(SOUND_CB_THROTTLE);
    uint8_t chSteering = static_cast<uint8_t>(SOUND_CB_STEERING);
    uint8_t chGearbox  = static_cast<uint8_t>(SOUND_CB_GEARBOX);

    if (chThrottle < snap->nAnalog) {
        pulseWidth[SOUND_CH_THROTTLE]   = cbValToPulse(snap->analog[chThrottle]);
    }
    if (chSteering < snap->nAnalog) {
        pulseWidth[SOUND_CH_STEERING]   = cbValToPulse(snap->analog[chSteering]);
    }
    if (chGearbox < snap->nAnalog) {
        pulseWidth[SOUND_CH_GEARBOX]    = cbValToPulse(snap->analog[chGearbox]);
    }

      // --- Digital channels → ON/OFF pulse ---
    uint8_t chHorn   = static_cast<uint8_t>(SOUND_CB_HORN);
    uint8_t chLights = static_cast<uint8_t>(SOUND_CB_LIGHTS);

    if (chHorn < snap->nDigital) {
        pulseWidth[SOUND_CH_HORN]       = snap->digital[chHorn]
                                          ? SOUND_HAL_DIGITAL_ON_US
                                          : SOUND_HAL_DIGITAL_OFF_US;
    }
    if (chLights < snap->nDigital) {
        pulseWidth[SOUND_CH_FUNCTION_R] = snap->digital[chLights]
                                          ? SOUND_HAL_DIGITAL_ON_US
                                          : SOUND_HAL_DIGITAL_OFF_US;
    }

      // --- ENGINE ON via RunLevel + keyOn flag ---
    //  The rc_engine_sound engine-on state is driven by pulseWidth[FUNCTION_R].
    //  RunLevel RUNNING is additionally used to ensure the engine is requested.
    //  keyOn propagates through the COMBUS_FLAG_KEY_ON bit in flags.
    bool keyOn = (snap->flags & COMBUS_FLAG_KEY_ON) != 0u;
    bool running = ((RunLevel)snap->runLevel == RunLevel::RUNNING ||
                    (RunLevel)snap->runLevel == RunLevel::STARTING);

    if (!keyOn && !running) {
          // Force engine off when key is removed
        pulseWidth[SOUND_CH_FUNCTION_R] = SOUND_HAL_DIGITAL_OFF_US;
    }
}

/**
 * Return true if HAL is currently in failsafe.
 */
bool sound_hal_is_failsafe() {
    return failSafe;
}

// EOF sound_hal.cpp
