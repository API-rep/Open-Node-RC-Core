/******************************************************************************
 * @file  combus_pairing.cpp
 * @brief Pairing of a coupled link: who is hitched to whom.
 *
 * @details The rules are in combus_pairing.h.  The trigger is common to both ends; then
 *   each role has its own step and its own reading of the received frames.
 *****************************************************************************/

#include "combus_pairing.h"

#include <string.h>
#include <esp_random.h>   // esp_random()

#include <core/system/debug/logging/debug.h>



// =============================================================================
// 1. HELPERS
// =============================================================================

namespace {

	// Delay between two rounds: 0.75 to 1.25 × the base period.
constexpr uint32_t kMinRoundDelayMs = (kCouplingBasePeriodMs * 3u) / 4u;
constexpr uint32_t kMaxRoundDelayMs = (kCouplingBasePeriodMs * 5u) / 4u;


	/// This node's identity, read once: the factory MAC does not change.
const NodeAddr& localAddr()
{
	static const NodeAddr s_addr = combus_coupling_local_addr();

	return s_addr;
}


	/// A value drawn in [lo, hi].
uint32_t draw( uint32_t lo, uint32_t hi )
{
	return lo + (esp_random() % (hi - lo + 1u));
}


	/// Drawn between kMinRoundDelayMs and kMaxRoundDelayMs.
uint32_t roundDelay()
{
	return draw(kMinRoundDelayMs, kMaxRoundDelayMs);
}


	/// Same type and same bytes.
bool sameAddr( const NodeAddr& a, const NodeAddr& b )
{
	return a.type == b.type && memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}


	/// The two ages of the hitch agree within the tolerance of the CouplingDev.
bool agrees( const ComLinkPairing& p, const CouplingDev& dev, uint16_t wireAge, uint32_t now )
{
	const uint32_t own  = combus_coupling_hitch_age(now - p.t0Ms);
	const uint32_t diff = (own > wireAge) ? (own - wireAge) : (wireAge - own);

	return diff <= dev.ageToleranceMs;
}


	/// Number of bits set: the rounds a candidate took part in.
uint8_t popcount8( uint8_t v )
{
	uint8_t n = 0u;

		// Clear the lowest bit set, until none is left
	for (; v; v &= (uint8_t)(v - 1u)) { ++n; }

	return n;
}


/**
 * @brief Candidate lookup. Gives the index of an identity, added if new.
 *
 * @param p     Pairing state.
 * @param addr  Identity heard.
 *
 * @return Its index, or -1 when kCouplingMaxCandidates are already known.
 */

int candidateOf( ComLinkPairing& p, const NodeAddr& addr )
{
		// Check for a known identity
	for (uint8_t i = 0u; i < p.nCand; ++i) {
		if (sameAddr(p.cand[i].addr, addr)) { return i; }
	}

		// Check for room, then add it
	if (p.nCand >= kCouplingMaxCandidates) { return -1; }

	p.cand[p.nCand] = PairingCandidate{ addr, 0u };

	return p.nCand++;
}


/**
 * @brief Coupling frame sender. Writes one request or answer to the transport.
 *
 * @details The functions mask is reserved: always 0 until defined.
 *
 * @param com    Transport of the link; nullptr or without write: nothing is sent.
 * @param msg    Request or answer.
 * @param round  Round of the pairing.
 * @param age    Age of the hitch of this end.
 */

void send( NodeCom* com, CouplingMsg msg, uint8_t round, uint16_t age )
{
	if (!com || !com->write) { return; }

	CouplingFrame f;
	f.msg        = msg;
	f.round      = round;
	f.addr       = localAddr();
	f.hitchAgeMs = age;
	f.features   = 0u;

	uint8_t buf[kCombusCouplingFrameLen];
	const uint8_t len = combus_coupling_encode(buf, sizeof(buf), f);

	if (len) { com->write(com->ctx, buf, len); }
}


