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
 * - Add SRV_A and SRV_B VBatSenseConfig instances once servo sensing is wired
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>
#include <machines/config/boards/drivers/drivers.h>
#include <Arduino.h>

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

 	/// Onboard logic voltage reference (V)
static constexpr float AdcRefVoltage = 3.3f;

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
static constexpr int8_t DecayPin[CH_COUNT] = { 26, 15 };  // CH_A, CH_B — brake channel pin

  // DC drivers control pins
static constexpr int8_t DrvEnPin  = 33;
static constexpr int8_t DrvSlpPin = 25;
static constexpr int8_t DrvFltPin = 34;



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

  // built-in ESP32 ESP32-DevKitC V4 built-in serial port (connected to USB port).
static constexpr int8_t Txd0Pin = 1;    // ESP32 built-in TX pin
static constexpr int8_t Rxd0Pin = 3;    // ESP32 built-in RX pin

  // Extension port — UART link to the sound node (or any future ext device).
inline HardwareSerial& SerialExt = Serial2;       // SerialExt maps to the physical UART port

static constexpr int8_t  TxdExtPin = 18;  // UART ext TX pin
static constexpr int8_t  RxdExtPin = 13;  // UART ext RX pin

  // General UART hardware ceiling for all ext/com ports on this board.
static constexpr uint32_t UartMaxBaud = 115200u;  // hardware ceiling for all UART links on this board

  // Maximum number of simultaneously UART ports on this board (including Serial0/USB).
static constexpr uint8_t  UartComMaxPorts = 3u;

  // Pin registry capacity — ESP32 has 40 GPIOs, 32 slots covers all usable pins.
static constexpr uint8_t  PinRegMaxEntry = 32u;

  // servo power sensing pins (also used by VBatSrvACfg / VBatSrvBCfg)
static constexpr uint8_t SrvASensePin  = 39;  // SRV-A power rail ADC sense pin
static constexpr uint8_t SrvBSensePin  = 35;  // SRV-B power rail ADC sense pin
static constexpr uint8_t VBatSensePin  = 36;  // V-BAT main battery ADC sense pin



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



/**
 * @brief Voltage sensing hardware configuration for this board.
 *
 * @details Each VBatSenseConfig instance groups all hardware parameters for
 *   one sensing channel (pin, resistor divider, diode drop, logic reference).
 *   Threshold fields (cutoff, hysteresis, interval) are filled with module
 *   defaults — overridable from platformio.ini build flags.
 *
 * NOTE: SRV_A and SRV_B instances are prepared below for future use.
 *   Their threshold parameters may differ from main battery sensing (single
 *   cell / regulator monitoring, no per-cell logic needed).
 */

#ifdef VBAT_SENSING
#include <core/system/vbat/vbat_sense.h>

// --- Active sensing channels ---
// Add or comment entries to enable/disable channels. Keep VBAT_CH_COUNT last and active.
enum VBatChannel {
  VBAT_MAIN  = 0,
  VBAT_SRV_A,
  VBAT_SRV_B,
  VBAT_CH_COUNT
};

	/// Board sensing channel config array — defined in .cpp, indexed by VBatChannel.
extern const VBatSenseConfig vBatSenseConfigArray[VBAT_CH_COUNT];
	/// Board sensing channel state array — defined in .cpp, zero-initialized.
extern VBatSenseState        vBatSenseStateArray[VBAT_CH_COUNT];
	/// Top-level sensing container — pre-wired at startup, passed to vbat_sense_init().
extern VBatSense             vBatSense;



#endif // VBAT_SENSING


// =============================================================================
// SWITCH INPUTS  (none wired on this board revision)
// =============================================================================

/// Switch channel index — no switches on this board revision.
enum Switch {
  SW_CH_COUNT = 0
};

  /// Top-level switch port container — passed to switch_init().
extern SwitchPort switchPort;  ///< count = 0 (no switches configured).


// EOF ESP32_8M_6S.h