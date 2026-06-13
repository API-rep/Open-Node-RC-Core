# Core Architecture

This document explains how Open RC Node is organized and how responsibilities are distributed across the repository.

For an introduction to the ecosystem concepts themselves, refer to `ecosystem.md`.

---

## Design Principles

The repository follows a simple separation of responsibilities:

* `include/` and `src/core/` contain reusable elements shared by all nodes.
* Each node type (`machines`, `remotes`, extensions, ...) owns its specific implementation.
* Configuration defines how systems are assembled.
* Documentation should remain close to the code it describes.

---

## Repository Overview

```text
include/
    Shared definitions and structures

src/core/
    Reusable infrastructure and services

src/machines/
    Machine-specific implementations

src/remotes/
    Remote-specific implementations

doc/
    Concepts, guides and development conventions
```

---

## Shared Layer (`include/`)

The `include/` directory contains concepts shared across the entire ecosystem.

Examples include:

* Enumerations and identifiers;
* Shared structures;
* Generic containers;
* Common interfaces.

Refer to:

* `include/README.md`
* `include/defs/README.md`
* `include/struct/README.md`

---

## Core Configuration (`src/core/config/`)

Defines reusable configuration prototypes and defaults used to assemble systems.

Typical contents include:

* hardware-related definitions;
* machine and node prototypes;
* input and output configurations;
* sound and monitoring configurations;
* shared configuration helpers.

These files describe **what can be configured**, not how it behaves at runtime.

---

## Core Runtime (`src/core/system/`)

Implements the shared services used once the system is running.

Typical contents include:

* ComBus communication;
* debug and dashboard systems;
* hardware services;
* input and output processing;
* sound services;
* monitoring utilities;
* system orchestration.

These modules provide the actual behavior reused across nodes.

---

## Node Configuration (`src/<node>/config/`)

Describes a node and its capabilities.

Typical contents include:

* board definitions;
* machine descriptions;
* mappings and selectors;
* node-specific configuration.

These files define **what the node is**.

---

## Node Initialization (`src/<node>/init/`)

Builds the runtime environment from the selected configuration.

Typical contents include:

* hardware setup;
* communication setup;
* module initialization;
* startup sequencing.

Initialization transforms static definitions into an operational system.

---

## Node Runtime (`src/<node>/system/`)

Contains the behavior executed once initialization has completed.

Typical contents include:

* control logic;
* runtime utilities;
* node-specific services;
* monitoring and diagnostics.

This layer implements **what the node does**.

---

## Documentation

Documentation exists at multiple levels:

* Conceptual documentation (`doc/`);
* Local module documentation (`README.md`);
* Source code documentation.

Documentation should remain as close as possible to the implementation it describes.

---

This organization separates reusable infrastructure from node-specific behavior while maintaining a clear distinction between configuration, initialization and runtime execution.
