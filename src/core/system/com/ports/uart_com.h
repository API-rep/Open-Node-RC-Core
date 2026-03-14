/******************************************************************************
 * @file uart_com.h
 * @brief UART adapter — provides a NodeCom.
 *
 * @details Initializes a HardwareSerial port once and returns a claimed
 * NodeCom* that encapsulates the UART read/write/available calls.
 *
 * Guard: a second call with the same serial pointer fails loudly and returns
 * nullptr — preventing two modules from calling Serial.begin() on the same
 * port with different parameters.
 *
 * Pool is sized to UartComMaxPorts (declared in the board header, included via
 * config/config.h). A static_assert in uart_com.cpp validates it is >= 1.
 *
 * Serial0 pre-claim: call uart_com_init(&Serial, ...) early in sys_init
 * (gated on DEBUG_* / DEBUG_DASHBOARD flags) so the duplicate guard blocks
 * any accidental reuse as a transport port.
 *
 * Typical use:
 * @code
 *   NodeCom* com = uart_com_init(&Serial2, 115200, TX_PIN, RX_PIN, "combus");
 *   combus_tx_init(com, ...);   // protocol layer receives the same pointer
 *   combus_rx_init(com, ...);   // both share the same claimed port
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <Arduino.h>

#include "../node_com.h"


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize a UART port and return a claimed NodeCom*.
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
 * @return Pointer to a ready-to-use NodeCom, or nullptr on failure.
 */
NodeCom* uart_com_init( HardwareSerial* serial,
                        uint32_t        baud,
                        int             txPin,
                        int             rxPin,
                        const char*     owner );

// EOF uart_com.h
