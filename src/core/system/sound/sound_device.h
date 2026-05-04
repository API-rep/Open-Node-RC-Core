/*!****************************************************************************
 * @file  sound_device.h
 * @brief Sound device descriptor — static config and runtime state types.
 *
 * @details Defines the two central types of the table-driven sound engine:
 *   `SoundDevice` (Flash, `constexpr`) and `SoundDevState` (RAM, read-write).
 *
 *   A `SoundDevice` is a compile-time constant that fully describes one
 *   logical sound source: its ComBus input, its behaviour mode, and its
 *   optional tuning config.  An array of `SoundDevice` descriptors replaces
 *   the legacy `soundMapper[]` / `SoundChannel` table and the scattered
 *   `#ifdef LOADER_MODE / EXCAVATOR_MODE` control blocks.
 *
 *   The behaviour mode of a device is encoded by the concrete type pointed to
 *   by `SoundDevice::cfg` (see `sound_device_cfg.h`):
 *   - `nullptr`          — passthrough: `behaviorFn` handles everything, or
 *                          a legacy `sound_apply_*()` adapter is used.
 *   - `HydRampCfg*`      — single-channel hydraulic volume ramp.
 *   - `HydPumpCfg*`      — multi-cylinder hydraulic pump load with optional
 *                          engine feedback.
 *   - `TriggerCfg*`      — delta-triggered one-shot event.
 *
 *   `SoundDevState` is a single flat struct that covers all behaviour modes.
 *   Fields unused by a given mode remain zero — the `behaviorFn` knows which
 *   fields apply.  A single struct avoids per-device `void*` casts in the
 *   interpreter loop and keeps RAM usage predictable (~36 bytes × device count).
 *
 *   `SoundDevice::state` points into a statically allocated per-device RAM
 *   block (initialised to zero by `sound_dev_init()`).  Devices that use no
 *   inter-frame state (pure passthrough) set `state = nullptr`.
 *
 *   A `SoundDevice` may optionally gate its output on the computed state of
 *   another device (e.g. a pump gated on engine RPM, a rattle gated on speed).
 *   The gate device index is stored as `cfg->hdr.gateDevID` — the first field
 *   at offset 0 in every cfg struct (see `SoundCfgHdr` in `sound_device_cfg.h`).
 *   The interpreter casts `dev->cfg` to `const SoundCfgHdr*` to read the index,
 *   then passes `devices[gateDevID].state` as `gateState` to `behaviorFn`.
 *   Devices with `cfg == nullptr` (passthrough) never receive a gate state.
 *******************************************************************************
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <optional>
#include <defs/machines_defs.h>               // DevUsage
#include <struct/combus_struct.h>             // ComBus
#include <core/config/machines/combus_ids.h>  // AnalogComBusID, DigitalComBusID
#include "sound_device_cfg.h"                 // HydRampCfg, HydPumpCfg, TriggerCfg


// =============================================================================
// 1. FORWARD DECLARATIONS
// =============================================================================

struct SoundDevice;
struct SoundDevState;


// =============================================================================
// 2. BEHAVIOUR FUNCTION TYPE
// =============================================================================

/**
 * @brief Behaviour callback type. Per-frame logic for one sound device.
 *
 * @details Called by the interpreter once per ComBus update cycle for every
 *   `SoundDevice` that has a non-null `behaviorFn`.  Devices with
 *   `behaviorFn = nullptr` use a legacy `sound_apply_*()` adapter instead and
 *   require no inter-frame state.
 *
 *   The function must:
 *   - Read the relevant channel(s) from @p bus using `dev->chanID` (or the
 *     channel list in `dev->cfg` for multi-channel modes).
 *   - Update @p state with the new computed values.
 *   - Call the appropriate `sound_core_set_*()` entry point to push the result
 *     to the mixer.
 *
 *   @p gateState is non-null only when `dev->cfg != nullptr`,
 *   `cfg->hdr.gateDevID >= 0`, and the gate device has a non-null `state`.
 *   The function may read (but must not write) the gate state.
 *
 * @param bus        Current ComBus snapshot — read-only.
 * @param dev        Owning device descriptor — read-only (Flash).
 * @param state      Mutable runtime state for this device. Never null for
 *                   devices that have a non-null `behaviorFn`.
 * @param gateState  Runtime state of the gate device, or null when
 *                   `cfg == nullptr`, `cfg->hdr.gateDevID == -1`, or the
 *                   gate device has no state.
 */
