/*!****************************************************************************
 * @file  combus_pairing.h
 * @brief Pairing of a coupled link: who is hitched to whom (link rework 3.4c).
 *
 * @details The state machine of one end of a link ON_COUPLING, driven by
 *   combus_links: it follows the trigger of the CouplingDev (with its
 *   debounce), sends and answers the coupling frames (combus_coupling.h) and
 *   concludes.  combus_links acts on the state: OFF while DETACHED, coupling
 *   frames only while PAIRING, the link of today once COUPLED.
 *
 *   Vehicle (P2 to P4): at the trigger edge (t0), kCouplingRounds requests
 *   (round, MAC, age of the hitch), 75 to 125 ms apart, then one base period of
 *   listening.  An answer counts when its age agrees with the vehicle's within
 *   CouplingDev::ageToleranceMs.  No identity: FAILED; several: CONFLICT; one
 *   with at least kCouplingMinAnswers rounds: COUPLED, else FAILED.  A FAILED
 *   pairing is retried (kCouplingRetries, every kCouplingRetryMs) while the
 *   trigger stays true; a CONFLICT never.
 *
 *   Attachment (P5, P6): at the trigger edge, it listens; a request that agrees
 *   gets an answer (same round, MAC, age) 0 to kCouplingAnswerJitterMaxMs
 *   later.  One vehicle with at least kCouplingMinAnswers requests: COUPLED;
 *   two: CONFLICT (no more answers); fewer: it listens again.
 *
 *   Both ends leave every state once the trigger stays false for
 *   CouplingDev::releaseMs (DETACHED); a shorter drop is only logged.
 *******************************************************************************///
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/protocol/combus_protocol.h>            // CouplingDev
#include <core/system/combus/protocol/frame/combus_coupling.h>      // CouplingFrame, NodeAddr
#include <core/system/hw/node_com.h>                                // NodeCom



// =============================================================================
// 1. CONSTANTS (P12)
// =============================================================================

static constexpr uint8_t  kCouplingRounds            = 5u;      ///< requests of one pairing
static constexpr uint8_t  kCouplingMinAnswers        = 4u;      ///< rounds needed from the single candidate
static constexpr uint32_t kCouplingBasePeriodMs      = 100u;    ///< base delay between rounds (drawn 0.75 to 1.25 ×), final listening
static constexpr uint32_t kCouplingAnswerJitterMaxMs = 10u;     ///< answer delay of the attachment, drawn 0 to this
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
    COUPLED,    ///< paired: the link of today
    CONFLICT,   ///< two candidates: no link until the trigger goes off then on
    FAILED      ///< too few answers: no link, retried while the trigger stays true
};


/**
 * @brief One identity heard during a pairing, and the rounds it took part in.
 */

struct PairingCandidate {
    NodeAddr addr   = {};
    uint8_t  rounds = 0u;   ///< bit r-1 set: round r answered (vehicle) or requested (attachment)
};


/**
 * @brief Pairing state of one end of a coupled link.
 */

struct ComLinkPairing {
    PairingState state         = PairingState::DETACHED;

        // --- trigger and its debounce (P9) ---
    bool         rawTrigger    = false;   ///< last value read on the bus
    uint32_t     rawChangeMs   = 0u;      ///< millis() at the last change of rawTrigger
    bool         engaged       = false;   ///< debounced trigger
    bool         glitch        = false;   ///< dropped while engaged, not yet for releaseMs
    uint32_t     t0Ms          = 0u;      ///< raw rising edge of the trigger: origin of the age of the hitch

        // --- rounds ---
    uint8_t      round         = 0u;      ///< vehicle: last request sent
    uint32_t     nextMs        = 0u;      ///< vehicle: next request; attachment: end of the window
    uint8_t      retriesLeft   = 0u;      ///< vehicle: new pairings left after a FAILED one
    PairingCandidate cand[kCouplingMaxCandidates] = {};
    uint8_t      nCand         = 0u;

        // --- attachment: the answer to send ---
    bool         answerPending = false;
    uint8_t      answerRound   = 0u;
    uint32_t     answerAtMs    = 0u;
};



// =============================================================================
// 3. API (called by combus_links)
// =============================================================================

/**
 * @brief Step the pairing of one end: trigger, debounce, rounds, conclusion.
 *
 * @param p        pairing state of the link
 * @param dev      CouplingDev of the link
 * @param role     role of this node on the link
 * @param trigger  value of the trigger channel on the bus
 * @param com      transport of the link (coupling frames are written to it)
 * @param now      millis()
 * @return the state after the step
 */

PairingState combus_pairing_update( ComLinkPairing&    p,
                                    const CouplingDev& dev,
                                    ComLinkRole        role,
                                    bool               trigger,
                                    NodeCom*           com,
                                    uint32_t           now );


/**
 * @brief Take a received coupling frame into account (vehicle: answers; attachment: requests).
 */

void combus_pairing_on_frame( ComLinkPairing&      p,
                              const CouplingDev&   dev,
                              ComLinkRole          role,
                              const CouplingFrame& frame,
                              uint32_t             now );


/**
 * @brief Printable name of a pairing state, for the logs.
 */

const char* combus_pairing_state_name( PairingState state );

// EOF combus_pairing.h
