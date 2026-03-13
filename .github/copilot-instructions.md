# Copilot Session Rules (Persistent)

These instructions are loaded for every coding session in this workspace.

## 1) Mandatory style re-check before coding
Before creating or editing C/C++ code, always read:
- `doc/code_style_synthesis.md`
- `doc/template_module.h`
- `doc/template_module.cpp`

If a generated file does not match these documents, fix formatting before finalizing.

## 2) C/C++ formatting baseline
- Respect the project section format (`// =============================================================================`).
- Keep Doxygen usage aligned with `doc/code_style_synthesis.md`.
- Keep comment indentation rules and spacing rules exactly as documented.
- Keep the final file marker: `// EOF <file>`.
- In every `.h/.cpp` file, `@file` must exactly match the real filename.

## 3) Workflow extension area

#### Transport layer architecture
Implemented in `src/core/system/transport/`:

```
transport.h          — NodeLink (function-pointer table: write/readByte/available + ctx)
adapter/uart_link.h/.cpp — UART adapter: Serial.begin() + claim guard (3-port static pool)
protocol/combus_tx.h/.cpp   — ComBus TX, transport-agnostic
protocol/combus_rx.h/.cpp   — ComBus RX, transport-agnostic
```

**Rules:**
- Physical transport init (`uart_link_init`) is called **once per port** by the top-level init (output_init, sound_module/main). It returns a `NodeLink*`.
- Protocol modules (`combus_tx_init`, `combus_rx_init`) receive a `NodeLink*` — they never call `Serial.begin()` or touch pins directly.
- Guard: `uart_link_init` logs a fatal error and returns `nullptr` if the serial pointer is already claimed.
- Adding a new physical transport = new `*_transport.h/.cpp` implementing the 3 function pointers.
- Adding a new protocol module = new module receiving `NodeLink*` — no transport-specific code inside.

### Debug flag policy
- New debug points must use shared `DEBUG_*` flags only (`DEBUG_INPUT`, `DEBUG_HW`, `DEBUG_SYSTEM`, `DEBUG_COMBUS`, `DEBUG_ALL`).
- Do not introduce or reintroduce ad-hoc per-module flags (for example `DEBUG_HW_INIT`).
- Shared activation logic must stay centralized in `src/core/system/debug/debug.h`.

### Commit message convention
Use the following format for all commits suggested or written during a session:

```
<scope> (<type>): <short description>

<optional body — explain WHY, not what>
```

Types:
- `feat`     — new feature or capability
- `fix`      — bug fix
- `refactor` — code change with no behavior change
- `chore`    — build, deps, config (no production code)
- `docs`     — documentation only
- `test`     — tests only

Scope examples: `hw_init`, `input`, `combus`, `bt`, `drv`, `srv`, `vbat`, `debug`

Examples:
```
bt (fix): replace btStart() spin-wait with FreeRTOS-safe vTaskDelay poll
input (feat): add PS4 touchpad mapping to digital bus
drv (refactor): extract clone config logic into applyParentConfig()
deps (chore): pin PS4_Controller_Host to API-rep fork commit b58a05d
```

- Keep the short description under 72 characters, lowercase, no period.
- Body is optional but recommended for `fix` and `refactor` involving non-obvious reasoning.

### Session wrap-up checklist
At the end of every session that includes a successful build/upload cycle, run through these steps in order:

1. **Commit staged work** — propose a commit message following the convention above (scope + type + short description).
2. **Upload readiness** — if the session ended with a successful build, explicitly state: *"Le dernier build (Xs, exit 0) est valide — tu peux uploader directement sans recompiler."* Do not let the user relaunch a build unnecessarily.
3. **PS4 lib check** — see the *PS4_Controller_Host lib — periodic portability check* section below.
4. **Workflow update** — if any new convention or architecture rule was agreed during the session, propose the minimal addition to this file.

The checklist reminder should be triggered once, briefly, at the first natural wrap-up point (stable build after a feature or fix block). Do not repeat it within the same session.

### Periodic reminder note
- At appropriate moments (after a stable build/upload cycle, before large refactors, or at session wrap-up), proactively suggest refining this workflow section.
- Keep the reminder short and actionable (one concrete proposal at a time).
- When the reminder concerns debug readability, explicitly point to `doc/code_style_synthesis.md` section `7) Debug serial output formatting (WIP)`.

