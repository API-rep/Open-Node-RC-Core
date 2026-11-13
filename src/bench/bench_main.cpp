/******************************************************************************
 * @file  bench_main.cpp
 * @brief Two-board bench: one end of a real link, driven from a PC console.
 *
 * @details Each board of the bench runs this firmware with one role flag:
 *     BENCH_TRACTOR / BENCH_TRAILER   the TEST coupler, socket and plug (coupled link)
 *     BENCH_MACHINE / BENCH_REMOTE    the link between a dumper truck and its remote
 *     BENCH_NODE                      the link between two boards of one node (SYSTEM)
 *   The link uses UART2 on GPIO 17 (TX) and 16 (RX): the two boards are wired
 *   crossed (17 → 16 both ways).
 *
 *   The PC drives the board over the USB serial (test/two_boards/bench.py), one
 *   command per line:
 *     a <idx> <raw>   write an analog channel (raw value)
 *     d <idx> <0|1>   write a digital channel
 *     t <0|1>         sending on / off (off: the link falls silent)
 *     r <0|1>         reception on / off (off: the received bytes are dropped)
 *     g <n>           send n random bytes on the link (noise on the wire, n <= 255)
 *     w <ms>          block the board for ms (busy wait, ms <= 5000), answered after it
 *     s               print the status line
 *     x               restart the board
 *   The status line:
 *     S st=<state> ct=<0|1> nx=<0|1> pr=<pairing> age=<ms> den=<denied> up=<ms> lp=<ms>
 *       A=<v,...> D=<01...>
 *   (up: time since boot; lp: longest loop since the last status line, in ms, without
 *   the stall asked by `w`;
 *   cbmax, cbavg: longest and mean ComBus time of a loop, RX + TX, in us; ws: stalls run
 *   since boot.)
 *   The READY line carries the reset reason (esp_reset_reason()).
 *   The channels are given by index: the PC reads the generated combus_ids.h.
 *
 *   Bench only: not part of any machine firmware.
 *****************************************************************************/

#include <Arduino.h>

#include <core/system/combus/combus_access.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/link/combus_links.h>
#include <core/system/combus/protocol/link/combus_link_watch.h>
#include <core/system/com/com_manager.h>
#include <core/system/hw/transport/uart_com.h>
#include <combus.h>                // generated: comBus, the IDs
#include <combus_peer_links.h>     // generated: the tables of the links between nodes



// =============================================================================
// 1. THE LINK OF THE ROLE
// =============================================================================

	/// Line rate, frame rate, loss timeout, startup grace: the same at both ends.
static constexpr ComProtocolCfg kProto = { 115200u, 50u, 250u, 3000u };

	/// The port of the bench: UART2, GPIO 17 TX / 16 RX.
static const ComPort kPort = ComPort::makeUart("bench", { 2, 17, 16, 115200u });

#if defined(BENCH_TRACTOR) || defined(BENCH_TRAILER)
	/// Coupled link: hitched by TEST_HITCHED, its loss in TEST_HITCH_LINK_FAIL.
static constexpr CouplingDev kHitch = {
	"test_coupler", DigitalComBusID::TEST_HITCHED, DigitalComBusID::TEST_HITCH_LINK_FAIL, 200u, 50u, 300u
};
static constexpr ComDev kDev = {
	"convoy", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL,
	&kProto, LinkActivation::ON_COUPLING, &kHitch, &combus::peer::TestCoupler::kLink
};
#elif defined(BENCH_MACHINE) || defined(BENCH_REMOTE)
	/// The link between the dumper truck and its remote: its own table and MD5.
static constexpr ComDev kDev = {
	"remote", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL,
	&kProto, LinkActivation::ALWAYS, nullptr, &combus::peer::kRemoteLink
};
#elif defined(BENCH_NODE)
	/// The link between two boards of one node: the SYSTEM set, no table.
static constexpr ComDev kDev = {
	"node", LinkScope::INTRA_NODE, LinkType::BIDIRECTIONAL,
	&kProto, LinkActivation::ALWAYS, nullptr
};
#else
	#error "bench: set one role flag (BENCH_TRACTOR, BENCH_TRAILER, BENCH_MACHINE, BENCH_REMOTE, BENCH_NODE)"
#endif

static const ComDevMap kMaps[] = { { &kDev, &kPort, LinkEnd::TX_RX } };

	// Storage of the link
static UartCtx        s_uart[2];          // the link, and the debug serial
static CombusTxState  s_tx[1];
static CombusRxState  s_rx[1];
static ComLinkRuntime s_runtime[1];
static ComBusLink     s_links[1];



// =============================================================================
// 2. CONSOLE
// =============================================================================

static bool     s_txOn      = true;
static bool     s_rxOn      = true;
static uint32_t s_maxLoopUs = 0u;     // longest loop since the last status line
static uint32_t s_maxCbUs   = 0u;     // longest ComBus time (RX + TX) since then
static uint32_t s_maxRxUs   = 0u;     // longest RX part since then
static uint32_t s_maxTxUs   = 0u;     // longest TX part since then
static uint32_t s_sumCbUs   = 0u;     // ComBus time summed since then, for the mean
static uint32_t s_nCb       = 0u;     // loops since then
static uint32_t s_stallUs   = 0u;     // the stall asked by `w` in this loop: not counted in lp
static uint32_t s_stalls    = 0u;     // stalls run since boot: the PC sees whether its `w` arrived


	/// One status line: the state of the link, then every channel of the bus.
