/*!****************************************************************************
 * @file drv_control.h
 * @brief DC drivers control functions
 * This section contain function used to control DC drivers durring init and runtime
 * By this it :
 * - Batch apply states (sleep, enable ...)
 * 
 * TODO:
 * - setSleepPin() - Init function : configure controller sleep pin (output - LOW/HIGH)
 * - setEnablePin() - Init function : configure controller enable pin (output - LOW/HIGH)
 * - setFaultPin() - Init function : configure controller fault pin (input)
 * - breakAllDrivers (CH-A, CH-B, ALL) - via lib ESP32PWM_MOTOR
 * - breakDriver (DRV_) - break only one driver (in single mode)
 * - donotBreakAllDrivers (CH-A, CH-B, ALL) via lib ESP32PWM_MOTOR
 * - donotBreakDriver (DRV_) - stop breaking only one driver (in single mode)
 * - stopAllDrivers()
 * - stopDriver(DRV_)
 * 
 * Once ready, put all these function into machine.drvDev[] structure
 *******************************************************************************///
#pragma once

#include <config/config.h>
#include <init/init.h>

/**
 * @brief Sleep all DC drivers
 * Sleep all DC drivers if hardware sleep pin is available
 * 
 */
 void sleepAllDcDrivers(const Machine &config);



 /**
 * @brief Wakeup all DC drivers
 * Wakeup all DC drivers if hardware sleep pin is available
 * 
 */
 void sleepAllDcDrivers(const Machine &config);

 // EOF drv_control.h