# Copilot Session Rules (Persistent)

These instructions are loaded for every coding session in this workspace.

## 0) Next session — READ FIRST

### En cours (WIP)

| Session | Sujet | État |
|---------|-------|------|
| 2026-05-16 | `SimDev` — ComBus inertia processor | Architecture définie, **non implémenté** — winter 2026 |
| 2026-05-16 | `SrvDevType::ESC_RC` | À ajouter dans `machines_defs.h` — winter 2026 |
| 2026-05-16 | Backup commits (parent + sound_module) | **En attente** |
| 2026-05-17 | `DRIVE_STATE_BUS` — DriveState 3-bit sur le fil | ✅ Implémenté |
| 2026-05-17 | `TRACTION_BUS` packed [bit15:BRAKE\|bit14:FWD\|13-bit magnitude] | Architecture décidée — winter 2026 (ComBus v2) |

### État validé
- Tous les bugs son (horn/volume, engine key, hydraulique) ✅ corrigés et validés hardware (2026-03-22/23)
- `sound_config.h` mergé dans `config.h` ✅
- Refactor sound engine (étapes 0–8, L1, L2 A–D) ✅ **toutes complétées (2026-05-07 → 2026-05-14)**
  - Architecture 4 couches : `SoundInterpreter` → `SoundCore` → `MixerState` → `SoundHalAudio`
  - `1_Vehicle.h` retiré, `sound_audio_task_setup()` supprimé, DiYGuy inline dans `dumper_truck_sound.cpp`
  - `VehicleSoundProfile` / `VehicleLegacyCfg` en place ; `CaboverCAT3408.h` supprimé
- `GearShiftProfile` + `gear_fsm` refactorisés (session 2026-05-14) — voir section ci-dessous

### Template migration DiYGuy pour autres `vehicles/*.h`
1. Créer `config/profiles/<class>/<vehicle>_sound.h/.cpp` — inclure les `sounds/*.h` DiYGuy directement dans le `.cpp` (seul TU). Peupler `kEngineGenCfg`/`kEffectsCfg` avec les variables DiYGuy directement.
2. Rien à changer dans `main.cpp` (toutes les lectures DiYGuy ont été remplacées par des champs struct).

### Règle cross-core permanente
Ne jamais recréer `syncEngineSimState()` ni de flat global cross-core.
Toute nouvelle donnée partagée entre cores → champ `volatile` dans `gEngineSimState`, écrit directement.

**Variables flat restantes (non migrées — par conception) :**
- `escInReverse` : Core 0 écrit ET lit — même-core, pas de risque.
- `hydraulicLoad` : Core 1 écrit, Core 0 + Core 1 lisent — maintenu volatile flat.
- `engineRunning`, `engineState`, `engineOn` : machine à états Core 0, lus Core 1 — migration future (winter 2026).
- `driveState` : Core 0 écrit, Core 1 lit — migration future.

### GearShiftProfile + gear_fsm (session 2026-05-14)
- `GearShiftProfile` : struct plat avec pointeurs `const int16_t*` + champ `gears` — aucune limite de rapport.
  Les tableaux de seuils sont définis séparément dans `motion_presets.h` (ex. `kHeavy3_upShift[]`).
- `gear_fsm` déplacé dans `src/core/system/simulation/` (accessible machine + sound node).
- Preset `kGearShift_VolvoD16J` (tableaux `kVolvoD16J_steps[]` / `kVolvoD16J_subSteps[]`) dans `simulation_presets.h` ; alias `kDumperTruckGearShift` (`const GearShiftProfile*`) dans `dumper_truck_motion.h`.
- `VehicleSoundProfile::upShift/downShift/downShiftBraking` = `const int16_t*` pointant vers le preset (plus de copie).
- Seuils en RPM (0–2100 CAT 3408). Conversion machine-side : `rpm = |pos − CbusNeutral| × maxRpm / CbusNeutral`.

### Commits en attente

**sound_module submodule:**
1. `sound (fix): remove residual .gearboxGated/.drivesFlash initialisers from dumper_truck profile`
2. `sound (refactor): apply project style to effects_gen.* (airy, explicit names, step comments)`
3. `sound (refactor): invert generate() loops — switch once per device, not per sample`

