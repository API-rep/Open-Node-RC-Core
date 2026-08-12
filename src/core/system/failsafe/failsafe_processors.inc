// =============================================================================
// failsafe_processors.inc
// Failsafe processors — contributor CbProc / CbChain table fragments.
//
// At step 1, this fragment is intentionally empty: no contributor is
// wired yet. Contributor modules will add their `CbChain` / `CbProc`
// here from step 12.5 onward (input link, VBAT, ComBus link, etc.).
//
// The `failsafe_update()` orchestrator iterates `kFailsafeChain`
// (defined in `failsafe_chain.cpp`); the tables referenced here
// will be registered in the upcoming steps.
// =============================================================================
