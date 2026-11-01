/*!****************************************************************************
 * @file  cb_guard.h
 * @brief CbProc guard — blocks the rest of the chain until its input has settled.
 *
 * @details The guard reads its input (`inCh`, true = open) and claims the
 *   chain (the following processors are skipped, the pipeline value is
 *   committed unchanged) until the input has stayed true for `settleMs`.  A
 *   contact that bounces, or opens for a moment, never passes.
 *
 *   With `whileZeroOnly`, the guard only acts while the pipeline value is 0:
 *   it guards an engagement (0 to non-zero), never a release (link rework
 *   3.4e: coupling needs the engaged switch, uncoupling never does).
 *
 *   Typical declaration (in the chain of a coupler):
 *   @code
 *     { .name  = "engaged_guard",
 *       .inCh  = DigitalComBusID::FRONT_COUPLER_ENGAGED_SW,
 *       .fn    = cb_guard_fn,
 *       .cfg   = &kEngagedGuardCfg,
 *       .state = &gEngagedGuardState },
 *   @endcode
 *******************************************************************************
 */
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc


/**
 * @brief Configuration of a guard.
 */
struct CbGuardCfg {
    uint16_t settleMs;       ///< the input stays true this long before the guard opens (0 = at once)
    bool     whileZeroOnly;  ///< guard only while the pipeline value is 0 (an engagement, not a release)
};

/**
 * @brief Runtime state of a guard — zero-initialise (`{}`).
 */
struct CbGuardState {
    bool     wasOn;        ///< input at the previous call
    uint32_t onSinceMs;    ///< millis() when the input last rose
};

/**
 * @brief Guard processor: claims the chain until the input has been true for settleMs.
 */
void cb_guard_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF cb_guard.h
