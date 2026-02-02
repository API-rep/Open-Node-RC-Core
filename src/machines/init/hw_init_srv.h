/*****************************************************************************
 * @file hw_init_srv.h
 * @brief Servo hardware initialization and configuration
 ***************************************************************************/
#pragma once

#include <ESP32_PWM_Servo.h>
#include <core/config/combus/combus.h>

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// --- Global pointer to Servo object array ---
extern ESP32_PWM_Servo* srvDevObj;

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

// EOF hw_init_srv.h
