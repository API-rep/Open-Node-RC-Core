// input_ids_digital.inc — Raw input acquisition DIGITAL channels (LOCAL, direction=none)
// =============================================================================
// LEGACY HAND-WRITTEN MIRROR of input.cb (raw digital channels).
//
// This file is consumed by the build via direct #include in combus_ids.h.
// It MUST stay in lockstep with src/core/config/machines/dumper_truck/inputs/input.cb
// until the combus_builder is wired into platformio.ini (see scripts/combus_builder/).
//
// ID source: DigitalComBusID tail (after MACHINE_END + SYSTEM tail).
//
// Scope: LOCAL (intra-firmware / acquisition layer).
// Direction: none → not on the wire (NOT uplink, NOT downlink).
// =============================================================================

CRUISE_TOGGLE_BTN,    ///< Raw cruise toggle button (SQUARE) — written by input_update, read by cruise processor.
CRUISE_UPDATE_BTN,    ///< Raw cruise update button (L3) — written by input_update, read by cruise processor.
DIRECT_DRIVE_BTN,     ///< Raw direct-drive toggle button (OPTIONS) — written by input_update, read by sim_dd.
GEAR_DOWN_BTN,        ///< Raw gear down button (DOWN_ARROW) — written by input_update, read by sim_gear.
GEAR_UP_BTN,          ///< Raw gear up button (UP_ARROW) — written by input_update, read by sim_gear.
SUBGEAR_SET_BTN,      ///< Raw sub-gear toggle button (SHARE) — written by input_update, read by sim_subgear.