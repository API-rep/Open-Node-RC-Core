// ---- Core ------------------------------------------------------
HORN_BTN = 0,     ///< Horn button — raw input from operator.
LIGHTS,           ///< Light master switch — set by machine from operator input.
KEY_BTN,          ///< Ignition key — raw button input from operator.
BATTERY_LOW,      ///< Battery low flag — written by vbat module, read by all receivers.
CORE_END,         ///< Range marker — first index after core group

// ---- Light -----------------------------------------------------
INDICATOR_LEFT  = CORE_END, ///< Left turn indicator — set by machine from operator input.
INDICATOR_RIGHT,            ///< Right turn indicator — set by machine from operator input.
HAZARDS,                    ///< Hazard lights (both indicators) — set by machine from operator input.
HIGH_BEAM,                  ///< High-beam / flasher toggle — set by machine from operator input.
ROOF_LIGHT,                 ///< Roof light (gyrophare / blue light) — set by machine from operator input.
LOW_BEAM,                   ///< Low-beam headlights — set by machine from operator input.
LIGHT_END,                  ///< Range marker — first index after light group

// ---- Motion ---------------------------------------------------
BRAKING = LIGHT_END,        ///< ESC / inertia pipeline is in a braking state — written by motion (MotionOutput::isBraking).
KEY_ACTIVE,                 ///< Processed ignition key state — 1=engine active, 0=inactive. Written by key_runlevel chain. Transmitted to sound node.
MOTION_END,                 ///< Range marker — first index after motion group

// ---- Common umbrella ------------------------------------------
// Cross-machine-type Failsafe Digital REMOTE channel IDs (e.g.
// FAILSAFE_INPUT_LINK). Pulled in only when HAS_FAILSAFE is defined.
#include <core/config/machines/common/combus/combus_ids_remote_digital_common.inc>
