/******************************************************************************
 * @file uart_com.cpp
 * @brief UART transport — port init, claim guard and channel helpers.
 *****************************************************************************/

#include "uart_com.h"

#include <core/system/debug/logging/debug.h>
#include <core/system/hw/pin_reg.h>
#include <struct/uart_struct.h>


// =============================================================================
// 1. PORT REGISTRY  (externally-owned storage — see uart_com_register_pool)
// =============================================================================

static UartCtx* g_ports    = nullptr;  ///< caller-owned static storage, registered at boot
static uint8_t  g_capacity = 0u;       ///< number of usable slots in g_ports
static uint8_t  g_used     = 0u;       ///< number of slots currently claimed

void uart_com_register_pool(UartCtx* buffer, uint8_t capacity) {
	if (g_ports != nullptr) {
		sys_log_err("[UART_COM] FATAL: pool already registered — ignoring second call\n");
		return;
	}
	if (buffer == nullptr || capacity == 0u) {
		sys_log_err("[UART_COM] FATAL: uart_com_register_pool called with null buffer or zero capacity\n");
		return;
	}
	g_ports    = buffer;
	g_capacity = capacity;
	g_used     = 0u;
}


// =============================================================================
// 2. PORT CALLBACKS
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

NodeCom* uart_com_init(
    HardwareSerial* serial,
    uint32_t        baud,
    int             txPin,
    int             rxPin,
    const char*     owner,
    PinReg*         reg )
 {
		// --- 1. Guard checks ---
		// --- 1.1: Pool not registered yet ---
	if (g_ports == nullptr) {
		sys_log_err("[UART_COM] FATAL: no pool registered — call uart_com_register_pool() before init ('%s')\n", owner);
		return nullptr;
	}

		// --- 1.2: Null serial pointer ---
	if (!serial) {
		sys_log_err("[UART_COM] nullptr serial — init aborted ('%s')\n", owner);
		return nullptr;
	}

		// --- 1.3: Reject duplicate claim — one owner per physical port ---
	for (uint8_t i = 0u; i < g_used; i++) {
		if (g_ports[i].serial == serial) {
			sys_log_err("[UART_COM] FATAL: port already claimed by '%s', rejected for '%s'\n", g_ports[i].owner, owner);
			return nullptr;
		}
	}

		// --- 1.4: Pool capacity exceeded ---
	if (g_used >= g_capacity) {
		sys_log_err("[UART_COM] FATAL: port pool full (%u max), rejected for '%s'\n", (unsigned)g_capacity, owner);
		return nullptr;
	}

		// --- 2. Claim a context entry from the registry and record port metadata ---
	UartCtx* port    = &g_ports[g_used++];
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

	sys_log_info("[UART_COM] '%s' — baud=%u  tx=%d  rx=%d\n", owner, baud, txPin, rxPin);

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


// EOF uart_com.cpp
