/*****************************************************************************
 * @file hw_init_drv.h
 * @brief DC drivers hardware initialization and memory allocation
 ***************************************************************************/
#pragma once

#include <ESP32_PWM_Motor.h>
#include <core/config/combus/combus.h>

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// --- Global pointer to DC motor object array ---
extern ESP32_PWM_Motor* dcDevObj;

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

// EOF hw_init_drv.h
