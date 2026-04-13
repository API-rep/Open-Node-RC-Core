/*****************************************************************************
 * @file hw_init_drv.h
 * @brief DC drivers hardware initialization and memory allocation
 ***************************************************************************/
#pragma once

#include <DcMotorCore.h>
#include <core/config/machines/combus_types.h>

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// --- Global pointer to DC motor object array ---
extern DcMotorCore* dcDevObj;

/**
 * @brief Initialize and allocate DC driver objects in RAM
 */
void allocateDrivers(int8_t count);



// =============================================================================
// 2. CONFIGURATION INHERITANCE
// =============================================================================

/**
 * @brief Apply parent's configuration to child drivers
 */
void applyParentConfig(const Machine &config);



// =============================================================================
// 3. HARDWARE INITIALIZATION
// =============================================================================

/**
 * @brief Initialize DC drivers defined for the machine configuration
 */
void dcDriverInit(const Machine &config);


// =============================================================================
// 4. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify DC driver configuration coherence.
 * @return true when at least one error is detected.
 */
bool checkDrvHwConfig(const Machine &config);

// EOF hw_init_drv.h
