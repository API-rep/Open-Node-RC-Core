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
 *     c <pm> <bits>   corrupt pm per thousand of the writes to the link, 1 to `bits` bits
 *                     flipped at random in each; the counters start again (c 0 1: off,
 *                     the counters are kept)
 *     k <0|1>         watch of the bus: `k 1` takes the bus as the reference, then counts
 *                     every wrong value a frame writes on it (link alive, frame in place)
 *     f <kind> <ms>   forge frames on the link, as a third node would: one now (ms = 0),
 *                     or one every ms (f 0 0: off).  Kinds:
 *                       1 handshake request, foreign MD5      2 handshake request, other MAJOR
 *                       3 NEXT handshake request, foreign MD5
 *                       4 data: the last frame sent, other analog values
 *                       5 capture the last data frame sent    6 replay the frame captured
 *                       7 coupling ANSWER, foreign address    8 coupling REQUEST, foreign address
 *     s               print the status line
 *     x               restart the board
 *   The status line:
 *     S st=<state> ct=<0|1> nx=<0|1> pr=<pairing> age=<ms> den=<denied> up=<ms> lp=<ms>
 *       A=<v,...> D=<01...>
 *   (up: time since boot; lp: longest loop since the last status line, in ms, without
 *   the stall asked by `w`;
 *   cbmax, cbavg: longest and mean ComBus time of a loop, RX + TX, in us; ws: stalls run
 *   since boot; cx, tw: writes corrupted and writes made since `c`; kw: frames that
 *   wrote a wrong value on the bus, kl: loops spent with one, ka: loops with the link
 *   alive and a frame in place, since `k 1`; kf: first channel that differed, analog n
 *   or digital 100 + n, -1: none; kr: differences left by a loss, seen after a recovery
 *   before its first frame; lc: losses of the link since boot (alive, then not); fs:
 *   frames forged by `f` since boot; rf: data frames received since boot; bd, hz, to: line
 *   rate, frame rate and loss timeout of the link; fl: length of the last data frame sent.)
 *   The watch reads the bus once per loop: a wrong frame followed by a good one in the
 *   same pass is not seen, the counts are a lower bound.
 *   The READY line carries the reset reason (esp_reset_reason()).
 *   The channels are given by index: the PC reads the generated combus_ids.h.
 *
 *   Bench only: not part of any machine firmware.
 *****************************************************************************/

#include <Arduino.h>

#include <core/system/combus/combus_access.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/protocol/link/combus_link_setup.h>
#include <core/system/combus/protocol/link/combus_link_supervisor.h>
#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>
#include <core/system/combus/protocol/coupling/combus_coupling.h>
#include <core/system/com/com_manager.h>
#include <core/system/hw/transport/uart_port.h>
#include <combus.h>                // generated: comBus, the IDs
#include <combus_peer_links.h>     // generated: the tables of the links between nodes

#if defined(BENCH_MACHINE)
#include <core/system/input/remote_source.h>                    // remote_sources_update()
#include <core/system/input/remote_node/remote_node_source.h>   // the remote, a command source
#endif



// =============================================================================
// 1. THE LINK OF THE ROLE
// =============================================================================

	/// Line rate and frame rate, set at build time for the load tests (the same at both ends).
#ifndef BENCH_BAUD
	#define BENCH_BAUD 115200u
#endif
#ifndef BENCH_HZ
	#define BENCH_HZ 50u
#endif

	/// Loss timeout, set at build time for the mismatch tests (each end its own).
#ifndef BENCH_TIMEOUT
	#define BENCH_TIMEOUT 250u
#endif

	/// Line rate, frame rate, loss timeout, startup grace: the same at both ends, but in the mismatch tests.
static constexpr ComProtocolCfg kProto = { BENCH_BAUD, BENCH_HZ, BENCH_TIMEOUT, 3000u };

	/// The port of the bench: UART2, GPIO 17 TX / 16 RX.