	/// Identity at the coupling: read when there is a reader and it reads, else the default.
void identify( ComLinkPairing& p, const CouplingDev& dev )
{
	CouplingId read;

	p.id = (dev.readId && dev.readId(read)) ? read : dev.defaultId;
}


	/// What the debounced trigger did at this step.
enum class TriggerStep : uint8_t {
	IDLE,       ///< not engaged
	ENGAGED,    ///< just engaged (the trigger has stayed true for engageMs)
	ACTIVE,     ///< engaged, trigger true
	HELD,       ///< engaged, trigger dropped for less than releaseMs: the link goes on
	RELEASED    ///< just released (the trigger has stayed false for releaseMs)
};


/**
 * @brief Trigger debounce. Turns the raw trigger into one TriggerStep.
 *
 * @details In order:
 *   1. A raw change: its time is kept.  t0 is the raw rising edge, not the debounced
 *      one, so that both ends count the age of the hitch from the same moment.  A drop
 *      while engaged is a glitch; logged if the trigger comes back in time.
 *   2. Not engaged: ENGAGED once true for engageMs.
 *   3. Engaged: ACTIVE while true, HELD while false for less than releaseMs, then
 *      RELEASED.
 *
 * @param p        Pairing state.
 * @param dev      CouplingDev, for engageMs and releaseMs.
 * @param trigger  Raw value of the trigger.
 * @param now      millis().
 *
 * @return The step.
 */

TriggerStep stepTrigger( ComLinkPairing& p, const CouplingDev& dev, bool trigger, uint32_t now )
{
		// 1. Raw change
	if (trigger != p.rawTrigger) {
		p.rawTrigger  = trigger;
		p.rawChangeMs = now;

		if (trigger && !p.engaged) { p.t0Ms = now; }

			// Check for a glitch that is over
		if (trigger && p.glitch) {
			p.glitch = false;
			sys_log_info("[COMBUS_PAIRING] %s: hitch glitch (trigger back within %u ms)\n",
			             dev.name, (unsigned)dev.releaseMs);
		}

		if (!trigger && p.engaged) { p.glitch = true; }
	}

		// 2. Not engaged
	if (!p.engaged) {
		if (!trigger || (now - p.rawChangeMs) < dev.engageMs) { return TriggerStep::IDLE; }

		p.engaged = true;

		return TriggerStep::ENGAGED;
	}

		// 3. Engaged
	if (trigger) { return TriggerStep::ACTIVE; }
	if ((now - p.rawChangeMs) < dev.releaseMs) { return TriggerStep::HELD; }

	p.engaged = false;
	p.glitch  = false;

	return TriggerStep::RELEASED;
}


