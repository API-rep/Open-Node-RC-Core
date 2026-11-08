/******************************************************************************
 * @file  cb_runlevel.cpp
 * @brief CbProc — RunLevel setters (continuous + edge-triggered) and hold — implementation.
 ******************************************************************************/

#include "cb_runlevel.h"


// =============================================================================
// 1. SHARED LEVER LOGIC
// =============================================================================

	// --- Select branch and write value if defined; claim only if a write occurred ---
static void applyLever(const CbRunlevelCfg* cfg, bool lever, uint16_t& value, bool& claimed) {
	const std::optional<RunLevel>& branch = lever ? cfg->high : cfg->low;

	if (!branch.has_value()) { return; }

	value = static_cast<uint16_t>(branch.value());

	if (cfg->claim) { claimed = true; }
}


// =============================================================================
// 2. CONTINUOUS (STATELESS) VARIANT
// =============================================================================

void cb_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed) {
	const auto* cfg = static_cast<const CbRunlevelCfg*>(proc->cfg);

	applyLever(cfg, proc->inValue != 0u, value, claimed);
}


// =============================================================================
// 3. EDGE-TRIGGERED (STATEFUL) VARIANT
// =============================================================================

void cb_runlevel_once_fn(CbProc* proc, uint16_t& value, bool& claimed) {
	const auto* cfg   = static_cast<const CbRunlevelCfg*>(proc->cfg);
	auto*       state = static_cast<CbRunlevelOnceState*>(proc->state);

	const uint16_t cur = proc->inValue;

	// --- Transition only: apply the same logic as cb_runlevel_fn ---
	if (cur != state->prevValue) { applyLever(cfg, cur != 0u, value, claimed); }

	// --- Always update prevValue on exit ---
	state->prevValue = cur;
}


// =============================================================================
// 4. HOLD VARIANT
// =============================================================================

void cb_runlevel_hold_fn(CbProc* proc, uint16_t& value, bool& claimed) {
	const auto* cfg = static_cast<const CbRunlevelHoldCfg*>(proc->cfg);

	if (value == static_cast<uint16_t>(cfg->hold)) {
		claimed = true;
	}
}


// EOF cb_runlevel.cpp
