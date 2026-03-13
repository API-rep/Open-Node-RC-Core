/******************************************************************************
 * @file uart_transport.cpp
 * @brief UART adapter implementation for TransportIface.
 *****************************************************************************/

#include "uart_transport.h"

#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Maximum number of simultaneously registered UART ports.
static constexpr uint8_t UART_TRANSPORT_MAX_PORTS = 3u;

/**
 * @brief Per-port internal context.
 *
 * @details Each entry owns a TransportIface instance whose ctx pointer
 *   points back to this UartCtx, allowing the static adapter functions
 *   to reach the correct HardwareSerial without captured state.
 */
struct UartCtx {
	HardwareSerial* serial   = nullptr;
	const char*     owner    = nullptr;
	bool            claimed  = false;
	TransportIface  iface    = {};
};

static UartCtx s_ports[UART_TRANSPORT_MAX_PORTS];
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

TransportIface* uart_transport_init( HardwareSerial* serial,
                                     uint32_t        baud,
                                     int             txPin,
                                     int             rxPin,
                                     const char*     owner ) {

	if (!serial) {
		sys_log_err("[UART_TRANSPORT] nullptr serial — init aborted\n");
		return nullptr;
	}

		// --- Guard: reject duplicate claim on the same port ---
	for (uint8_t i = 0u; i < s_portCount; i++) {
		if (s_ports[i].serial == serial) {
			sys_log_err("[UART_TRANSPORT] FATAL: port already claimed by '%s', rejected for '%s'\n",
			            s_ports[i].owner, owner);
			return nullptr;
		}
	}

		// --- Guard: pool exhausted ---
	if (s_portCount >= UART_TRANSPORT_MAX_PORTS) {
		sys_log_err("[UART_TRANSPORT] FATAL: port pool full (%u max), rejected for '%s'\n",
		            (unsigned)UART_TRANSPORT_MAX_PORTS, owner);
		return nullptr;
	}

		// --- Allocate slot ---
	UartCtx* p    = &s_ports[s_portCount++];
	p->serial     = serial;
	p->owner      = owner;
	p->claimed    = true;

		// --- Fill TransportIface (ctx → this slot) ---
	p->iface.ctx       = p;
	p->iface.write     = uart_write;
	p->iface.readByte  = uart_readByte;
	p->iface.available = uart_available;
	p->iface.name      = owner;

		// --- Initialize hardware ---
	serial->begin(baud, SERIAL_8N1, rxPin, txPin);

	sys_log_info("[UART_TRANSPORT] '%s' — baud=%u  tx=%d  rx=%d\n",
	             owner, baud, txPin, rxPin);

	return &p->iface;
}

// EOF uart_transport.cpp