**parent repo:**
4. `motion (refactor): GearShiftProfile pointer arrays, gear_fsm to core/simulation, RPM scale`
5. `sim (refactor): add ComBus-only sim_dev_update overload`
6. `sim (refactor): route per-gear ramp through TRACTION_RAMP_BUS`
   ```
   GearShiftProfile gains rampTime[] — single source of truth for per-gear
   inertia ramp. sim_gear writes ramp time to TRACTION_RAMP_BUS (local channel)
   after each gear update. sim_traction reads from bus, falls back to
   defaultRampTime when no sim_gear upstream.
   SimTractionCfg loses rampTime[]+gearCount — replaced by defaultRampTime.
   ```
7. `motion (feat): add DRIVE_STATE_BUS wire channel — 3-bit DriveState encoding`
   ```
   Drive state (standing/fwd/rev/braking) now transmitted on the wire.
   DriveStateBus::encode()/decode() in motion_struct.h.
   Consumers (sound node) can read the state without re-deriving it
   from ESC_SPEED_BUS sign. To be replaced by TRACTION_BUS at ComBus v2.
   ```


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

#### Vehicle config hierarchy ✅ implemented
Each vehicle lives under `src/machines/config/machines/<vehicle>/`.
Include chain (top-down, driven by build flags):

```
machines.h
  └─ (MACHINE == VOLVO_A60_H_BRUDER)
       volvo_A60H_bruder/volvo_A60H_bruder.h   ← Level 0: kVehicleName, kVehicleCombusLayout
         ├─ (IS_MAINBOARD) mainboard/mainboard.h   ← env umbrella
         │    └─ (BOARD == ESP32_8M_6S) ESP32_8M_6S/envCfg.h/.cpp
         └─ (IS_EXT_BOARD) ext_board/ext_board.h   ← future
              └─ (BOARD == ...) .../envCfg.h/.cpp
```

**Build flags:**
- `-D MACHINE=VOLVO_A60_H_BRUDER` — selects vehicle in `machines.h`.
- `-D IS_MACHINE` — structural flag for ComBus headers (always set in `[env:machines]`).
- `-D IS_MAINBOARD` — selects the mainboard env tree in `volvo_A60H_bruder.h`.
- `-D IS_EXT_BOARD` — selects the extension-board env tree in `volvo_A60H_bruder.h`.
- `-D BOARD=ESP32_8M_6S` — optional board override (default set in `mainboard/mainboard.h`).

**Rules:**
- Level-0 (`volvo_A60H_bruder.h`) declares only `kVehicleName` and `kVehicleCombusLayout`.
  Sound node and remote envs include it **with** the env-type flag (`IS_EXT_BOARD` for sound node).
- Device enums (`DrvDev`, `SrvDev`, `SigDev`), extern arrays, and `inline constexpr EnvCfg machine` live in the board-specific `envCfg.h` under `<board>/`.
- `envCfg.h` uses `kVehicleName`/`kVehicleCombusLayout` from Level-0 in the `machine` aggregate — no literal strings.
- Adding a new vehicle = new `<vehicle>/` folder with the same three-level structure.
- Adding a new board for an existing vehicle = new folder under `mainboard/` (or `ext_board/`) + `#elif` branch in the env umbrella.

#### Machine-class config hierarchy (`src/core/config/machines/`) ✅ implemented 2026-05-11

Config shared between **all environments** that build for a given machine class
(machine node, sound node, future remote) lives in:

```
src/core/config/machines/
  machine_config.h          ← top dispatcher: #if MACHINE == <id> → class umbrella
  combus_ids.h              ← struct-safe dispatcher (no cycle): #if MACHINE == <id> → *_ids.h
  combus_types.h            ← full combus dispatcher: #if MACHINE == <id> → *combus.h
  <class>/
    <class>_config.h        ← class umbrella (combus + motion + inputs_map + sound)
    combus/<class>.h/.cpp   ← channel IDs, array externs, comBus extern
    combus/<class>_ids.h    ← IDs only (zero deps — safe for struct headers)
    motion/<class>_motion.h ← traction preset alias
    inputs_map/             ← input → ComBus channel mapping
    sound/<class>_sound.h/.cpp  ← kVehicleSoundDynamics (VehicleSoundProfile)
```

**Dispatch rule:** `#if MACHINE == VOLVO_A60_H_BRUDER` — the same token-comparison
trick as `machines.h` (undefined token = 0, vehicle id = non-zero integer).
Never use a separate integer constant or a second build flag for the machine class.

**`MACHINE_TYPE` rule:** defined as `#define MACHINE_TYPE DUMPER_TRUCK` (enum token)
in each vehicle's Level-0 header (`volvo_A60H_bruder.h`).
Used exclusively where a C++ enum value is needed (`CombusLayout::MACHINE_TYPE`).
**NOT** used as a preprocessor dispatch integer — use `MACHINE ==` for that.

