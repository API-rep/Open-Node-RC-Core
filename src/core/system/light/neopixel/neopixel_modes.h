/******************************************************************************
 * @file neopixel_modes.h
 * @brief Sound module — WS2812 NeoPixel animation modes.
 *
 * @details Legacy animation dispatcher for the NEOPIXEL_ENABLED path
 *          (active only when LIGHT_ENABLE is absent).  Moved from main.cpp;
 *          state machine and global references unchanged.
 ******************************************************************************/
#pragma once


// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Drive WS2812 strip animations; called every loop() pass.
 *
 * @details Dispatches on neopixelMode: 1=Demo, 2=Knight Rider, 3=Bluelight,
 *   4=Union Jack, 5=B33lz3bub Austria.  All mode-local timers are static.
 *   No-op when NEOPIXEL_ENABLED is not defined.
 */
void updateRGBLEDs();


// EOF neopixel_modes.h
