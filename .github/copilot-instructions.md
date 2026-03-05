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
Add new persistent workflow rules here over time, for example:
- debug workflow presets
- commit message conventions
- test/build order
- release checklist

### Debug flag policy
- New debug points must use shared `DEBUG_*` flags only (`DEBUG_INPUT`, `DEBUG_HW`, `DEBUG_SYSTEM`, `DEBUG_COMBUS`, `DEBUG_ALL`).
- Do not introduce or reintroduce ad-hoc per-module flags (for example `DEBUG_HW_INIT`).
- Shared activation logic must stay centralized in `src/core/utils/debug/debug.h`.

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
  src/core/utils/debug/dashboard.h / .cpp
    Pure rendering primitives (frame, nav bar, keyboard, event ring buffer,
    render-slot registry, refresh timer). Zero knowledge of any mode or module.

Layer 2 — Env dashboard  (one per execution environment)
  src/machines/utils/dashboard_machine.h / .cpp
  src/remotes/utils/dashboard_remote.h / .cpp   (future)
    Owns the environment-level overview view.
    Auto-detects which sub-modules are configured (from build flags and
    runtime state) and registers them as sub-module slots into core.
    Generates the dynamic navigation bar from registered slots only —
    no dead keys, no views for absent modules.

Layer 3 — Module views  (one per sub-module, optional)
  src/machines/utils/dashboard_drv.h / .cpp     (DC drivers)
  src/machines/utils/dashboard_input.h / .cpp   (input / combus)
  src/machines/utils/dashboard_vbat.h / .cpp    (battery)
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
  src/core/utils/debug/hw_init_debug.cpp
  src/core/utils/debug/input_manager_debug.cpp
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
using DashRenderFn = void (*)(void);
void dashboard_register_slot(uint8_t key, const char* label, DashRenderFn fn);
```
- `key`   — keyboard character that selects this view (`'1'`, `'2'`, …).
- `label` — short string shown in the nav bar (`"drivers"`, `"battery"`, …).
- `fn`    — render function provided by the module.
The env dashboard (`dashboard_machine_setup()`) calls `dashboard_register_slot()`
for each module that is present and configured. Slots not registered are
invisible — they do not appear in the nav bar and their key does nothing.

Core always owns slot `'0'` (overview) and `'Q'` (quit/suspend) — these are
never overridable by env or module layers.

#### Module view layout (Layer 3)
Each module view renders two sections inside the same frame:
```
┌──────────────────────────────────────────────────────────────────────┐
│  <MODULE NAME>   live                              uptime: MM:SS.t  │
├──────────────────────────────────────────────────────────────────────┤
│  ... live state rows ...                                             │
├──────────────────────────────────────────────────────────────────────┤
│  config (at init)                                                    │
├──────────────────────────────────────────────────────────────────────┤
│  ... static init-config rows ...                                     │
└──────────────────────────────────────────────────────────────────────┘
  0:overview  1:drivers  2:inputs  3:battery  Q:quit
```
- Live section refreshes every `DashRefreshMs` (200 ms default).
- Config section is written once at `dashboard_machine_setup()` and cached — no
  repeated reads of const config structs during runtime.

#### Frame and layout rules
- Frame style: **full single-line** box drawing (`┌─┐│└─┘├─┤`) — no double borders.
- Inner content width: **70 characters** (`DashInnerW = 70`), outer frame = 72.
- One header row per view (module name + mode tag + uptime on the same line).
- Column separators inside dense tables: `│` (U+2502).
- Navigation bar is printed **outside** the bottom border, left-aligned, 2-space indent.
- Nav bar format: `  0:overview  1:<label>  2:<label>  ...  Q:quit`
- Only registered slots appear in the nav bar.

#### Refresh and keyboard
- Default refresh interval: `DashRefreshMs = 200` ms (configurable via constant).
- Keyboard handling: single character, non-blocking (`Serial.available()` check).
- View change forces an immediate redraw (reset `s_lastRefresh = 0`).
- `Q` / `q` suspends the dashboard (prints a one-line notice, no redraw until next key).

#### Event ring buffer
- Depth: `DashEventCount = 6` visible lines.
- Each entry: `{ uint32_t ms; char msg[56]; }`.
- Overflow: oldest entry overwritten (circular).
- Timestamp displayed as relative age: `-MM:SS.t` from current uptime.
- `dashboard_push_event()` is the **only** public write path — called from any
  module that wants to surface a runtime event (runlevel change, failsafe, etc.).

#### File map
```
src/core/utils/debug/
  dashboard.h      ← public API: setup, push_event, update, register_slot
  dashboard.cpp    ← frame primitives, slot table, nav bar, keyboard, event buffer

src/machines/utils/
  dashboard_machine.h / .cpp   ← env layer: overview view, auto-detect, slot registration
  dashboard_drv.h / .cpp        ← module view: DC drivers (live state + init config)
  dashboard_input.h / .cpp      ← module view: inputs / combus
  dashboard_vbat.h / .cpp       ← module view: battery sensing
```
Remote equivalent (future): `src/remotes/utils/dashboard_remote.h/.cpp`

#### Build flag
Activated via `-D DEBUG_DASHBOARD` in `platformio.ini`.  
All public API degrades to inline no-ops when the flag is absent — zero overhead.

## 4) Safe change policy
- Prefer minimal, focused edits.
- Do not reformat unrelated code.
- Keep changes consistent with existing project style.
