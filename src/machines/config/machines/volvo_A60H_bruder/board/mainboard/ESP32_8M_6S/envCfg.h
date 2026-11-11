/*!****************************************************************************
 * @file    envCfg.h
 * @brief   Volvo A60H Bruder — ESP32_8M_6S board environment configuration.
 *
 * @details Declares device-index enums and extern arrays for the Bruder
 *   Volvo A60H full-electric conversion.  Physical layout:
 *   - 6 DC motors for traction (one per wheel, cabin + trailer axles).
 *   - 1 DC motor for steering (one or two actuators wired in parallel).
 *   - 1 DC motor for the dump body (two actuators wired in parallel).
 *   - No servo-driven devices on this build.
 *   - 6 signal sources: horn, lights, engine key, two indicators, hazards.
 *
 *   Array contents are defined in `envCfg.cpp`.
 *   The `machine` constexpr aggregates all pointers for use by init code.
 *******************************************************************************
 */
#pragma once

#if defined (BOARD_ESP32_8M_6S)
  
  // Machine identity and board flag — must come first so that BOARD is visible
  // to boards.h (included indirectly via struct/struct.h).
  #include <machines/config/machines/volvo_A60H_bruder/board/mainboard/mainboard.h>
  
  #include <const.h>
  #include <struct/struct.h>
  #include <defs/defs.h>
  
  #include <core/system/combus/combus_defs.h>
  #include <core/system/hw/node_com.h>               // ComPort
  #include <machines/config/machines/volvo_A60H_bruder/board/com_dev.h>   // kComDevExtUni, kComDevExtBi, ComDevMap

  // Board-specific hardware definitions (pins, ports, constants)
  #include <machines/config/boards/ESP32_8M_6S.h>
  
  
  // =============================================================================
  // Build-time parameters
  // =============================================================================
  
    // ComBus node group for this machine environment (see ComBusOwner::GRP_* in combus_struct.h).
    // Change only when adding a new execution environment that requires its own group.
    // NOTE: EnvNodeGroup is no longer needed with the simplified ChanLayer system.
    // static constexpr uint8_t EnvNodeGroup = ComBusOwner::GRP_MACHINE;
  
  #define MAX_FW_SPEED         100.00     ///< Maximum forward speed cap (0–100 %).
  #define MAX_BACK_SPEED       100.00     ///< Maximum reverse speed cap (0–100 %).
  
  #define M_DEF_PWM_FREQ        16000     ///< Default DC-motor PWM frequency (Hz).
  #define SRV_DEF_PWM_FREQ         50     ///< Default servo PWM frequency (Hz).
  #define COOLING_FAN_SPEED       100     ///< Cooling fan duty cycle (%).
  
  
  // =============================================================================
  // Device index enums
  // =============================================================================
  
  /**
   * @brief Indices into `dcDevArray[]`.
   *
   * @details `DC_DRV_COUNT` is used as the array size and loop bound — do not
   *   assign an explicit value to any enumerator after it.
   */
  enum DrvDev {
      STEERING = 0,               ///< Steering actuator (1 or 2 wired in //).
      CABIN_LEFT_MOTOR,           ///< Front-left traction motor (cabin axle, left).
      CABIN_RIGHT_MOTOR,          ///< Front-right traction motor (cabin axle, right).
      TRAILER_FRONT_LEFT_MOTOR,   ///< Middle-left traction motor (trailer front axle).
      TRAILER_FRONT_RIGHT_MOTOR,  ///< Middle-right traction motor (trailer front axle).
      TRAILER_REAR_LEFT_MOTOR,    ///< Rear-left traction motor (trailer rear axle).
      TRAILER_REAR_RIGHT_MOTOR,   ///< Rear-right traction motor (trailer rear axle).
      DUMP_ACTUATOR,              ///< Dump-body actuator (2 actuators wired in //).
      DC_DRV_COUNT                ///< Sentinel — number of DC-motor devices.
  };
  
  /// DC-motor device table. Data defined in `envCfg.cpp`.
  extern DcDevice dcDevArray[DC_DRV_COUNT];
  
  
  /**
   * @brief Indices into `SrvDevArray[]`.
   *
   * @note No servo-driven devices on this build. `SRV_COUNT = 0` prevents
   *   zero-size array issues in the device control loop.
   */
  enum SrvDev {
      // (no servo devices currently)
      SRV_COUNT  ///< Sentinel — number of servo devices (0 on this build).
  };
  
  /// Servo device table. Data defined in `envCfg.cpp`.
  extern SrvDevice SrvDevArray[];
  
  
  // SigDev enum and sigDevArray are declared in
  // volvo_A60H_bruder.h (included above) — they are board-independent.
  
  
  // =============================================================================
  // Vehicle config aggregate
  // =============================================================================
  
  /**
   * @brief Top-level machine descriptor for the Volvo A60H Bruder.
   *
   * @details Aggregates all device array pointers and counts.  Consumed by
   *   `machine_init()` and the output module init functions.
   */
  inline constexpr EnvCfg machine {
    .infoName      = kVehicleName,
    .dcDev         = dcDevArray,

    .dcDevCount    = DC_DRV_COUNT,
    .srvDev        = SrvDevArray,
    .srvDevCount   = SRV_COUNT,
    .sigDev          = sigDevArray,
    .sigDevCount     = SIG_COUNT,
    .inputChain      = kInputChains,
    .inputChainCount = INPUT_CH_COUNT,
    .simChain        = kSimChannels,
    .simChainCount   = SIM_CH_COUNT
  };
  
  
  // =============================================================================
  // Communication ports
  // =============================================================================

  /**
   * @brief Communication ports of the ESP32_8M_6S board (Volvo A60H Bruder).
   *
   * @details One entry per port; values come from the active board header
   *   (ESP32_8M_6S.h) included above.  The ports in use are the ones the links
   *   of the board (comLinks below) wire.
   */
  extern const ComPort comPorts[];
  extern const uint8_t comPortsCount;

    /// Index of the extension port (COM/GPIO, UART2) in comPorts[].
  inline constexpr uint8_t kComPortExt = 1u;


  // =============================================================================
  // Communication links
  // =============================================================================

  /**
   * @brief ComBus links of the ESP32_8M_6S board (Volvo A60H Bruder).
   *
   * @details One entry per link: a ComDev of the node (board/com_dev.h) wired on
   *   one port of this board, with the role of this end.  Started by
   *   board_links_init().  To wire the extension link both ways, use
   *   kComDevExtBi with LinkEnd::TX_RX.
   */
  inline const ComDevMap comLinks[] = {
    { &kComDevExtUni, &comPorts[kComPortExt], LinkEnd::TX },   // to the extension board, transmit only
  };

  inline constexpr uint8_t comLinksCount = static_cast<uint8_t>(sizeof(comLinks) / sizeof(comLinks[0]));
  
#endif  // BOARD_ESP32_8M_6S

// EOF envCfg.h