### PS4_Controller_Host lib — periodic portability check
- The project currently uses a **forked version** of `PS4_Controller_Host` pinned at commit `b58a05d` (`API-rep/PS4_Controller_Host`).
- The fork contains a BT init fix required for **arduino-esp32 2.0.x / IDF 4.4** (`espressif32@6.7.0`).
- The project is currently **blocked on IDF 4.4** because `ps4.c` SPP APIs are incompatible with IDF 5.x.
- **At session wrap-up or before any platform upgrade**, check the following:
  1. Has `pablomarquez76/PS4_Controller_Host` upstream released a new version?
  2. Does the new version include IDF 5.x compatibility in `ps4.c` / `ps4_spp.c`?
  3. If yes → evaluate migrating `platform` to a recent `espressif32` (IDF 5.x) and test BT init.
  4. If migration is viable → update `platformio.ini` (`platform` + `lib_deps`) and re-validate on hardware.
- Keep this note until a successful IDF 5.x migration is confirmed.

### Dashboard architecture

#### Intent
Provide a non-intrusive, modular ANSI terminal dashboard for runtime monitoring.
The machine and remote code must remain completely ignorant of the dashboard.
All dashboard logic lives in dedicated `*_dashboard` files.

#### Four-layer architecture
```
Layer 1 — Core shell
  src/core/system/debug/dashboard.h / .cpp
    Pure rendering primitives (frame, nav bar, keyboard, event ring buffer,
    render-slot registry, refresh timer). Zero knowledge of any mode or module.

Layer 2 — Env dashboard  (one per execution environment)
  src/machines/system/debug/dashboard_machine.h / .cpp
  src/remotes/system/dashboard_remote.h / .cpp   (future)
    Owns the environment-level overview view.
    Auto-detects which sub-modules are configured (from build flags and
    runtime state) and registers them as sub-module slots into core.
    Generates the dynamic navigation bar from registered slots only —
    no dead keys, no views for absent modules.

Layer 3 — Module views  (one per sub-module, optional)
  src/machines/system/debug/dashboard_drv.h / .cpp     (DC drivers)
  src/machines/system/debug/dashboard_input.h / .cpp   (input / combus)
  src/machines/system/debug/dashboard_vbat.h / .cpp    (battery)
  ...
    Each module view is a single self-contained screen combining:
      • Live state  — current values, runtime metrics (top section).
      • Init config — static snapshot of the config used at init time
                      (bottom section, same screen, no extra key needed).
    "debug" is renamed "config" in this context — it represents the
    configuration that was applied at init, not a debug dump.
    Module views read their own data directly (Machine, ComBus, vbat API…).
    They never call into app code.

Layer 4 — Serial init log  (transitional, destined to disappear)
  src/core/system/debug/hw_init_debug.cpp
  src/core/system/debug/input_manager_debug.cpp
  ...
    One-shot text dumps called by the init sequence. Kept as-is until the
    corresponding Layer 3 module view exists and covers the same information
    in its "config (at init)" section.
    Migration order: implement the Layer 3 module view → validate parity →
    remove the corresponding *_debug.cpp.
    Long-term goal: Layer 4 is fully absorbed into Layer 3. No *_debug.cpp
    files remain once all module views are implemented.
```

#### No dashboard code in app code
- `main.cpp`, `machine_init()`, `loop()` and all machine/remote logic files must
  contain **zero** dashboard-specific calls beyond `dashboard_update()` in loop
  and the single `dashboard_machine_setup()` call at end of `machine_init()`.
- `dashboard_machine_setup()` (Layer 2) is the sole setup entry point — it calls
  core `dashboard_setup()` internally and registers all module slots.
- New features never add dashboard calls inline — they push to the event buffer
  via `dashboard_push_event()` if a runtime notification is needed.

#### Render-slot registration
Core declares a fixed-size slot table of function pointers:
```cpp
using DashRenderFn    = void (*)(void);
using DashDetailFn    = void (*)(void);
using DashDetailCount = uint8_t (*)(void);
void dashboard_register_slot(uint8_t key, const char* label, DashRenderFn fn);
void dashboard_register_detail(uint8_t slotKey, DashDetailCount countFn, DashDetailFn renderFn);
uint8_t dashboard_detail_index();
```
- `key`   — keyboard character that selects this view (`'1'`, `'2'`, …).
- `label` — short string shown in the nav bar (`"drivers"`, `"battery"`, …).
- `fn`    — render function provided by the module.
- `countFn` — returns the number of detail sub-items at runtime (0 = no detail).
- `DashDetailFn` — renders one sub-item; call `dashboard_detail_index()` inside.
The env dashboard (`dashboard_machine_setup()`) calls `dashboard_register_slot()`
for each module that is present and configured. Slots not registered are
invisible — they do not appear in the nav bar and their key does nothing.

Core always owns slot `'1'` (overview) and `'Q'` (quit/suspend) — these are
never overridable by env or module layers.

#### Module view layout (Layer 3)

