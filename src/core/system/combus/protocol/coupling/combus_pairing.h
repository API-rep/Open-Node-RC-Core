/*!****************************************************************************
 * @file  combus_pairing.h
 * @brief Pairing of a coupled link: who is hitched to whom.
 *
 * @details The state machine of one end of a link ON_COUPLING, stepped by
 *   combus_coupling_follow().  It follows the trigger of the CouplingDev, with its
 *   debounce, sends and answers the coupling frames (combus_coupling.h), and concludes.
 *   The link follows the state: off while DETACHED, coupling frames only while PAIRING,
 *   the whole link once COUPLED.
 *
 *   Socket (the end that carries): at the rising edge of the trigger (t0), it sends
 *   kCouplingRounds requests (round, address, age of the hitch), 75 to 125 ms apart,
 *   then listens for one base period.  An answer counts when its age agrees with the
 *   socket's own within CouplingDev::ageToleranceMs.  No identity heard: FAILED;
 *   several: CONFLICT; one, in at least kCouplingMinAnswers rounds: COUPLED, else
 *   FAILED.  A FAILED pairing is retried (kCouplingRetries, every kCouplingRetryMs)
 *   while the trigger stays true; a CONFLICT never is.
 *
 *   Plug (the end that is carried): at the rising edge, it listens.  A request that
 *   agrees gets an answer (same round, its address, its age) 0 to
 *   kCouplingAnswerJitterMaxMs later.  One socket in at least kCouplingMinAnswers
 *   rounds: COUPLED; a second socket: CONFLICT, and no more answers; fewer rounds: it
 *   listens again.
 *
 *   Both ends leave every state once the trigger stays false for CouplingDev::releaseMs
 *   (DETACHED).  A shorter drop is only logged: a coupled link goes on, a pairing under
 *   way pauses and then resumes.
 *******************************************************************************///
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/protocol/link/combus_link_defs.h>      // CouplingDev
#include <core/system/combus/protocol/coupling/combus_coupling.h>   // CouplingFrame, NodeAddr
#include <core/system/hw/node_com.h>                                // NodeCom



// =============================================================================
// 1. CONSTANTS
// =============================================================================

static constexpr uint8_t  kCouplingRounds            = 5u;      ///< requests of one pairing
static constexpr uint8_t  kCouplingMinAnswers        = 4u;      ///< rounds needed from the single candidate
static constexpr uint32_t kCouplingBasePeriodMs      = 100u;    ///< between rounds (0.75 to 1.25 ×), final listening
static constexpr uint32_t kCouplingAnswerJitterMaxMs = 10u;     ///< answer delay of the plug, drawn 0 to this
static constexpr uint8_t  kCouplingRetries           = 3u;      ///< new pairings after a FAILED one, trigger still true
static constexpr uint32_t kCouplingRetryMs           = 2000u;   ///< delay before a new pairing
static constexpr uint8_t  kCouplingMaxCandidates     = 2u;      ///< a second identity is a conflict

static_assert(kCouplingRounds <= 8u, "the answered rounds are a bitmask of uint8_t");



// =============================================================================
// 2. STATE
// =============================================================================

/**
 * @brief State of the pairing of one end.
 */

enum class PairingState : uint8_t {
	DETACHED,   ///< trigger false: the link is off
	PAIRING,    ///< coupling frames only
	COUPLED,    ///< paired: the whole link
	CONFLICT,   ///< two candidates: no link until the trigger goes off, then on
	FAILED      ///< too few answers: no link, retried while the trigger stays true
};


/**
 * @brief One identity heard during a pairing, and the rounds it took part in.
 */

struct PairingCandidate {
	NodeAddr addr   = {};
	uint8_t  rounds = 0u;   ///< bit r-1 set: round r answered (socket) or requested (plug)
};


/**
 * @brief Pairing state of one end of a coupled link.
 *
 * @details Also used by a passive coupler, which only needs the trigger and the identity.
 */

