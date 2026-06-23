# Machine Family Configuration

## Design Philosophy

Machine family configurations are **definitions** of a vehicle category.

They describe what exists and how components are connected. They act both as
reusable templates and as a common reference shared by multiple node
sub-projects for inter-node interactions such as ComBus communication.

They define:

- communication channels;
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

## Configuration Hierarchy

The machine configuration system exposes several entry points depending on the
level of information required by the consumer.

| Umbrella Header | Purpose |
|----------------|----------|
| `machine_config.h` | Imports the complete machine family configuration |
| `combus_types.h` | Imports the selected machine family's ComBus definition |
| `combus_ids.h` | Imports only the selected machine family's channel ID definitions |

All three headers ultimately reference the same machine family configuration but
provide different dependency levels to avoid unnecessary includes and prevent
cyclic dependencies.

---

# Machine Configuration Architecture

## 1. What This Directory Contains

Each subdirectory represents a **machine family** — a reusable vehicle
template.

```text
machines/
├── dumper_truck/              ← One machine family
│   ├── combus/
│   │   └── dumper_truck.h     ← ComBus channel definitions + arrays
│   ├── inputs_map/
│   │   └── inputs_map.h       ← Input device → ComBus routing
│   ├── motion/
│   │   └── motion.h           ← Drive-train and actuator config
│   └── dumper_truck_config.h  ← Family umbrella
└── (future families)
```

A machine family is **not** a single vehicle instance.

It is a reusable configuration pattern that can be instantiated on any
compatible hardware platform.

---

## 2. The Problem: Include Cycles with Struct Headers

### 2.1 The Symptom

`machines_struct.h` (in `include/struct/`) needs to use `AnalogComBusID` and
`DigitalComBusID` as field types:

```cpp
struct MachineCfg {
    // …
    std::optional<AnalogComBusID> tractionLeft;
    // …
};
```

The compiler only needs the **size** of the enum to lay out this structure —
not the actual enumerator values.

Without the type definition, however, the type remains unknown.

### 2.2 The Failed Solutions

**Attempt A — Include the full ComBus file**

```cpp
#include <core/config/machines/dumper_truck/combus/dumper_truck.h>
```

→ `dumper_truck.h` includes `<struct/combus_struct.h>` for the `ComBus` type.

→ `combus_struct.h` includes `<struct/machines_struct.h>` for machine
configuration structures.

→ Include cycle. Compilation fails.

---

**Attempt B — Split definitions into multiple files**

```text
combus.h          → enums + arrays + comBus
combus_types.h    → enum definitions only
combus_ids.h      → minimal ID-only definitions
```

Problems:

- Adding a new machine required updating multiple files.
- Parallel umbrella files tended to drift apart.
- Missing a dispatcher branch could silently break builds.

### 2.3 The Working Solution: Forward Declaration

C++11 allows forward declaration of scoped enums when the underlying type is
known:

```cpp
enum class AnalogComBusID  : uint8_t;
enum class DigitalComBusID : uint8_t;
```

This completely breaks the dependency cycle.

`machines_struct.h` no longer depends on machine configuration headers, while
the compiler still knows the size of both enums.

The actual enum values are resolved later when code includes the selected
machine family's ComBus definitions.

---

## 3. The Two-Section Pattern

Each machine family's ComBus definition file follows a two-section layout
controlled by `IS_MACHINE`.

```cpp
// =============================================================================
// 1. ENUM DEFINITIONS (always compiled)
// =============================================================================

namespace DumperTruck {

enum class AnalogComBusID : uint8_t {
    // ...
};

enum class DigitalComBusID : uint8_t {
    // ...
};

} // namespace DumperTruck

// =============================================================================
// 2. MACHINE-SIDE DATA (IS_MACHINE only)
// =============================================================================

#ifdef IS_MACHINE

#include <struct/combus_struct.h>

extern AnalogComBusArray [...];
extern DigitalComBusArray [...];
extern ComBus comBus;

#include <inputs_map/inputs_map.h>

#endif
```

### Why the `#ifdef`?

| Build target | `IS_MACHINE` defined | Compiled sections |
|--------------|----------------------|-------------------|
| Machine node | Yes | 1 + 2 |
| Remote node | No | 1 only |
| Sound node | No | 1 only |
| Struct headers | No | 1 only (via forward declaration) |

Section 2 introduces dependencies that would recreate the include cycle if they
became visible from shared structures.

