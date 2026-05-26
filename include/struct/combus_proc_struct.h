/*!****************************************************************************
 * @file  combus_proc_struct.h
 * @brief ComBus processor pipeline — unified CbProc / CbChain types.
 *
 * @details Defines the single shared proc/channel types used by both the
 *   simulation layer (CbChain) and the ctrl layer (CbChain).
 *
 *   **Key architectural rules:**
 *   - `CbProcFn` has NO `ComBus& bus` parameter.
 *     Bus access is handled exclusively by the runner via `optInCh` /
 *     `outCh` (channel level) and `inCh` / `outCh` (proc level).
 *   - Each proc declares all its bus dependencies in its struct fields.
 *     No bus channel ID may be hidden inside a `cfg` struct.
 *     Exception: `AnalogComBusID` / `DigitalComBusID` in cfg are allowed
 *     only for procs that are explicitly designated as multi-input bus
 *     readers (e.g. `sim_subgear_btn_fn` — inCh covers all 3 inputs).
 *   - `ChanOwner` flows from `CbChain::chainOwner` to the runner — never
 *     from an external parameter on the update function.
 *
 *   **Runner contract (cb_chain_update):**
 *   1. Pre-read  — runner reads `ch.optInCh` → seeds `value`.
 *   2. Proc loop — for each proc (stops on `claimed = true`):
 *        a. Inject secondary inputs: `inValue[i]` ← `bus[inCh[i]]`.
 *        b. Call `proc.fn(&proc, value, claimed, ch.chainOwner)`.
 *        c. Commit proc output: `bus[outCh]` ← `outValue`.
 *   3. Post-write — runner writes `value` → `ch.outCh`.
 *      The post-write happens ALWAYS (even when `claimed = true`).
 *      `claimed` only aborts the remaining proc chain — never the write.
 *
 *   **Adding a new CbProcFn:**
 *   1. Add cfg + state structs in the appropriate layer struct file
 *      (`simulation_struct.h` or a dedicated proc header).
 *   2. Create `<layer>_<name>.h/.cpp` in `src/core/system/simulation/`
 *      (temporary location — future home: `src/core/system/combus/processor/`).
 *   3. Register in the relevant config file (`sim_config.cpp`, `ctrl_config.cpp`).
 *   4. Nothing else changes here.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <optional>
#include <variant>

#include <core/config/machines/combus_ids.h>  // AnalogComBusID, DigitalComBusID
#include <struct/combus_struct.h>              // ChanOwner


// =============================================================================
// 1. FORWARD DECLARATION
// =============================================================================

struct CbProc;  ///< Forward — allows CbProcFn to reference CbProc by pointer.


// =============================================================================
// 2. FUNCTION POINTER TYPE
// =============================================================================

/**
 * @brief ComBus processor function — one per CbProc instance.
 *
 * @details Called once per channel per runner cycle.  The function must NOT
 *   access `ComBus` directly — all bus I/O is injected/committed by the
 *   via `CbProc::inValue[]` and `CbProc::outValue`.
 *
 * @param proc     Processor descriptor — cast `cfg`/`state` to concrete types.
 *                 May also read `proc->inValue[]` and write `proc->outValue`.
 * @param value    Pipeline value (in/out) — seeded by runner from `CbChain::optInCh`;
 *                 committed by runner to `CbChain::outCh` after all procs.
 * @param claimed  Set to `true` to abort the remaining proc chain.
 *                 Does NOT suppress the final channel write.
 * @param owner    Identity forwarded from `CbChain::chainOwner`.
 */
using CbProcFn = void (*)(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner owner);


// =============================================================================
// 3. PROC DESCRIPTOR
// =============================================================================

/**
 * @brief One processing unit within a CbChain pipeline.
 *
 * @details All ComBus dependencies must be declared as `inCh[i]` or
 *   `outCh` — never read directly from a bus parameter.
 *
 *   `dynCfg` is an optional runtime-override for `cfg`: a preceding proc
 *   may write `proc->dynCfg` to point at a RAM-resident (mutable) config
 *   struct to allow per-cycle mutation without touching flash.  The fn
 *   resolves the effective config as:
 *   @code
 *   const MyCfg* eff = proc->dynCfg
 *                          ? static_cast<const MyCfg*>(proc->dynCfg)
 *                          : static_cast<const MyCfg*>(proc->cfg);
 *   @endcode
 *
 * @note `inValue[]` and `outValue` are written/read by the runner —
 *   they are mutable even though the rest of the struct may be `constexpr`.
 *   In practice the proc array is declared `static` (non-const) so the
 *   runner can update these fields each cycle.
 */
struct CbProc {
    const char*  name;   ///< Human-readable stage label (debug / dashboard).

    // --- Secondary bus inputs (injected by runner before fn call) ------------
    /// Up to 3 secondary input channels — nullopt slots are skipped.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> inCh[3] = {};
    uint16_t     inValue[3] = {};  ///< Populated by runner from inCh[i] before fn.

    // --- Proc output (committed by runner after fn call) ----------------------
    /// Optional proc output channel — nullopt = no write.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> outCh = {};
    uint16_t     outValue = 0u;       ///< Written by fn; committed by runner to outCh.

    // --- Behaviour -----------------------------------------------------------
    CbProcFn     fn;               ///< Processing function. nullptr = passthrough.
    const void*  cfg;              ///< Flash — static config struct. Cast inside fn.
    void*        dynCfg = nullptr; ///< Runtime cfg override — nullptr = use cfg.
    void*        state;            ///< RAM — mutable runtime state. Cast inside fn.
};


// =============================================================================
// 4. CHAIN DESCRIPTOR
// =============================================================================

/**
 * @brief One named combus processing chain — ordered CbProc list with I/O declaration.
 *
 * @details The chain declares its primary input and output channels.
 *   The runner pre-reads `optInCh` before the proc chain and post-writes
 *   `outCh` after (regardless of `claimed`).
 *
 *   `chainOwner` is forwarded to every fn call and to all bus writes
 *   (primary + secondary) — no external owner parameter on the runner.
 */
struct CbChain {
    const char*  name;  ///< Human-readable chain name (debug / dashboard).

    // --- Primary I/O (runner-owned — no read/write proc needed) --------------
    /// Primary input channel — nullopt = no pre-read (value starts at 0).
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> optInCh  = {};
    /// Primary output channel — nullopt = no post-write.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> outCh = {};

    // --- Proc chain ----------------------------------------------------------
    CbProc*  procs;      ///< Processor array (nullptr when procCount == 0).
    uint8_t  procCount;  ///< Number of processors in procs[].

    // --- Identity ------------------------------------------------------------
    ChanOwner  chainOwner;  ///< Identity forwarded to every fn call and bus write.
};


// EOF combus_proc_struct.h
