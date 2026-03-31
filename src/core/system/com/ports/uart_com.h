/******************************************************************************
 * @file uart_com.h
 * @brief UART port initialisation
 *
 * @details Initializes a HardwareSerial port once and returns a claimed
 * NodeCom* pointer that encapsulates the UART read/write/available calls.
 *
 * A second call with the same serial interface at the same baud rate returns the
 * existing NodeCom* (port sharing — bidirectional TX+RX use case). A second call
 * with a different baud rate fails loudly and returns nullptr.
 *
 * Available port pool is sized to UartComMaxPorts (declared in the board header,
 * included via config/config.h). A static_assert in uart_com.cpp validates if this
 * value is >= 1 (at least 1 for USB/UART debug).
 *
 * Serial0 (USB/UART) is early pre-claimed via call in sys_init if DEBUG_* or 
 * DEBUG_DASHBOARD compile flags are set. This avoids any accidental reuse
 * of this port by an other module.
 *
 * Typical use in a protocol layer:
 * @code
 *     // port init and use of NodeCom return pointer in protocol init:
 *   NodeCom* com = uart_com_init(&Serial1, 115200, TX_PIN, RX_PIN, "combus");
 *   combus_tx_init(com, ...);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <Arduino.h>

#include "../node_com.h"
#include <core/system/hw/pin_reg.h>


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
 * @param reg      Optional pin registry — if non-null, TX and RX pins are claimed
 *                 before serial.begin(). Pass nullptr to skip pin claiming.
 *
 * @return Pointer to a ready-to-use NodeCom, or nullptr on failure.
 */

NodeCom* uart_com_init( HardwareSerial* serial,
                        uint32_t        baud,
                        int             txPin,
                        int             rxPin,
                        const char*     owner,
                        PinReg*         reg = nullptr );

/**
 * @brief Map an ESP32 Arduino UART index to its HardwareSerial instance.
 *
 * @param n  UART index: 0 = Serial (UART0), 1 = Serial1 (UART1), 2 = Serial2 (UART2).
 * @return   Pointer to the HardwareSerial instance, or nullptr for unsupported indices.
 *
 * @details Used by init modules that select a UART port via the value-based build flags
 *   COMBUS_UART_TX=N, COMBUS_UART_RX=N, or COMBUS_UART=N.
 */
HardwareSerial* uart_serial_for(int n);

// EOF uart_com.h