The `IS_MACHINE` guard guarantees that only machine builds see runtime ComBus
objects and routing data.

---

## 4. The Namespace Strategy

### 4.1 Why Namespaces?

A remote build may communicate with multiple machine families in the same
translation unit:

```cpp
auto left = DumperTruck::AnalogComBusID::TRACT_LEFT;
auto boom = Excavator::AnalogComBusID::BOOM_LIFT;
```

Without namespaces, channel identifiers from different machine families would
collide.

### 4.2 Machine-Side Convenience

A machine build only targets one machine family at a time.

Requiring:

```cpp
DumperTruck::AnalogComBusID::TRACT_LEFT
```

everywhere would add unnecessary verbosity.

Umbrella dispatchers therefore expose the selected family through:

```cpp
using namespace DumperTruck;
```

allowing machine-side code to simply write:

```cpp
AnalogComBusID::TRACT_LEFT
```

while remote builds continue using fully qualified names.

### 4.3 The `combus_ids.h` Entry Point

`combus_ids.h` is the minimal ComBus entry point.

It provides channel ID definitions without importing runtime ComBus structures,
routing arrays, or machine-side declarations.

Typical usage:

```cpp
#pragma once

#if MACHINE == VOLVO_A60_H_BRUDER
    #include <core/config/machines/dumper_truck/combus/combus_ids.h>
    using namespace DumperTruck;
#endif
```

This keeps dependency chains minimal and avoids introducing include cycles.

---

## 5. Adding a New Machine Family

### Step 1 — Create the Family Directory

```text
src/core/config/machines/<family_name>/
```

### Step 2 — Create the ComBus Definition

```text
<family_name>/combus/<family_name>.h
```

Follow the two-section pattern:

- Section 1 → enum definitions
- Section 2 → machine-side runtime declarations under `#ifdef IS_MACHINE`

### Step 3 — Update Dispatcher Headers

Add a branch in:

- `machine_config.h`
- `combus_types.h`
- `combus_ids.h`

Example:

```cpp
#elif MACHINE == MY_NEW_MACHINE
    #include <core/config/machines/my_new_machine/my_new_machine_config.h>
```

```cpp
#elif MACHINE == MY_NEW_MACHINE
    #include <core/config/machines/my_new_machine/combus/my_new_machine.h>
    using namespace MyNewMachine;
```

```cpp
#elif MACHINE == MY_NEW_MACHINE
    #include <core/config/machines/my_new_machine/combus/combus_ids.h>
    using namespace MyNewMachine;
```

No additional umbrellas or parallel configuration trees should be required.

---

## 6. Configuration Domains

### ComBus

Reference communication layout for the machine family.

Defines:

- channel identifiers;
- channel types;
- communication contracts;
- expected data flow.

All compatible vehicles within the same family share the same reference ComBus
structure.

### Motion

Motion-related configuration associated with the machine family.

Typical examples include:

- drive train descriptors;
- steering models;
- transmission presets;
- actuator configuration;
- simulation parameters.

The current implementation also hosts CBProc chains used to derive and process
machine states.

This organization may evolve in the future. CBProc configurations are generic
enough to become a dedicated configuration domain if required.

### Input Mapping

Default mappings between user inputs and ComBus channels.

These mappings define how commands enter the system while remaining independent
from the execution logic.

### Sound

Optional sound-related definitions associated with the machine family.

These resources may be consumed by dedicated sound nodes or future integrated
implementations.

---

## 7. Key Invariants

| Invariant | Rationale |
|------------|------------|
| `IS_MACHINE` is defined only in `[env:machines]` | Prevents machine runtime data from leaking into remote or sound builds |
| Enum forward declarations use `: uint8_t` | Guarantees enum size without requiring the definition |
| `combus_ids.h` must remain dependency-light | Prevents include cycles and keeps shared structures isolated |
| `using namespace <Family>` belongs in dispatcher headers only | Keeps family definition files reusable and self-contained |
| One dispatcher branch per family | Keeps scaling predictable and maintenance straightforward |

---

## 8. Build Environment Reference

`IS_MACHINE` is provided through build flags:

```ini
[env:machines]
build_flags =
    -D IS_MACHINE
    -D MACHINE=VOLVO_A60_H_BRUDER
```

Remote and sound node environments intentionally omit `IS_MACHINE`.

Do not rename or remove this flag.

The two-section ComBus architecture depends on it.

---