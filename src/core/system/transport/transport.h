/******************************************************************************
 * @file transport.h
 * @brief Generic transport interface for inter-node communication.
 *
 * @details Defines TransportIface — a lightweight function-pointer table
 * that decouples protocol modules (combus_tx, combus_rx) from their
 * physical transport (UART, ESP-Now, …).
 *
 * Protocol modules receive a TransportIface* at init time and call only:
 *   iface->write()     — send bytes
 *   iface->readByte()  — receive one byte (-1 if none)
 *   iface->available() — bytes waiting in the RX buffer
 *
 * Physical transport adapters (uart_transport, …) implement these pointers
 * and expose an init function that returns a claimed TransportIface*.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stddef.h>


// =============================================================================
// 1. TRANSPORT INTERFACE
// =============================================================================

/**
 * @brief Generic transport function-pointer table.
 *
 * @details Filled by each transport adapter (uart_transport_init, …).
 *   ctx is an opaque pointer carried through every call so the implementation
 *   can reach its own private state without globals.
 */
struct TransportIface {
	void*       ctx;                                          ///< opaque adapter context
	void      (*write)    (void* ctx, const uint8_t* data, size_t len); ///< send bytes
	int       (*readByte) (void* ctx);                        ///< read one byte — -1 if none
	int       (*available)(void* ctx);                        ///< RX bytes ready to read
	const char* name;                                         ///< human-readable port id (logs, guard)
};

// EOF transport.h
