/******************************************************************************
 * @file transport.h
 * @brief Generic inter-node-link communication interface.
 *
 * @details Defines NodeLink — a lightweight structure of functions that acts as
 * a hardware abstraction layer between the communication modules (combus_tx/rx,
 *  ...) and the physical transport (UART, ESP-Now, …) layer.
 *
 * Communicationmodules receive a NodeLink* structure pointer at init time used  
 * to call only:
 *   link->write()     — send bytes
 *   link->readByte()  — receive one byte (-1 if none)
 *   link->available() — bytes waiting in the RX buffer
 *
 * Physical transport layer (uart_transport, …) implement function that returns
 * a NodeLink* pointer, usable by the communication modules.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stddef.h>


// =============================================================================
// 1. TRANSPORT INTERFACE
// =============================================================================

/**
 * @brief Generic transport structure of function pointers.
 *
 * @details Filled by each physical transport layer adapter (uart_transport_init, …)
 * during initialization and passed to the communication modules (combus_tx/rx, …).
 *   ctx is an opaque pointer carried through every call so the implementation
 *   can reach its own private state without globals.
 *
 * @var NodeLink::ctx        Opaque pointer use to passed argument to every function
 *                           pointer below.
 * @var NodeLink::write      Send @p len bytes from @p data via the physical transport.
 * @var NodeLink::readByte   Read one byte from the RX buffer; returns -1 if none available.
 * @var NodeLink::available  Number of bytes currently waiting in the RX buffer.
 * @var NodeLink::name       Human-readable port identifier.
 */

struct NodeLink {
	void*       ctx;                                          ///< adapter private context
	void      (*write)    (void* ctx, const uint8_t* data, size_t len); ///< send bytes
	int       (*readByte) (void* ctx);                        ///< read one byte (-1 = empty)
	int       (*available)(void* ctx);                        ///< RX bytes waiting
	const char* name;                                         ///< port id
};

// EOF transport.h
