/******************************************************************************
 * @file  cb_dir.h
 * @brief CbProc function — direction detector (bipolaire value → DRIVE_STATE_BUS).
 *
 * @details `cb_dir_fn()` is a pure observer: it reads the post-ramp bipolaire
 *   `value`, determines direction vs. cfg->neutral, and writes the encoded
 *   result as a side effect.  `value` is NOT modified.
 *
 *   Encoding:
 *   - value > cfg->neutral  → DriveState::kDriveFwd
 *   - value < cfg->neutral  → DriveState::kDriveRev
 *   - value == cfg->neutral → DriveState::kStanding
 *   Result written via `DriveStateBus::encode()` to `proc->secOutValue`;
 *   the runner then commits it to `proc->optSecOutCh` (e.g. DRIVE_STATE_BUS).
 *
 *   Does NOT set `claimed`.  cfg = &CbDirCfg, state = nullptr.
 *
 *   Typical placement: after cb_ramp_fn, before cb_gear_fn:
 *   @code
 *     { .fn = cb_bypass_fn, ... }
 *     { .fn = cb_ramp_fn,   ... }
 *     { .fn = cb_dir_fn,    .optSecOutCh = AnalogComBusID::DRIVE_STATE_BUS, ... }
 *     { .fn = cb_gear_fn,   ... }
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                            // CbProc, CbProcFn, ChanOwner
#include <struct/combus/processors/motion/cb_dir_struct.h>       // CbDirCfg


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Direction detector — assigned to `CbProc::fn`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `const CbDirCfg*`.
 *                `optSecOutCh` = target analog channel (e.g. DRIVE_STATE_BUS).
 * @param value   Read-only — post-ramp bipolaire position; NOT modified.
 * @param claimed Not modified.
 * @param chainOwner  Not used.
 */
void cb_dir_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_dir.h