using SoundBehaviorFn = void (*)(const ComBus*       bus,
                                 const SoundDevice*  dev,
                                 SoundDevState*      state,
                                 const SoundDevState* gateState);


// =============================================================================
// 3. RUNTIME STATE
// =============================================================================

/**
 * @brief Runtime state for one sound device (RAM, read-write, zero-initialised).
 *
 * @details Single flat struct covering all behaviour modes.  The `behaviorFn`
 *   of each device knows which fields are meaningful; unused fields stay zero.
 *
 *   Field groups by behaviour mode:
 *
 *   **All ramp modes** (`HydRampCfg`, `HydPumpCfg`):
 *   - `volume`        — current smoothed output volume (0–100 %).
 *   - `target`        — immediate ramp target before smoothing.
 *   - `lastUpdateMs`  — timestamp of the last ramp step (milliseconds).
 *
 *   **Multi-cylinder mode** (`HydPumpCfg`) additionally:
 *   - `internal[8]`   — raw per-channel contribution before accumulation.
 *   - `loadFeedback`  — engine RPM drop fed back to the engine simulation.
 *   - `knockExtra`    — extra diesel-knock volume fed back to the mixer.
 *
 *   **Trigger mode** (`TriggerCfg`):
 *   - `active`        — true when the trigger fired this frame.
 *   - `lastInput1`    — previous-frame value of the primary channel.
 *   - `lastInput2`    — previous-frame value of the secondary channel
 *                       (`TriggerCfg::chan2ID`, or unused when `kNoChan`).
 *
 *   **Passthrough** (`cfg = nullptr`):
 *   - All fields ignored; `SoundDevice::state` should be null.
 *
 * @param volume        Current smoothed output volume (0–100 %). Written by
 *                      ramp behaviour; read by mixer and child-device gating.
 * @param target        Immediate ramp target from ComBus input before smoothing.
 *                      Ramp advances volume toward target by ±1 step.
 * @param lastUpdateMs  Timestamp of the last ±1 ramp step in milliseconds.
 *                      Gates ramp advancement to rampPeriodMs cadence.
 * @param active        True when the delta-trigger fired on the current frame.
 *                      Consumed and cleared by the interpreter or audio task.
 * @param lastInput1    Previous-frame ComBus value of the primary channel
 *                      (SoundDevice::chanID). Used to compute the inter-frame delta.
 * @param lastInput2    Previous-frame ComBus value of the secondary channel
 *                      (TriggerCfg::chan2ID). Unused when chan2ID is kNoChan.
 * @param internal      Raw per-channel contribution before accumulation
 *                      (0–peakPerChan[i] %). Indexed by HydPumpCfg::chanIDs.
 * @param loadFeedback  Engine RPM drop from current hydraulic load. Written by
 *                      HydPumpCfg behaviour. Zero when loadFeedback cfg is false.
 * @param knockExtra    Extra diesel-knock volume (%) on top of base mapping.
 *                      Written by HydPumpCfg behaviour; read by the audio mixer.
 */
struct SoundDevState {

	// --- Ramp modes: volume tracking ---
	uint16_t  volume;          ///< Current smoothed output volume (0–100 %).
	uint16_t  target;          ///< Ramp target before smoothing.
	uint32_t  lastUpdateMs;    ///< Last ramp-step timestamp (ms).

	// --- Trigger mode: event detection ---
	bool      active;          ///< True when delta-trigger fired this frame.
	uint16_t  lastInput1;      ///< Previous-frame value of the primary channel.
	uint16_t  lastInput2;      ///< Previous-frame value of the secondary channel.

	// --- Multi-cylinder mode: per-channel contributions ---
	uint16_t  internal[8];     ///< Per-channel raw contribution before accumulation.