**Main slot view** — live data table only, no config section:
```
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│  [ MODULE ]  <descriptor>                                         uptime: HH:MM:SS  │
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│  Col1  Col2  Col3  ...                                                              │
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│  row1 data ...                                                                       │
│  row2 data ...                                                                       │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
  1 : OVERVIEW    2 : INPUTS    3 : DRIVERS    4 : BATTERY    Q : QUIT    RETURN : DETAIL
```

**Detail sub-view (per item, reached via Enter):**
```
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│  [ MODULE ]  #N - <item name> detail                              uptime: HH:MM:SS  │
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│  FieldA :  valueA  |  FieldB :  valueB  |  FieldC :  valueC               (live)   │
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│  Config line 1 :  value                                                              │
│  Config line 2 :  value                                                              │
│  ...                                                                                 │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
  - Item 1/3 + (- +)    MODULE detail    Q : back
```
- Live section refreshes every `DashRefreshMs` (200 ms default).
- Config-at-init in the **detail view only** — not in the main slot view.
- Live row in detail: pipe-separated `Field :  value  |  Field :  value` format.
- Config lines in detail: `  Label :  value` (two-space indent, label, space, colon, two spaces, value).

#### Frame and layout rules
- Frame style: **full single-line** box drawing (`┌─┐│└─┘├─┤`) — no double borders.
- Inner content width: **120 characters** (`DashInnerW = 120`), outer frame = 122.
- One header row per view (module name + descriptor + uptime on the same line).
- Column separators inside dense tables: `│` (U+2502).
- Navigation bar is printed **outside** the bottom border, centered within frame width.
- Nav bar format: `  X : LABEL    Y : LABEL  ...  Q : QUIT`  (1-based keys, uppercase labels, spaced colons).
- Only registered slots appear in the nav bar.
- **Uniform row height** — all conditional states of a content section (e.g. connected vs disabled channel, populated vs empty slot) must produce the **same number of content rows**. Varying row counts cause visual layout shift when navigating between detail sub-items. Use fixed-width placeholder fields (`---`, `N/C`) instead of `dEmpty()` padding to fill missing data.

#### Refresh and keyboard
- Default refresh interval: `DashRefreshMs = 200` ms (configurable via constant).
- Keyboard handling: non-blocking `Serial.available()` check; ESC sequences polled with 10 ms timeout.
- View change forces an immediate redraw (reset `s_lastRefresh = 0`).
- `Q` / `q` at top level: suspends the dashboard (one-line notice, no redraw until next key).
- `Q` / `q` in detail mode: exits detail mode and returns to the parent slot.
- `Enter` (`\r`/`\n`): if the current slot has a registered detail sub-view and count > 0,
  enters detail mode at sub-item 0.
- `←` arrow (`\x1B[D`) in detail mode: navigate to previous sub-item (wraps).
- `→` arrow (`\x1B[C`) in detail mode: navigate to next sub-item (wraps).
- `<` key in detail mode: same as `←` — fallback for serial monitors that don’t transmit ANSI sequences.
- `>` key in detail mode: same as `→` — fallback for serial monitors that don’t transmit ANSI sequences.
- Slot selection keys (`1`–`9`) are **ignored** while in detail mode.
- Normal nav bar appends `RETURN : DETAIL` when the current slot has a registered detail sub-view with at least one item.

#### Detail sub-view system
Any slot can optionally register a **detail sub-view** providing per-item drill-down screens
(e.g. one screen per battery channel, one per driver, etc.).

```
Top-level view (slot)            →  Enter  →  Detail sub-view (item 0)
  ← / → arrows                               navigate items (wraps)
  Q                                           back to parent slot
```

**Registration** (called alongside `dashboard_register_slot`):
```cpp
dashboard_register_detail('4', vbat_channel_count, render_vbat_detail);
//                         ^    ^                   ^
//                         key  countFn             renderFn
```
- `slotKey`  — must match the parent slot key.
- `countFn`  — `uint8_t (*)(void)`, queried at runtime; 0 disables detail entry.
- `renderFn` — `void (*)(void)`, calls `dashboard_detail_index()` to get current item.

**Nav bar in detail mode** (replaces normal slot bar):
```
  - Ch 1/2 +  (- +)      BATTERY detail      Q : back
```

