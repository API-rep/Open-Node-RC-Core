/******************************************************************************
 * @file output_init.cpp
 * @brief Implementation of output peripherals initialization.
 *****************************************************************************/

#include "output_init.h"
#include <machines/config/config.h>
#include <core/system/debug/debug.h>

#ifdef SOUND_OUTPUT_UART
  #include <core/system/transport/combus_uart_tx.h>
#endif


// =============================================================================
// 1. OUTPUT INITIALIZATION
// =============================================================================

/**
 * @brief Initialise output transport layer.
 *
 * @details Each output module is conditionally compiled by build flags.
 * Adding a new output peripheral means adding its init call here, guarded by its
 * own build flag. Multiple non conflictual modules can be active at the same time
 * (ex: UART + wireless outputs).
 * Pin assignments come from the active environment configuration (ex: boards/*.h).
 */

void output_init() {
  sys_log_info("[OUTPUT] output_init ...\n");

	// --- Sound node UART TX ---
#ifdef SOUND_OUTPUT_UART
  combus_uart_tx_init(
      &Serial2,
      static_cast<uint8_t>(CombusLayout::MACHINE_TYPE),
      static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
      static_cast<uint8_t>(DigitalComBusID::CH_COUNT),
      SoundUartBaud,
      Txd1Pin,
      Rxd1Pin,
      SoundTransportTxHz);
#endif

  sys_log_info("[OUTPUT] output_init done\n\n");
}

// EOF output_init.cpp
