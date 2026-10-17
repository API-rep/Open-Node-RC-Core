# Machine Family Configuration

## Design Philosophy

Machine family configurations are **definitions** of a vehicle category.

They describe what exists and how components are connected. They act both as
reusable templates and as a common reference shared by multiple node
sub-projects for inter-node interactions such as ComBus communication.

They define:

- communication channels (the ComBus contract, written as `.cb` files);
- subsystem descriptors;
- hardware mappings;
- routing and ComBus processing chains;
- reusable machine presets.

They do not implement runtime behavior.

Runtime behavior is implemented elsewhere by Runtime Engines, CBProc chains,
device processors, and subsystem execution code.

This separation helps keep configurations reusable, minimizes duplication
between machine variants, and allows execution logic to evolve independently
from machine definitions.

---

## 1. What This Directory Contains

Each subdirectory represents a **machine family** — a reusable vehicle
template.

```text
machines/
├── machine_type.h             ← Dispatcher: MACHINE_TYPE_* → <family>_config.h
├── combus_types.h             ← Entry point for core code needing the ComBus IDs
├── dumper_truck/              ← One machine family
│   ├── combus/*.cb            ← ComBus channels of the family (BASE scope)
│   ├── inputs/
│   │   ├── input.cb           ← Raw input acquisition channels
│   │   └── inputs_map/        ← Input device → ComBus routing
│   ├── light/                 ← Light channels (light.cb) and presets
│   ├── motion/                ← Drive-train and actuator config
│   ├── sound/                 ← Sound presets (see the roadmap, Machine / Sound convergence)
│   └── dumper_truck_config.h  ← Family umbrella
├── excavator/                 ← Stub (config skeleton + sound, not dispatched yet)
└── loader/                    ← Stub (config skeleton + sound, not dispatched yet)
```

A machine family is **not** a single vehicle instance. It is a reusable
configuration pattern that can be instantiated on any compatible hardware
platform. Vehicle instances (for example the Volvo A60H Bruder) live under
`src/machines/config/machines/`.

---

## 2. How a Family Is Selected

Selection happens in two stages:

```text
-D MACHINE_VOLVO_A60_H_BRUDER            (platformio.ini)
  └─► machines.h                          (src/machines/config/machines/)
        └─► volvo_A60H_bruder.h           defines MACHINE_TYPE_DUMPER_TRUCK
              └─► machine_type.h          (this directory)
                    └─► dumper_truck/dumper_truck_config.h
```

- `-D MACHINE_*` picks the vehicle instance.
- The instance header defines the class it belongs to, `MACHINE_TYPE_*`.
- `machine_type.h` dispatches on `MACHINE_TYPE_*` and pulls the matching
  `<family>_config.h`. Consumers include `machine_type.h` only, never a
  per-family path.

---

## 3. ComBus Definitions

The ComBus channels of a family are declared in `.cb` files, not in C++. The
**ComBus Builder** (`scripts/combus_builder/`) reads them at build time and
GENERATES the C++ headers and sources under `<build_dir>/combus_generated/`.
The SCons hook adds that folder to the include path, so the code includes the
generated files directly:

```cpp
#include <combus.h>              // channel arrays, `comBus`, all enums (generated)
#include <combus_ids.h>          // channel ID enums only (generated)
#include <combus_base_ids.h>     // BASE view enums only (generated)
```

For core code that only needs the ID enums, include
`core/config/machines/combus_types.h` (it includes `<combus_ids.h>`).

The `.cb` files of a family use the `BASE` scope: the standard profile of the
machine type, shared by every compatible node. Channels that belong to one
machine instance or to one board use the `SYSTEM` and `BOARD` scopes. See
`doc/configuration/cb_syntax.md` for the syntax and
`scripts/combus_builder/README.md` for the generator.

`combus.h` is unique in the project: it is always the generated file. The
former `core/system/combus/combus.h` (the `combus_init()` API) is now
`core/system/combus/combus_init.h`.

---

## 4. Include Cycles and Forward Declarations

### 4.1 The Problem

`machines_struct.h` (in `include/struct/`) needs `AnalogComBusID` and
`DigitalComBusID` as field types:

```cpp
struct MachineCfg {
    // …
    std::optional<AnalogComBusID> tractionLeft;
    // …
};
```

The compiler only needs the **size** of the enum to lay out this structure,
not the enumerator values. Including the full generated headers from shared
structure headers would couple them to the ComBus runtime and recreate include
cycles.

### 4.2 The Solution: Forward Declaration

C++11 allows forward declaration of scoped enums when the underlying type is
known:

```cpp
enum class AnalogComBusID  : uint8_t;
enum class DigitalComBusID : uint8_t;
```

