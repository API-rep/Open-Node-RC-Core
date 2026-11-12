/******************************************************************************
 * @file  bench_echo.cpp
 * @brief Two-board bench: the echo board, a TX <-> RX jumper made of a board.
 *
 * @details The loopback suites (test_combus_loopback, test_peer_link,
 *   test_remote_link) need a jumper between GPIO 17 and 16 of their board.  On the
 *   two-board bench, the wires are crossed between the boards: this firmware, on the
 *   other board, sends back on its TX every byte received on its RX.  The delay it
 *   adds is about one byte (87 us at 115200 baud).
 *
 *   Bench only: not part of any machine firmware.
 *****************************************************************************/

#include <Arduino.h>



// =============================================================================
// 1. SETUP AND LOOP
// =============================================================================

void setup()
{
	Serial.begin(115200);
	Serial2.begin(115200, SERIAL_8N1, 16, 17);   // RX 16, TX 17: the bench wires

	Serial.println("READY echo started");
}


void loop()
{
		// Each byte received goes back at once
	while (Serial2.available() > 0) {
		Serial2.write(static_cast<uint8_t>(Serial2.read()));
	}
}

// EOF bench_echo.cpp
