# Structure Design

This directory contains the shared structures used throughout Open RC Node.

These structures act as common contracts between the different environments of the project: machines, remotes, dashboard views, services, simulations, and future extensions. They define how data is organized and exchanged, independently from the modules that process it.

The goal is to keep these definitions lightweight, explicit, and reusable. Shared structures should describe *what exists* and *how pieces relate to each other*, without embedding execution logic or subsystem behaviour.

Because of their declarative nature, they are widely used throughout node definitions, configuration files, communication contracts, and other shared interfaces.

The following patterns represent the preferred way of designing structures within Open RC Node.

---

## Structures Organize Subsystems

A shared structure generally represents a coherent subsystem or concept.

It gathers the information required to describe that subsystem while remaining purely declarative.

Typical examples include:

- a battery monitor;
- a lighting controller;
- a motor description;
- a sound profile;
- a communication endpoint.

The top-level structure should expose the subsystem as a whole.

When appropriate, it may internally separate its data into dedicated substructures.

Typical metadata describing the subsystem itself usually belongs at this level:

- `name` — human-readable label;
- `id` — numeric identifier;
- `category` — logical grouping;
- `version` — schema version.

These fields describe *what the subsystem is*, independently from how it operates.

---

## Configuration–State Separation

When a subsystem contains both definitions and mutable execution data, these responsibilities should be explicitly separated.

A subsystem is generally decomposed into two complementary layers:

### Configuration (`cfg`)

Immutable definitions describing how the subsystem behaves.

Configuration is typically provided by:

- board files;
- machine definitions;
- presets;
- shared references.

It usually resides in Flash memory.

Configuration should contain behavioural parameters such as:

- limits;
- thresholds;
- gains;
- timings;
- tuning values.

Example:

```cpp
struct MotorCfg {
    uint16_t minSpeed;
    uint16_t maxSpeed;
};
```

---

### Runtime State (`state`)

Mutable data evolving during execution.

State is owned by the subsystem and typically resides in RAM.

It contains information such as:

- counters;
- accumulated values;
- timers;
- cached computations;
- current measurements.

Example:

```cpp
struct MotorState {
    uint16_t currentSpeed;
};
```

---

Separating configuration from state improves:

- ownership clarity;
- memory placement;
- subsystem reusability;
- execution predictability.

It also allows the same subsystem definitions to be reused under different configurations without modifying execution logic.

The exact naming may vary when appropriate, but the distinction between immutable definitions and mutable state should remain clear.

---

## Structures Describe Data

Shared structures are intended to describe data rather than behaviour.

They should answer questions such as:

- What information exists?
- How is it organized?
- How do different pieces relate to one another?

They should not define:

- execution flow;
- subsystem orchestration;
- dispatch mechanisms;
- processing responsibilities.

When behaviour becomes significant enough to introduce concepts such as update cycles, sequencing, or local decision-making, it should evolve into a dedicated Runtime Engine rather than remain part of the shared structure model.

This distinction helps preserve the declarative nature of shared structures while keeping execution mechanisms explicit and localized.

---

## Structures as the Foundation of Execution

These structures are instantiated by node definitions, configuration files, or subsystem initialization code, then passed to the execution layer responsible for manipulating them.

Execution therefore does not replace structures.

It builds upon them.

```text
Structure definitions
        ↓
Configured instances
        ↓
Runtime Engines
```

This progression preserves a clear separation between:

- **what exists** (data definitions);
- **how it is configured** (instances);
- **how it behaves** (execution).

As a result, the same structures may be reused by:

- configuration systems;
- dashboards;
- simulations;
- communication layers;
- Runtime Engines;

without duplication.

---

## Dynamic Configuration Overrides

Some Runtime Engines may introduce an optional **dynamic configuration override** (`dynCfg`).

A `dynCfg` uses the same structure type as the corresponding static configuration (`cfg`), but resides in RAM and temporarily takes precedence during execution.

Its purpose is to allow selected parameters to evolve at runtime without modifying the original Flash-resident configuration.

```text
cfg (Flash)
    ↓
Runtime Engine
    ↓
dynCfg (RAM, optional)
```

When present, the effective configuration becomes:

```cpp
const FooCfg* eff = dynCfg ? dynCfg : cfg;
```

Because `dynCfg` duplicates all or part of the original configuration in RAM, its memory impact should be carefully considered.

For small configurations, duplicating the entire structure is often acceptable.

For larger configurations, it may be preferable to separate immutable and mutable parameters into distinct structures, allowing only the truly dynamic portion to be instantiated in RAM.

```text
Small cfg
    ↓
Duplicate entirely

Large cfg
    ↓
cfg       → immutable data (Flash)
dynCfg    → mutable subset only (RAM)
```

Dynamic configuration overrides are therefore considered an execution concern introduced by Runtime Engines rather than a fundamental property of shared structures.