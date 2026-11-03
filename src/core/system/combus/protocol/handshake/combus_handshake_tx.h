/******************************************************************************
 * @file  combus_handshake_tx.h
 * @brief ComBus handshake, TX side: boot burst, periodic requests and ACK.
 *
 * @details Constants and per-link context: combus_handshake.h.
 *****************************************************************************/
#pragma once

#include <stdint.h>

#include <core/system/combus/protocol/handshake/combus_handshake.h> // constants, CombusHandshakeContext
#include <core/system/hw/node_com.h>                  // NodeCom


/**
 * @brief Drive the handshake emission of a link, on every poll of combus_tx_update().
 *
 * @details In this order:
 *     1. a request received by the RX side is answered with an ACK of its kind;
 *     2. while the boot burst is active: one request every
 *        kCombusHandshakeBurstPeriodMs, kCombusHandshakeBurstCount times, stopped
 *        as soon as the contract is STABLE.  The first one leaves on the first
 *        call after startBurst().  Skipped with COMBUS_HANDSHAKE_BURST_DISABLE;
 *     3. after the burst: one request every kCombusHandshakePeriodMs, whatever the
 *        state of the contract.  It establishes the contract, asks for the periodic
 *        ACK, and on a transmit-only end lets a receiver started later establish
 *        within one period.
 *
 *   These frames are extra `write()` calls, apart from the timer of the control
 *   frames.
 *
 * @param[in] ctx      Per-link handshake context, never shared across links.
 * @param[in] nodeCom  Transport to write through.
 *
 * @return Number of bytes written; 0 if nothing left, or if `ctx`, `nodeCom`, its
 *   `write` callback or the MD5 of the link is missing (link not initialised).
 */

uint8_t combus_handshake_tx_update( CombusHandshakeContext* ctx,
                                    NodeCom*                nodeCom );

// EOF combus_handshake_tx.h
