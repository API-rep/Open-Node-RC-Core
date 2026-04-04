/******************************************************************************
 * @file uart_com.cpp
 * @brief UART transport — port init, claim guard and ComBus channel helpers.
 *****************************************************************************/

#include <config/config.h>
#include "uart_com.h"

#include <core/system/debug/debug.h>
#include <core/system/hw/pin_reg.h>
#include <struct/uart_struct.h>

// Board UartComMaxPorts config validation from config/config.h (board header)
static_assert(UartComMaxPorts >= 1u, "UartComMaxPorts must be >= 1 (check board header)");


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/**
 * @brief Define UART port registry entries..
 *
 * @details During initialization, the `uart_com_init` function claims one entry
 *   in the static `ports[]` registry and fills its metadata (serial pointer, owner name).
 *
 *   Each UART NodeCom instance is associated with a `UartCtx` entry. This lets the
 *   three port callbacks functions(`uart_write/readByte/available`) retrieve
 *   the correct `HardwareSerial*` at runtime.
 *
 *   Because the registry is statically allocated, the `NodeCom*` pointer returned to
 *   the caller by `uart_com_init` remains valid for the lifetime of the program.
 */

struct UartCtx {
	HardwareSerial* serial  = nullptr;  ///< Pointer to the associated HardwareSerial instance
	const char*     owner   = nullptr;  ///< Name of the module owning this port
	uint32_t        baud    = 0u;       ///< Baud rate recorded at init (used for duplicate detection)
	bool            claimed = false;    ///< Indicates if the port is already allocated
	NodeCom         com     = {};       ///< NodeCom instance linked to this port
};


static UartCtx ports[UartComMaxPorts];  ///< Static pool of UART contexts
static uint8_t portsCount = 0;          ///< Number of allocated UART ports



// =============================================================================
// 2. PORT CALLBACKS
// =============================================================================

/**
 * Static functions linked to the `NodeCom` function-pointer interface
 * for UART transport. They are assigned to `NodeCom.write/readByte/available`
 *  during `uart_com_init()` (step 3).
 *
 * Each receives a `void* ctx` context pointer cast to `UartCtx*`, giving access
 * to the correct `HardwareSerial*` port registry metadata.
 */

	/// @brief UART port Write function
static void uart_write(void* ctx, const uint8_t* data, size_t len) {
	static_cast<UartCtx*>(ctx)->serial->write(data, len);
}

	/// @brief UART port Read function
static int uart_readByte(void* ctx) {
	return static_cast<UartCtx*>(ctx)->serial->read();
}

	/// @brief UART port Available function
static int uart_available(void* ctx) {
	return static_cast<UartCtx*>(ctx)->serial->available();
}



// =============================================================================
// 3. PUBLIC API
// =============================================================================

/**
 * @brief Initialize a UART port and return a NodeCom pointer.
 *
 * @details Initialization sequence:
 *   1. Guard checks — null serial pointer, duplicate claim on the same
 *      physical port, registry capacity exceeded. Returns nullptr on failure.
 *   2. Claim a context entry from the registry and record port metadata.
 *   3. Bind the NodeCom function pointers to this context.
 *   4. Claim GPIO pins in the optional pin registry (if `reg` is non-null).
 *   5. Configure and start the hardware serial port.
 *
 * @return NodeCom* Pointer to the initialized NodeCom interface, or nullptr on error.
 */

