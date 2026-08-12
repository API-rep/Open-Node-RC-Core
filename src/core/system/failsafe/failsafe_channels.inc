// =============================================================================
// failsafe_channels.inc
// Failsafe channels — FAILSAFE_X sub-ComBus fragments.
//
// At step 1, this fragment is intentionally empty: no contributor is
// wired yet. Contributor modules will add their `FAILSAFE_X`
// declarations here from step 12.5 onward (input link, VBAT,
// ComBus link, etc.).
//
// This fragment is included by `failsafe_processors.inc` (and
// vice-versa) and remains conditional / unconditional at step 1:
// the fragments are preserved as empty slots for the upcoming steps.
// =============================================================================