**Multi-class `.cpp` guard:** when a `*_sound.cpp` (or future `*_combus.cpp`)
defines a global symbol, wrap it in `#if MACHINE == <its_vehicle_id>` to prevent
duplicate-symbol linker errors when multiple class files are compiled simultaneously.
Until a real vehicle exists for that class, use `#if 0` with a comment.

**Classes defined:**
- `dumper_truck/` — articulated hauler (active: Volvo A60H)
- `excavator/`   — hydraulic-arm (combus/motion/inputs_map: TODO winter 2026; sound stub ready)
- `loader/`      — wheel loader  (combus/motion/inputs_map: TODO winter 2026; sound stub ready)

**Adding a new machine class:**
1. Create `src/core/config/machines/<class>/` with the folder structure above.
2. Add one `#elif MACHINE == <vehicle_id>` branch in `machine_config.h`, `combus_ids.h`, `combus_types.h`.
3. Add one `#elif MACHINE == <vehicle_id>` branch in `sound_module/config/profiles/profiles.h`.
4. No other file needs to change.

#### Transport layer architecture
Implemented in `src/core/system/com/`:

```
node_com.h           — NodeCom (function-pointer table: write/readByte/available + ctx)
ports/uart_com.h/.cpp — UART adapter: Serial.begin() + claim guard (3-port static pool)
protocol/combus_tx.h/.cpp   — ComBus TX, transport-agnostic
protocol/combus_rx.h/.cpp   — ComBus RX, transport-agnostic
```

**Rules:**
- Physical transport init (`uart_com_init`) is called **once per port** by the top-level init (output_init, sound_module/main). It returns a `NodeCom*`.
- Protocol modules (`combus_tx_init`, `combus_rx_init`) receive a `NodeCom*` — they never call `Serial.begin()` or touch pins directly.
- Guard: `uart_com_init` logs a fatal error and returns `nullptr` if the serial pointer is already claimed.
- Adding a new physical transport = new `adapter/*_com.h/.cpp` implementing the 3 function pointers.
- Adding a new protocol module = new module receiving `NodeCom*` — no transport-specific code inside.

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

### static_assert placement rule
- A `static_assert` that only references constants defined **within the same header** stays in that header.
- A `static_assert` that crosses **two distinct include chains** (e.g. a config constant vs. a board constant) belongs in the `.cpp` init file where both chains are guaranteed to be resolved.
- Rationale: headers can be included in many TUs at different points in the include graph; a cross-chain assert will fail unpredictably depending on include order.
- Known exception: `sound_init.h` (cross-checks `SOUND_CH_*` vs `2_Remote.h` macros) — intentionally left in place until the sound sub-project is refactored.

## 5) Deferred feature ideas

Re-read this section periodically and propose one item when the project is stable
enough to absorb it. Never propose more than one at a time.

### Sound HAL — zero-mapping semantic sound (architectural intent)
**Context:** The current sound HAL (`sound_hal.cpp`) maps ComBus channels to
`pulseWidth[]` slots by hand (explicit index constants). This is fragile and
requires a manual update whenever the ComBus layout or the vehicle profile changes.

**Core insight (refined):** Every ComBus channel is already bound to a machine
peripheral (driver, servo, actuator signal) with a known type (`TRACTION_DC`,
`HYDRAULIC_LINEAR`, `HORN_SIGNAL`, …). The peripheral type IS the sound role —
no separate mapping table needed.

The sound module simply includes the machine config header to access the full
peripheral descriptor and derives sound events directly:

```
ComBus analog[1]  → DRIVE_SPEED_BUS → motors[CABIN_LEFT].type = TRACTION_DC  → engine throttle
ComBus analog[2]  → DUMP_BUS        → actuators[DUMP].type   = HYDRAULIC_LINEAR → hydraulic flow
ComBus digital[0] → HORN            → signals[HORN].type      = HORN_SIGNAL   → horn trigger
```

**Implementation sketch:**
- Sound module `platformio.ini` env adds `build_flags = -I src/machines` (already implied
  by shared `src/core/` include path — may be zero cost).
- `sound_hal.cpp` includes the machine config header for the target vehicle.
- The HAL iterates `AnalogComBusID` / `DigitalComBusID` entries, reads the peripheral
  type from the machine descriptor, and calls the appropriate sound engine function.
- Adding a new ComBus channel = add it to the machine descriptor. Sound is automatic.
- No `SoundRole` enum, no separate `sound_roles.h`, no manual mapping file.

