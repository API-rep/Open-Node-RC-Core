/******************************************************************************
 * @file uart_com.cpp
 * @brief UART adapter implementation for NodeCom.
 *****************************************************************************/

#include <config/config.h>
#include "uart_com.h"

#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/**
 * @brief UART port internal context structure.
 *
 * @details Each serial port owns a NodeCom instance, which embeds a ctx pointer to
 *   this UartCtx structure. This allows the static adapter functions to access
 *   the correct HardwareSerial interface for each port.
 */

struct UartCtx {
	HardwareSerial* serial  = nullptr;  ///< Pointer to the associated HardwareSerial instance
	const char*     owner   = nullptr;  ///< Name of the module owning this port
	bool            claimed = false;    ///< Indicates if the port is already allocated
	NodeCom         com     = {};       ///< NodeCom instance linked to this port
};


static UartCtx ports[UartComMaxPorts];  ///< Static pool of UART contexts
static uint8_t portsCount = 0;          ///< Number of allocated UART ports



// =============================================================================
// 2. ADAPTER FUNCTIONS
// =============================================================================


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
 * @param serial  Pointer to the HardwareSerial instance to use
 * @param baud    Baudrate for the connection
 * @param txPin   TX pin number
 * @param rxPin   RX pin number
 * @param owner   Name of the owning module (for debug)
 * @return NodeCom* Pointer to the initialized NodeCom interface, or nullptr on error
 *
 * @details
 *  - Initialization is refused if the port is already allocated (multi-claim protection)
 *  - Refused if the port pool is full
 *  - Configures the HardwareSerial port and adapts it to the NodeCom API
 */

NodeCom* uart_com_init( HardwareSerial* serial,
                        uint32_t        baud,
                        int             txPin,
                        int             rxPin,
                        const char*     owner ) {

		// --- Step 1.1: Validate the serial pointer ---
	if (!serial) {
		sys_log_err("[UART_COM] nullptr serial — init aborted\n");
		return nullptr;
	}

		// --- Step 1.2: Reject duplicate claim — one owner per physical port ---
	for (uint8_t i = 0u; i < portsCount; i++) {
		if (ports[i].serial == serial) {
			sys_log_err("[UART_COM] FATAL: port already claimed by '%s', rejected for '%s'\n",
			            ports[i].owner, owner);
			return nullptr;
		}
	}

		// --- Step 1.3: Ensure that maximum port count is not reached ---
	if (portsCount >= UartComMaxPorts) {
		sys_log_err("[UART_COM] FATAL: port pool full (%u max), rejected for '%s'\n",
		            (unsigned)UartComMaxPorts, owner);
		return nullptr;
	}

		// --- Step 2: Claim a context entry from the pool and record port metadata ---
	UartCtx* port    = &ports[portsCount++];  ///< Next free context entry in the pool
	port->serial     = serial;
	port->owner      = owner;
	port->claimed    = true;

		// --- Step 3: Bind the NodeCom function pointers to this context ---
	port->com.ctx       = port;
	port->com.write     = uart_write;
	port->com.readByte  = uart_readByte;
	port->com.available = uart_available;
	port->com.name      = owner;

		// --- Step 4: Configure and start the hardware serial port ---
	serial->begin(baud, SERIAL_8N1, rxPin, txPin);

	sys_log_info("[UART_COM] '%s' — baud=%u  tx=%d  rx=%d\n",
	             owner, baud, txPin, rxPin);

	return &port->com;
}

// EOF uart_com.cpp
