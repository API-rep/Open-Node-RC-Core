/******************************************************************************
 * @file output_init.cpp
 * @brief Implementation of output peripherals initialization.
 *****************************************************************************/

#include "output_init.h"
#include <machines/config/config.h>
#include <core/system/debug/debug.h>

#ifdef SOUND_OUTPUT_UART
  #include <machines/system/sound_uart_tx.h>
#endif


// =============================================================================
// 1. OUTPUT INITIALIZATION
// =============================================================================

/**
 * @brief Initialise output transport peripherals.
 *
 * @details Each output module is conditionally compiled. Adding a new output
 * peripheral means adding its init call here, guarded by its own build flag.
 * Pin assignments come from the active board configuration (boards/*.h).
 */
void output_init() {
  sys_log_info("[OUTPUT] output_init ...\n");

	// --- Sound node UART TX ---
#ifdef SOUND_OUTPUT_UART
  sound_uart_tx_init(Txd1Pin, Rxd1Pin, SoundUartBaud, SoundTransportTxHz);
  sys_log_info("[OUTPUT] Sound UART TX — tx=%d rx=%d  baud=%u  rate=%uHz\n",
               static_cast<int>(Txd1Pin), static_cast<int>(Rxd1Pin),
               SoundUartBaud, SoundTransportTxHz);
#endif

  sys_log_info("[OUTPUT] output_init done\n\n");
}

// EOF output_init.cpp
