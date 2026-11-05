/*!****************************************************************************
 * @file  cb_delay.cpp
 * @brief CbProc delay: holds the rest of the chain until its input has been true for a delay.
 *******************************************************************************///

#include "cb_delay.h"

#include <Arduino.h>  // millis()



/**
 * @brief Delay step. Lets the chain go on once the input has been stable long enough.
 *
 * @details In order:
 *   1. The rising edge of the input restarts the delay.  It is followed even when the
 *      delay does not act, so that it is right when it does.
 *   2. With whileZeroOnly, a non-zero value is never held: a release always passes.
 *   3. The chain is claimed until the input has been on for delayMs.
 */

void cb_delay_fn(CbProc* proc, uint16_t& value, bool& claimed)
{
	const auto* cfg   = static_cast<const CbDelayCfg*>(proc->cfg);
	auto*       state = static_cast<CbDelayState*>(proc->state);
	const uint32_t now = millis();
	const bool     on  = (proc->inValue != 0u);

		// 1. Rising edge
	if (on && !state->wasOn) { state->onSinceMs = now; }

	state->wasOn = on;

		// 2. A release is never held
	if (cfg->whileZeroOnly && value != 0u) { return; }

		// 3. Claim until open
	const bool open = on && (now - state->onSinceMs) >= (uint32_t)cfg->delayMs;

	if (!open) { claimed = true; }
}

// EOF cb_delay.cpp
