/******************************************************************************
 * @file combus_tx.h
 * @brief ComBus transmitter — transport-agnostic.
 *
 * @details Serializes the live ComBus into a binary frame and sends it via
 * any TransportIface*. Timer-gated, non-blocking — safe to call every loop.
 *
 * The physical transport (UART, ESP-Now, …) is provided at init time as a
 * claimed TransportIface*. This module is completely unaware of the underlying
 * medium.
 *
 * Typical integration:
 * @code
 *   // In init:
 *   TransportIface* t = uart_transport_init(&Serial2, BAUD, TX_PIN, RX_PIN, "combus");
 *   combus_tx_init(t, envId, N_ANALOG, N_DIGITAL, TX_HZ);
 *
 *   // In loop:
 *   combus_tx_update(&comBus, failsafeActive);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "transport.h"
#include <struct/combus_struct.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the ComBus transmitter.
 *
 * @param transport  Claimed transport interface (from uart_transport_init or similar).
 * @param envId      Machine-type identifier embedded in each frame header.
 * @param nAnalog    Number of analog channels to encode per frame.
 * @param nDigital   Number of digital channels to encode per frame.
 * @param txHz       Frame transmit rate in Hz.
 */
void combus_tx_init( TransportIface* transport,
                     uint8_t         envId,
                     uint8_t         nAnalog,
                     uint8_t         nDigital,
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