**Multi-peripheral constraint:** A single ComBus analog channel can drive several
physical peripherals (e.g. two traction motors on the same `DRIVE_SPEED_BUS`).
The sound mapping is valid only if **all peripherals bound to that channel share
the same peripheral type**. Mixing types on one channel (e.g. one `TRACTION_DC`
and one `HYDRAULIC_LINEAR` on the same analog index) is undefined from a sound
perspective — the HAL cannot resolve which sound role applies.

A `static_assert` (or early-init fatal log) must verify this at build time or
boot time:
```cpp
// For each ComBus channel, all bound peripherals must have the same type.
// Checked in sound_hal_init() when machine config header is included.
static_assert(allSameType(DRIVE_SPEED_BUS), "Mixed peripheral types on same ComBus channel");
```
This constraint should be verified before the sound engine iterates any channel.

**Prerequisite:** The current `rc_engine_sound`-based sound module (`src.ino`) must
first be replaced by a native `.cpp` sound engine. This is a major refactor — do not
start until the `src.ino` dependency is fully removed from the PlatformIO build.

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
**Context:** `output_init.h` defines `ComBusUartMaxTxHz = 200u` as the
controller-side ceiling. `ESP32_8M_6S.h` has a matching TODO comment. The sound
module side has no equivalent `constexpr` yet — the cross-check is therefore
one-sided and the 200 Hz cap is an arbitrary placeholder.

**Proposed feature:** Once `sound_config.h` (sound node side) exposes:
```cpp
static constexpr uint32_t SoundRxMaxHz  = ...;   // max frame rate the node can process
static constexpr uint32_t SoundRxMaxBaud = ...;  // max supported baud
```
Add to `output_init.h` (under `#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)`):
```cpp
static_assert(ComBusUartTxHz  <= SoundRxMaxHz,   "TX rate exceeds sound node capability");
static_assert(ComBusUartBaud  <= SoundRxMaxBaud,  "Baud exceeds sound node capability");
```
Replace the `200u` magic number with `SoundRxMaxHz` and remove the `// TODO` comment from the board header.
Also update the `static_assert` in `combus_uart.h` from `UartMaxBaud` to `SoundRxMaxBaud` once the sound node exposes it.

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
### ComBus bidirectional convenience wrapper (`combus_link_init`)
**Context:** `combus_tx_init()` and `combus_rx_init()` are intentionally separate.
All current nodes are unidirectional: the machine transmits only (`output_init`),
the sound module receives only (`sound_module/main`). Both share the same `NodeCom*`
from `uart_com_init()` when needed — UART is full-duplex, no collision possible.

**Proposed feature:** When a bidirectional node exists (e.g. a remote that both
sends commands and returns telemetry), add a thin convenience wrapper:
```cpp
// combus_link.h / .cpp  —  call only when a node needs both TX and RX
void combus_link_init( NodeCom*       com,
                       ComBusFrameCfg txCfg,  uint32_t txHz,
                       ComBusFrameCfg rxCfg,
                       uint16_t*      analogBuf,
                       bool*          digitalBuf );
```
The wrapper just calls `combus_tx_init()` then `combus_rx_init()` — no logic
duplication. Unidirectional callers keep their single `_tx_init` / `_rx_init` call.

**Prerequisite:** A concrete bidirectional node exists in the project — do not
create the wrapper speculatively.
### Light module v2 — LedDescriptor + bitmask runlevels (winter 2026 rework)
**Context:** The current `light_core.cpp` is a direct port of the legacy `led()` / `updateRGBLEDs()`
functions.  The architecture decisions below were agreed 2026-04-18 and are the
template for the next light module rework.

**Architecture decisions (arrêtées — ne pas remettre en question) :**

1. **`LedDescriptor` per channel** — one struct per LED channel, in two parts:
   - `constexpr` config section (in flash): brightness levels, activeMask, type,
     optional sub-struct for special behaviour (see below).
   - RAM state section: current brightness, phase timer, on/off flag — written by
     the parser each loop pass.
   - `statusLED*` backend kept as-is for now; full class migration deferred to
     the same winter rework.

2. **Bitmask activation (`activeMask : uint8_t`)** — encodes *when* to drive this
   channel, not just *how bright*.  Each bit corresponds to a light runlevel step.
   The parser evaluates early-exit conditions in priority order (e.g. forced hazard
   > indicator active > sidemarker dim > off) using a series of `case`-style guards.
   Combinable example: clignotant actif si (canal INDICATOR actif) **ou** (feux de
   roulage actifs) **ou** (hazard) — chaque condition est un bit dans le mask.

