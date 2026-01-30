/*!****************************************************************************
 * @file  machine_defs.h
 * @brief Machine-specific hardware types and mechanical definitions.
 * @details This file defines harware related configuration constants.
 *******************************************************************************/// 

#pragma once

#include <cstdint>


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
    RESET       =  5
};


/******************************************************************************
 * @brief Motherboard related hardware definitions
 *******************************************************************************/// 

   /** @brief Available devices usages */
enum class DevUsage : uint8_t {
    UNDEFINED    = 0,    ///< Undefined usage
    GEN_WHEEL    = 1,    ///< Generic propulsion wheel
    GEN_ACTUATOR = 2     ///< Generic hydraulic actuator
};



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

  /** @brief Available DC driver mode */
enum class DcDrvMode : uint8_t {
    UNDEFINED              = 0,    ///< Mode not set
    TWO_WAY_NEUTRAL_CENTER = 1,    ///< Bidirectional (CW/CCW) with stop at 50% duty cycle
    ONE_WAY                = 2     ///< Unidirectional (0-100% duty cycle, sense set via polarity setting)
};



/**
 * @brief Servos and attached devices definition file
 */

  /** @brief available servo devices type */
enum class SrvDevType : uint8_t {
    UNDEFINED = 0,    ///< Uninitialized servo
    SERVO     = 1     ///< Standard position-controlled servo
    // GPIO   = 2    // GPIO on servo port
};

// EOF machine_defs.h