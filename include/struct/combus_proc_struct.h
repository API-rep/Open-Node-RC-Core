/*!****************************************************************************
 * @file  combus_proc_struct.h
 * @brief ComBus processor pipeline — unified CbProc / CbChain types.
 *
 * @details Defines the single shared proc/channel types used by both the
 *   simulation layer (CbChain) and the ctrl layer (CbChain).
 *
 *   **Key architectural rules:**
 *   - `CbProcFn` has NO `ComBus& bus` parameter.
 *     Bus access is handled exclusively by the runner via `CbChain::inCh` /
 *     `outCh` (channel level) and `CbProc::inCh` / `outCh` (proc level).
 *   - Each proc declares exactly one secondary input (`inCh`) and at most
 *     one output (`outCh`) — both optional.  No channel IDs inside `cfg`.
 *   - `ChanOwner` flows from `CbChain::chainOwner` to the runner — never
 *     from an external parameter on the update function.
 *
 *   **Runner contract (cb_chain_update):**
 *   1. Pre-read  — runner reads `ch.inCh` → seeds `value`.
 *   2. Proc loop — for each proc (stops on `claimed = true`):
 *        a. Inject secondary input: `inValue` ← `bus[inCh]`.
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
 *   runner via `CbProc::inValue` and `CbProc::outValue`.
 *
 * @param proc     Processor descriptor — cast `cfg`/`state` to concrete types.
 *                 May also read `proc->inValue` and write `proc->outValue`.
 * @param value    Pipeline value (in/out) — seeded by runner from `CbChain::inCh`;
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
 * @details One optional secondary input (`inCh`) and one optional output
 *   (`outCh`) — never read from a bus parameter directly.
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
 * @note `inValue` and `outValue` are written/read by the runner —
 *   they are mutable even though the rest of the struct may be `constexpr`.
 *   In practice the proc array is declared `static` (non-const) so the
 *   runner can update these fields each cycle.
 */
struct CbProc {
    const char*  name;   ///< Human-readable stage label (debug / dashboard).

    // --- Secondary bus input (injected by runner before fn call) -------------
    /// Single secondary input channel — nullopt = no read.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> inCh = {};
    uint16_t     inValue = 0u;     ///< Populated by runner from inCh before fn call.

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
 *   The runner pre-reads `inCh` before the proc chain and post-writes
 *   `outCh` after (regardless of `claimed`).
 *
 *   `chainOwner` is forwarded to every fn call and to all bus writes
 *   (primary + secondary) — no external owner parameter on the runner.
 */
struct CbChain {
    const char*  name;  ///< Human-readable chain name (debug / dashboard).

    // --- Primary I/O (runner-owned — no read/write proc needed) --------------
    /// Primary input channel — nullopt = no pre-read (value starts at 0).
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> inCh  = {};
    /// Primary output channel — nullopt = no post-write.
    std::optional<std::variant<AnalogComBusID, DigitalComBusID>> outCh = {};

    // --- Proc chain ----------------------------------------------------------
    CbProc*  procs;      ///< Processor array (nullptr when procCount == 0).
    uint8_t  procCount;  ///< Number of processors in procs[].

    // --- Identity ------------------------------------------------------------
    ChanOwner  chainOwner;  ///< Identity forwarded to every fn call and bus write.
};


// EOF combus_proc_struct.h