static void printStatus( void )
{
	ComLinkView v;
	combus_links_view(0u, v);

	Serial.printf("S st=%u ct=%u nx=%u pr=%u age=%lu den=%lu up=%lu lp=%lu cbmax=%lu cbavg=%lu rxmax=%lu txmax=%lu ws=%lu A=",
	              (unsigned)v.state, (unsigned)v.contractStable, (unsigned)v.nextMode,
	              (unsigned)combus_links_pairing_state(0u), (unsigned long)v.rxAgeMs,
	              (unsigned long)combus_denied_writes(), (unsigned long)millis(),
	              (unsigned long)(s_maxLoopUs / 1000u), (unsigned long)s_maxCbUs,
	              (unsigned long)(s_nCb ? s_sumCbUs / s_nCb : 0u),
	              (unsigned long)s_maxRxUs, (unsigned long)s_maxTxUs, (unsigned long)s_stalls);

	s_maxLoopUs = 0u;
	s_maxCbUs   = 0u;
	s_maxRxUs   = 0u;
	s_maxTxUs   = 0u;
	s_sumCbUs   = 0u;
	s_nCb       = 0u;

	for (uint8_t i = 0u; i < static_cast<uint8_t>(AnalogComBusID::CH_COUNT); ++i) {
		Serial.printf(i ? ",%u" : "%u", (unsigned)comBus.analogBus[i].value);
	}

	Serial.print(" D=");

	for (uint8_t i = 0u; i < static_cast<uint8_t>(DigitalComBusID::CH_COUNT); ++i) {
		Serial.print(comBus.digitalBus[i].value ? '1' : '0');
	}

	Serial.println();
}


	/// Run one command line; the answer is "OK <line>", or "ERR <line>".
static void runCommand( char* line )
{
	char cmd = 0;
	long a   = 0;
	long b   = 0;
	const int n = sscanf(line, " %c %ld %ld", &cmd, &a, &b);
	bool ok = true;

	switch (cmd) {
		case 'a':
			ok = (n == 3) && a >= 0 && a < static_cast<long>(AnalogComBusID::CH_COUNT);
			if (ok) { combus_set_raw_analog(comBus, static_cast<AnalogComBusID>(a), static_cast<uint16_t>(b)); }
			break;
		case 'd':
			ok = (n == 3) && a >= 0 && a < static_cast<long>(DigitalComBusID::CH_COUNT);
			if (ok) { combus_set_digital(comBus, static_cast<DigitalComBusID>(a), b != 0); }
			break;
		case 't':  ok = (n == 2); s_txOn = (a != 0); break;
		case 'r':  ok = (n == 2); s_rxOn = (a != 0); break;
		case 'g':
			ok = (n == 2) && a > 0 && a <= 255;
			for (long i = 0; ok && i < a; ++i) { Serial2.write(static_cast<uint8_t>(esp_random())); }
			break;
		case 'w': {
				// A module that blocks: busy wait, nothing else runs, the UART fills up
			ok = (n == 2) && a > 0 && a <= 5000;
			const uint32_t t0 = millis();
			const uint32_t u0 = micros();
			while (ok && (millis() - t0) < static_cast<uint32_t>(a)) { }
			s_stallUs = micros() - u0;
			if (ok) { ++s_stalls; }
			break;
		}
		case 's':  printStatus(); return;
		case 'x':  Serial.println("OK x"); Serial.flush(); ESP.restart(); return;
		default:   ok = false; break;
	}

	Serial.printf("%s %s\n", ok ? "OK" : "ERR", line);
}


	/// Read the console without blocking; one full line runs one command.
static void pollConsole( void )
{
	static char    buf[48];
	static uint8_t len = 0u;

	while (Serial.available() > 0) {
		const char c = static_cast<char>(Serial.read());

		if (c == '\n' || c == '\r') {
			if (len > 0u) { buf[len] = '\0'; runCommand(buf); len = 0u; }
			continue;
		}
		if (len < sizeof(buf) - 1u) { buf[len++] = c; }
	}
}



// =============================================================================
// 3. SETUP AND LOOP
// =============================================================================

void setup()
{
	Serial.begin(115200);
	uart_com_register_pool(s_uart, 2u);

	const ComLinksStorage storage = { s_tx, s_rx, s_runtime, s_links };
	const bool started = combus_links_init(kMaps, 1u, storage, nullptr, &comBus);

	Serial.printf("READY %s %s reason=%d\n", kDev.name, started ? "started" : "NOT STARTED",
	              (int)esp_reset_reason());
}


void loop()
{
	const uint32_t t0 = micros();

		// Reception off: the bytes of the link are dropped, as with a cut RX wire (through the
		// NodeCom of the port: its cache goes too)
	if (!s_rxOn) {
		NodeCom& com = s_uart[0].com;

		while (com.available(com.ctx) > 0) { com.readByte(com.ctx); }
	}

	const uint32_t rx0 = micros();

	com_rx_update(comBus);

	uint32_t cb = micros() - rx0;

	if (cb > s_maxRxUs) { s_maxRxUs = cb; }

		// After the update: the status line reads the contributors just published
	pollConsole();

	const uint32_t tx0 = micros();

	if (s_txOn) { combus_tx_update(&comBus); }

		// ComBus time alone, RX + TX
	const uint32_t tx = micros() - tx0;

	if (tx > s_maxTxUs) { s_maxTxUs = tx; }

	cb += tx;

	if (cb > s_maxCbUs) { s_maxCbUs = cb; }

	s_sumCbUs += cb;
	s_nCb++;

		// Longest loop, without the pause nor the stall asked (the loop after it counts)
	const uint32_t dt = micros() - t0 - s_stallUs;

	s_stallUs = 0u;

	if (dt > s_maxLoopUs) { s_maxLoopUs = dt; }

	delay(2);
}

// EOF bench_main.cpp
