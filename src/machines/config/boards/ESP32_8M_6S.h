/*!****************************************************************************
 * @file  ESP32_8M_6S.h
 * @brief ESP32 based 8 motors, 6 servos motherboard 
 * This board embeed :
 * - 8 DC motors driver with break, sleep, enable and fault feature
 * - 6 servos output with two dedicate power input line
 * - Battery monitoring (main, servo channel A and B)
 * - One extension port for external devices
 * 
 *  for more info : http://......
 * 
 * TODO:
 * - Create array + enum of com + ext port and put them in boardCfg
 * - Create VBAT sesing structure (boardCfg sub section)
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

/**
 * Board devices definition
 * Place here all config of devices embedded on the board such as :
 * - DC drivers ports
 * - Servo ports
 * - Battery voltage sensing circuit
 * 
 * NOTE:
 * - enum entry and devices Array[] MUST have the same order
 * - if a entry is not used :
 *   -> set  .devicePtr = nullptr
 */


/**
 * DC drivers configuration section
 * DC drivers group is devided in two channels (A and B). Some features are 
 * share by each channels or specific per channel.
 */

  // DC drivers index. Do not remove DRV_COUNT if section is empty
enum DrvPort { DRV_PORT_1A = 0,
               DRV_PORT_2A,
               DRV_PORT_3A,
               DRV_PORT_4A,
               DRV_PORT_1B,
               DRV_PORT_2B,
               DRV_PORT_3B,
               DRV_PORT_4B, 
               DRV_PORT_COUNT };

  // DC driver config structure declaration. Edit data in cpp file
extern DriverPort drvPortArray[DRV_PORT_COUNT];

  // DC drivers break channel index  
enum DrvPortDecayCh { CH_A = 0, CH_B, CH_COUNT };
  
  // DC drivers break channel
static constexpr int8_t DECAY_PIN[CH_COUNT] = { 26, 15 };  // CH_A, CH_B ... break channel pin
                                            
  // DC drivers control pin
static constexpr int8_t DRV_EN_PIN = 33;
static constexpr int8_t DRV_SLP_PIN = 25;
static constexpr int8_t DRV_FLT_PIN = 34;



/**
 * Servo line is devided in two channels (A and B)
 * Each channels have it's own power input who can be monitored
 */

  // servo output index Do not remove SRV_COUNT if section is empty
enum SrvPort { SRV_PORT_1A = 0,
               SRV_PORT_2A,
               SRV_PORT_3A,
               SRV_PORT_1B,
               SRV_PORT_2B,
               SRV_PORT_3B,
               SRV_PORT_COUNT };

  // servo outputs config structure declaration. Edit data in cpp file
extern ServoPort srvPortArray[SRV_PORT_COUNT];



/**
 * Extension and communication ports.
 * Can be use for serial connection, I2C, extension board output... 
 */

  // extension ports
static constexpr int8_t EXT1_PIN = 18;   // extension port 1
static constexpr int8_t EXT2_PIN = 13;   // extension port 2

  // built-in ESP32 ESP32-DevKitC V4 built-in serial port (connected to USB port). 

static constexpr int8_t TXD0_PIN = 1;    // ESP32 built-in TX pin
static constexpr int8_t RXD0_PIN = 3;    // ESP32 built-in RX pin



/**
 * @brief Board config structure definition
 * Place here all board configuration contants
 * NOTE:
 * -If no ServoPort and/or DriverPort is need, place .xxxCount to 0 and xxxConfig to nullptr
 */

inline constexpr Board boardCfg {
  .infoName = "ESP32 8 motors + 6 servos motherboard", // board name
  .drvPort = drvPortArray,                           // DC driver device config structure
  .drvPortCount = DRV_PORT_COUNT,                      // number of DC driver device configured
  .srvPort = srvPortArray,                             // DC driver device config structure
  .srvPortCount = SRV_PORT_COUNT                       // number of servo device configured
};




#define SRV_A_SENSE_PIN        39      // SRV_A battery/regulator voltage sensing pin
#define SRV_A_SENSE_HS_RES  10000      // SRV_A sensing circuit high side resistor value (in ohms)
#define SRV_A_SENSE_LS_RES   1500      // SRV_A sensing circuit lox side resistor value (in ohms)

#define SRV_B_SENSE_PIN        35      // SRV_B battery/regulator voltage sensing pin
#define SRV_B_SENSE_HS_RES  10000      // SRV_B sensing circuit high side resistor value (in ohms)
#define SRV_B_SENSE_LS_RES   1500      // SRV_B sensing circuit lox side resistor value (in ohms)

/**
 * Main Battery voltage monitoring
 * Used for LIPO battery under voltage protection
 */

#define VBAT_SENSE_PIN         36      // V-BAT voltage sensing pin
#define VBAT_SENSE_HS_RES   10000      // V-BAT sensing circuit high side resistor value (in ohms)
#define VBAT_SENSE_LS_RES    1500      // V-BAT sensing circuit low side resistor value (in ohms)
#define VBAT_DIODE_DROP      0.00      // V-BAT input diode voltage drop (in Volt)

// EOF ESP32_8M_6S.h