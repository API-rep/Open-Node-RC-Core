DIRECT_DRIVE,             ///< Direct-drive toggle state (inertia bypass) — written by ctrl chain, read by sim_bypass_fn.
DIRECT_DRIVE_BTN,         ///< Raw OPTIONS button — written by input_update every cycle; read by ctrl chain.
MANUAL_GEAR_SET,          ///< Manual gear mode active — written by INPUT chain, read by cb_bypass_fn (claim guard).
SUBGEAR_SET_BTN,          ///< Raw sub-gear toggle button — written by input_update; processed by ctrl chain.
GEAR_UP_BTN,              ///< Gear up button — written by input_update; read by INPUT chain.
GEAR_DOWN_BTN,            ///< Gear down button — written by input_update; read by INPUT chain.
GEAR_SHIFTING,            ///< Upshift accel-damp window active — set/cleared by gear_upshift_damp_fn.
CRUISE_TOGGLE_BTN,        ///< Raw □ button — written by input_manager every cycle; processed by INPUT cruise chain.
CRUISE_UPDATE_BTN,        ///< Raw L3 button — written by input_manager; triggers cruise speed update.
CRUISE_ACTIVE,            ///< Normal cruise active state — maintained by INPUT cruise chain.