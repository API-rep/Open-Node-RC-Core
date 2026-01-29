/*!****************************************************************************
 * @file hw_init_drv.h
 * @brief DC drivers initialisation script
 * This section contain scripts an routines used to parse DC drivers config file and
 * initialize devices.
 * By this it :
 * - Initialize and allocate DC drivers ojects pointer
 * - Parse config file and fill child's unset value from parent's one
 * - Create harware device object and initalize them with config file value
 * - Initialize DC drivers defined for the machine configuration
 * 
* This script MUST be include in hw_init.h top file
 *******************************************************************************/// 

#pragma once

#include <struct.h>
#include <const.h>
#include <macro.h>

#include <config/config.h>

#include <ESP32_PWM_Motor.h>

/**
 * @brief Initialize and allocate DC drivers pointer
 * @param count Number of DC driver to create
 */

extern ESP32_PWM_Motor* dcDevObj;

void allocateDrivers(int8_t count);



/**
 * @brief Apply DC driver parent's configutation to child
 * If parentID is set for a driver, unset child's entry will be fill with parent value
 * 
 * @param count Number of DC driver to create
 */

void applyParentConfig(Machine &config);


//          /**
//           * @brief Initialize DC drivers defined for the machine configuration
//           * Parse the DC drivers config structure and initialize DC driver objects.
//           * 
//           * @param config Reference to the machine configuration structure
//           */
//          
//          void dcDriverPortInit(DriverPort& port);


/**
 * @brief Initialize DC drivers defined for the machine configuration
 * Parse the DC drivers config structure and initialize DC driver objects.
 * 
 * @param config Reference to the machine configuration structure
 */

void dcDriverInit(const Machine &config);