3. **Optional behavioural sub-structs** — instead of a `type` enum with flat flags,
   special channels embed a typed sub-struct (à la `MotionCfg`):
   - `BeaconCfg { uint16_t onMs, offMs, pauseMs; uint8_t pulses; bool blueLight; }`
   - `XenonCfg  { uint32_t ignitionFlashMs; uint8_t flashBrightness; }`
   - `IndicatorCfg { uint16_t onMs, offMs; bool ledType; }`  (ledType = no ramp)
   A `nullptr` sub-struct pointer means "plain PWM channel" — no special behaviour.

4. **Variable-length channel array via enum** — same pattern as `DcDevID` in the
   machine config: an enum defines channel count, a `constexpr LedDescriptor[]`
   array is indexed by it.  No hardcoded channel constants anywhere in the parser.

5. **Parser loop** — `light_core_update()` iterates the descriptor array; each
   iteration evaluates `activeMask`, calls the appropriate driver (plain PWM,
   beacon flash, xenon flash, indicator), updates the RAM state section.
   Zero `switch(lightsState)` blocks — table-driven throughout.

**Prerequisite:** `statusLED` refactor to proper class (winter 2026). Until then,
`statusLED*` backend is kept per descriptor.  Do not start this rework during the
active season.

---

### SoundDevice — table-driven sound engine  (winter 2026 refactor)
**Context:** The current sound engine (`main.cpp`, 1637 lines) is a monolithic
rc_engine_sound fork built around a flat `pulseWidth[]` array and `#ifdef`-guarded
control functions (`loaderControl()`, `excavatorControl()`, `steamLocomotiveControl()`).
The `ComBusSoundInterpreter` → `sound_core_set_*()` → `MixerState` stack is already
clean (3-layer); the legacy backend (`pulseWidth[]` bridge, `soundMapper[]`/`SoundChannel`,
and the mode control functions) is what remains to replace.

**Architecture decisions (arrêtées — ne pas remettre en question) :**

- `SoundDevice` (Flash, `constexpr`) + `SoundDevState` (RAM, allocated by `soundDevInit()`)
  replace `SoundChannel` and the hand-written `soundMapper[]` table.
- `SoundBehaviorFn` (Option B — function pointer per device) signature:
  ```cpp
  using SoundBehaviorFn = void (*)(const ComBus*, const SoundDevice*, SoundDevState*, const SoundDevState* gateState);
  ```
- Gate device via `SoundCfgHdr::gateDevID` — **common header struct** pattern:
  `SoundCfgHdr { int8_t gateDevID; }` must be the **first field (offset 0)** in every
  cfg struct (`HydRampCfg`, `HydPumpCfg`, `TriggerCfg`). The interpreter reads
  `gateDevID` via `static_cast<const SoundCfgHdr*>(dev->cfg)->gateDevID` without
  knowing the concrete type. `static_assert(offsetof(T, hdr) == 0)` enforces the
  layout in `sound_device_cfg.h`. Gate device must have a lower array index (-1 = no gate).
- `DevUsage` kept as readable discriminant (dashboard + behaviorFn hint).
- `cfg : const void*` → typed tune structs (`EngineCfg`, `HydPumpCfg`, `nullptr` for simple).
- `assetID : uint8_t` for PCM sample binding; `looping : bool` for loop vs. one-shot.
- Accessor/removable modules via `src_filter` + `extern const SoundDevice` additions.

```cpp
struct SoundCfgHdr { int8_t gateDevID; };  // must be first field (offset 0) in every cfg struct
struct SoundDevice {
    const int8_t                   ID;
    const char*                    infoName;
    DevUsage                       usage;        // dashboard + behaviorFn hint
    std::optional<AnalogComBusID>  analogChan;   // primary analog channel, or nullopt
    std::optional<DigitalComBusID> digitalChan;  // primary digital channel, or nullopt
    SoundBehaviorFn                behaviorFn;   // nullptr = simple passthrough
    const void*                    cfg;          // HydRampCfg* / HydPumpCfg* / TriggerCfg* / nullptr
                                                 // gate: static_cast<SoundCfgHdr*>(cfg)->gateDevID
    SoundDevState*                 state;        // RW runtime state; nullptr for passthrough
};
struct SoundDevState { uint16_t volume; uint16_t target; uint32_t lastUpdateMs;
                       bool active; uint16_t lastInput1; uint16_t lastInput2;
                       uint16_t internal[8]; int16_t loadFeedback; uint16_t knockExtra; };
```

