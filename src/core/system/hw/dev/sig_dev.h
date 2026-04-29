/*****************************************************************************
 * @file sig_dev.h
 * @brief Signal device configuration check and initialization.
 *
 * @details A signal device (SigDevice) is a lightweight logical entry that
 *   maps a ComBus channel to a discrete output role (horn, light, solenoid).
 *   Unlike DC drivers or servos, signal devices carry no hardware driver
 *   object — they have no PWM timer, no ramp table, no motion state.
 *   Their sole purpose is to give the ComBus dispatcher a typed, named
 *   channel so that the sound interpreter and the combus sound interpreter
 *   can resolve which output role a given analog or digital channel drives.
 *
 *   They were introduced to avoid hard-coded channel-index constants in the
 *   sound HAL: instead of "analog[2] == horn", the dispatcher reads the
 *   SigDevice table and finds "analog[2] has usage SIG_HORN" without any
 *   manual mapping.
 *****************************************************************************/
#pragma once

#include <core/config/machines/combus_types.h>


// =============================================================================
// 1. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify signal device configuration coherence.
 *
 * @details Checks that each signal array index matches its declared ID and
 *   that only one combus's analogChannel or digitalChannel is set.
 *   Returns true when at least one error is detected — halting is the
 *   caller's responsibility.
 */

bool checkSigHwConfig(const EnvCfg &config);



// =============================================================================
// 2. INITIALIZATION
// =============================================================================

/**
 * @brief Log and validate signal devices from machine configuration.
 *
 * @details Signal devices have no hardware driver object — this step logs
 *   each configured entry for debug traceability.
 */

void sigDevInit(const EnvCfg &config);

// EOF sig.h
