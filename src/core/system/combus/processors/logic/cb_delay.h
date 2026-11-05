/*!****************************************************************************
 * @file  cb_delay.h
 * @brief CbProc delay — holds the rest of the chain until its input has been true for a delay.
 *
 * @details The delay reads its input (`inCh`) and claims the chain (the
 *   following processors are skipped, the pipeline value is committed
 *   unchanged) until the input has stayed true for `delayMs`, counted from
 *   its last rising edge.  A contact that bounces, or opens for a moment,
 *   starts the delay again: it never passes before having been stable.
 *
 *   With `whileZeroOnly`, the delay only acts while the pipeline value is 0:
 *   it holds an engagement (0 to non-zero), never a release (link rework
 *   3.4e: coupling waits for the engaged switch, uncoupling never does).
 *
 *   Typical declaration (in the chain of a coupler):
 *   @code
 *     { .name  = "engaged_delay",
 *       .inCh  = DigitalComBusID::FRONT_COUPLER_ENGAGED_SW,
 *       .fn    = cb_delay_fn,
 *       .cfg   = &kEngagedDelayCfg,
 *       .state = &gEngagedDelayState },
 *   @endcode
 *******************************************************************************
 */
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
 * @brief Runtime state of a delay — zero-initialise (`{}`).
 */
struct CbDelayState {
	bool     wasOn;        ///< input at the previous call
	uint32_t onSinceMs;    ///< millis() when the input last rose
};

/**
 * @brief Delay processor: claims the chain until the input has been true for delayMs.
 */
void cb_delay_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF cb_delay.h