	// --- Multi-cylinder mode: engine feedback ---
	int16_t   loadFeedback;    ///< Engine RPM drop from hydraulic load.
	uint16_t  knockExtra;      ///< Extra diesel-knock volume % from hydraulic load.
};


// =============================================================================
// 4. SOUND DEVICE DESCRIPTOR
// =============================================================================

/**
 * @brief Static descriptor for one logical sound source (Flash, constexpr).
 *
 * @details A `SoundDevice` binds a ComBus input channel to a sound role and
 *   defines how the interpreter should process it each cycle.  An array of
 *   these descriptors (typically declared `constexpr` in the vehicle profile)
 *   drives the entire sound engine without any per-device `switch` or `#ifdef`.
 *
 *   **Behaviour mode selection** — determined by the concrete type of `cfg`:
 *
 *   | `cfg` value          | Mode                        | `state` required |
 *   |----------------------|-----------------------------|-----------------|
 *   | `nullptr`            | Passthrough — direct `sound_core_set_*()`  | No  (`nullptr`) |
 *   | `HydRampCfg*`        | Single-channel volume ramp  | Yes             |
 *   | `HydPumpCfg*`        | Multi-cylinder pump load    | Yes             |
 *   | `TriggerCfg*`        | Delta-trigger one-shot      | Yes             |
 *
 *   **Gate device** — when `cfg->hdr.gateDevID >= 0`, the interpreter looks up
 *   `devices[gateDevID].state` and passes it to `behaviorFn` as `gateState`.
 *   The gate device must appear earlier in the array (lower index) so its
 *   state is already computed when this device executes.
 *
 *   **Lifecycle** — descriptors live in Flash for the entire program lifetime.
 *   `SoundDevState` blocks are statically allocated (e.g. as a file-scope
 *   array in the profile `.cpp`) and zero-initialised before the first
 *   interpreter call.
 *
 * @param ID          Unique device index in the array (0-based). Must match
 *                    the entry position; used for parent lookups and dashboard.
 * @param infoName    Human-readable device name (Flash string literal, not null).
 *                    Used for diagnostics and dashboard display only.
 * @param usage       Mechanical role (DevUsage). Dashboard hint only; not
 *                    consulted by the interpreter at runtime.
 * @param analogChan  Analog ComBus channel for this device, or nullopt.
 *                    Exactly one of analogChan / digitalChan must be set
 *                    (except passthrough devices where both may be nullopt).
 *                    Overridden by HydPumpCfg::chanIDs for multi-cylinder mode.
 *                    ComBus v2 TODO: replace with typed GlobalSoundBus accessor
 *                    once the bus API is refactored (winter 2026).
 * @param digitalChan Digital ComBus channel for this device, or nullopt.
 *                    ComBus v2 TODO: same as analogChan — see above.
 * @param behaviorFn  Per-frame callback. Null for passthrough (legacy apply fn).
 *                    Receives the ComBus snapshot, this descriptor, and state.
 * @param cfg         Behaviour config in Flash: HydRampCfg*, HydPumpCfg*,
 *                    TriggerCfg*, or nullptr for passthrough. behaviorFn casts
 *                    to the expected type.
 * @param state       Mutable runtime state, zero-initialised by sound_dev_init().
 *                    Null for passthrough devices (cfg = nullptr).
 */
struct SoundDevice {
	const int8_t                      ID;           ///< Array index (0-based); must match position.
	const char*                       infoName;     ///< Human-readable name (Flash literal, not null).
	DevUsage                          usage;        ///< Mechanical role — dashboard hint only.
	std::optional<AnalogComBusID>     analogChan;   ///< Analog channel, or nullopt. TODO ComBus v2: typed GlobalSoundBus accessor.
	std::optional<DigitalComBusID>    digitalChan;  ///< Digital channel, or nullopt. TODO ComBus v2: typed GlobalSoundBus accessor.
	SoundBehaviorFn                   behaviorFn;   ///< Per-frame callback.
	const void*                       cfg;          ///< Behaviour config (HydRampCfg* / HydPumpCfg* / TriggerCfg* / nullptr). Gate: cfg->hdr.gateDevID.
	SoundDevState*                    state;          ///< RW runtime state; null for passthrough devices.
};

// EOF sound_device.h
