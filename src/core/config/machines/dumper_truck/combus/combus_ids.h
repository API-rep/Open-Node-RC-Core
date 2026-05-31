/*!****************************************************************************
 * @file  combus_ids.h
 * @brief Dumper truck com-bus channel ID enumerations
 *
 * Contains ONLY the AnalogComBusID / DigitalComBusID enums inside
 * namespace DumperTruck. Zero project dependencies — safe to include
 * from any header without risk of include cycles.
 *
 * Consumers:
 *   - combus.h   — include these IDs as part of the full layout
 *   - combus_ids.h     — umbrella dispatcher for struct headers (src/core/config/machines/combus_ids.h)
 *
 * On machine builds, combus_ids.h and combus_types.h adds using namespace DumperTruck;
 * So all existing code uses the names unqualified.
 * Remote builds will keep the explicit prefix (DumperTruck::AnalogComBusID::...) to
 * avoid collisions with other machine configs.
 *******************************************************************************///
#pragma once

#include <cstdint>


namespace DumperTruck {

/// @brief Com-bus analog channel identifiers
enum class AnalogComBusID : uint8_t {
  STEERING_BUS = 0,
  ESC_RPM_BUS,             ///< Engine RPM magnitude [0..maxRpm], pre-inertia, post-ratio — written by SIM_THROTTLE. Transmitted to sound node.
  DUMP_BUS,
  ESC_SPEED_BUS,       ///< Inertia-smoothed speed — drives ESC output + currentSpeed (owner: SYSTEM_EXT)
  GEAR,              ///< Active virtual gear (1–3) — derived from ESC_SPEED_BUS deviation by motion pipeline. Never 0; direction encoded separately in ESC_REVERSE.
  DRIVE_STATE_BUS,   ///< DriveState 3-bit bitmask — see DriveStateBus::encode/decode in simulation_struct.h.
                     ///<   bit0 ACTIVE | bit1 FWD | bit2 BRAKE  (0 = standing)

  /// Wire frontier — channels below this index are transmitted over UART.
  /// Channels at or above this index are node-local (never on the wire).
  /// Use WIRE_END (not CH_COUNT) for ComBus frame n_analog on both TX and RX sides.
  WIRE_END,

  // ---- Machine node local (never transmitted on wire) -----
  BRAKE_BUS = WIRE_END,         ///< Brake pedal input (L2, ComBus domain) — written by input_manager, read by sim_brake.
  SUBGEAR_BUS,                  ///< Active sub-gear index (1..N, 0 = inactive) — written by sim_gear.
  DUMP_RAMPED_BUS,              ///< Inertia-smoothed dump position — written by sim_ramp, read by DUMP_ACTUATOR.
  STEERING_RAMPED_BUS,          ///< Inertia-smoothed steering position — written by sim_ramp, read by STEERING.
  THROTTLE_BUS,                 ///< Throttle stick position — written by input_manager, read by SIM_THROTTLE pipeline.
  CH_COUNT                      ///< Total channel count (wire + local) — drives AnalogComBusArray size.
};

/// @brief Com-bus digital channel identifiers
enum class DigitalComBusID : uint8_t {

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

  /// Wire frontier — channels below this index are transmitted over UART.
  /// Channels at or above this index are sound-node-local (never on the wire).
  /// Use WIRE_END (not CH_COUNT) for ComBus frame n_digital on both TX and RX sides.
  WIRE_END = MOTION_END,

  // ---- Machine node local (never transmitted on wire) ---
  DIRECT_DRIVE = WIRE_END,  ///< Direct-drive toggle state (inertia bypass) — written by ctrl chain, read by sim_bypass_fn.
  DIRECT_DRIVE_BTN,         ///< Raw OPTIONS button — written by input_update every cycle; read by ctrl chain.
  MANUAL_GEAR_SET,          ///< Manual gear mode active — written by INPUT chain, read by cb_bypass_fn (claim guard).
  SUBGEAR_SET_BTN,          ///< Raw sub-gear toggle button — written by input_update; processed by ctrl chain.
  GEAR_UP_BTN,              ///< Gear up button — written by input_update; read by INPUT chain (manual gear + subgear inc).
  GEAR_DOWN_BTN,            ///< Gear down button — written by input_update; read by INPUT chain (manual gear + subgear dec).
  MACHINE_END,

  // ---- Sound node local (never transmitted on wire) ---
  ALWAYS_ON = MACHINE_END,  ///< Always true — continuous sources driven purely by volMod (value=true at init).
  SIREN,                 ///< Siren / cannon mode active — toggled by FSM.
  INDICATOR_TICK,        ///< Indicator tick gate — combined INDICATOR_LEFT|RIGHT|HAZARDS — written by FSM.
  ESC_REVERSE,           ///< ESC is currently driving in reverse — written by FSM.
  SOUND1,                ///< One-shot sound 1 trigger — written by FSM, cleared by EffectsGen.
  WASTEGATE,             ///< Wastegate blowoff trigger — written by FSM, cleared by EffectsGen.
  BRAKE,                 ///< Air brake trigger (EngineGen.airBrake || simState) — resolved by EffectsGen.
  PARKING_BRAKE,         ///< Parking brake level flag (EngineGen) — resolved by EffectsGen.
  SHIFTING,              ///< Gear shift trigger — written by FSM, cleared by EffectsGen.
  COUPLING,              ///< Trailer coupling trigger — written by FSM, cleared by EffectsGen.
  UNCOUPLING,            ///< Trailer uncoupling trigger — written by FSM, cleared by EffectsGen.
  BUCKET_RATTLE,         ///< Bucket rattle trigger — written by FSM, cleared by EffectsGen.
  OUT_OF_FUEL,           ///< Out-of-fuel message trigger — written by FSM, cleared by EffectsGen.

  CH_COUNT ///< Total channel count (wire + local) — drives DigitalComBusArray size.
};

}  // namespace DumperTruck

// EOF combus_ids.h
