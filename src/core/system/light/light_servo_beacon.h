/******************************************************************************
 * @file light_servo_beacon.h
 * @brief Servo-driven beacon protocol update (light side).
 *
 * @details Drives the beacon controller connected on servo CH3 when the
 *   corresponding `SrvDevice` usage is `DevUsage::SIG_BEACON`.
 *   The pulse stepping protocol is handled here so light behavior stays grouped
 *   in the light subsystem, even when the physical actuator is a servo output.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. API
// =============================================================================

/// Update the servo-beacon state machine once per control loop.
/// @param blueLightOn  True when rotating beacon should be active.
void light_servo_beacon_update(bool blueLightOn);


// EOF light_servo_beacon.h