NodeCom* uart_com_init(
    HardwareSerial* serial, // Pointer to the HardwareSerial instance to use
    uint32_t        baud,   // Baudrate for the connection
    int             txPin,  // TX pin number
    int             rxPin,  // RX pin number
    const char*     owner,  // Name of the owning module (for debug)
    PinReg*         reg   ) // Optional pin registry — claims tx/rx pins if non-null
 {

		// --- 1. Guard checks ---
		// --- 1.1: Null serial pointer ---
	if (!serial) {
		sys_log_err("[UART_COM] nullptr serial — init aborted\n");
		return nullptr;
	}

		// --- 1.2: Reject duplicate claim — one owner per physical port ---
	for (uint8_t i = 0u; i < portsCount; i++) {
		if (ports[i].serial == serial) {
			sys_log_err("[UART_COM] FATAL: port already claimed by '%s', rejected for '%s'\n", ports[i].owner, owner);
			return nullptr;
		}
	}

		// --- 1.3: Registry capacity exceeded ---
	if (portsCount >= UartComMaxPorts) {
		sys_log_err("[UART_COM] FATAL: port pool full (%u max), rejected for '%s'\n", (unsigned)UartComMaxPorts, owner);
		return nullptr;
	}

		// --- 2. Claim a context entry from the registry and record port metadata ---
	UartCtx* port    = &ports[portsCount++];  // next free slot in the registry
	port->serial     = serial;
	port->owner      = owner;
	port->baud       = baud;
	port->claimed    = true;

		// --- 3. Bind the NodeCom function pointers to this context ---
	port->com.ctx       = port;
	port->com.write     = uart_write;
	port->com.readByte  = uart_readByte;
	port->com.available = uart_available;
	port->com.name      = owner;

		// --- 4. Claim pins in the registry before starting the port ---
	if (reg != nullptr) {
		PinOwner portOwner = PinOwner::Uart0;
		if      (serial == &Serial1) portOwner = PinOwner::Uart1;
		else if (serial == &Serial2) portOwner = PinOwner::Uart2;
		if (txPin >= 0) pin_claim(*reg, (uint8_t)txPin, portOwner, "TX", true);
		if (rxPin >= 0) pin_claim(*reg, (uint8_t)rxPin, portOwner, "RX", true);
	}

		// --- 5. Configure and start the hardware serial port ---
	serial->begin(baud, SERIAL_8N1, rxPin, txPin);

	sys_log_info("[UART_COM] '%s' — baud=%u  tx=%d  rx=%d\n",owner, baud, txPin, rxPin);

	return &port->com;
}


// =============================================================================
// 4. UART INDEX HELPER
// =============================================================================

HardwareSerial* uart_serial_for(int n) {
	switch (n) {
		case 0:  return &Serial;
		case 1:  return &Serial1;
		case 2:  return &Serial2;
		default: sys_log_err("[UART_COM] uart_serial_for: unsupported index %d\n", n); return nullptr;
	}
}


// =============================================================================
// 5. COMBUS UART CHANNEL INIT  (compile-flag driven)
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

// Board-level UART pin table — defined in the active env's board .cpp, resolved at link time.
extern const UartPinCfg uartPins[];

static NodeCom* s_com[UartComMaxPorts] = {};


void uart_init(uint32_t baud, PinReg* reg)
{
		// --- Resolve UART channel and GPIO pins from build flag ---
	#if defined(COMBUS_UART)
		constexpr int uartCh    = COMBUS_UART;
		const     int uartTxPin = uartPins[uartCh].tx;
		const     int uartRxPin = uartPins[uartCh].rx;
	#elif defined(COMBUS_UART_TX)
		constexpr int uartCh    = COMBUS_UART_TX;
		const     int uartTxPin = uartPins[uartCh].tx;
		constexpr int uartRxPin = -1;
	#else  // COMBUS_UART_RX
		constexpr int uartCh    = COMBUS_UART_RX;
		constexpr int uartTxPin = -1;
		const     int uartRxPin = uartPins[uartCh].rx;
	#endif

		// --- Open UART port once ---
	s_com[uartCh] = uart_com_init(uart_serial_for(uartCh), baud,
	                              uartTxPin, uartRxPin, "combus", reg);
}


NodeCom* uart_get_com(int uartCh)
{
	if (uartCh < 0 || uartCh >= UartComMaxPorts) {
		sys_log_err("[UART_COM] uart_get_com: channel %d out of range.\n", uartCh);
		return nullptr;
	}
	return s_com[uartCh];
}

#endif  // COMBUS_UART_TX / COMBUS_UART_RX / COMBUS_UART

// EOF uart_com.cpp