**ComBus v2 migration note (winter 2026 — do not forget):**
- `analogChan` / `digitalChan` use `std::optional<AnalogComBusID>` / `std::optional<DigitalComBusID>`,
  same pattern as `SigDeviceCfg` in `machines_struct.h`.
- At ComBus v2 refactor: replace both fields with typed `GlobalSoundBus` accessors
  (e.g. `combus_get_analog(bus, dev->analogChan)`) — the optional fields become redundant
  once the bus API is typed.
- `HydPumpCfg::chanIDs[8]` (currently `uint8_t`) must also migrate to `AnalogComBusID[]`
  at the same time — the `uint8_t` raw index is a temporary workaround for the same reason.

**Migration path — by layer, in order:**
| # | Scope | What changes |
|---|---|---|
| 1 | `include/struct/sound_struct.h` | Add `SoundDevice` + `SoundDevState`; keep `SoundChannel` until all callers migrated |
| 2 | `config/profiles/dumper_truck/` | Replace `soundMapper[]` (6 `SoundChannel` entries) with `kSoundDevArray[]` (`SoundDevice`) |
| 3 | `system/sound_interpreter.cpp` | Iterate `kSoundDevArray[]`; call `dev.behaviorFn()` or passthrough |
| 4 | `main.cpp` | Remove `#ifdef LOADER_MODE loaderControl()` / `EXCAVATOR_MODE excavatorControl()` blocks — absorbed by behaviour fns |
| 5 | `main.cpp` | Remove `pulseWidth[]` consumer blocks (mapThrottle, escPulseWidth, loaderControl reads) once all replaced by ComBus → MixerState path |
| 6 | Legacy tab headers | `2_Remote.h`–`8_Sound.h` absorbed into `vehicle_legacy_cfg.h` per profile (dumper_truck already done); remaining profiles follow |
| 7 | `vehicles/*.h` (60+ files) | Keep frozen; new vehicles get a `config/profiles/<name>/` folder instead |

**Current residues (legacy, do not touch until migration starts):**
- `vehicles/*.h` — 60+ original rc_engine_sound vehicle presets (frozen, do not delete)
- `0_GeneralSettings.h`, `1_Vehicle.h`, `2_Remote.h`–`8_Sound.h` — legacy tab headers still included by `main.cpp`
- `pulseWidth[]` global array (1000–2000 µs) — bridge between `ComBusSoundInterpreter` and monolithic backend; removed once backend is replaced
- `soundMapper[]` / `SoundChannel` in `dumper_truck.cpp` — direct migration target for step 2 above
- `loaderControl()` / `excavatorControl()` / `steamLocomotiveControl()` — called from `loop()` under `#ifdef`; replaced by `SoundBehaviorFn` per device
- `escPulseWidth` / `escPulseWidthOut` local variables in `main.cpp` — legacy throttle bridge; replaced by `MixerState` ESC_SPEED_BUS slot

**Prerequisite:** DiYGuy external linkage migration (L2, Phase A–D above) must be complete first.
Phase D removes `sound_audio_task_setup()` and all DiYGuy includes from `main.cpp` — that is the
precondition for replacing the remaining FSM functions with `SoundBehaviorFn` per device.
The `SoundDevice` table-driven architecture is the step AFTER DiYGuy removal, not before.

---

### ComBus v2 — layered bus architecture (winter 2026 rework)
**Context:** The current ComBus is a single flat struct exchanged between the
machine node and the sound node.  The architecture decisions below were agreed
2026-04-18 and define the next ComBus rework.

**Architecture decisions (arrêtées — ne pas remettre en question) :**

```
ComBus Core (noyau)
  ├─ RunLevel, batteryIsLow, nodeId, sequence           ← obligatoire, échangé par TOUS
  │
  ├─ GlobalLightBus   { digital[], analog[] }           ← partagé inter-node (machine → sound)
  ├─ GlobalSoundBus   { digital[], analog[] }           ← partagé inter-node
  │     IDs définis dans les enums machine (dumper_truck_ids.h, etc.)
  │     Données INTER-NODE : IMPERATIVEMENT dans ce bus global
  │
  └─ LocalLightBus    { LightState, LedDescriptor[] }   ← local au sound node uniquement
      LocalSoundBus   { EngineSimState, ... }            ← local au sound node
          Jamais transmis sur le fil — API interne seulement
```