	/// A fresh pairing: the socket sends its first request at once, the plug has no window yet.
void startPairing( ComLinkPairing& p, uint32_t now )
{
	p.state         = PairingState::PAIRING;
	p.round         = 0u;
	p.nextMs        = now;
	p.nCand         = 0u;
	p.answerPending = false;
}


/**
 * @brief Conclusion of a pairing. Sets the state and logs it.
 *
 * @details COUPLED: the identity of the coupled element is read, and the single
 *   candidate becomes the peer of the coupling.  Any other state is logged with its
 *   reason.
 *
 * @param p      Pairing state.
 * @param dev    CouplingDev, for the identity and the logs.
 * @param state  COUPLED, CONFLICT or FAILED.
 * @param why    Reason, logged when not COUPLED.
 */

void conclude( ComLinkPairing& p, const CouplingDev& dev, PairingState state, const char* why )
{
	p.state = state;

	if (state == PairingState::COUPLED) {
		identify(p, dev);
		p.peer          = p.cand[0].addr;
		p.foreignFrames = 0u;
		sys_log_info("[COMBUS_PAIRING] %s: paired\n", dev.name);
	} else {
		sys_log_warn("[COMBUS_PAIRING] %s: %s (%s)\n", dev.name, combus_pairing_state_name(state), why);
	}
}



// =============================================================================
// 2. EACH ROLE
// =============================================================================

/**
 * @brief Socket step. Requests, final listening, conclusion.
 *
 * @details In order:
 *   0. CONFLICT with another socket: the one request sent back, once.
 *   1. A FAILED pairing starts again once its delay is over, while retries are left.
 *   2. Requests: one per round when due, the next one after a drawn delay; after the
 *      last one, one base period of listening.
 *   3. Conclusion: no answer or too few rounds: FAILED, retried later; two identities:
 *      CONFLICT; one, in enough rounds: COUPLED.
 *
 * @param p    Pairing state.
 * @param dev  CouplingDev of the link.
 * @param com  Transport of the link.
 * @param now  millis().
 */

void stepSocket( ComLinkPairing& p, const CouplingDev& dev, NodeCom* com, uint32_t now )
{
		// 0. Another socket heard: one request back.  It is pairing, since it has just
		//    requested: it hears this one and sees the conflict too.
	if (p.state == PairingState::CONFLICT && p.answerPending) {
		p.answerPending = false;
		send(com, CouplingMsg::REQUEST, p.answerRound, combus_coupling_hitch_age(now - p.t0Ms));

		return;
	}

		// 1. Retry
	if (p.state == PairingState::FAILED && p.retriesLeft > 0u && (int32_t)(now - p.nextMs) >= 0) {
		p.retriesLeft--;
		startPairing(p, now);
	}

	if (p.state != PairingState::PAIRING) { return; }

		// 2. Requests
	if (p.round < kCouplingRounds) {
		if ((int32_t)(now - p.nextMs) >= 0) {
			p.round++;
			send(com, CouplingMsg::REQUEST, p.round, combus_coupling_hitch_age(now - p.t0Ms));
			p.nextMs = now + ((p.round < kCouplingRounds) ? roundDelay() : kCouplingBasePeriodMs);
		}

		return;
	}

		// Check that the final listening is over
	if ((int32_t)(now - p.nextMs) < 0) { return; }

		// 3. Conclusion
	if (p.nCand == 0u) {
		conclude(p, dev, PairingState::FAILED, "no answer");
	} else if (p.nCand > 1u) {
		conclude(p, dev, PairingState::CONFLICT, "two plugs answered");
	} else if (popcount8(p.cand[0].rounds) >= kCouplingMinAnswers) {
		conclude(p, dev, PairingState::COUPLED, "");
	} else {
		conclude(p, dev, PairingState::FAILED, "too few answers");
	}

	if (p.state == PairingState::FAILED) { p.nextMs = now + kCouplingRetryMs; }
}


/**
 * @brief Plug step. Sends the pending answer, then concludes when the window is over.
 *
 * @details In order:
 *   1. The pending answer is sent once its drawn delay is over.
 *   2. Nothing heard yet, an answer still pending, or the window still open: wait.
 *   3. Conclusion: enough rounds: COUPLED; fewer: the candidate is forgotten and the
 *      plug listens again.
 *
 * @param p    Pairing state.
 * @param dev  CouplingDev of the link.
 * @param com  Transport of the link.
 * @param now  millis().
 */

void stepPlug( ComLinkPairing& p, const CouplingDev& dev, NodeCom* com, uint32_t now )
{
	if (p.state != PairingState::PAIRING) { return; }

		// 1. Pending answer
	if (p.answerPending && (int32_t)(now - p.answerAtMs) >= 0) {
		p.answerPending = false;
		send(com, CouplingMsg::ANSWER, p.answerRound, combus_coupling_hitch_age(now - p.t0Ms));
	}

		// 2. Wait
	if (p.nCand == 0u || p.answerPending || (int32_t)(now - p.nextMs) < 0) { return; }

		// 3. Conclusion
	if (popcount8(p.cand[0].rounds) >= kCouplingMinAnswers) {
		conclude(p, dev, PairingState::COUPLED, "");
	} else {
		p.nCand = 0u;
	}
}


/**
 * @brief Socket reception. Counts an answer that agrees, to a round already requested.
 *
 * @details In order:
 *   1. A request that agrees, from another identity: another socket is hitched to this
 *      one.  CONFLICT at once, never retried, and one request is sent back (see
 *      stepSocket()).  Its own requests (an echo) are ignored.
 *   2. Answers that agree, to a round already requested.
 *   3. A second identity is kept as a second candidate: the conclusion makes it a
 *      CONFLICT.  Beyond kCouplingMaxCandidates, the frame is ignored.
 *
 * @param p      Pairing state.
 * @param dev    CouplingDev of the link.
 * @param frame  Frame received.
 * @param bit    Bit of its round.
 * @param now    millis().
 */

void onSocketFrame( ComLinkPairing& p, const CouplingDev& dev, const CouplingFrame& frame, uint8_t bit, uint32_t now )
{
		// 1. Another socket
	if (frame.msg == CouplingMsg::REQUEST) {
		if (!sameAddr(frame.addr, localAddr()) && agrees(p, dev, frame.hitchAgeMs, now)) {
			conclude(p, dev, PairingState::CONFLICT, "another socket requested");

			p.answerPending = true;
			p.answerRound   = frame.round;
		}

		return;
	}

		// 2. Answers that agree
	if (frame.msg != CouplingMsg::ANSWER || frame.round > p.round) { return; }
	if (!agrees(p, dev, frame.hitchAgeMs, now)) { return; }

		// 3. Candidate
	const int c = candidateOf(p, frame.addr);

	if (c < 0) { return; }

	p.cand[c].rounds |= bit;
}


/**
 * @brief Plug reception. Answers a request that agrees.
 *
 * @details In order:
 *   1. Requests only, whose age agrees.
 *   2. A second socket: CONFLICT at once, and the pending answer is dropped.
 *   3. The first request opens the window: room for every round at its longest delay,
 *      plus one base period.  The last round closes it sooner, one base period after
 *      its answer.
 *   4. The answer is scheduled after a delay drawn from 0 to kCouplingAnswerJitterMaxMs.
 *
 * @param p      Pairing state.
 * @param dev    CouplingDev of the link.
 * @param frame  Frame received.
 * @param bit    Bit of its round.
 * @param now    millis().
 */

void onPlugFrame( ComLinkPairing& p, const CouplingDev& dev, const CouplingFrame& frame, uint8_t bit, uint32_t now )
{
		// 1. Requests that agree
	if (frame.msg != CouplingMsg::REQUEST) { return; }
	if (!agrees(p, dev, frame.hitchAgeMs, now)) { return; }

		// 2. A second socket
	const int c = candidateOf(p, frame.addr);

	if (c != 0 || p.nCand > 1u) {
		p.answerPending = false;
		conclude(p, dev, PairingState::CONFLICT, "two sockets requested");

		return;
	}

		// 3. Window
	if (p.cand[0].rounds == 0u) {
		p.nextMs = now + (uint32_t)kCouplingRounds * kMaxRoundDelayMs + kCouplingBasePeriodMs;
	}

		// 4. Answer
	p.cand[0].rounds |= bit;
	p.answerPending   = true;
	p.answerRound     = frame.round;
	p.answerAtMs      = now + draw(0u, kCouplingAnswerJitterMaxMs);

		// Check for the last round: conclude soon after its answer
	if (frame.round == kCouplingRounds) { p.nextMs = p.answerAtMs + kCouplingBasePeriodMs; }
}

} // namespace