static const ComPortCfg kPort = ComPortCfg::makeUart("bench", { 2, 17, 16, 115200u });

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
	/// The link between the dumper truck and its remote: its own table and MD5.  On the
	/// machine, a command source: its module writes the inputs (remote_node_source.h).
static constexpr ComDev kDev = {
	"remote", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL,
	&kProto, LinkActivation::ALWAYS, nullptr, &combus::peer::kRemoteLink,
#if defined(BENCH_MACHINE)
	true
#else
	false
#endif
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

#if defined(BENCH_MACHINE)
	/// The sources of the machine: its remote alone, on the link of the bench.
static RemoteNodeSource      s_remote   = { 0u, &combus::peer::kRemoteLink };
static const RemoteSourceDev kSources[] = { { "remote", remote_node_poll, remote_node_apply, &s_remote } };
static RemoteSources         s_sources  = {
	.devs   = kSources,
	.count  = 1u,
	.inputs = {
		{ combus::peer::kRemoteAnalogInputs,  combus::peer::kRemoteAnalogInputCount },
		{ combus::peer::kRemoteDigitalInputs, combus::peer::kRemoteDigitalInputCount },
	},
};
#endif



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

	// Corruption of the writes to the link (`c`)
static uint16_t s_cxPermille = 0u;    // share of the writes corrupted, per thousand
static uint8_t  s_cxBits     = 1u;    // a corrupted write gets 1 to s_cxBits bits flipped
static uint32_t s_cxWrites   = 0u;    // writes corrupted since `c`
static uint32_t s_txWrites   = 0u;    // writes made since `c`
static void   (*s_rawWrite)(void* ctx, const uint8_t* data, size_t len) = nullptr;

	// Watch of the bus (`k`): the values a wrong frame would write
static constexpr uint8_t kNA = static_cast<uint8_t>(AnalogComBusID::CH_COUNT);
static constexpr uint8_t kND = static_cast<uint8_t>(DigitalComBusID::CH_COUNT);
static bool     s_kOn       = false;
static bool     s_kWasWrong = false;
static uint16_t s_kA[kNA]   = {};
static bool     s_kD[kND]   = {};
static uint32_t s_kWrong    = 0u;     // wrong values written on the bus, counted once per frame
static uint32_t s_kWrongLp  = 0u;     // loops spent with a wrong value
static uint32_t s_kAlive    = 0u;     // loops with the link alive and a frame in place
static int      s_kFirst    = -1;     // first channel that differed
static uint32_t s_kRecov    = 0u;     // differences seen after a recovery, before its first frame
static bool     s_kWasAlive = false;
static bool     s_kFresh    = false;  // a frame arrived since the link came back alive
static uint8_t  s_kAliveSeq = 0u;     // seq of the snapshot when the link came back alive

	// Losses of the link, counted on every loop: a short one is seen too
static uint32_t s_lost      = 0u;
static bool     s_wasAlive  = false;

	// Forged frames (`f`): the last data frame sent, and the one captured for a replay
static uint8_t  s_fLast[255];
static uint8_t  s_fLastLen  = 0u;
static uint8_t  s_fCap[255];
static uint8_t  s_fCapLen   = 0u;
static uint8_t  s_fKind     = 0u;     // kind sent every s_fPeriod ms; 0: none
static uint32_t s_fPeriod   = 0u;
static uint32_t s_fAtMs     = 0u;
static uint32_t s_fSent     = 0u;     // frames forged since boot

	// Data frames received: one more each time the snapshot changes (one loop is far shorter
	// than the period of a frame)
static uint32_t s_rxFrames  = 0u;
static uint8_t  s_rxSeq     = 0u;


	/// The writes to the link go through here: some are corrupted (`c`), on a copy.
static void benchWrite( void* ctx, const uint8_t* data, size_t len )
{
	s_txWrites++;

		// Keep the last data frame (seq 1..255): the base of the forged data (`f`)
	if (len >= CombusFrameMinLen && len <= sizeof(s_fLast) && data[0] == CombusFrameSof
	 && data[1u + offsetof(CombusFrameHeader, seq)] != 0u) {
		memcpy(s_fLast, data, len);
		s_fLastLen = static_cast<uint8_t>(len);
	}

		// Check for a write left as it is
	if (s_cxPermille == 0u || len == 0u || len > 255u || (esp_random() % 1000u) >= s_cxPermille) {
		s_rawWrite(ctx, data, len);
		return;
	}

	uint8_t buf[255];
	memcpy(buf, data, len);

	const uint8_t nBits = static_cast<uint8_t>(1u + esp_random() % s_cxBits);

	for (uint8_t k = 0u; k < nBits; ++k) {
		const uint32_t bit = esp_random() % (static_cast<uint32_t>(len) * 8u);
		buf[bit / 8u] ^= static_cast<uint8_t>(1u << (bit % 8u));
	}

	s_cxWrites++;
	s_rawWrite(ctx, buf, len);
}


	/// `k 1`: the bus, with a frame in place, becomes the reference; false: no frame yet.
static bool watchStart( void )
{
	if (!combus_rx_snapshot(0u)) { return false; }

	for (uint8_t i = 0u; i < kNA; ++i) { s_kA[i] = comBus.analogBus[i].value; }
	for (uint8_t i = 0u; i < kND; ++i) { s_kD[i] = comBus.digitalBus[i].value; }

	s_kWrong = s_kWrongLp = s_kAlive = s_kRecov = 0u;
	s_kFirst    = -1;
	s_kWasWrong = false;
	s_kWasAlive = false;

	return true;
}


	/// Once per loop: does the bus differ from the reference?  Only while the link is alive
	/// with a frame in place: outside, the received channels hold their safe value.  A link
	/// back alive keeps its safe values until its first frame: that difference counts apart.
static void watchFrame( void )
{
	ComLinkView v;
	combus_links_view(0u, v);

	const ComBusFrame* snap = combus_rx_snapshot(0u);

		// Check for a live link with a frame
	if (v.state != ComLinkState::ALIVE || !snap) { s_kWasWrong = false; s_kWasAlive = false; return; }

		// Check for a link back alive, then for its first frame
	if (!s_kWasAlive) {
		s_kWasAlive = true;
		s_kFresh    = false;
		s_kAliveSeq = snap->header.seq;
	} else if (!s_kFresh && snap->header.seq != s_kAliveSeq) {
		s_kFresh = true;
	}

	s_kAlive++;

	int diff = -1;

	for (uint8_t i = 0u; diff < 0 && i < kNA; ++i) { if (comBus.analogBus[i].value  != s_kA[i]) { diff = i; } }
	for (uint8_t i = 0u; diff < 0 && i < kND; ++i) { if (comBus.digitalBus[i].value != s_kD[i]) { diff = 100 + i; } }

	if (diff < 0) { s_kWasWrong = false; return; }

		// Check for a difference left by the loss: no frame since the recovery
	if (!s_kFresh) {
		if (!s_kWasWrong) { s_kRecov++; }
		s_kWasWrong = true;
		return;
	}

		// A new wrong frame: counted once, the loops it lasts apart
	s_kWrongLp++;

	if (!s_kWasWrong) {
		s_kWrong++;
		if (s_kFirst < 0) { s_kFirst = diff; }
	}

	s_kWasWrong = true;
}


	/// A handshake request as another node would send it: foreign MD5 or other MAJOR, legacy or NEXT.
static void forgeHandshake( bool foreignMd5, bool otherMajor, bool next )
{
	uint8_t frame[CombusFrameHandshakeMinLen] = {};
	const uint8_t* md5 = s_runtime[0].handshake.expected.stable;

	frame[0] = CombusFrameSof;
	frame[1u + offsetof(CombusFrameHeader, flags)] = next ? kCombusHandshakeFlagNext : 0u;

	for (uint8_t i = 0u; i < 16u; ++i) { frame[CombusFrameHeaderLen + i] = md5 ? md5[i] : 0u; }

	if (foreignMd5) { frame[CombusFrameHeaderLen] ^= 0xFFu; }

	frame[CombusFrameHeaderLen + 16u] = static_cast<uint8_t>(combus::wire::kProjectVersionMajor + (otherMajor ? 1u : 0u));
	frame[CombusFrameHeaderLen + 17u] = combus::wire::kProjectVersionMinor;
	frame[CombusFrameHeaderLen + 18u] = combus::wire::kProjectVersionDev;

	combus_frame_crc_put(frame, static_cast<uint8_t>(sizeof(frame) - CombusFrameCrcLen));
	s_rawWrite(s_uart[0].port.ctx, frame, sizeof(frame));
}


	/// A coupling frame from a node that is not the peer: a made-up local address.
static void forgeCoupling( CouplingMsg msg )
{
	CouplingFrame f;
	f.msg        = msg;
	f.round      = 1u;
	f.addr.type  = LinkAddrType::MAC48;
	f.hitchAgeMs = 40u;

	const uint8_t addr[6] = { 0x02u, 0xBEu, 0x4Cu, 0x00u, 0x00u, 0x07u };
	memcpy(f.addr.bytes, addr, sizeof(addr));

	uint8_t buf[kCombusCouplingFrameLen];

	if (combus_coupling_encode(buf, sizeof(buf), f) == sizeof(buf)) {
		s_rawWrite(s_uart[0].port.ctx, buf, sizeof(buf));
	}
}


	/// One forged frame of `kind`; false if it cannot be made (no data frame sent yet).
static bool forge( uint8_t kind )
{
	switch (kind) {
		case 1u: forgeHandshake(true,  false, false); break;
		case 2u: forgeHandshake(false, true,  false); break;
		case 3u: forgeHandshake(true,  false, true);  break;
		case 4u: {
				// The last frame sent, every analog value moved: the layout of the link, other values
			if (s_fLastLen == 0u) { return false; }

			uint8_t buf[255];
			memcpy(buf, s_fLast, s_fLastLen);

			const uint8_t nA = buf[1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog)];
			const uint8_t nD = buf[1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital)];
			const uint8_t at = static_cast<uint8_t>(CombusFrameHeaderLen + (nD + 7u) / 8u);

			for (uint8_t i = 0u; i < nA; ++i) { buf[at + 2u * i] ^= 0x55u; }

			combus_frame_crc_put(buf, static_cast<uint8_t>(s_fLastLen - CombusFrameCrcLen));
			s_rawWrite(s_uart[0].port.ctx, buf, s_fLastLen);
			break;
		}
		case 5u:
			if (s_fLastLen == 0u) { return false; }
			memcpy(s_fCap, s_fLast, s_fLastLen);
			s_fCapLen = s_fLastLen;
			return true;
		case 6u:
			if (s_fCapLen == 0u) { return false; }
			s_rawWrite(s_uart[0].port.ctx, s_fCap, s_fCapLen);
			break;
		case 7u: forgeCoupling(CouplingMsg::ANSWER);  break;
		case 8u: forgeCoupling(CouplingMsg::REQUEST); break;
		default: return false;
	}

	s_fSent++;

	return true;
}


	/// One status line: the state of the link, then every channel of the bus.
