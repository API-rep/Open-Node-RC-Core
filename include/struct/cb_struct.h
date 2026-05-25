/*!****************************************************************************
 * @file  cb_struct.h
 * @brief ComBus processor pipeline — unified CbProc / CbChannel types.
 *
 * @details Defines the single shared proc/channel types used by both the
 *   simulation layer (SimChannel) and the ctrl layer (CtrlChannel).
 *
 *   **Key architectural rules:**
 *   - `CbProcFn` has NO `ComBus& bus` parameter.
 *     Bus access is handled exclusively by the runner via `optInCh` /
 *     `optOutCh` (channel level) and `secInCh` / `optSecOutCh` (proc level).
 *   - Each proc declares all its bus dependencies in its struct fields.
 *     No bus channel ID may be hidden inside a `cfg` struct.
 *     Exception: `AnalogComBusID` / `DigitalComBusID` in cfg are allowed
 *     only for procs that are explicitly designated as multi-input bus
 *     readers (e.g. `sim_subgear_btn_fn` — secInCh covers all 3 inputs).
 *   - `ChanOwner` flows from `CbChannel::chanOwner` to the runner — never
 *     from an external parameter on the update function.
 *
 *   **Runner contract (cb_channel_update):**
 *   1. Pre-read  — runner reads `ch.optInCh` → seeds `value`.
 *   2. Proc loop — for each proc (stops on `claimed = true`):
 *        a. Inject secondary inputs: `secInValue[i]` ← `bus[secInCh[i]]`.
 *        b. Call `proc.fn(&proc, value, claimed, ch.chanOwner)`.
 *        c. Commit secondary output: `bus[optSecOutCh]` ← `secOutValue`.
 *   3. Post-write — runner writes `value` → `ch.optOutCh`.
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
 *   runner via `CbProc::secInValue[]` and `CbProc::secOutValue`.
 *
 * @param proc     Processor descriptor — cast `cfg`/`state` to concrete types.
 *                 May also read `proc->secInValue[]` and write `proc->secOutValue`.
 * @param value    Pipeline value (in/out) — seeded by runner from `CbChannel::optInCh`;
 *                 committed by runner to `CbChannel::optOutCh` after all procs.
 * @param claimed  Set to `true` to abort the remaining proc chain.
 *                 Does NOT suppress the final channel write.
 * @param owner    Identity forwarded from `CbChannel::chanOwner`.
 */
using CbProcFn = void (*)(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner owner);


// =============================================================================
// 3. PROC DESCRIPTOR
// =============================================================================

/**
 * @brief One processing unit within a CbChannel pipeline.
 *
 * @details All ComBus dependencies must be declared as `secInCh[i]` or
 *   `optSecOutCh` — never read directly from a bus parameter.
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
 * @note `secInValue[]` and `secOutValue` are written/read by the runner —
 *   they are mutable even though the rest of the struct may be `constexpr`.
 *   In practice the proc array is declared `static` (non-const) so the
 *   runner can update these fields each cycle.
 */
struct CbProc {
    const char*  name;   ///< Human-readable stage label (debug / dashboard).

    // --- Secondary bus inputs (injected by runner before fn call) ------------
    /// Up to 3 secondary input channels — nullopt slots are skipped.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> secInCh[3] = {};
    uint16_t     secInValue[3] = {};  ///< Populated by runner from secInCh[i] before fn.

    // --- Secondary bus output (committed by runner after fn call) ------------
    /// Optional secondary output channel — nullopt = no secondary write.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> optSecOutCh = {};
    uint16_t     secOutValue = 0u;    ///< Written by fn; committed by runner to optSecOutCh.

    // --- Behaviour -----------------------------------------------------------
    CbProcFn     fn;               ///< Processing function. nullptr = passthrough.
    const void*  cfg;              ///< Flash — static config struct. Cast inside fn.
    void*        dynCfg = nullptr; ///< Runtime cfg override — nullptr = use cfg.
    void*        state;            ///< RAM — mutable runtime state. Cast inside fn.
};


// =============================================================================
// 4. CHANNEL DESCRIPTOR
// =============================================================================

/**
 * @brief One named processing channel — ordered CbProc list with I/O declaration.
 *
 * @details The channel declares its primary input and output channels.
 *   The runner pre-reads `optInCh` before the proc chain and post-writes
 *   `optOutCh` after (regardless of `claimed`).
 *
 *   `chanOwner` is forwarded to every fn call and to all bus writes
 *   (primary + secondary) — no external owner parameter on the runner.
 */
struct CbChannel {
    const char*  name;  ///< Human-readable channel name (debug / dashboard).

    // --- Primary I/O (runner-owned — no read/write proc needed) --------------
    /// Primary input channel — nullopt = no pre-read (value starts at 0).
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> optInCh  = {};
    /// Primary output channel — nullopt = no post-write.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> optOutCh = {};

    // --- Proc chain ----------------------------------------------------------
    CbProc*  procs;      ///< Processor array (nullptr when procCount == 0).
    uint8_t  procCount;  ///< Number of processors in procs[].

    // --- Identity ------------------------------------------------------------
    ChanOwner  chanOwner;  ///< Identity forwarded to every fn call and bus write.
};


// EOF cb_struct.h
