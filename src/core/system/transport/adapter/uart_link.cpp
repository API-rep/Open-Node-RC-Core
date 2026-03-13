/******************************************************************************
 * @file uart_link.cpp
 * @brief UART adapter implementation for NodeLink.
 *****************************************************************************/

#include "uart_link.h"

#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Maximum number of simultaneously registered UART ports.
static constexpr uint8_t UART_LINK_MAX_PORTS = 3u;

/**
 * @brief Per-port internal context.
 *
 * @details Each entry owns a NodeLink instance whose ctx pointer
 *   points back to this UartCtx, allowing the static adapter functions
 *   to reach the correct HardwareSerial without captured state.
 */
struct UartCtx {
	HardwareSerial* serial   = nullptr;
	const char*     owner    = nullptr;
	bool            claimed  = false;
	NodeLink  link    = {};
};

static UartCtx s_ports[UART_LINK_MAX_PORTS];
static uint8_t s_portCount = 0u;


// =============================================================================
// 2. ADAPTER FUNCTIONS
// =============================================================================

static void uart_write(void* ctx, const uint8_t* data, size_t len) {
	static_cast<UartCtx*>(ctx)->serial->write(data, len);
}

static int uart_readByte(void* ctx) {
	return static_cast<UartCtx*>(ctx)->serial->read();
}

static int uart_available(void* ctx) {
	return static_cast<UartCtx*>(ctx)->serial->available();
}


// =============================================================================
// 3. PUBLIC API
// =============================================================================

NodeLink* uart_link_init( HardwareSerial* serial,
                                     uint32_t        baud,
                                     int             txPin,
                                     int             rxPin,
                                     const char*     owner ) {

	if (!serial) {
		sys_log_err("[UART_LINK] nullptr serial — init aborted\n");
		return nullptr;
	}

		// --- Guard: reject duplicate claim on the same port ---
	for (uint8_t i = 0u; i < s_portCount; i++) {
		if (s_ports[i].serial == serial) {
			sys_log_err("[UART_LINK] FATAL: port already claimed by '%s', rejected for '%s'\n",
			            s_ports[i].owner, owner);
			return nullptr;
		}
	}

		// --- Guard: pool exhausted ---
	if (s_portCount >= UART_LINK_MAX_PORTS) {
		sys_log_err("[UART_LINK] FATAL: port pool full (%u max), rejected for '%s'\n",
		            (unsigned)UART_LINK_MAX_PORTS, owner);
		return nullptr;
	}

		// --- Allocate slot ---
	UartCtx* p    = &s_ports[s_portCount++];
	p->serial     = serial;
	p->owner      = owner;
	p->claimed    = true;

		// --- Fill NodeLink (ctx → this slot) ---
	p->link.ctx       = p;
	p->link.write     = uart_write;
	p->link.readByte  = uart_readByte;
	p->link.available = uart_available;
	p->link.name      = owner;

		// --- Initialize hardware ---
	serial->begin(baud, SERIAL_8N1, rxPin, txPin);

	sys_log_info("[UART_LINK] '%s' — baud=%u  tx=%d  rx=%d\n",
	             owner, baud, txPin, rxPin);

	return &p->link;
}

// EOF uart_link.cpp