static void printStatus( void )
{
	ComLinkView v;
	combus_links_view(0u, v);

	Serial.printf("S st=%u ct=%u nx=%u pr=%u age=%lu den=%lu up=%lu lp=%lu cbmax=%lu cbavg=%lu rxmax=%lu txmax=%lu ws=%lu cx=%lu tw=%lu kw=%lu kl=%lu ka=%lu kf=%d kr=%lu lc=%lu fs=%lu rf=%lu bd=%lu hz=%lu to=%lu fl=%u A=",
	              (unsigned)v.state, (unsigned)v.contractStable, (unsigned)v.nextMode,
	              (unsigned)combus_links_pairing_state(0u), (unsigned long)v.rxAgeMs,
	              (unsigned long)combus_denied_writes(), (unsigned long)millis(),
	              (unsigned long)(s_maxLoopUs / 1000u), (unsigned long)s_maxCbUs,
	              (unsigned long)(s_nCb ? s_sumCbUs / s_nCb : 0u),
	              (unsigned long)s_maxRxUs, (unsigned long)s_maxTxUs, (unsigned long)s_stalls,
	              (unsigned long)s_cxWrites, (unsigned long)s_txWrites, (unsigned long)s_kWrong,
	              (unsigned long)s_kWrongLp, (unsigned long)s_kAlive, s_kFirst, (unsigned long)s_kRecov,
	              (unsigned long)s_lost, (unsigned long)s_fSent, (unsigned long)s_rxFrames,
	              (unsigned long)kProto.rate, (unsigned long)kProto.txHz, (unsigned long)kProto.linkTimeoutMs,
	              (unsigned)s_fLastLen);

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
		case 'c':
			ok = (n == 3) && a >= 0 && a <= 1000 && b >= 1 && b <= 32 && s_rawWrite != nullptr;
			if (ok) {
				s_cxPermille = static_cast<uint16_t>(a);
				s_cxBits     = static_cast<uint8_t>(b);
				if (a > 0) { s_cxWrites = s_txWrites = 0u; }   // c 0 keeps the counts, to be read
			}
			break;
		case 'k':
			ok = (n == 2);
			s_kOn = ok && (a != 0) && watchStart();
			ok = ok && (a == 0 || s_kOn);
			break;
		case 'f':
			ok = (n == 3) && a >= 0 && a <= 8 && b >= 0 && b <= 10000 && s_rawWrite != nullptr;
			if (ok) {
				s_fKind   = (b > 0) ? static_cast<uint8_t>(a) : 0u;
				s_fPeriod = static_cast<uint32_t>(b);
				s_fAtMs   = millis();
				if (a > 0 && b == 0) { ok = forge(static_cast<uint8_t>(a)); }
			}
			break;
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
	uart_port_register_pool(s_uart, 2u);

	const ComLinksStorage storage = { s_tx, s_rx, s_runtime, s_links };
	const bool started = combus_links_init(kMaps, 1u, storage, nullptr, &comBus);

		// The writes to the link go through benchWrite (`c`)
	if (started && s_uart[0].port.write) {
		s_rawWrite           = s_uart[0].port.write;
		s_uart[0].port.write = benchWrite;
	}

	Serial.printf("READY %s %s reason=%d\n", kDev.name, started ? "started" : "NOT STARTED",
	              (int)esp_reset_reason());
}


