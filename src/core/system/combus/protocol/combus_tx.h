/******************************************************************************
 * @file combus_tx.h
 * @brief ComBus transmitter module
 *
 * @details Serializes the live ComBus into a binary frame and sends it via
 * any NodeCom* physical transport interface (UART, ESP-Now, …) provided at
 * init time. Timer-gated and non-blocking: the update function does nothing
 * if the transmit interval has not elapsed since the last frame.
 *
 * Typical integration:
 * @code
 *   // In init:
 *   NodeCom* com = uart_com_init(&Serial2, BAUD, TX_PIN, RX_PIN, "combus");
 *   constexpr ComBusFrameCfg cfg = { MACHINE_TYPE, N_ANALOG, N_DIGITAL };
 *   combus_tx_init(com, cfg, TX_HZ);
 *
 *   // In loop:
 *   combus_tx_update(&comBus, failsafeActive);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <struct/combus_struct.h>
#include <struct/outputs_struct.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the ComBus transmitter.
 *
 * @param nodeCom   Claimed transport interface (from *_com_init).
 * @param frameCfg  ComBus layout descriptor (envId, nAnalog, nDigital).
 * @param txHz      Frame transmit rate in Hz.
 */

void combus_tx_init( NodeCom*        nodeCom,
                     ComBusFrameCfg  frameCfg,
                     uint32_t        txHz );



/**
 * @brief Encode and transmit one ComBus frame if the period has elapsed.
 *
 * @details Timer-gated and non-blocking. Does nothing if the transmit interval
 *   has not elapsed since the last frame.
 *
 * @param bus         Live ComBus to encode.
 * @param failSafe    Set true to flag the frame as failsafe-active.
 */
void combus_tx_update( const ComBus* bus, bool failSafe );

// EOF combus_tx.h
