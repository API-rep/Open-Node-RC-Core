/*!****************************************************************************
 * @file  machine_defs.h
 * @brief Machine-specific hardware types and mechanical definitions.
 * @details This file defines harware related configuration constants.
 *******************************************************************************/// 

#pragma once

#include <cstdint>
#include <pin_defs.h>


/******************************************************************************
 * @brief Global machine system states definitions
 *******************************************************************************///
 
   /** @brief available run levels */
enum class RunLevel : int8_t {
    NOT_YET_SET = -1,
    IDLE        =  0,
    STARTING    =  1,
    RUNNING     =  2,
    TURNING_OFF =  3,
    SLEEPING    =  4,
    RESET       =  5,
    COUNT            ///< Number of indexable run levels (IDLE … RESET). NOT_YET_SET excluded.
};


// =============================================================================
// PERIPHERAL USAGE TAXONOMY
// =============================================================================

/**
 * @brief Functional role of a device in the vehicle's mechanical system.
 *
 * @details Each `DcDevice` or `SrvDevice` entry in a machine config declares its
 *   `usage` so that output modules (sound, lighting, telemetry) can determine the
 *   correct behavior without manual channel mapping.
 *
 *   Values are categorised by nibble: the upper 4 bits identify the category,
 *   the lower 4 bits identify the variant within that category.
 *   Use `devUsageCategory()` to compare categories without enumerating variants.
 *
 *   Category map:
 *     0x00        — undefined
 *     0x10–0x1F   — traction (wheel, track)
 *     0x20–0x2F   — hydraulics (linear, rotary, assist, pump)
 *     0x30–0x3F   — steering (servo, motor)
 *     0x40–0x4F   — signals (horn, light, solenoid)
 *     0x50–0xEF   — reserved for future categories
 */
enum class DevUsage : uint8_t {

    UNDEFINED     = 0x00,   ///< Not declared — safe default

    // --- Traction (0x10–0x1F) ---
    TRACT_WHEEL   = 0x10,   ///< Propulsion wheel (skid-steer, axle-diff …) → ENGINE_THROTTLE sound role
    TRACT_TRACK   = 0x11,   ///< Propulsion track (crawler) → ENGINE_THROTTLE sound role

    // --- Hydraulics (0x20–0x2F) ---
    HYD_LINEAR    = 0x20,   ///< Linear hydraulic cylinder (arm, bucket, dump …) → HYDRAULIC_ARM sound role
    HYD_ROTARY    = 0x21,   ///< Rotary hydraulic motor → HYDRAULIC_PUMP sound role
    HYD_ASSIST    = 0x22,   ///< Power-steering hydraulic assist → HYDRAULIC_STEER sound role (reduced volume)
    HYD_PUMP      = 0x23,   ///< Standalone hydraulic pump (no direct mechanical output)
    HYD_WINCH     = 0x24,   ///< Winch drum actuator (pull/release)

    // --- Steering (0x30–0x3F) ---
    STEER_SERVO   = 0x30,   ///< Servo-controlled steering
    STEER_MOTOR   = 0x31,   ///< Motor-driven steering actuator

    // --- Signals (0x40–0x4F) ---
    SIG_HORN      = 0x40,   ///< Audible horn or buzzer → HORN sound role
    SIG_LIGHT     = 0x41,   ///< Lighting output (no direct sound role)
    SIG_SOLENOID  = 0x42,   ///< Discrete solenoid valve or relay
    SIG_IGNITION  = 0x43,   ///< Engine ignition key → ENGINE_ON sound event
    SIG_BEACON    = 0x44,   ///< Rotating beacon protocol trigger (servo pulse sequencer)
};

/// Returns the category nibble (upper 4 bits) of a `DevUsage` value.
constexpr uint8_t devUsageCategory(DevUsage u) {
    return static_cast<uint8_t>(u) & 0xF0u;
}





/**
 * @brief DC drivers and attached devices definition file
 */

  /** @brief Available DC device types */
enum class DcDevType : uint8_t {
    UNDEFINED   = 0,    ///< Undefined type
    DC_MOTOR    = 1,    ///< Standard brushed DC motor
    DC_ACTUATOR = 2,    ///< Linear actuator (screw/piston)
    SOLENOID    = 3     ///< Electromagnetic plunger
};

  /** @brief DC driver output signal type */
enum class DcDrvSignal : uint8_t {
    UNDEFINED                = 0,    ///< Mode not set
    PWM_TWO_WAY_NEUTRAL_CENTER = 1,  ///< Bidirectional (CW/CCW) with stop at 50% duty cycle — plain PWM or H-bridge ESC
    PWM_ONE_WAY              = 2,    ///< Unidirectional (0-100% duty cycle, sense set via polarity setting)
    SERVO_SIG_NEUTRAL_CENTER = 3,    ///< ESC via servo signal — ServoCore, 50 Hz (1000–2000 µs, single pin)
};



/**
 * @brief Servos and attached devices definition file
 */

  /** @brief available servo devices type */
enum class SrvDevType : uint8_t {
    UNDEFINED = 0,    ///< Uninitialized servo
    SERVO     = 1     ///< Standard position-controlled servo
    // GPIO   = 2     // GPIO on servo port
};

/**
 * @brief ESC output protocol / library selector.
 *
 * @details Drives init and write dispatch in hw_init_esc — callers use
 *   `esc_write()` without any protocol-specific branching.
 *
 *   PWM_SERVO_SIG — external crawler ESC wired to a single servo-protocol pin
 *                 (50 Hz, 1000–2000 µs pulse-width).  Uses ServoCore.
 *   PWM_HBRIDGE — onboard H-bridge receiving duty-cycle PWM.  Uses DcMotorCore.
 *                 Requires a separate dir pin for bidirectional control.
 */
enum class EscType : uint8_t {
    UNDEFINED    = 0,   ///< Not declared — safe default
    PWM_SERVO_SIG = 1,   ///< External RC ESC — ServoCore, 50 Hz servo protocol (1 pin)
    PWM_HBRIDGE  = 2,   ///< On-board H-bridge — DcMotorCore, duty-cycle PWM (pwmPin + dirPin)
};


// EOF machine_defs.h