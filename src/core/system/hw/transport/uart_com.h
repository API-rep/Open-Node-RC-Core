/******************************************************************************
 * @file uart_com.h
 * @brief UART transport — port init, claim guard and ComBus channel helpers.
 *
 * @details Groups all transport-layer UART concerns: low-level port opening
 * with claim guard, ComBus-channel init driven by compile flags, and a
 * convenience accessor that resolves the active channel for protocol layers.
 *
 * Available port pool is sized to UartComMaxPorts (board header via config/config.h).
 * A static_assert in uart_com.cpp validates this value is >= 1.
 *
 * Serial0 (USB/UART) is pre-claimed in sys_init() when any DEBUG_* or
 * DEBUG_DASHBOARD flag is set, preventing accidental reuse by other modules.
 *
 * Call sequence:
 * @code
 *   sys_init();                            // debug serial + pin registry
 *   uart_init(ComBusUartBaud);             // machine: open port, claim pins
 *   uart_init(SOUND_UART_BAUD, &pinReg);   // sound:   open port, claim pins
 *   hw_init();                             // hardware peripherals
 *   combus_protocol_init(...);           // protocol layers wired to transport
 * @endcode
 *
 * When no COMBUS_UART* flag is defined, all ComBus helpers compile to inline
 * no-ops — zero overhead at call sites.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <Arduino.h>

#include "../node_com.h"
#include <core/system/hw/pin_reg.h>


// =============================================================================
// 1. LOW-LEVEL PORT INIT
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
 * @param reg      Optional pin registry — TX and RX pins are claimed before
 *                 serial.begin() when non-null.
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
 * @return   HardwareSerial* or nullptr for unsupported indices.
 */
HardwareSerial* uart_serial_for(int n);


// =============================================================================
// 2. COMBUS UART CHANNEL INIT  (compile-flag driven)
// =============================================================================
//
//   uart_init(baud, reg)    — open the ComBus UART port from the active flag.
//   uart_get_com(ch)        — NodeCom* for a given UART channel index.
//   uart_get_combus_com()   — shortcut: resolve active channel → uart_get_com().
//
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

/**
 * @brief Open the ComBus UART port and store the resulting NodeCom*.
 *
 * @details Resolves channel and GPIO pins from the active build flag and the
 * board-level uartPins[] table, sets the RX pin mode when applicable, then
 * calls uart_com_init() once.  The NodeCom* is retained internally and
 * retrieved later by uart_get_com() or uart_get_combus_com().
 *
 * @param baud  UART baud rate.
 * @param reg   Optional pin registry — TX and RX pins are claimed if non-null.
 */
void uart_init(uint32_t baud, PinReg* reg = nullptr);

/**
 * @brief Return the NodeCom* opened by uart_init() for the given UART channel.
 *
 * @param uartCh  0-based UART channel index.
 * @return        NodeCom* or nullptr if out of range or not yet initialised.
 */
NodeCom* uart_get_com(int uartCh);

/**
 * @brief Return the NodeCom* for the active ComBus channel.
 *
 * @details Resolves the UART channel from COMBUS_UART / COMBUS_UART_TX /
 * COMBUS_UART_RX and delegates to uart_get_com().  Protocol-layer callers
 * use this instead of repeating the flag-resolution chain.
 */
inline NodeCom* uart_get_combus_com()
{
#if defined(COMBUS_UART)
    return uart_get_com(COMBUS_UART);
#elif defined(COMBUS_UART_TX)
    return uart_get_com(COMBUS_UART_TX);
#else
    return uart_get_com(COMBUS_UART_RX);
#endif
}

#else   // No COMBUS_UART* flag

inline void     uart_init(uint32_t, PinReg* = nullptr) {}
inline NodeCom* uart_get_com(int)                       { return nullptr; }
inline NodeCom* uart_get_combus_com()                   { return nullptr; }

#endif  // COMBUS_UART_TX / COMBUS_UART_RX / COMBUS_UART

// EOF uart_com.h
