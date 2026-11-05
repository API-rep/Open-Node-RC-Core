/******************************************************************************
 * @file  node_com.h
 * @brief Transport interface and physical ports: what the protocol sees of the hardware.
 *
 * @details NodeCom is a small structure of functions, the bridge between the
 *   communication modules (combus_tx, combus_rx...) and the physical transport (UART,
 *   ESP-NOW...).  These modules do not need to know the details of the transport.
 *
 *   At init, a transport adapter (uart_com_init()) opens the physical port and returns a
 *   NodeCom, which the protocol keeps for the whole run.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stddef.h>



// =============================================================================
// 1. TRANSPORT INTERFACE
// =============================================================================

/**
 * @brief The functions of a transport, filled by its adapter.
 */

struct NodeCom {
	void*       ctx;                                                ///< private context of the adapter, passed to every function
	void      (*write)    (void* ctx, const uint8_t* data, size_t len); ///< send bytes
	int       (*readByte) (void* ctx);                              ///< read one byte (-1 = empty)
	int       (*available)(void* ctx);                              ///< number of RX bytes waiting
	const char* name;                                               ///< port identifier

		// The only peer this transport may deliver frames from, set once a coupled link is
		// paired, cleared when it is detached (addr nullptr).
		// nullptr: the transport has one possible peer by construction (a wire).
		// TODO(chantier 5, ESP-NOW): the ESP-NOW adapter implements it and drops, at
		// reception, every frame whose source MAC differs.
	void      (*setPeer)  (void* ctx, const uint8_t* addr, uint8_t len) = nullptr;
};



// =============================================================================
// 2. PHYSICAL PORT DESCRIPTION
// =============================================================================

/**
 * @brief Kind of communication hardware described by a ComPort.
 *
 * @details Only UART is implemented.  Each further kind (I2C, CAN, internal ESP-NOW
 *   controller) adds one value here, one configuration struct, one member in the ComPort
 *   union and one case where the port is opened.
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
 * @brief One communication port of a board.
 *
 * @details Purely descriptive, it lives in Flash.  The active member of the union is
 *   given by `type`.  Created with the static factories, so a board table reads as a
 *   list of ports:
 * @code
 *   const ComPort comPorts[] = {
 *       ComPort::makeUart("COM/GPIO", { 2, TxdExtPin, RxdExtPin, UartMaxBaud }),
 *   };
 * @endcode
 */

struct ComPort {
	const char* name;      ///< port name, as printed on the board silkscreen
	ComPortType type;      ///< selects the active member of the union below
	union {
		UartPortCfg uart;  ///< valid when type == ComPortType::UART
	};

	constexpr ComPort() : name(""), type(ComPortType::NONE), uart() {}
	constexpr ComPort(const char* portName, const UartPortCfg& cfg)
		: name(portName), type(ComPortType::UART), uart(cfg) {}

	static constexpr ComPort makeUart(const char* portName, const UartPortCfg& cfg) {
		return ComPort(portName, cfg);
	}
};

// EOF node_com.h
