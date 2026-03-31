/*!****************************************************************************
 * @file  pin_struct.h
 * @brief GPIO pin registry shared type definitions.
 *
 * @details Pins type definitions used by both `pin_reg.h` (core API) and
 *   board-level config arrays[] (board header files).
 *
 *   Shared between all environments: machine node, sound node, and remote node. 
 *******************************************************************************
 */
#pragma once

#include <stdint.h>


// =============================================================================
// 1. PIN OWNER CATEGORY
// =============================================================================

/**
 * @enum PinOwner
 * @brief Functional pin owner category for a claimed GPIO pin.
 * @note When adding/removing values, update `owner_name()` in `pin_reg.cpp`.
 */

enum class PinOwner : uint8_t {
    Free = 0,    ///< Unclaimed — initial registry state.
    Uart0,       ///< HardwareSerial0 (debug TX / RX).
    Uart1,       ///< HardwareSerial1 (RC input / ComBus RX).
    Uart2,       ///< HardwareSerial2 (ComBus / ext device).
    Light,       ///< LED light output.
    NeoPixel,    ///< WS2812 NeoPixel data output (one CH connector).
    ServoOut,    ///< Servo PWM output (CH connectors).
    Esc,         ///< ESC PWM output.
    Shaker,      ///< Shaker motor (built-in MOSFET driver).
    Sound,       ///< DAC audio output.
    Vbat,        ///< Battery voltage sensing (ADC).
    ServoIn,     ///< PWM RC input (CH connectors).
    Switch,      ///< Digital switch / sensor input.
};


// =============================================================================
// 2. STATIC DESCRIPTOR
// =============================================================================

/**
 * @brief One pin desciption entry used in a board-defined config array (e.g. pinsSerial[]).
 *
 * @details Pin configuration descriptor used in board.h/cpp files to define
 *   the intended use of each GPIO pin on the board. These are parsed during the
 *   init sequence.
 *
 *   The same GPIO may intentionally appear in multiple config arrays (shared pin).
 *   Priority is determined by claim order: the array claimed first wins.
 *   The losing claim is logged and its peripheral init must be skipped by the
 *   caller using `pin_resolve()`.
 *
 *   `critical = true`  → fatal firmware halt if the pin is already claimed.
 *   `critical = false` → conflict logged; `pin_claim()` returns false.
 */

struct PinDesc {
    uint8_t     pin;       ///< Physical GPIO number.
    PinOwner    role;      ///< Intended functional owner.
    const char* label;     ///< Human-readable use (e.g. "TAILLIGHT", "UART2_RX").
    bool        critical;  ///< True → fatal halt if another owner already holds this pin.
};


// =============================================================================
// 3. RUNTIME ENTRY  (RAM — registry slots)
// =============================================================================

  /// Sentinel value for an unused registry slot.
static constexpr uint8_t PIN_SLOT_EMPTY = 0xFFu;

/**
 * @brief One pin registry entry used in pinRegEntry[] top-level registry.
 *
 * @details Written once durring init script by `pin_claim()` on a successful claim.
 *  An unused pin == PIN_SLOT_EMPTY .
 */

struct PinRegEntry {
    uint8_t     pin;    ///< Physical GPIO number (PIN_SLOT_EMPTY = unused).
    PinOwner    owner;  ///< Current functional owner.
    const char* label;  ///< Label recorded at claim time (points to Flash string).
};


// =============================================================================
// 4. REGISTRY CONTAINER
// =============================================================================

/**
 * @brief Top-level pin registry container.
 *
 * @details Pointer-based registry. `pin_reg_init(reg, size)` allocates and
 *  initialises the backing array internally. The caller only provides the
 *  desired capacity (typically the board's `PinRegMaxEntry` constexpr).
 *
 *   Typical usage in an init file:
 *   @code
 *       PinReg pinReg;
 *       pin_reg_init(pinReg, PinRegMaxEntry);  // PinRegMaxEntry from board header
 *   @endcode
 */

struct PinReg {
    PinRegEntry* pinRegEntry;       ///< Backing array; allocated by pin_reg_init().
    uint8_t      pinRegEntryCount;  ///< Array capacity (from board's PinRegMaxEntry).
    uint8_t      pinEntryCursor;    ///< Index for next pin_claim() write.
};

// EOF pin_struct.h
