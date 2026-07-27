/******************************************************************************
 * @file uart_com.h
 * @brief UART transport — port init, claim guard and ComBus channel helpers.
 *
 * @details Groups all transport-layer UART concerns: low-level port opening
 * with claim guard, ComBus-channel init driven by compile flags, and a
 * convenience accessor that resolves the active channel for protocol layers.
 *
 * Core owns ZERO static storage for the port registry. The caller (machine,
 * sound node, or any future integrator) allocates a static UartCtx[] array
 * sized to its own real needs and hands it to core exactly once via
 * uart_com_register_pool(), before the first uart_com_init() call. This pool
 * is shared by every UART owner in the program (ComBus, Sound, future
 * modules) — its capacity is fixed for the program's lifetime.
 *
 * Serial0 (USB/UART) is pre-claimed in sys_init() when any DEBUG_* or
 * DEBUG_DASHBOARD flag is set, preventing accidental reuse by other modules.
 *
 * Call sequence:
 * @code
 *   sys_init();                                    // debug serial + pin registry
 *   static UartCtx pool[UartComMaxPorts];           // machine: static storage
 *   uart_com_register_pool(pool, UartComMaxPorts);  // machine: register once
 *   uart_init(ComBusUartBaud, UartComMaxPorts, &pinReg);  // machine: open port
 *   hw_init();                                      // hardware peripherals
 *   combus_protocol_init(...);                    // protocol layers wired to transport
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
// 1. PORT REGISTRY ENTRY  (storage layout — allocated and owned by the caller)
// =============================================================================

/**
 * @brief One UART port registry slot.
 * @details Exposed (not opaque) so the machine can declare its static array
 *   directly, without an internal core header. Core never allocates this —
 *   only writes into slots of a buffer handed to it via
 *   uart_com_register_pool().
 */
struct UartCtx {
	HardwareSerial* serial  = nullptr;  ///< Pointer to the associated HardwareSerial instance
	const char*     owner   = nullptr;  ///< Name of the module owning this port
	uint32_t        baud    = 0u;       ///< Baud rate recorded at init (used for duplicate detection)
	bool            claimed = false;    ///< Indicates if the port is already allocated
	NodeCom         com     = {};       ///< NodeCom instance linked to this port
};


// =============================================================================
// 2. POOL REGISTRATION  (call ONCE at boot, before any uart_com_init)
// =============================================================================

/**
 * @brief Register the UART port registry storage.
 *
 * @param buffer    Statically-allocated UartCtx array (caller-owned, must
 *                   outlive the program — no heap, no local/temporary storage).
 * @param capacity  Number of usable slots in buffer.
 *
 * @details A second call is rejected (logged, ignored) — the pool is meant
 *   to be wired once at boot, by whichever init sequence runs first.
 */
void uart_com_register_pool(UartCtx* buffer, uint8_t capacity);


// =============================================================================
// 3. LOW-LEVEL PORT INIT
// =============================================================================

/**
 * @brief Initialize a UART port and return a claimed NodeCom*.
 *
 * @details Calls serial.begin() once and registers the port in the pool
 * handed to core via uart_com_register_pool(). Fails safely (logged,
 * nullptr) if called before the pool is registered, on a null serial
 * pointer, on a duplicate claim of the same physical port, or once the
 * pool's capacity is exhausted.
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
// 4. COMBUS UART CHANNEL INIT  (compile-flag driven)
// =============================================================================
//
//   uart_init(baud, maxChannels, reg) — open the ComBus UART port from the active flag.
//   uart_get_com(ch)                  — NodeCom* for a given UART channel index.
//   uart_get_combus_com()             — shortcut: resolve active channel → uart_get_com().
//
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

/**
 * @brief Open the ComBus UART port and store the resulting NodeCom*.
 *
 * @param baud        UART baud rate.
 * @param maxChannels Number of hardware UART channels this board exposes to
 *                     ComBus — bounds uart_get_com() only. Unrelated to the
 *                     shared port pool from uart_com_register_pool(); this is
 *                     purely a channel-index sanity ceiling, capped internally
 *                     at the ESP32's 3 physical UARTs.
 * @param reg         Optional pin registry — TX and RX pins are claimed if non-null.
 */
void uart_init(uint32_t baud, uint8_t maxChannels, PinReg* reg = nullptr);

/**
 * @brief Return the NodeCom* opened by uart_init() for the given UART channel.
 */
NodeCom* uart_get_com(int uartCh);

/**
 * @brief Return the NodeCom* for the active ComBus channel.
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

inline void     uart_init(uint32_t, uint8_t, PinReg* = nullptr) {}
inline NodeCom* uart_get_com(int)                                { return nullptr; }
inline NodeCom* uart_get_combus_com()                             { return nullptr; }

#endif  // COMBUS_UART_TX / COMBUS_UART_RX / COMBUS_UART

// EOF uart_com.h