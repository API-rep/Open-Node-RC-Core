/*!****************************************************************************
 * @file  sound_device_cfg.h
 * @brief Sound device behaviour configuration descriptors (Flash, constexpr).
 *
 * @details Declares the optional per-device configuration structs that are
 *   pointed to by the `cfg` field of `SoundDevice`.  The presence and concrete
 *   type of `cfg` encodes the behaviour mode of the device.
 *
 *   Three behaviour classes are defined:
 *
 *   `HydRampCfg` handles a single hydraulic channel where a deadband around
 *   neutral is mapped to a smoothly ramped volume output.  Typical use:
 *   loader boom/bucket cylinders, dump body actuators.
 *
 *   `HydPumpCfg` accumulates the contribution of up to eight hydraulic
 *   cylinders into a combined pump-load figure and optionally feeds back
 *   engine RPM drop and diesel-knock extra volume.  Typical use: excavator
 *   multi-cylinder pump load, loader pump load.
 *
 *   `TriggerCfg` fires a one-shot sound event when the absolute delta of
 *   one or two ComBus channels between consecutive frames exceeds a threshold.
 *   The event is gated by a minimum parent engine RPM.  Typical use: bucket /
 *   dipper rattle trigger on stick movement.
 *
 *   All structs are plain aggregates — they carry no methods and contain only
 *   `constexpr`-compatible types, so they can live entirely in Flash when
 *   declared `constexpr` by the profile.
 *
 *   `sound_device_cfg.h` is included by `sound_device.h`; client code needs
 *   only `sound_device.h`.
 *
 *   **Common header rule** — every cfg struct must embed `SoundCfgHdr` as its
 *   very first field (offset 0).  This allows the interpreter to read the gate
 *   device index from any `const void* cfg` pointer via a single
 *   `static_cast<const SoundCfgHdr*>(dev->cfg)->gateDevID` cast, without
 *   knowing the concrete cfg type.  A `static_assert(offsetof(T, hdr) == 0)`
 *   enforces the layout constraint for every struct in this file.
 *******************************************************************************
 */
#pragma once

#include <stdint.h>
#include <stddef.h>   // offsetof


// =============================================================================
// 0. COMMON CONFIG HEADER
// =============================================================================

/**
 * @brief Common header embedded as the first field of every cfg struct.
 *
 * @details Placing `SoundCfgHdr` at offset 0 in every cfg struct lets the
 *   interpreter resolve the gate device index from an opaque `const void* cfg`
 *   pointer without knowing the concrete type:
 *
 *   @code
 *   int8_t gateID = (dev->cfg != nullptr)
 *       ? static_cast<const SoundCfgHdr*>(dev->cfg)->gateDevID
 *       : -1;
 *   @endcode
 *
 *   **Layout constraint:** `hdr` must always be the first non-static member of
 *   any cfg struct.  A `static_assert(offsetof(T, hdr) == 0)` enforces this
 *   for every struct declared in this file.
 *
 * @param gateDevID  Index of the gate device in the `SoundDevice` array, or
 *                   -1 for no gate.  When >= 0, the interpreter passes
 *                   `devices[gateDevID].state` as `gateState` to `behaviorFn`.
 *                   The gate device must have a lower array index (already
 *                   computed this frame).
 */
struct SoundCfgHdr {
	int8_t  gateDevID;   ///< Gate device index in array, or -1 (no gate).
};


// =============================================================================
// 1. VOLUME-RAMP CONFIG — single hydraulic channel
// =============================================================================

/**
 * @brief Behaviour descriptor for a single-channel volume ramp.
 *
 * @details Maps one ComBus analog channel to a smoothly ramped output volume.
 *   A symmetric deadband around the neutral point is discarded; beyond the
 *   deadband the value is linearly mapped to [0, `maxVolume`].  The ramp
 *   advances by ±1 step every `rampPeriodMs` milliseconds, giving a smoothed
 *   response that avoids abrupt volume jumps.
 *
 *   Used with `DevUsage::HYD_LINEAR` (cylinder extensions) or
 *   `DevUsage::HYD_PUMP` (single-axis pump load).
 *
 *   `chanID` in the owning `SoundDevice` identifies the analog channel.
 *
 * @param hdr           Common config header. `hdr.gateDevID` is typically -1
 *                      for standalone cylinder configs (ramp is self-contained).
 *                      Set >= 0 to gate the device on another device's volume.
 * @param deadbandHalf  Half-width of the symmetric deadband in ComBus units
 *                      (0–32767, centred on 32767). Values within
 *                      ±deadbandHalf of neutral produce zero volume.
 * @param peakValue     ComBus value corresponding to full output. Must exceed
 *                      deadbandHalf. Values beyond this are clamped.
 * @param maxVolume     Volume ceiling reached at peakValue and beyond (0–100 %).
 * @param rampPeriodMs  Period between ±1 ramp steps in milliseconds.
 *                      4 ms matches the legacy 250 Hz loop cadence.
 */
