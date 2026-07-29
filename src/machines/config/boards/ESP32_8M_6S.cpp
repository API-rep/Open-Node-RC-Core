/*!****************************************************************************
 * @file  ESP32_8M_6S.cpp
 * @brief ESP32_8M_6S motherboard backend.
 *
 * Defines the complete hardware description of the ESP32_8M_6S motherboard.
 * This backend instantiates every board-level configuration object declared
 * in ESP32_8M_6S.h:
 *   - DC driver ports
 *   - Servo outputs
 *   - Battery sensing channels
 *   - Switch inputs
 *
 * This file is intentionally declarative and shall not contain runtime logic.
 ******************************************************************************/

#if defined(BOARD_ESP32_8M_6S)
  #include "ESP32_8M_6S.h"
  #include <core/config/vbat/config.h>  // HAS_VBAT_SENSING

  // =============================================================================
  // DC DRIVER PORTS
  // Order must match DrvPort enum in ESP32_8M_6S.h.
  // =============================================================================

  DriverPort drvPortArray[DRV_PORT_COUNT] = {

    {
      .ID = DRV_PORT_1A,
      .infoName = "M-1A",
      .driverModel = boardDcDrivers,
      .pwmPin = 32,
      .brkPin = DecayPin[CH_A],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },

    {
      .ID = DRV_PORT_2A,
      .infoName = "M-2A",
      .driverModel = boardDcDrivers,
      .pwmPin = 27,
      .brkPin = DecayPin[CH_A],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },

    {
      .ID = DRV_PORT_3A,
      .infoName = "M-3A",
      .driverModel = boardDcDrivers,
      .pwmPin = 14,
      .brkPin = DecayPin[CH_A],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },

    {
      .ID = DRV_PORT_4A,
      .infoName = "M-4A",
      .driverModel = boardDcDrivers,
      .pwmPin = 12,
      .brkPin = DecayPin[CH_A],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },

    {
      .ID = DRV_PORT_1B,
      .infoName = "M-1B",
      .driverModel = boardDcDrivers,
      .pwmPin = 21,
      .brkPin = DecayPin[CH_B],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },

    {
      .ID = DRV_PORT_2B,
      .infoName = "M-2B",
      .driverModel = boardDcDrivers,
      .pwmPin = 5,
      .brkPin = DecayPin[CH_B],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },

    {
      .ID = DRV_PORT_3B,
      .infoName = "M-3B",
      .driverModel = boardDcDrivers,
      .pwmPin = 0,
      .brkPin = DecayPin[CH_B],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },

    {
      .ID = DRV_PORT_4B,
      .infoName = "M-4B",
      .driverModel = boardDcDrivers,
      .pwmPin = 2,
      .brkPin = DecayPin[CH_B],
      .enPin  = DrvEnPin,
      .slpPin = DrvSlpPin,
      .fltPin = DrvFltPin
    },
  };


  // =============================================================================
  // SERVO OUTPUTS
  // Order must match SrvPort enum in ESP32_8M_6S.h.
  // =============================================================================

  ServoPort srvPortArray[SRV_PORT_COUNT] = {

    {
      .ID = SRV_PORT_1A,
      .infoName = "SRV-1A",
      .pwmPin = 19,
      .sensePin = SrvASensePin
    },

    {
      .ID = SRV_PORT_2A,
      .infoName = "SRV-2A",
      .pwmPin = 22,
      .sensePin = SrvASensePin
    },

    {
      .ID = SRV_PORT_3A,
      .infoName = "SRV-3A",
      .pwmPin = 23,
      .sensePin = SrvASensePin
    },

    {
      .ID = SRV_PORT_1B,
      .infoName = "SRV-1B",
      .pwmPin = 4,
      .sensePin = SrvBSensePin
    },

    {
      .ID = SRV_PORT_2B,
      .infoName = "SRV-2B",
      .pwmPin = 16,
      .sensePin = SrvBSensePin
    },

    {
      .ID = SRV_PORT_3B,
      .infoName = "SRV-3B",
      .pwmPin = 17,
      .sensePin = SrvBSensePin
    },
  };


  // =============================================================================
  // BATTERY SENSING
  // Order must match VBatChannel enum in ESP32_8M_6S.h.
  // =============================================================================

  #ifdef HAS_VBAT_SENSING
    const VBatSenseConfig vBatSenseConfigArray[VBAT_CH_COUNT] = {
    
      {   // VBAT_MAIN — main battery (pin 36, 10k/1.5k divider)
        .infoName      = "Main battery",
        .pin           = VBatSensePin,
        .adcRefVoltage = AdcRefVoltage,
        .hsResOhm      = 10000,
        .lsResOhm      = 1500,
        .diodeDrop     = 0.00f,
        .cutoffVolt    = VBatCutoffVoltage,
        .chargedVolt   = VBatChargedVoltage,
        .hysteresis    = VBatHysteresis,
        .intervalMs    = VBatSenseInterval,
      },
    
      {   // VBAT_SRV_A — servo channel A power rail
        .infoName      = "Servo A power",
        .pin           = SrvASensePin,
        .adcRefVoltage = AdcRefVoltage,
        .hsResOhm      = 10000,
        .lsResOhm      = 1500,
        .diodeDrop     = 0.00f,
        .cutoffVolt    = VBatCutoffVoltage,
        .chargedVolt   = VBatChargedVoltage,
        .hysteresis    = VBatHysteresis,
        .intervalMs    = VBatSenseInterval,
      },
    
      {   // VBAT_SRV_B — servo channel B power rail
        .infoName      = "Servo B power",
        .pin           = SrvBSensePin,
        .adcRefVoltage = AdcRefVoltage,
        .hsResOhm      = 10000,
        .lsResOhm      = 1500,
        .diodeDrop     = 0.00f,
        .cutoffVolt    = VBatCutoffVoltage,
        .chargedVolt   = VBatChargedVoltage,
        .hysteresis    = VBatHysteresis,
        .intervalMs    = VBatSenseInterval,
      },
    };

    VBatSenseState vBatSenseStateArray[VBAT_CH_COUNT];

    VBatSense vBatSense {
      vBatSenseConfigArray,
      VBAT_CH_COUNT,
      vBatSenseStateArray
    };

  #endif // HAS_VBAT_SENSING


  // =============================================================================
  // SWITCH INPUTS
  // =============================================================================
  // No switches are wired on this board revision.

  SwitchPort switchPort;  // count = 0, cfg/state = nullptr

#endif  // BOARD_ESP32_8M_6S

// EOF ESP32_8M_6S.cpp