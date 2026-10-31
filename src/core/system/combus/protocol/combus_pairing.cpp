/******************************************************************************
 * @file  combus_pairing.cpp
 * @brief Pairing of a coupled link (link rework 3.4c).
 *
 * @details See combus_pairing.h for the rules (P2 to P9 of the WIP).
 *****************************************************************************/

#include "combus_pairing.h"

#include <string.h>
#include <esp_random.h>   // esp_random()

#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. HELPERS
// =============================================================================

namespace {

    // This node's identity, read once.
const NodeAddr& localAddr()
{
    static const NodeAddr s_addr = combus_coupling_local_addr();
    return s_addr;
}

    // A value drawn in [lo, hi].
uint32_t draw( uint32_t lo, uint32_t hi )
{
    return lo + (esp_random() % (hi - lo + 1u));
}

    // Delay between two rounds: 0.75 to 1.25 × the base period.
uint32_t roundDelay()
{
    return draw((kCouplingBasePeriodMs * 3u) / 4u, (kCouplingBasePeriodMs * 5u) / 4u);
}

bool sameAddr( const NodeAddr& a, const NodeAddr& b )
{
    return a.type == b.type && memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

    // The two ages of the hitch agree within the tolerance of the CouplingDev.
bool agrees( const ComLinkPairing& p, const CouplingDev& dev, uint16_t wireAge, uint32_t now )
{
    const uint32_t own  = combus_coupling_hitch_age(now - p.t0Ms);
    const uint32_t diff = (own > wireAge) ? (own - wireAge) : (wireAge - own);
    return diff <= dev.ageToleranceMs;
}

uint8_t popcount8( uint8_t v )
{
    uint8_t n = 0u;
    for (; v; v &= (uint8_t)(v - 1u)) { ++n; }
    return n;
}

    // Index of an identity among the candidates, added if new (kCouplingMaxCandidates at most); -1 if full.
int candidateOf( ComLinkPairing& p, const NodeAddr& addr )
{
    for (uint8_t i = 0u; i < p.nCand; ++i) {
        if (sameAddr(p.cand[i].addr, addr)) { return i; }
    }
    if (p.nCand >= kCouplingMaxCandidates) { return -1; }
    p.cand[p.nCand] = PairingCandidate{ addr, 0u };
    return p.nCand++;
}

void send( NodeCom* com, CouplingMsg msg, uint8_t round, uint16_t age )
{
    if (!com || !com->write) { return; }
    CouplingFrame f;
    f.msg        = msg;
    f.round      = round;
    f.addr       = localAddr();
    f.hitchAgeMs = age;
    f.features   = 0u;                     // reserved (B5)
    uint8_t buf[kCombusCouplingFrameLen];
    const uint8_t len = combus_coupling_encode(buf, sizeof(buf), f);
    if (len) { com->write(com->ctx, buf, len); }
}

void startPairing( ComLinkPairing& p, uint32_t now )
{
    p.state         = PairingState::PAIRING;
    p.round         = 0u;
    p.nextMs        = now;                 // vehicle: first request at once; attachment: no window yet
    p.nCand         = 0u;
    p.answerPending = false;
}

void conclude( ComLinkPairing& p, const CouplingDev& dev, PairingState state, const char* why )
{
    p.state = state;
    if (state == PairingState::COUPLED) {
        sys_log_info("[COMBUS_PAIRING] %s: paired\n", dev.name);
    } else {
        sys_log_warn("[COMBUS_PAIRING] %s: %s (%s)\n", dev.name, combus_pairing_state_name(state), why);
    }
}

} // namespace


// =============================================================================
// 2. STEP
// =============================================================================

PairingState combus_pairing_update( ComLinkPairing&    p,
                                    const CouplingDev& dev,
                                    ComLinkRole        role,
                                    bool               trigger,
                                    NodeCom*           com,
                                    uint32_t           now )
{
        // --- 1. Trigger and its debounce (P9): t0 is the raw edge, not the debounced one ---
    if (trigger != p.rawTrigger) {
        p.rawTrigger  = trigger;
        p.rawChangeMs = now;
        if (trigger && !p.engaged) { p.t0Ms = now; }
        if (trigger && p.glitch) {
            p.glitch = false;
            sys_log_info("[COMBUS_PAIRING] %s: hitch glitch (trigger back within %u ms)\n",
                         dev.name, (unsigned)dev.releaseMs);
        }
        if (!trigger && p.engaged) { p.glitch = true; }
    }

    if (!p.engaged) {
        if (!trigger || (now - p.rawChangeMs) < dev.engageMs) { return p.state; }
        p.engaged     = true;
        p.retriesLeft = kCouplingRetries;
        startPairing(p, now);
    } else if (!trigger) {
        if ((now - p.rawChangeMs) < dev.releaseMs) { return p.state; }   // the link goes on meanwhile
        p.engaged = false;
        p.glitch  = false;
        p.state   = PairingState::DETACHED;
        return p.state;
    }

        // --- 2. Vehicle: requests, listening, conclusion (P2 to P4), retries (P9) ---
    if (role == ComLinkRole::VEHICLE) {
        if (p.state == PairingState::FAILED && p.retriesLeft > 0u && (int32_t)(now - p.nextMs) >= 0) {
            p.retriesLeft--;
            startPairing(p, now);
        }
        if (p.state != PairingState::PAIRING) { return p.state; }

        if (p.round < kCouplingRounds) {
            if ((int32_t)(now - p.nextMs) >= 0) {
                p.round++;
                send(com, CouplingMsg::REQUEST, p.round, combus_coupling_hitch_age(now - p.t0Ms));
                p.nextMs = now + ((p.round < kCouplingRounds) ? roundDelay() : kCouplingBasePeriodMs);
            }
            return p.state;
        }
        if ((int32_t)(now - p.nextMs) < 0) { return p.state; }           // final listening

        if (p.nCand == 0u) {
            conclude(p, dev, PairingState::FAILED, "no answer");
        } else if (p.nCand > 1u) {
            conclude(p, dev, PairingState::CONFLICT, "two attachments answered");
        } else if (popcount8(p.cand[0].rounds) >= kCouplingMinAnswers) {
            conclude(p, dev, PairingState::COUPLED, "");
        } else {
            conclude(p, dev, PairingState::FAILED, "too few answers");
        }
        if (p.state == PairingState::FAILED) { p.nextMs = now + kCouplingRetryMs; }
        return p.state;
    }

        // --- 3. Attachment: answers, window, conclusion (P5, P6) ---
    if (p.state != PairingState::PAIRING) { return p.state; }

    if (p.answerPending && (int32_t)(now - p.answerAtMs) >= 0) {
        p.answerPending = false;
        send(com, CouplingMsg::ANSWER, p.answerRound, combus_coupling_hitch_age(now - p.t0Ms));
    }
    if (p.nCand == 0u || p.answerPending || (int32_t)(now - p.nextMs) < 0) { return p.state; }

    if (popcount8(p.cand[0].rounds) >= kCouplingMinAnswers) {
        conclude(p, dev, PairingState::COUPLED, "");
    } else {
        p.nCand = 0u;                                                     // too few requests: listen again
    }
    return p.state;
}


// =============================================================================
// 3. RECEIVED FRAME
// =============================================================================

void combus_pairing_on_frame( ComLinkPairing&      p,
                              const CouplingDev&   dev,
                              ComLinkRole          role,
                              const CouplingFrame& frame,
                              uint32_t             now )
{
    if (p.state != PairingState::PAIRING) { return; }                     // COUPLED: coupling frames ignored (P8)
    if (frame.round == 0u || frame.round > kCouplingRounds) { return; }
    const uint8_t bit = (uint8_t)(1u << (frame.round - 1u));

        // --- Vehicle: answers only, to a round already requested, that agree (P3) ---
    if (role == ComLinkRole::VEHICLE) {
        if (frame.msg != CouplingMsg::ANSWER || frame.round > p.round) { return; }
        if (!agrees(p, dev, frame.hitchAgeMs, now)) { return; }
        const int c = candidateOf(p, frame.addr);
        if (c < 0) { return; }
        p.cand[c].rounds |= bit;                          // a second identity: CONFLICT at the conclusion
        return;
    }

        // --- Attachment: requests only, that agree (P5); a second vehicle is a conflict (P6) ---
    if (frame.msg != CouplingMsg::REQUEST) { return; }
    if (!agrees(p, dev, frame.hitchAgeMs, now)) { return; }
    const int c = candidateOf(p, frame.addr);
    if (c != 0 || p.nCand > 1u) {
        p.answerPending = false;
        conclude(p, dev, PairingState::CONFLICT, "two vehicles requested");
        return;
    }
    if (p.cand[0].rounds == 0u) {
        p.nextMs = now + (uint32_t)kCouplingRounds * ((kCouplingBasePeriodMs * 5u) / 4u) + kCouplingBasePeriodMs;
    }
    p.cand[0].rounds |= bit;
    p.answerPending   = true;
    p.answerRound     = frame.round;
    p.answerAtMs      = now + draw(0u, kCouplingAnswerJitterMaxMs);
    if (frame.round == kCouplingRounds) { p.nextMs = p.answerAtMs + kCouplingBasePeriodMs; }   // last round: conclude soon after
}


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
