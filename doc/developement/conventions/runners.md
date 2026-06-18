# Runtime Engines

This document introduces the concept of **Runtime Engines**, the execution mechanisms used throughout Open RC Node when configured data alone is no longer sufficient.

Runtime Engines provide behaviour while preserving the project's data-first philosophy.

---

## From Data to Execution

Open RC Node is built around shared structures defined and instantiated through configuration files.

At some point, execution becomes necessary to initialize the environment and/or drive the system at runtime.

This is where Runtime Engines appear.

```text
Structure definitions
        ↓
Configured subsystem data
        ⇅
Runtime Engines
```

Execution therefore does not replace structures.

It builds upon them.

---

## What Is a Runtime Engine?

A Runtime Engine is the execution layer responsible for animating configured data.

It does not define the data itself.

Instead, it receives configured subsystem data and applies behaviour to it.

A Runtime Engine occupies a role conceptually similar to a lightweight execution object.

Unlike traditional classes, however, it operates on externally owned structures rather than encapsulating the data it manipulates.

This preserves:

* explicit ownership;
* predictable memory placement;
* loose coupling between subsystems;
* traceable execution flow.

---

## Anatomy of a Runtime Engine

A Runtime Engine interacts with several categories of data without owning them.

The structures already exist as configured subsystem data. The Runtime Engine simply communicates with them.

```text
┌─────────────────────────────┐
│ Configured Subsystem Data   │
│ (defined in config files)   │
│                             │
│ • cfg                       │
│ • dynCfg (optional)         │
│ • state                     │
└─────────────────────────────┘
              ⇅
      ┌───────────────┐
      │ Runtime Engine│
      │               │
      │  init()       │
      │  update()     │
      │  ...          │
      │               │
      │ Runner        │
      │ Internal Data │
      └───────────────┘
```

The double arrow intentionally represents **interaction rather than ownership**.

Runtime Engines temporarily operate on subsystem data while remaining independent from it.

Only their internal execution bookkeeping belongs to the Runtime Engine itself.

---

## Runtime Engines Coordinate Rather Than Own

Runtime Engines should generally be understood as coordinators.

They connect execution logic to existing subsystem data rather than encapsulating that data within execution objects.

Their role is therefore to:

* interpret configuration;
* update shared state;
* maintain private execution bookkeeping;
* coordinate behaviour locally.

They should not become containers responsible for defining or owning the subsystem itself.

---

## Interaction with Shared Data

Runtime Engines may interact with several categories of subsystem data:

* **Static configuration (****`cfg`****)**
* **Dynamic configuration overrides (****`dynCfg`****)**
* **Shared subsystem state (****`state`****)**

Most Runtime Engines only use a subset of these categories.

Their detailed purpose and implementation conventions are documented separately.

---

## Persistent and Transient Execution

Runtime Engines may be used in two different ways depending on their needs.

* **Persistent execution**

  The Runtime Engine is instantiated once, optionally initialized, then reused throughout the subsystem lifetime.

  ```text
  init()
      ↓
  update()
      ↓
  update()
      ↓
  ...
  ```

  This approach is typically used when the engine maintains private execution bookkeeping between calls.

* **Transient execution**

  The Runtime Engine does not maintain persistent execution data and behaves as a specialized execution routine operating directly on configured subsystem data.

  ```text
  update(&subsystem)
  ```

  This approach is suitable when no private runtime context is required.

Both approaches follow the same philosophy: execution remains separate from the configured subsystem data it manipulates.

---

## When to Introduce a Runtime Engine

Introduce a Runtime Engine when:

* a subsystem needs ordered execution logic;
* a structure starts requiring behaviour in addition to data;
* a small dedicated class would naturally be the object-oriented solution.

Do not introduce a Runtime Engine when:

* a simple structure with lightweight helpers is sufficient;
* the logic can be expressed as a standalone function;
* the pattern would evolve into a framework or generic execution layer.

Refer to the coding conventions and implementation examples for practical guidance on applying these principles.
