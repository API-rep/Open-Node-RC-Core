/******************************************************************************
 * @file uart_link.h
 * @brief UART adapter — provides a NodeLink.
 *
 * @details Initializes a HardwareSerial port once and returns a claimed
 * NodeLink* that encapsulates the UART read/write/available calls.
 *
 * Guard: a second call with the same serial pointer fails loudly and returns
 * nullptr — preventing two modules from calling Serial.begin() on the same
 * port with different parameters.
 *
 * Supports up to UART_TRANSPORT_MAX_PORTS simultaneous UART ports (default 3).
 *
 * Typical use:
 * @code
 *   NodeLink* t = uart_link_init(&Serial2, 115200, TX_PIN, RX_PIN, "combus");
 *   combus_tx_init(link, ...);   // protocol layer receives the same pointer
 *   combus_rx_init(link, ...);   // both share the same claimed port
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <Arduino.h>

#include "../transport.h"


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize a UART port and return a claimed NodeLink*.
 *
 * @details Calls serial.begin() once and registers the port in an internal
 * claim table. A second call with the same serial pointer logs a fatal error
 * and returns nullptr.
 *
 * @param serial   HardwareSerial port (e.g. &Serial2).
 * @param baud     Baud rate.
 * @param txPin    GPIO TX pin (-1 to use Arduino default).
 * @param rxPin    GPIO RX pin (-1 to use Arduino default).
 * @param owner    Caller identifier logged in the claim table (e.g. "combus").
 *
 * @return Pointer to a ready-to-use NodeLink, or nullptr on failure.
 */
NodeLink* uart_link_init( HardwareSerial* serial,
                                     uint32_t        baud,
                                     int             txPin,
                                     int             rxPin,
                                     const char*     owner );

// EOF uart_link.h