void loop()
{
	const uint32_t t0 = micros();

		// Reception off: the bytes of the link are dropped, as with a cut RX wire (through the
		// ComPort of the slot: its cache goes too)
	if (!s_rxOn) {
		ComPort& port = s_uart[0].port;

		while (port.available(port.ctx) > 0) { port.readByte(port.ctx); }
	}

	const uint32_t rx0 = micros();

	com_rx_update(comBus);

#if defined(BENCH_MACHINE)
		// The sources write the inputs, and NODE_LINK_FAIL
	remote_sources_update(s_sources, comBus);
#endif

	uint32_t cb = micros() - rx0;

	if (cb > s_maxRxUs) { s_maxRxUs = cb; }

	if (s_kOn) { watchFrame(); }

		// Data frames received
	if (const ComBusFrame* snap = combus_rx_snapshot(0u)) {
		if (snap->header.seq != s_rxSeq) { s_rxSeq = snap->header.seq; s_rxFrames++; }
	}

		// Losses of the link, every loop
	{
		ComLinkView v;
		combus_links_view(0u, v);

		const bool alive = (v.state == ComLinkState::ALIVE);

		if (s_wasAlive && !alive) { s_lost++; }

		s_wasAlive = alive;
	}

		// Forged frames, one every s_fPeriod ms
	if (s_fKind != 0u && (millis() - s_fAtMs) >= s_fPeriod) {
		s_fAtMs = millis();
		forge(s_fKind);
	}

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
