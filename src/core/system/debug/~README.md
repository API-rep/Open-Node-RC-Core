# Debug — Dashboard System

This directory contains the **Layer 1 core** of the ANSI terminal dashboard.  
The full four-layer architecture is documented in `.github/copilot-instructions.md`
under *Dashboard architecture*.

---

## Debug Architecture

Open RC Node debugging is split into two complementary systems:

### Initialization Logging

Used during hardware and system startup.

This layer provides categorized serial logs controlled through the shared debug flags (`DEBUG_HW`, `DEBUG_SYSTEM`, `DEBUG_COMBUS`, etc.). Its primary purpose is to expose initialization status, configuration information and startup diagnostics.

Initialization logs remain available throughout runtime but are normally silent once the system is fully initialized.

---

### Runtime Dashboard

Used for live monitoring and diagnostics while the system is running.

The dashboard provides:

- Real-time system monitoring
- Module status views
- Runtime event reporting
- Interactive navigation and inspection

Runtime information should be surfaced through dashboard views and the event system rather than through continuous serial output whenever possible.

---

Together, these two systems provide complete visibility into both system startup and runtime operation while remaining decoupled from application logic.

---

## Directory map

```
src/core/system/debug/
  dashboard.h       Public API: setup, update, register_slot, register_detail,
                    detail_index, push_event, serial_trylock/unlock, start_task
  dashboard.cpp     Frame primitives (dPre/dTop/dMid/dBot/dLine/dMidLabel),
                    slot/detail tables, nav bars, keyboard (incl. ANSI arrows),
                    event ring buffer, FreeRTOS render mutex, task entry point
  debug.h           Shared log macros (sys_log_info, sys_log_dbg …) and
                    bridge wrappers (_dash_push_log, _dash_serial_trylock/unlock)
  debug.cpp         log_impl backend — pushes to ring buffer; skips Serial write
                    if the render mutex is held (non-blocking trylock)

src/machines/system/debug/
  dashboard_machine.h / .cpp   Layer 2 — env overview, module auto-detect, slot registration
  dashboard_drv.h   / .cpp     Layer 3 — DC-driver live table + per-driver detail
  dashboard_input.h / .cpp     Layer 3 — inputs / combus (future)
  dashboard_vbat.h  / .cpp     Layer 3 — battery sensing + per-channel detail
```

---

## Build flag

All dashboard code is compiled only when `-D DEBUG_DASHBOARD` is set in
`platformio.ini`. Every public symbol has a no-op inline stub in the `#else`
block of its header — zero overhead when the flag is absent.

---

## FreeRTOS task

`dashboard_start_task()` spawns a dedicated task **pinned to Core 0** (priority 1).

It must be called **after** all hardware initialization and the operator pause
block so the first render sees a fully initialized system.

The canonical call site is step 8 of `src/machines/init/init.cpp`.

The task owns a `SemaphoreHandle_t s_renderMutex`.

Any call to `Serial.print` from other tasks (via `log_impl`) performs a
non-blocking `xSemaphoreTake(..., 0)`:

- If the mutex is available, the message is written normally.
- If the mutex is held, the message is already stored in the dashboard event
  buffer and the Serial write is skipped.

This prevents terminal frame corruption while keeping logging non-blocking.

---

## Integration rules (for AI-assisted development)

> These rules apply to any Copilot / Claude Code session working on this module.

### 1. No dashboard code in application files

`main.cpp`, `machine_init()`, `loop()` and all domain logic files must contain
zero dashboard-specific calls beyond the single
`dashboard_machine_setup()` performed during initialization.

---

### 2. Single compilation gate

Do not introduce per-file `#ifdef` variants.

Use only:

```cpp
-D DEBUG_DASHBOARD
```

Every public header must expose a no-op inline stub in its `#else` block.

---

### 3. Layer discipline

- Core (Layer 1): zero knowledge of application modules.
- Environment Dashboard (Layer 2): reads machine/remote state and registers views.
- Module Views (Layer 3): self-contained and responsible for their own data.
- Serial Init Logs (Layer 4): one-shot startup diagnostics that should gradually migrate to dedicated dashboard views when appropriate.

---

### 4. Uniform row height in detail views

All conditional states of a content section must produce the same number of rows.

Use placeholders such as:

```text
---
N/C
```

instead of hiding rows.

This prevents layout shifts while navigating between detail views.

---

### 5. Event ring buffer is the only runtime notification path

Modules that need to surface a runtime event (runlevel changes, failsafe triggers, etc.) must use:

```cpp
dashboard_push_event(...)
```

Direct dashboard manipulation from application code should be avoided.

---

### 6. Commit scope

Use:

```text
dashboard
```

Example:

```text
dashboard (fix): remove center label from detail nav bar
```