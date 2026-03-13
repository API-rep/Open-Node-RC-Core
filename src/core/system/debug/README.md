# Debug — Dashboard System

This directory contains the **Layer 1 core** of the ANSI terminal dashboard.  
The full four-layer architecture is documented in `.github/copilot-instructions.md`
under *Dashboard architecture*.

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
`platformio.ini`.  Every public symbol has a no-op inline stub in the `#else`
block of its header — zero overhead when the flag is absent.

---

## FreeRTOS task

`dashboard_start_task()` spawns a dedicated task **pinned to Core 0** (priority 1).  
It must be called **after** all hardware init and the operator pause block so the
first render sees a fully initialised system.  The canonical call site is step 8
of `src/machines/init/init.cpp`.

The task owns a `SemaphoreHandle_t s_renderMutex`.  Any call to `Serial.print`
from other tasks (via `log_impl`) does a non-blocking `xSemaphoreTake(..., 0)`:
if the mutex is held the message is already in the ring buffer and the Serial
write is skipped, preventing frame corruption.

---

## Integration rules (for AI-assisted development)

> These rules apply to any Copilot / Claude Code session working on this module.

1. **No dashboard code in application files.**  
   `main.cpp`, `machine_init()`, `loop()` and all domain logic files must
   contain zero dashboard-specific calls beyond `dashboard_update()` (removed —
   now handled by the task) and the single `dashboard_machine_setup()` at the
   end of `machine_init()`.

2. **Single compilation gate.**  
   Do not introduce per-file `#ifdef` variants.  Use only `-D DEBUG_DASHBOARD`.
   Every header must expose a no-op inline stub in the `#else` block.

3. **Layer discipline.**  
   - Core (Layer 1): zero knowledge of any application module.  
   - Env dashboard (Layer 2): reads machine/remote state; calls `register_slot`.  
   - Module views (Layer 3): self-contained; read their own data directly.  
   - Serial init logs (Layer 4 / `*_debug.cpp`): one-shot text dumps; migrate
     to Layer 3 once the corresponding module view covers the same information.

4. **Uniform row height in detail views.**  
   All conditional states of a content section must produce the same number of
   rows.  Use `---` / `N/C` placeholders instead of skipping rows to prevent
   layout shift when navigating between detail sub-items.

5. **Event ring buffer is the only runtime notification path.**  
   Modules that need to surface a runtime event (runlevel change, failsafe …)
   call `dashboard_push_event()` — never inline dashboard calls.

6. **Commit scope for this module:** `dashboard`.  
   Example: `dashboard (fix): remove center label from detail nav bar`.