struct HydRampCfg {
	SoundCfgHdr  hdr;            ///< Common header — must be first (offset 0). gateDevID = -1 if unneeded.
	uint16_t     deadbandHalf;   ///< Deadband half-width in ComBus units (centred on neutral 32767).
	uint16_t  peakValue;      ///< ComBus value for full output; must exceed deadbandHalf.
	uint8_t   maxVolume;      ///< Volume ceiling at peakValue and beyond (0–100 %).
	uint8_t   rampPeriodMs;   ///< Ramp step period in milliseconds (4 ms = 250 Hz legacy cadence).
};


// =============================================================================
// 2. MULTI-CYLINDER PUMP-LOAD CONFIG
// =============================================================================

/**
 * @brief Behaviour descriptor for a multi-cylinder hydraulic pump load.
 *
 * @details Accumulates the individual contributions of up to eight ComBus
 *   analog channels (one per cylinder / swing axis) into a combined pump-load
 *   figure.  Each channel is mapped through its own deadband to a partial
 *   contribution (0–`peakPerChan[i]` %) and the results are summed and clamped
 *   to 100 %.
 *
 *   When `loadFeedback` is `true` the combined load additionally drives:
 *   - An engine RPM drop proportional to the load, capped at `maxLoadDrop`
 *     (written to `SoundDevState::loadFeedback`).
 *   - An extra diesel-knock volume proportional to the load
 *     (written to `SoundDevState::knockExtra`).
 *
 *   Like `HydRampCfg`, the combined output is ramped (±1 step every
 *   `rampPeriodMs` ms) to avoid abrupt transitions.
 *
 *   Typical use: excavator four-cylinder pump (bucket, dipper, boom, swing).
 *
 *   `chanIDs[0..chanCount-1]` in this struct override the single `chanID` of
 *   the owning `SoundDevice`; the `SoundDevice::chanID` field is unused when
 *   a `HydPumpCfg*` is present.
 *
 * @param hdr           Common config header. `hdr.gateDevID` identifies the
 *                      device whose `SoundDevState` is passed as `gateState`
 *                      to the pump `behaviorFn` (typically the engine device,
 *                      for engine-load feedback gating).
 * @param chanCount     Number of active channels (1–8). Excess entries in
 *                      chanIDs and peakPerChan are ignored.
 * @param chanIDs       ComBus analog channel indices, one per cylinder
 *                      (length: chanCount).
 * @param peakPerChan   Maximum contribution of each channel to the combined
 *                      load (0–100 %). Contributions are summed and clamped.
 * @param deadbandHalf  Symmetric deadband half-width in ComBus units, shared
 *                      across all channels. Tune per-channel via peakPerChan.
 * @param loadFeedback  When true, combined load reduces engine RPM by up to
 *                      maxLoadDrop and boosts diesel-knock volume.
 * @param maxLoadDrop   Maximum engine RPM drop at full hydraulic load
 *                      (0–255 RPM units). Ignored when loadFeedback is false.
 * @param rampPeriodMs  Period between ±1 ramp steps in milliseconds.
 */
struct HydPumpCfg {
	SoundCfgHdr  hdr;            ///< Common header — must be first (offset 0). gateDevID = engine device index.
	uint8_t      chanCount;      ///< Number of active channels (1–8).
	uint8_t      chanIDs[8];     ///< ComBus analog channel indices per cylinder.
	uint8_t      peakPerChan[8]; ///< Max per-channel load contribution (0–100 %).
	uint16_t     deadbandHalf;   ///< Shared deadband half-width in ComBus units.
	bool         loadFeedback;   ///< True: feed load back to engine RPM and knock volume.
	uint8_t      maxLoadDrop;    ///< Max engine RPM drop at full load (0–255 units).
	uint8_t      rampPeriodMs;   ///< Ramp step period in milliseconds.
};