// =============================================================================
// 3. STEP
// =============================================================================

/**
 * @brief Pairing step. The trigger first, then the step of the role.
 *
 * @details In order:
 *   1. Trigger: IDLE or HELD, nothing changes, so a pairing under way pauses.
 *      RELEASED: DETACHED, the identity and the peer are forgotten.  ENGAGED: a new
 *      pairing, with every retry.
 *   2. The step of the role of this end.
 */

PairingState combus_pairing_update( ComLinkPairing&    p,
                                    const CouplingDev& dev,
                                    ComLinkRole        role,
                                    bool               trigger,
                                    NodeCom*           com,
                                    uint32_t           now )
{
		// 1. Trigger
	switch (stepTrigger(p, dev, trigger, now)) {
		case TriggerStep::IDLE:
		case TriggerStep::HELD:     return p.state;
		case TriggerStep::RELEASED:
			p.state = PairingState::DETACHED;
			p.id    = {};
			p.peer  = {};
			return p.state;
		case TriggerStep::ENGAGED:
			p.retriesLeft = kCouplingRetries;
			startPairing(p, now);
			break;
		case TriggerStep::ACTIVE:   break;
	}

		// 2. Step of the role
	if (role == ComLinkRole::SOCKET) { stepSocket(p, dev, com, now); }
	else                             { stepPlug(p, dev, com, now); }

	return p.state;
}


	/// The same debounce as a coupled link: COUPLED once engaged, DETACHED once released.