**Rules:**
- Detail view owns the full frame — it renders `dPre()` / `dTop()` / `dBot()` itself.
- Detail render fn must display both live state and config-at-init in the same frame.
- `dashboard_detail_index()` is 0-based; display as 1-based (`idx + 1`) in titles.
- Slot selection keys (`1`–9`) do nothing while in detail mode; only `←`, `→`, `Q` are active.
- Never add detail logic to app code — register from the module's `*_register()` function only.

#### Event ring buffer
- Depth: `DashEventCount = 6` visible lines.
- Each entry: `{ uint32_t ms; char msg[56]; }`.
- Overflow: oldest entry overwritten (circular).
- Timestamp displayed as relative age: `-MM:SS.t` from current uptime.
- `dashboard_push_event()` is the **only** public write path — called from any
  module that wants to surface a runtime event (runlevel change, failsafe, etc.).

#### File map
```
src/core/system/debug/
  dashboard.h      ← public API: setup, push_event, update, register_slot,
                                register_detail, detail_index
  dashboard.cpp    ← frame primitives, slot table, detail table, nav bars,
                     keyboard (incl. ESC arrow sequences), event buffer

src/machines/system/debug/
  dashboard_machine.h / .cpp   ← env layer: overview view, auto-detect, slot registration
  dashboard_drv.h / .cpp        ← module view: DC drivers (live table + detail per driver)
  dashboard_input.h / .cpp      ← module view: inputs / combus
  dashboard_vbat.h / .cpp       ← module view: battery sensing + detail per channel
```
Remote equivalent (future): `src/remotes/system/dashboard_remote.h/.cpp`

#### Build flag
Activated via `-D DEBUG_DASHBOARD` in `platformio.ini`.  
All public API degrades to inline no-ops when the flag is absent — zero overhead.

## 4) Safe change policy
- Prefer minimal, focused edits.
- Do not reformat unrelated code.
- Keep changes consistent with existing project style.

## 5) Deferred feature ideas

Re-read this section periodically and propose one item when the project is stable
enough to absorb it. Never propose more than one at a time.

### ExtPort conflict detection (board-level)
**Context:** Extension/communication pins (`TxdExtPin`, `RxdExtPin`, …) in board headers
are bare `constexpr` pin numbers. Nothing prevents two modules activated by separate
build flags from both claiming the same pin — the conflict is silent until hardware misbehaves.

**Proposed feature:** Add a lightweight `ExtPort` registry in the board header:
- A `constexpr` array of `{ int8_t pin; const char* owner; }` entries, one per
  assignable port.
- `output_init()` (and any future init using a port) calls a compile-time or
  early-runtime `port_claim(pin, "module_name")` that `static_assert`s / logs a
  fatal error on duplicate.
- Zero RAM overhead when only one claimant exists; small ROM cost otherwise.

**Prerequisite:** Project is stable with ≥ 2 independent output modules active
simultaneously — only then does the risk justify the added infrastructure.
### Sound transport cap cross-check (controller ↔ sound module)
**Context:** `output_init.h` defines `SoundTransportMaxTxHz = 200u` as the
controller-side ceiling. `ESP32_8M_6S.h` has a matching TODO comment. The sound
module side has no equivalent `constexpr` yet — the cross-check is therefore
one-sided and the 200 Hz cap is an arbitrary placeholder.

**Proposed feature:** Once `sound_config.h` (sound node side) exposes:
```cpp
static constexpr uint32_t SoundRxMaxHz  = ...;   // max frame rate the node can process
static constexpr uint32_t SoundRxMaxBaud = ...;  // max supported baud
```
Add to `output_init.h` (under `#ifdef SOUND_OUTPUT_UART`):
```cpp
static_assert(SoundTransportTxHz  <= SoundRxMaxHz,   "TX rate exceeds sound node capability");
static_assert(SoundUartBaud       <= SoundRxMaxBaud,  "Baud exceeds sound node capability");
```
Replace the `200u` magic number with `SoundRxMaxHz` and remove the `// TODO` comment from the board header.
Also update the `static_assert` in `sound_uart.h` from `UartMaxBaud` to `SoundRxMaxBaud` once the sound node exposes it.

**Prerequisite:** Sound node firmware is stable enough to characterise its real
receive throughput and commit it as a `constexpr` in `sound_config.h`.
### Debug serial opt-out for single-UART architectures
**Context:** `combus_uart_tx_init` / `combus_uart_rx_init` take a `HardwareSerial*`,
so swapping `SerialExt` from `Serial2` → `Serial` is a one-line change in the board header.
The blocker is that `Serial` is already opened for debug output (`sys_log_info` etc.).
If a future board has only one usable UART (no Serial2/Serial1), debug must be
silenceable at build time to free UART0 for transport.

**Proposed feature:** Add a `-D NO_DEBUG_SERIAL` build flag that:
- Makes all `sys_log_info` / `*_log_dbg` macros expand to nothing (already
  partially enabled by the existing `DEBUG_*` flag logic in `debug.h`).
- Suppresses the `Serial.begin(115200)` in `sys_init.cpp`.
- Allows the board header to declare `inline HardwareSerial& SerialExt = Serial;` without conflict.

**Prerequisite:** A real board with only one UART available — not worth the
complexity otherwise.