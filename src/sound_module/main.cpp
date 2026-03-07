/******************************************************************************
 * @file main.cpp
 * Sound module — entry point for the sound/light ESP32 node.
 *
 * @details This replaces the rc_engine_sound src.ino entry point.
 * The input layer (SBUS/IBUS/PWM/PPM reading) is replaced by the ComBus
 * transport HAL. All sound engine logic is preserved unchanged.
 *
 * Node wiring (relative to this sound ESP32):
 *   Serial2 RX ← Machine ESP32 UART TX   (sound_uart_rx)
 *   DAC1 (GPIO25) + DAC2 (GPIO26) → amplifier (unchanged)
 *   GPIO light outputs → unchanged from rc_engine_sound
 *
 * Build environment: sound_node_volvo (see platformio.ini)
 * Required build flags:
 *   -D SOUND_NODE                  identifies this env as a sound node
 *   -D COMBUS_HAL_INPUT            disables RC receiver input in src.ino
 *   -D MACHINE_TYPE=DUMPER_TRUCK   sets ComBus layout
 *   -D <vehicle profile defines>   same as original rc_engine_sound env
 *****************************************************************************/

#include <Arduino.h>

// --- Sound module HAL & transport ---
#include "hal/sound_hal.h"
#include "transport/sound_uart_rx.h"

// --- Vehicle & sound engine includes (rc_engine_sound) ---
// Uncomment (or set -D SOUND_ENGINE_READY) once engine sources are copied
// into src/sound/engine/ (see doc/sound_integration.md for procedure).
#ifdef SOUND_ENGINE_READY
#include "engine/0_GeneralSettings.h"
#include "engine/1_Vehicle.h"
// NOTE: 2_Remote.h is NOT included — channel assignments are in sound_config.h
#include "engine/3_ESC.h"
#include "engine/4_Transmission.h"
#include "engine/5_Shaker.h"
#include "engine/6_Lights.h"
#include "engine/7_Servos.h"
#include "engine/8_Sound.h"
// NOTE: 9_Dashboard.h (LCD TFT) is not used on the sound node
// NOTE: 10_Trailer.h — include only if trailer support is needed
#endif // SOUND_ENGINE_READY


// =============================================================================
// UART PIN ASSIGNMENT  (sound ESP32 side)
// =============================================================================

/// GPIO for UART RX — connect to Machine ESP32 TX pin.
/// Adjust to match your board layout.
#define SOUND_RX_PIN  16   // GPIO16 = Serial2 RX default on ESP32
#define SOUND_TX_PIN  -1   // TX not used (uni-directional link for now)


// =============================================================================
// SETUP
// =============================================================================

void setup() {
      // --- Debug serial (same as rc_engine_sound) ---
    Serial.begin(115200);
    Serial.println(F("[SOUND_NODE] starting up"));

      // --- Initialize ComBus UART receiver ---
    sound_uart_rx_init(SOUND_RX_PIN, SOUND_TX_PIN);

      // --- Initialize sound HAL (neutral pulses, failsafe active) ---
    sound_hal_init();

      // --- Initialize sound engine (rc_engine_sound setup sequence) ---
    //  The engine setup is in: src/sound/engine/sound_engine_setup.cpp
    //  (extracted from the Task0/Task1 setup in the original src.ino)
    //  sound_engine_setup();    // TODO: extract from src.ino setup()
}


// =============================================================================
// LOOP (Task 0 — Arduino main task)
// =============================================================================

void loop() {
      // --- 1. Receive ComBus frame from machine ESP32 ---
    sound_uart_rx_update();

      // --- 2. Translate snapshot → pulseWidth[] ---
    sound_hal_update();

      // --- 3. Run sound + light engine ---
    //  sound_engine_loop();     // TODO: extract from src.ino loop()

      // --- Small yield to prevent watchdog trigger ---
    vTaskDelay(1);
}

// EOF main.cpp
