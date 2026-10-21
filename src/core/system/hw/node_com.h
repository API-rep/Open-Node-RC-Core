/******************************************************************************
 * @file node_com.h
 * @brief Generic inter-node communication interface.
 *
 * @details NodeCom is a lightweight structure of functions that acts as a bridge
 * between the communication modules (combus_tx/rx, ...) and the physical transport
 * layer (UART, ESP-Now, …). In this way, communication modules do not need to
 * know the details of the underlying transport implementation.
 *
 * At communication init, a transport adapter (e.g. uart_com_init) is called to
 * initialize the physical port. The adapter init returns a NodeCom* pointer 
 * that is passed to the protocol layer init (ex: `combus_tx_init()`) to interact
 * with the transport layer.
 * 
 * @code
 *   com->write()     — send bytes
 *   com->readByte()  — receive one byte (-1 if none)
 *   com->available() — bytes waiting in the RX buffer
 * @endcode
 *
 * The protocol module retains that pointer for the lifetime of the program.
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


// =============================================================================
// 2. PHYSICAL PORT DESCRIPTION
// =============================================================================

/**
 * @brief Kind of communication hardware described by a ComPort.
 *
 * @details Only UART is implemented. Each further kind (I2C, CAN, internal
 *   ESP-NOW controller) adds one value here, one configuration struct, one
 *   member in the ComPort union and one case where the port is opened.
 */

enum class ComPortType : uint8_t {
	NONE = 0,   ///< unused entry
	UART        ///< hardware UART
};


/**
 * @brief UART port parameters.
 */

struct UartPortCfg {
	uint8_t  index;     ///< UART peripheral number (0 = Serial, 1 = Serial1, 2 = Serial2)
	int8_t   txPin;     ///< GPIO TX pin (-1 = not wired)
	int8_t   rxPin;     ///< GPIO RX pin (-1 = not wired)
	uint32_t maxBaud;   ///< hardware baud-rate ceiling of this port
};


/**
 * @brief One communication port of a board (hardware, with or without pins).
 *
 * @details Purely descriptive and Flash-resident. The active member of the
 *   union is given by `type`. Created with the static factories, so a board
 *   table reads as a list of ports:
 *
 * @code
 *   const ComPort comPorts[] = {
 *       ComPort::makeUart("COM/GPIO", { 2, TxdExtPin, RxdExtPin, UartMaxBaud }),
 *   };
 * @endcode
 */

struct ComPort {
	const char*  name;      ///< port name, as printed on the board silkscreen
	ComPortType  type;      ///< selects the active member of the union below
	union {
		UartPortCfg uart;   ///< valid when type == ComPortType::UART
	};

	constexpr ComPort() : name(""), type(ComPortType::NONE), uart() {}
	constexpr ComPort(const char* portName, const UartPortCfg& cfg)
		: name(portName), type(ComPortType::UART), uart(cfg) {}

	static constexpr ComPort makeUart(const char* portName, const UartPortCfg& cfg) {
		return ComPort(portName, cfg);
	}
};

// EOF transport.h