// =============================================================================
// 3. DELTA-TRIGGER CONFIG — event on fast stick movement
// =============================================================================

/**
 * @brief Behaviour descriptor for a delta-trigger one-shot event.
 *
 * @details Fires a one-shot sound trigger (typically a rattle or clank) when
 *   the absolute difference between the current and previous frame value of
 *   one or two ComBus analog channels exceeds `deltaThreshold`.  The trigger
 *   The event is gated: it is ignored unless the gate device's volume
 *   (read from `SoundDevState::volume` of the device at `cfg.hdr.gateDevID`) is
 *   at or above `minGateVolume`.
 *
 *   A second channel (`chan2ID`) is optional: set to `kNoChan` (0xFF) to use
 *   only the primary `SoundDevice::chanID`.  Either channel alone is
 *   sufficient to fire the trigger.
 *
 *   Typical use: excavator bucket / dipper rattle on fast stick input.
 *
 * @param hdr              Common config header. `hdr.gateDevID` identifies the
 *                         gate device (typically the engine) whose volume must
 *                         reach `minGateVolume` for the trigger to fire.
 *                         Set `hdr.gateDevID = -1` and `minGateVolume = 0` to
 *                         disable gating entirely.
 * @param deltaThreshold   Absolute ComBus-unit delta between frames that fires
 *                         the trigger (raw units, 0–65535 full scale).
 * @param chan2ID          Optional second analog channel index (0-based).
 *                         Set to kNoChan (0xFF) to monitor only the primary channel.
 * @param minGateVolume    Minimum `SoundDevState::volume` of the gate device
 *                         required to allow the trigger (0–100 %). 0 disables
 *                         the volume check; `hdr.gateDevID` still applies.
 */
struct TriggerCfg {
	static constexpr uint8_t kNoChan = 0xFFu;   ///< Sentinel: no secondary channel.

	SoundCfgHdr  hdr;              ///< Common header — must be first (offset 0). gateDevID = engine device index.
	uint16_t     deltaThreshold;   ///< Frame-delta threshold that fires the trigger (ComBus units).
	uint8_t      chan2ID;          ///< Secondary channel index, or kNoChan (0xFF) if unused.
	uint8_t      minGateVolume;    ///< Gate device volume floor (0–100 %); 0 = no volume check.
};


// =============================================================================
// 4. THROTTLE RPM CONFIG  (throttle_behaviorFn — RPM domain input)
// =============================================================================

/**
 * @brief Configuration for `throttle_behaviorFn` when `RPM_BUS` carries
 *   engine RPM magnitude [0..maxRpm] (unipolar, 0 = idle).
 *
 * @details The behavior fn converts RPM to ComBus domain [0..CbusMaxVal]
 *   bipolar (CbusNeutral = center) using `DRIVE_STATE_BUS` for direction,
 *   so `gMixerState.throttleVal` stays in the format expected by DiYGuy.
 *
 *   `maxRpm` = top-gear upShift ceiling (e.g. 2100 for kHeavy3Speed).
 *
 *   `hdr.gateDevID = -1` — no gate device.
 */
struct ThrottleRpmCfg {
	SoundCfgHdr hdr;     ///< Must be first (offset 0). Set gateDevID = -1.
	uint16_t    maxRpm;  ///< Full-stick RPM ceiling — top-gear upShift value.
};


// =============================================================================
// LAYOUT CONSTRAINTS
// =============================================================================

// SoundCfgHdr must be the first non-static field (offset 0) in every cfg struct.
// The interpreter casts const void* cfg → const SoundCfgHdr* to read gateDevID.
static_assert(offsetof(HydRampCfg,      hdr) == 0, "SoundCfgHdr hdr must be first in HydRampCfg");
static_assert(offsetof(HydPumpCfg,      hdr) == 0, "SoundCfgHdr hdr must be first in HydPumpCfg");
static_assert(offsetof(TriggerCfg,      hdr) == 0, "SoundCfgHdr hdr must be first in TriggerCfg");
static_assert(offsetof(ThrottleRpmCfg,  hdr) == 0, "SoundCfgHdr hdr must be first in ThrottleRpmCfg");

// EOF sound_device_cfg.h
