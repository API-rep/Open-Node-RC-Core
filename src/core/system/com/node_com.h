/******************************************************************************
 * @file node_com.h
 * @brief Generic inter-node communication interface.
 *
 * @details NodeCom is a lightweight structure of functions that acts as a bridge
 * between the communication modules (combus_tx/rx, ...) and the physical transport
 * layer (UART, ESP-Now, …). In this way, communication modules do not need to
 * know the details of the underlying transport implementation.
 *
 * Communication modules receive a NodeCom* structure pointer from the transport
 * layer at init time and use it to call only the following functions:
 *   com->write()     — send bytes
 *   com->readByte()  — receive one byte (-1 if none)
 *   com->available() — bytes waiting in the RX buffer
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stddef.h>


// =============================================================================
// 1. TRANSPORT INTERFACE
// =============================================================================

/**
 * @brief Generic transport structure of NodeCom function pointers.
 *
 * @details Filled by each physical transport layer adapter (uart_transport_init, …)
 * during initialization and passed to the communication modules (combus_tx/rx, …).
 *
 * @var NodeCom::ctx        Opaque pointer passed through every function pointer below.
 * @var NodeCom::write      Send @p len bytes from @p data via the physical transport.
 * @var NodeCom::readByte   Read one byte from the RX buffer; returns -1 if none available.
 * @var NodeCom::available  Number of bytes currently waiting in the RX buffer.
 * @var NodeCom::name       Human-readable port identifier.
 */

struct NodeCom {
	void*       ctx;                                          ///< adapter private context
	void      (*write)    (void* ctx, const uint8_t* data, size_t len); ///< send bytes
	int       (*readByte) (void* ctx);                        ///< read one byte (-1 = empty)
	int       (*available)(void* ctx);                        ///< RX bytes waiting
	const char* name;                                         ///< port id
};

// EOF transport.h