- **Règle dure** : toute donnée échangée entre remote ↔ machine ↔ sous-module
  doit résider dans le ComBus Core ou dans un GlobalXxxBus.  Les LocalXxxBus
  sont strictement privés au nœud.
- **Branchement conditionnel** : chaque module local garde un `const GlobalXxxBus*`
  pointant vers la section correspondante du ComBus reçu par UART.  Le pointeur
  est `nullptr` si le module est absent (guard `-D LIGHT_ENABLE`).
- **Ce mode de fonctionnement devient le template** pour tout nouveau module
  (ventilation, remorque, etc.) ajouté après la saison 2026.

**Prerequisite:** Refonte complète des structs ComBus + transport frame.
Ne pas commencer avant la fin de saison 2026.

**TRACTION_BUS packed format (décidé 2026-05-17) :**
Remplacement prévu de `ESC_SPEED_BUS` + `DRIVE_STATE_BUS` en un seul canal sémantique :
```
bit15  : BRAKE     — état freinant actif
bit14  : FWD       — direction avant (0 = arrière)
bit13-0: magnitude — vitesse 0..8191 (0 = à l'arrêt)
```
Décodeur : `mag = v & 0x1FFFu`, `fwd = v & 0x4000u`, `brake = v & 0x8000u`. Standing = 0x0000.
Tous les consommateurs de `ESC_SPEED_BUS` doivent être mis à jour simultanément.
Retirer `DriveStateBus` de `motion_struct.h` et remplacer par les helpers TRACTION_BUS.

---

### SimDev — ComBus inertia processor  (winter 2026 refactor)
**Context:** The current `esc()` FSM in `sound_module/main.cpp` (~400 lines) mixes
inertia simulation, a 4-state drive machine, and hardware dispatch into one function.
Architecture agreed 2026-05-16 separates these concerns cleanly.

**Core insight:** The inertia/FSM layer is a pure ComBus processor — it reads one
analog input channel, applies dynamics, and writes one analog output channel.  It
has no knowledge of hardware ports, pins, or device type.  This is the same
non-hardware pattern as `SigDevice` (hence the name analogy: `SimDev` ↔ `SigDev`).

**Architecture:**
```
ComBus[inCh]  ──►  sim_dev_update()  ──►  ComBus[outCh]
                   (inertia, 4-state        (read by srvDev/dcDev as a
                    drive FSM, gear-ramp,    normal analog channel)
                    failsafe, speed-limit)
```

**Hardware binding (separate concern):**
- Classic RC ESC wired to a servo port → `SrvDevice` with new `SrvDevType::ESC_RC`.
  Only hardware data in the descriptor (pulse range, pin) — no inertia or FSM logic.
  Behavior deduced from `SrvDevType` in the update loop, same pattern as other `*DevType` values.
- Brushless motor → future `BsDevice` with MCPWM backend (out of scope until needed).

**Struct proposals:**
```cpp
struct SimCfg {
    uint16_t rampTime[3];         // per-gear ramp ms (gear 1/2/3)
    uint8_t  brakeSteps;
    uint8_t  accelSteps;
    uint16_t neutralBand;         // ComBus units around CbusNeutral = STAND
    uint8_t  lowRangePct;
    uint8_t  autoReverseAccelPct;
    uint16_t crawlerRampTime;
};

struct SimState {
    uint16_t inertiaPos;          // current position in ComBus domain [0..CbusMaxVal]
    int8_t   driveState;          // 0=STAND 1=FWD 2=BRK_FWD 3=REV 4=BRK_REV
    bool     escInReverse;
    bool     escIsBraking;
    bool     escIsDriving;
    bool     brakeDetect;
    uint16_t currentRampTime;
    uint32_t lastUpdateMs;
};

struct SimDev {
    const int8_t       ID;
    const char*        infoName;
    AnalogComBusID     inCh;      // source channel (e.g. throttle ComBus channel)
    AnalogComBusID     outCh;     // output channel (read by the matching srvDev/dcDev)
    const SimCfg*      cfg;
    SimState*          state;     // mutable runtime
};
```

**EnvCfg addition:**
```cpp
SimDev*  simDev      = nullptr;
uint8_t  simDevCount = 0;
```

**Clone guard:** If two `SimDev` entries share the same `inCh`, the second is a no-op
(same input → same output already computed). Checked in `sim_dev_update()`.

