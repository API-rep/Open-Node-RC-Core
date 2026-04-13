/*****************************************************************************
 * @file hw_init_srv.h
 * @brief Servo hardware initialization and configuration
 ***************************************************************************/
#pragma once

#include <ServoCore.h>
#include <core/config/machines/combus_types.h>

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// --- Global pointer to Servo object array ---
extern ServoCore* srvDevObj;

/**
 * @brief Initialize and allocate Servo objects in RAM
 */
void allocateServos(int8_t count);

// =============================================================================
// 2. HARDWARE INITIALIZATION
// =============================================================================

/**
 * @brief Initialize all servos defined for the machine configuration
 */
void servoInit(const Machine &config);


// =============================================================================
// 3. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify servo configuration coherence.
 * @return true when at least one error is detected.
 */
bool checkSrvHwConfig(const Machine &config);

// EOF hw_init_srv.h
