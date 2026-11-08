/******************************************************************************
 * @file  cb_runlevel.cpp
 * @brief CbProc: RunLevel setters (continuous and edge-triggered) and hold.
 *
 * @details The two setters share applyLever(); they differ only in when they call
 *   it.  The behaviour of each is in cb_runlevel.h.
 ******************************************************************************/

#include "cb_runlevel.h"



// =============================================================================
// 1. SHARED LEVER LOGIC
// =============================================================================

/**
 * @brief Lever. Writes the run level of the branch the lever selects.
 *
 * @details In order:
 *   1. The branch: `high` when the lever is on, `low` when it is off.
 *   2. A branch without a value: no write, no claim.
 *   3. The write, then the claim when the configuration asks for it.
 *
 * @param cfg      Configuration of the setter.
 * @param lever    State of the lever.
 * @param value    Pipeline value, written when the branch has a value.
 * @param claimed  Set to true after a write, when cfg->claim.
 */

static void applyLever(const CbRunlevelCfg* cfg, bool lever, uint16_t& value, bool& claimed) {
		// 1. Branch
	const std::optional<RunLevel>& branch = lever ? cfg->high : cfg->low;

		// 2. No value
	if (!branch.has_value()) { return; }

		// 3. Write, then claim
	value = static_cast<uint16_t>(branch.value());

	if (cfg->claim) { claimed = true; }
}



// =============================================================================
// 2. CONTINUOUS (STATELESS) VARIANT
// =============================================================================

	/// Every cycle: the lever pins the run level as long as it holds.
void cb_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed) {
	const auto* cfg = static_cast<const CbRunlevelCfg*>(proc->cfg);

	applyLever(cfg, proc->inValue != 0u, value, claimed);
}



// =============================================================================
// 3. EDGE-TRIGGERED (STATEFUL) VARIANT
// =============================================================================

/**
 * @brief Edge-triggered setter. Acts once, on a change of the lever.
 *
 * @details In order:
 *   1. A change of the lever since the last cycle: the same lever logic.
 *   2. The lever is kept, every cycle, for the next comparison.
 */

void cb_runlevel_once_fn(CbProc* proc, uint16_t& value, bool& claimed) {
	const auto* cfg   = static_cast<const CbRunlevelCfg*>(proc->cfg);
	auto*       state = static_cast<CbRunlevelOnceState*>(proc->state);

	const uint16_t cur = proc->inValue;

		// 1. Change of the lever
	if (cur != state->prevValue) { applyLever(cfg, cur != 0u, value, claimed); }

		// 2. Lever kept
	state->prevValue = cur;
}



// =============================================================================
// 4. HOLD VARIANT
// =============================================================================

	/// Claims while the run level is the held one: no later processor can leave it.
void cb_runlevel_hold_fn(CbProc* proc, uint16_t& value, bool& claimed) {
	const auto* cfg = static_cast<const CbRunlevelHoldCfg*>(proc->cfg);

		// Check for the held run level
	if (value == static_cast<uint16_t>(cfg->hold)) {
		claimed = true;
	}
}

// EOF cb_runlevel.cpp
