/*!****************************************************************************
 * @file  ctrl_struct.h
 * @brief Control channel pipeline — structs and behaviour configs.
 *
 * @details The ctrl layer is the digital counterpart of the SimChannel pipeline.
 *   It processes raw digital ComBus channels (written by input_manager from
 *   raw operator buttons) and produces processed digital ComBus channels
 *   consumed by the rest of the pipeline (SimChannel, output drivers, etc.).
 *
 *   Architecture (mirrors SimProc / SimChannel):
 *   @code
 *     ctrl_read_fn (reads inCh from bus)
 *       → CtrlProc[1..n-2]  (behaviour functions — speed gate, toggle, …)
 *         → ctrl_write_fn   (writes outCh to bus)
 *   @endcode
 *
 *   Pipeline contract:
 *   - `value` carries the channel state (bool).  Procs may read and write it.
 *   - `claimed` may be set true by any proc to abort the remaining proc chain.
 *     Useful for the read proc to skip the chain when the input is not driven.
 *   - `cfg`    — `const void*` pointer to a constexpr config in flash.  Never mutated.
 *   - `state`  — `void*` pointer to mutable runtime state in RAM.
 *   - Bus I/O is always done through explicit `ctrl_read_fn` / `ctrl_write_fn` procs.
 *     The runner has no knowledge of which ComBus channels a channel reads or writes.
 *
 *   **Adding a new CtrlProcFn:**
 *   1. Add `MyCtrlCfg` + `MyCtrlState` in this file (section 3+).
 *   2. Create `ctrl_<name>.h/.cpp` in `src/core/system/simulation/`.
 *   3. Register in `ctrl_config.cpp` of the relevant vehicle folder.
 *   4. Nothing else changes here.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <core/config/machines/combus_ids.h>  // DigitalComBusID, AnalogComBusID
#include <struct/combus_struct.h>              // ComBus, ChanOwner


// =============================================================================
// 1. PIPELINE TYPES
// =============================================================================

struct CtrlProc;   ///< Forward declaration for CtrlProcFn.

/**
 * @brief Control processor function pointer.
 *
 * @details Matches the `SimProcFn` pattern for digital channels.
 *   All procs receive `ComBus& bus` — I/O procs (`ctrl_read_fn`, `ctrl_write_fn`)
 *   need it; pure logic procs (toggle, speed gate) may access it as needed.
 *
 * @param proc     Proc descriptor — `cfg` and `state` cast by each fn.
 * @param value    Channel bool value (in/out) — proc may modify.
 * @param bus      Full ComBus — available to all procs; I/O procs use it directly.
 * @param claimed  Set true to abort remaining procs in the chain.
 * @param owner    ChanOwner to use when writing to the bus (I/O procs).
 */
using CtrlProcFn = void (*)(CtrlProc* proc, bool& value, ComBus& bus,
                            bool& claimed, ChanOwner owner);

/**
 * @brief One stage in a CtrlChannel pipeline.
 *
 * @details Mirrors SimProc.
 */
struct CtrlProc {
    const char*  name;       ///< Human-readable stage name (debug / dashboard).
    CtrlProcFn   fn;         ///< Behaviour function — must not be nullptr.
    const void*  cfg;        ///< Constexpr config (flash) — cast by fn. May be nullptr.
    void*        state;      ///< Mutable runtime state (RAM) — cast by fn. May be nullptr.
};

/**
 * @brief One ctrl processing channel — an ordered proc array.
 *
 * @details The channel has no `inCh`/`outCh` metadata: bus I/O is fully delegated
 *   to `ctrl_read_fn` (first proc) and `ctrl_write_fn` (last proc).
 *   The runner only iterates procs; it never touches the bus directly.
 */
struct CtrlChannel {
    const char*  name;       ///< Human-readable channel name (debug / dashboard).
    CtrlProc*    procs;      ///< Proc array — may be nullptr when procCount == 0.
    uint8_t      procCount;  ///< Number of procs in the array.
};


// =============================================================================
// 2. I/O PROC CONFIGS  (ctrl_read_fn / ctrl_write_fn — see ctrl_io.h)
// =============================================================================

/**
 * @brief Config for `ctrl_read_fn` — reads one digital channel from the bus.
 *
 * @details `value` is set to `isDrived && value` of `inCh`.
 *   When `!isDrived`, `value = false` flows through the chain (toggle retains
 *   its current active state; no rising edge fires).
 */
struct CtrlReadCfg {
    DigitalComBusID inCh;  ///< Source digital channel (raw button).
};

/**
 * @brief Config for `ctrl_write_fn` — writes one digital channel to the bus.
 */
struct CtrlWriteCfg {
    DigitalComBusID outCh;  ///< Destination digital channel (processed state).
};


// =============================================================================
// 3. SPEED GATE PROC CONFIG  (ctrl_speed_gate_fn — see ctrl_speed_gate.h)
// =============================================================================

/**
 * @brief Config for `ctrl_speed_gate_fn` — suppresses engagement above a speed threshold.
 *
 * @details Gate logic:
 *   - If `maxEngageSpd == 0`: gate is a no-op (always allow).
 *   - If the output channel (`activeCh`) is already true (active): gate bypasses —
 *     disengagement is always allowed regardless of speed.
 *   - Otherwise: suppresses `value` (sets it false) when
 *     `analogBus[speedCh].value > maxEngageSpd`.
 *
 *   Placement: insert before `ctrl_toggle_fn` in the proc chain so the toggle
 *   never sees a rising edge when speed is too high to engage.
 */
struct CtrlSpeedGateCfg {
    AnalogComBusID  speedCh;       ///< Speed channel to evaluate.
    uint16_t        maxEngageSpd;  ///< Max engage speed (ComBus units). 0 = no guard.
    DigitalComBusID activeCh;      ///< Output channel — gate bypasses when already active.
};


// =============================================================================
// 4. TOGGLE PROC STATE  (ctrl_toggle_fn — see ctrl_toggle.h)
// =============================================================================

/**
 * @brief Mutable runtime state for `ctrl_toggle_fn`.
 *
 * @details `ctrl_toggle_fn` has no config (pass `nullptr` for `cfg`).
 *   Zero-initialised by default construction.
 */
struct CtrlToggleState {
    bool active;   ///< Current output state (ON / OFF).
    bool prevBtn;  ///< Raw button state last cycle — rising-edge detection.
};

// EOF ctrl_struct.h