struct ComLinkPairing {
	PairingState state         = PairingState::DETACHED;

		// Trigger and its debounce
	bool         rawTrigger    = false;   ///< last value read on the bus
	uint32_t     rawChangeMs   = 0u;      ///< millis() at the last change of rawTrigger
	bool         engaged       = false;   ///< debounced trigger
	bool         glitch        = false;   ///< dropped while engaged, not yet for releaseMs
	uint32_t     t0Ms          = 0u;      ///< raw rising edge of the trigger: origin of the age of the hitch

		// Rounds
	uint8_t      round         = 0u;      ///< socket: last request sent
	uint32_t     nextMs        = 0u;      ///< socket: next request or retry; plug: end of the window
	uint8_t      retriesLeft   = 0u;      ///< socket: new pairings left after a FAILED one
	PairingCandidate cand[kCouplingMaxCandidates] = {};
	uint8_t      nCand         = 0u;

		// Plug: the answer to send
	bool         answerPending = false;
	uint8_t      answerRound   = 0u;
	uint32_t     answerAtMs    = 0u;

		// Presence detector of an active coupler (same debounce), and identity
	bool         detRaw        = false;
	uint32_t     detChangeMs   = 0u;
	bool         detSettled    = false;
	CouplingId   id            = {};      ///< read (readId) or default at the coupling, cleared at the release

		// The paired peer, kept while COUPLED and handed to the transport (NodeCom::setPeer)
	NodeAddr     peer          = {};      ///< identity of the paired peer, type NONE otherwise
	uint16_t     foreignFrames = 0u;      ///< coupling frames of another identity received while COUPLED
};



// =============================================================================
// 3. API
// =============================================================================

/**
 * @brief Step the pairing of one end: trigger, debounce, rounds, conclusion.
 *
 * @param p        Pairing state of the link.
 * @param dev      CouplingDev of the link.
 * @param role     Role of this node on the link.
 * @param trigger  Value of the trigger channel on the bus.
 * @param com      Transport of the link: the coupling frames are written to it.
 * @param now      millis().
 *
 * @return The state after the step.
 */

PairingState combus_pairing_update( ComLinkPairing&    p,
                                    const CouplingDev& dev,
                                    ComLinkRole        role,
                                    bool               trigger,
                                    NodeCom*           com,
                                    uint32_t           now );


/**
 * @brief Step a passive coupler: its trigger is its detector.
 *
 * @details COUPLED once the trigger has settled, with no pairing and no frame.
 *
 * @param p        Pairing state of the coupler.
 * @param dev      CouplingDev of the coupler.
 * @param trigger  Value of the trigger channel on the bus.
 * @param now      millis().
 *
 * @return The state after the step.
 */

PairingState combus_pairing_update_passive( ComLinkPairing&    p,
                                            const CouplingDev& dev,
                                            bool               trigger,
                                            uint32_t           now );


/**
 * @brief Debounce the presence detector of an active coupler, with engageMs and releaseMs.
 *
 * @param p    Pairing state of the link.
 * @param dev  CouplingDev of the link.
 * @param raw  Value of the detector channel on the bus.
 * @param now  millis().
 *
 * @return true once the detector has settled on.
 */

bool combus_pairing_detector( ComLinkPairing& p, const CouplingDev& dev, bool raw, uint32_t now );


/**
 * @brief Take a received coupling frame into account: answers on the socket, requests on the plug.
 *
 * @param p      Pairing state of the link.
 * @param dev    CouplingDev of the link.
 * @param role   Role of this node on the link.
 * @param frame  The coupling frame received.
 * @param now    millis().
 */

void combus_pairing_on_frame( ComLinkPairing&      p,
                              const CouplingDev&   dev,
                              ComLinkRole          role,
                              const CouplingFrame& frame,
                              uint32_t             now );


	/// Printable name of a pairing state, for the logs.
const char* combus_pairing_state_name( PairingState state );

// EOF combus_pairing.h
