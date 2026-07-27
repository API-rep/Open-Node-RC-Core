/**
 * @file input_manager.h
 * @brief Input management module for Open-Node-RC-Core
 *
 * @details Core-only responsibility: physical acquisition of the configured
 *   input device and device-level processing (deadband, connection state).
 *   Produces the input device value tables (inputAnalogValue / inputDigitalValue)
 *   consumed downstream by the machine-side input->ComBus mapping
 *   (see machines/system/input/input_update.h). This header never depends on
 *   ComBus or any machine-specific mapping — that dependency direction is
 *   reserved to the machine layer.
 */

#pragma once

#include <core/config/inputs/inputs.h>       // AnalogInputDevID, DigitalInputDevID, inputDev
#include <core/system/debug/logging/debug.h>

/** @brief Initialize the selected input module */
void input_setup();


/**
 * @brief Refresh physical input device values.
 * @details Pure device-domain refresh: physical acquisition + device-level
 *   deadband filtering + connection-loss detection. Never touches ComBus.
 *   On signal loss, leaves inputAnalogValue[]/inputDigitalValue[] at their
 *   last known state — input_is_connected() is the source of truth for the
 *   machine layer, which owns the failsafe behavior.
 * NOTE:
 * - ".isInverted" parrameter of Analog/digitalInputDevArray not used. Implementation
 *   had to be done later
 */

void input_refresh();


/** @brief True while the input source is actively reporting data. */
bool input_is_connected();

/** @brief Current processed value per analog device (raw device domain, post-deadband). */
extern int16_t inputAnalogValue[static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)];

/** @brief Current raw state per digital device (unprocessed — no inversion applied). */
extern bool inputDigitalValue[static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)];

/** @brief Return the configured input device name (from input config infoName). */
const char* input_get_name();

// EOF input_manager.h