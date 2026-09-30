/******************************************************************************
 * @file uart_com.h
 * @brief UART transport — port init, claim guard and channel helpers.
 *
 * @details Groups all transport-layer UART concerns: low-level port opening
 * with claim guard, and a convenience accessor that maps an ESP32 UART
 * index to its HardwareSerial instance.
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
 * Call sequence (per ComBus link, board-side):
 * @code
 *   sys_init();                                    // debug serial + pin registry
 *   static UartCtx pool[UartComMaxPorts];          // machine: static storage
 *   uart_com_register_pool(pool, UartComMaxPorts); // machine: register once
 *   combus_uart_init();                            // env: opens one NodeCom per ComBusLink[]
 *   hw_init();                                     // hardware peripherals
 *   combus_protocol_init(...);                     // protocol layers wired to transport
 * @endcode
 *
 * Each ComBusLink entry resolves its own UART channel + pins from the
 * active COMBUS_UART* build flag and calls uart_com_init() directly — there
 * is no longer a single-link helper in this header.
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
 * @brief ESP32 exposes 3 UART peripherals (Serial/Serial1/Serial2), matching
 *   uart_serial_for(). Fixed architectural fact, useful when sizing a
 *   board's own uartPins[] table.
 */
static constexpr uint8_t UartHwChannelMax = 3u;

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


// EOF uart_com.h
