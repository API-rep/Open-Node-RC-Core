/*****************************************************************************
 * @file hw_init_sig.h
 * @brief Signal device configuration check and initialization.
 *****************************************************************************/
#pragma once

#include <core/config/combus/combus_types.h>


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

bool checkSigHwConfig(const Machine &config);



// =============================================================================
// 2. INITIALIZATION
// =============================================================================

/**
 * @brief Log and validate signal devices from machine configuration.
 *
 * @details Signal devices have no hardware driver object — this step logs
 *   each configured entry for debug traceability.
 */

void sigDevInit(const Machine &config);

// EOF hw_init_sig.h
