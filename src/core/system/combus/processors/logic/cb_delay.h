/*!****************************************************************************
 * @file  cb_delay.h
 * @brief CbProc delay: holds the rest of the chain until its input has been true for a delay.
 *
 * @details The delay reads its input (`inCh`) and claims the chain until the input has
 *   stayed true for `delayMs`, counted from its last rising edge: the following
 *   processors are skipped, and the pipeline value is committed unchanged.  A contact
 *   that bounces, or opens for a moment, starts the delay again: it never passes before
 *   having been stable.
 *
 *   With `whileZeroOnly`, the delay only acts while the pipeline value is 0: it holds
 *   an engagement (0 to non-zero), never a release.  A coupling waits for the engaged
 *   switch, an uncoupling never does.
 *
 *   Typical declaration, in the chain of a coupler:
 *   @code
 *     { .name  = "engaged_delay",
 *       .inCh  = DigitalComBusID::TEST_COUPLER_ENGAGED_SW,
 *       .fn    = cb_delay_fn,
 *       .cfg   = &kEngagedDelayCfg,
 *       .state = &s_engagedDelay },
 *   @endcode
 *******************************************************************************///
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc



/**
 * @brief Configuration of a delay.
 */

struct CbDelayCfg {
	uint16_t delayMs;        ///< the input stays true this long before the chain goes on (0 = at once)
	bool     whileZeroOnly;  ///< act only while the pipeline value is 0 (an engagement, not a release)
};


/**
 * @brief Runtime state of a delay; zero-initialise it (`{}`).
 */

struct CbDelayState {
	bool     wasOn;        ///< input at the previous call
	uint32_t onSinceMs;    ///< millis() when the input last rose
};


/**
 * @brief Delay processor: claims the chain until the input has been true for delayMs.
 *
 * @param proc     The processor: `cfg` is a CbDelayCfg, `state` a CbDelayState.
 * @param value    Pipeline value, read only.
 * @param claimed  Set to true to hold the rest of the chain.
 */

void cb_delay_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF cb_delay.h
