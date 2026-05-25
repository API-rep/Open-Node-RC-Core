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
 *     DigitalComBusID inCh (raw button)
 *       → CtrlProc[0..n]  (behaviour functions — toggle, speed gate, …)
 *         → DigitalComBusID outCh (processed state)
 *   @endcode
 *
 *   Pipeline contract:
 *   - `value` carries the channel state (bool).  Procs may read and write it.
 *   - `claimed` can be set true by any proc to signal the channel runner that
 *     the output has been written directly to the bus; the runner then skips
 *     the default `combus_set_digital(outCh, value)` write.
 *   - `cfg`    — `const void*` pointer to a constexpr config in flash.  Never mutated.
 *   - `state`  — `void*` pointer to mutable runtime state in RAM.
 *
 *   **Adding a new CtrlProcFn:**
 *   1. Add `MyCtrlCfg` + `MyCtrlState` in this file.
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
 *
 * @param proc     Proc descriptor — `cfg` and `state` cast by each fn.
 * @param value    Channel bool value (in/out) — proc may modify.
 * @param bus      Full ComBus — proc may read analog or other digital channels.
 * @param claimed  Set true to prevent the runner from writing `outCh`.
 * @param owner    ChanOwner to use when writing to the bus.
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
 * @brief One ctrl processing channel: inCh → procs[0..n] → outCh.
 *
 * @details Mirrors SimChannel.
 */
struct CtrlChannel {
    const char*     name;       ///< Human-readable channel name (debug / dashboard).
    DigitalComBusID inCh;       ///< Source digital channel (raw button).
    DigitalComBusID outCh;      ///< Destination digital channel (processed state).
    CtrlProc*       procs;      ///< Proc array — may be nullptr when procCount == 0.
    uint8_t         procCount;  ///< Number of procs in the array.
};


// =============================================================================
// 2. TOGGLE PROC CONFIG AND STATE
// =============================================================================

/**
 * @brief Config for `ctrl_toggle_fn` — toggles output ON/OFF on each button press.
 *
 * @details The toggle behaviour:
 *   - Rising edge (OFF→ON) of `inCh`: engage if speed guard passes, else ignore.
 *   - Rising edge (ON→ON) of `inCh`: disengage (always immediate).
 *   - `isDrived` guard is enforced by the runner before any proc is called —
 *     procs always receive a valid `value`.
 *
 *   Speed guard:
 *   - `speedCh == nullopt` or `maxEngageSpd == 0`: no guard, engage unconditionally.
 *   - Otherwise: engage only when `analogBus[speedCh].value <= maxEngageSpd`.
 */
struct CtrlToggleCfg {
    std::optional<AnalogComBusID> speedCh;      ///< Speed channel for engage guard. nullopt = no guard.
    uint16_t                      maxEngageSpd; ///< Max speed for engagement (ComBus units). 0 = no guard.
};

/**
 * @brief Mutable runtime state for `ctrl_toggle_fn`.
 *
 * @details Zero-initialised by default construction.
 */
struct CtrlToggleState {
    bool active;   ///< Current output state (ON / OFF).
    bool prevBtn;  ///< Raw button state last cycle — rising-edge detection.
};

// EOF ctrl_struct.h