PairingState combus_pairing_update_passive( ComLinkPairing&    p,
                                            const CouplingDev& dev,
                                            bool               trigger,
                                            uint32_t           now )
{
	switch (stepTrigger(p, dev, trigger, now)) {
		case TriggerStep::ENGAGED:
			p.state = PairingState::COUPLED;
			identify(p, dev);
			sys_log_info("[COMBUS_PAIRING] %s: attached (passive)\n", dev.name);
			break;
		case TriggerStep::RELEASED:
			p.state = PairingState::DETACHED;
			p.id    = {};
			sys_log_info("[COMBUS_PAIRING] %s: detached (passive)\n", dev.name);
			break;
		default:
			break;
	}

	return p.state;
}


	/// Settles on after engageMs of true, off after releaseMs of false.
bool combus_pairing_detector( ComLinkPairing& p, const CouplingDev& dev, bool raw, uint32_t now )
{
		// Check for a raw change
	if (raw != p.detRaw) {
		p.detRaw      = raw;
		p.detChangeMs = now;
	}

	const uint32_t stable = now - p.detChangeMs;

	if (raw && !p.detSettled && stable >= dev.engageMs)  { p.detSettled = true; }
	if (!raw && p.detSettled && stable >= dev.releaseMs) { p.detSettled = false; }

	return p.detSettled;
}



// =============================================================================
// 4. RECEIVED FRAME
// =============================================================================

/**
 * @brief Coupling frame reception. Hands a frame of the pairing to the reading of the role.
 *
 * @details In order:
 *   1. COUPLED: the coupling frames have nothing more to say.  One from another identity
 *      means another node on the medium: the first one is logged, all are counted.
 *   2. Outside PAIRING, or a round out of range: ignored.
 *   3. The reading of the role of this end.
 */

void combus_pairing_on_frame( ComLinkPairing&      p,
                              const CouplingDev&   dev,
                              ComLinkRole          role,
                              const CouplingFrame& frame,
                              uint32_t             now )
{
		// 1. Another identity while COUPLED
	if (p.state == PairingState::COUPLED && !sameAddr(frame.addr, p.peer)) {
		if (p.foreignFrames == 0u) {
			sys_log_warn("[COMBUS_PAIRING] %s: coupling frame from another identity while coupled\n", dev.name);
		}

		if (p.foreignFrames < 0xFFFFu) { p.foreignFrames++; }
	}

		// 2. Frames of a pairing only
	if (p.state != PairingState::PAIRING) { return; }
	if (frame.round == 0u || frame.round > kCouplingRounds) { return; }

	const uint8_t bit = (uint8_t)(1u << (frame.round - 1u));

		// 3. Reading of the role
	if (role == ComLinkRole::SOCKET) { onSocketFrame(p, dev, frame, bit, now); }
	else                             { onPlugFrame(p, dev, frame, bit, now); }
}


	/// "?" for a value out of the enum.
const char* combus_pairing_state_name( PairingState state )
{
	switch (state) {
		case PairingState::DETACHED: return "DETACHED";
		case PairingState::PAIRING:  return "PAIRING";
		case PairingState::COUPLED:  return "COUPLED";
		case PairingState::CONFLICT: return "CONFLICT";
		case PairingState::FAILED:   return "FAILED";
	}

	return "?";
}

// EOF combus_pairing.cpp