Struct headers and core modules (`machines_struct.h`, `combus_defs.h`,
`combus_access.h`, the `hw/dev` headers, `light_interpreter.h`...) only forward
declare the enums. The enumerator values are resolved by the translation units
that include the generated headers.

The generated enums are emitted in the **global scope**, without a per-family
namespace.

---

## 5. Adding a New Machine Family

### Step 1 — Create the Family Directory

```text
src/core/config/machines/<family_name>/
```

### Step 2 — Declare the ComBus Contract

Write the family channels as `.cb` files under `<family_name>/combus/`, with
`scope: BASE` (see `doc/configuration/cb_syntax.md`). The builder discovers
every `.cb` file under `src/`; no registration is needed.

### Step 3 — Create the Family Umbrella

```text
<family_name>/<family_name>_config.h
```

Guard it with `#if defined(MACHINE_TYPE_<FAMILY>)` and include the sub-modules
of the family (motion, inputs, light, sound).

### Step 4 — Add a Branch to `machine_type.h`

```cpp
#elif defined(MACHINE_TYPE_MY_NEW_MACHINE)
  #include <core/config/machines/my_new_machine/my_new_machine_config.h>
```

### Step 5 — Create a Vehicle Instance

Under `src/machines/config/machines/<instance>/`: an instance header that
defines `MACHINE_TYPE_<FAMILY>`, and a branch for it in `machines.h`. Then add
the `MACHINE_*` build flag in `platformio.ini`.

Also declare the new type in the builder's `MACHINE_TYPE_FLAGS`
(`scripts/combus_builder/machine_type.py`) so channels under the family folder
are filtered by machine type.

---

## 6. Configuration Domains

### ComBus

Reference communication layout for the machine family: channel identifiers,
types, directions, expected data flow. All compatible vehicles within the same
family share the same reference ComBus structure.

### Motion

Motion-related configuration: drive train descriptors, steering models,
transmission presets, actuator configuration, simulation parameters.

The current implementation also hosts CBProc chains used to derive and process
machine states. This organization may evolve; CBProc configurations are
generic enough to become a dedicated configuration domain if required.

### Input Mapping

Default mappings between user inputs and ComBus channels
(`inputs/inputs_map/`). These mappings define how commands enter the system
while remaining independent from the execution logic.

### Light

Light channels and presets of the family.

### Sound

Optional sound-related definitions (engine sound profile of the family). They
are included only when `SOUND_ENABLED` is defined, which no environment does
yet. The former `SOUND_NODE` guard was removed on 2026-10-18 together with the
stand-alone sound environment. The integration of these resources belongs to
the "Machine / Sound convergence" work in `doc/roadmap.md`.

---

## 7. Key Invariants

| Invariant | Rationale |
|------------|------------|
| Consumers include `machine_type.h`, never a per-family path | Keeps every `MACHINE_TYPE_*` dispatch in one place |
| ComBus channels are declared in `.cb` files only | The generated C++ is rebuilt on every build and must not be edited or committed |
| `combus.h`, `combus_ids.h` and `combus_*_ids.h` are generated | They come from `<build_dir>/combus_generated/`, not from this tree |
| Shared structure headers only forward declare the ComBus ID enums (`: uint8_t`) | Guarantees the enum size without the definition and avoids include cycles |
| One branch per family in `machine_type.h` | Keeps scaling predictable |

---

## 8. Build Environment Reference

```ini
[env:machines]
build_flags =
    -D IS_MACHINE
```

```ini
[volvo_A60H_id]
build_flags =
    -D MACHINE_VOLVO_A60_H_BRUDER
```

(`[volvo_A60H_id]` is a shared section, referenced as
`${volvo_A60H_id.build_flags}` by the Volvo environments.)

`MACHINE_*` selects the vehicle instance (and so the family). `IS_MACHINE` no
longer guards any C++ code; the ComBus Builder hook only checks that the
structural flags (`IS_MACHINE`, `IS_REMOTE`) are present. Do not remove it.

---

## 9. History

Until August 2026 the ComBus definitions were hand-written per family
(`.inc` fragments assembled by `combus.h` / `combus_ids.h` dispatchers, enums
wrapped in a per-family namespace and a `using namespace <Family>` in the
dispatchers, and a two-section layout guarded by `#ifdef IS_MACHINE`). The
ComBus Builder (2026-08-22) replaced all of it with generated headers in the
global scope. The `.inc` fragments, the per-instance `combus.h` /
`combus_ids.h` redirection headers and the namespaces were removed (the last
cleanup is dated 2026-10-18). Older commits and documents use these names.
