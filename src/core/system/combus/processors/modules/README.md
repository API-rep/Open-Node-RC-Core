# ComBus Processor Modules

Specialized micro-environments — cohesive systems with FSM state and business logic.

Each module resides in its own subfolder (multiple files allowed).

## Distinction vs. Generic Processors

| **Processors** (`base/`, `input/`, `motion/`) | **Modules** (`modules/`) |
|-----------------------------------------------|--------------------------|
| Single responsibility, composable             | Complete system with FSM |
| 1 file, ~50–100 lines                         | N files allowed          |
| Reusable tools (ramp, bypass, center)         | Domain-specific logic (gear, trailer, winch) |
| No complex FSM state                          | Multi-cycle state (FSM, timing, accumulators) |

## Module Architecture

Example: `modules/gear/`

```
gear/
  ├─ gear_fsm.h / .cpp        — Pure FSM primitives (RPM → gear), no ComBus dependency
  ├─ cb_gear.h / .cpp         — 4 CbProc wrappers (sim_gear_fn, sim_apply_ratio_fn…)
  └─ (structs in include/struct/combus/processors/modules/gear_struct.h)
```

**Separation rule:**
- `*_fsm.h/.cpp` — Pure logic, unit-testable, no `ComBus&` nor `CbProc*`.
- `cb_*.h/.cpp` — CbProc wrappers calling FSM primitives, accessing ComBus.

## Current Modules

- **`gear/`** — N-speed virtual gearbox (FSM + RPM conversions + shift-delta)
- **`sound/`** (future refactor, winter 2026) — Sound engine with mixer, generators, effects

## When to Add a Module

Create a new module when:
- Multiple functions form a **cohesive system** (FSM + conversions + bridges)
- Runtime state is **complex** (not reducible to a simple counter or timer)
- The system has clear **business semantics** (transmission, hydraulics, winch…)

If a function is standalone and reusable → `processors/base|input|motion/`.