**Migration path (winter 2026):**
1. Add `SrvDevType::ESC_RC` to `machines_defs.h`.
2. Define `SimCfg`, `SimState`, `SimDev` in `include/struct/machines_struct.h`.
3. Add `simDev` / `simDevCount` to `EnvCfg`.
4. `sim_dev_update(SimDev*, ComBus&, SimContext&)` extracted from `esc()` in `sound_module/main.cpp`.
5. `esc()` in `main.cpp` replaced by loop over `simDev[]`.
6. Static locals (`escPulseWidth`, `driveRampRate`, `escMillis`…) removed → `SimState`.
7. `hw_init_esc.cpp` TODO comment resolved; `esc_init()` pulse-range globals gated by `#ifdef ESC_OUTPUT_ENABLED`.

**Prerequisite:** Active season over. Do not start before winter 2026.

---

### SwitchDevice → EnvCfg  (winter 2026 refactor)
**Context:** `switch_dev` currently uses a board-level `SwitchPort*` interface
(same category as `VBatSense` and `LightPort`).  `SwitchPortCfg` contains GPIO
pin data — a board concern — so it cannot be placed in `EnvCfg` as-is.

**Planned split:**
- **Board side** — richer `SwitchPortCfg` replacing the current minimal struct:
  ```cpp
  struct SwitchPortCfg {
    const char* infoName;    // human-readable name
    int8_t      pin;         // GPIO (-1 = disabled)
    bool        pullUp;      // INPUT_PULLUP / INPUT_PULLDOWN
    bool        activeHigh;  // true = pressed == HIGH (new field)
    uint16_t    debounceMs;
  };
  ```
- **Machine side** — new `SwitchDevice` struct with a role/usage field (analogue
  to `DcDevice::DevType` / `SrvDevice::type`) and an optional `parentID` for
  switches that belong to a parent device (e.g. a limit switch on an actuator):
  ```cpp
  struct SwitchDevice {
    const int8_t ID;
    const char*  infoName;
    DevUsage     usage;           // SW_COUPLER, SW_LIMIT_DUMP, …
    int8_t       parentID = -1;   // parent DcDevice / SrvDevice ID (-1 = standalone)
  };
  ```
  Stored in `EnvCfg` alongside a `switchDev` / `switchDevCount` pair.
- `switchDevInit(const EnvCfg&, SwitchPort*, PinReg&)` when both sides exist.


### Simulation struct — sound/motion config unification (winter 2026 rework)
**Context:** Dynamic parameters (engine curves, ratios, dynamics) are currently duplicated in both sound and motion configs for each vehicle, leading to risk of drift and heavy maintenance.

**Problem:** Fixes or changes in one section (sound or motion) are not always reflected in the other, causing profile divergence and complexity when adding vehicles or evolving the physical model.

**Target solution:**
- Create a shared struct (e.g. `VehicleSimulationProfile`) containing all dynamic parameters common to sound and motion.
- Sound and motion modules reference this struct for shared fields.
- Module-specific parameters (audio samples, GPIO mapping, activation flags) remain in their respective configs.
- The struct is declared in a shared header (e.g. `simulation_struct.h`) and instantiated per vehicle.

**RPM-primary motion model (architectural decision 2026-05-14):**
The current model is speed-primary : `stick → targetPos → inertia on pos → currentPos → rpm (derived)`.  
The target model for winter 2026 is RPM-primary :
```
stick → targetRpm → inertia on rpm (engine mass) → currentRpm
                                                  → speed = currentRpm × gearRatio[gear]
                                                  → ComBus motor command
```
Motivation : RPM is the physically correct primary signal (throttle pedal = RPM request, not speed request);
gear ratios become meaningful (upshift drops RPM, maintains speed); `gear_fsm` receives `currentRpm` directly
with no artificial conversion; machine and sound nodes share the same simulation primitive.

Implementation delta vs. current state :
- `GearShiftProfile` gains `uint16_t gearRatio[gears]` (‰ of max speed, e.g. 300/500/700 for 3 gears).
- `MotionRuntime` : `currentRpm` becomes primary inertia variable ; `currentPos` derived via `rpm × ratio`.
- `motion_process()` / `motion_update()` : refonte complète — inertia on RPM, speed derived.
- `gear_fsm_update()` : **no change** — already receives `rpm` + `throttlePct`, returns `gear`.
- `machines/main.cpp` FSM call : remove artificial RPM conversion (`|pos − CbusNeutral| × maxRpm / CbusNeutral`).

**Timing:** Refactor planned for winter 2026, after sound/motion profiles and ComBus v2 are stabilized.

**Prerequisite:** Active season over. Do not start before winter 2026.
