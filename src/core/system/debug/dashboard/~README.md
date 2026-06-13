# Dashboard System

This directory contains the **Layer 1 core** of the ANSI terminal dashboard.

The dashboard provides a runtime view of the system through an interactive terminal interface. Unlike serial debug logs, which explain what the software is doing internally, the dashboard exposes what the system is doing right now.

---

## Philosophy

Open RC Node debugging is split into two complementary systems:

* **Initialization Logging**, used during startup and diagnostics;
* **Runtime Dashboard**, used to monitor the system while it is running.

The dashboard focuses on operational visibility.

It helps answer questions such as:

* What is the current state of the machine?
* Which modules are active?
* What values are changing?
* Are abnormal situations occurring?
* What happened recently?

---

## Dashboard Responsibilities

The dashboard is responsible for:

* Real-time system monitoring;
* Module status visualization;
* Interactive navigation;
* Detail inspection;
* Runtime event reporting.

It should expose runtime state without leaking application logic into the user interface.

---

## Architecture

The dashboard follows a layered architecture.

### Layer 1 — Dashboard Core

Located in this directory.

Responsibilities:

* Terminal frame rendering;
* Navigation bars;
* Slot registration;
* Detail registration;
* Keyboard handling;
* Event buffering;
* Rendering synchronization;
* Dashboard task execution.

Core components:

```text
dashboard.h       Public dashboard API
dashboard.cpp     Rendering engine and runtime core
debug.h           Bridge wrappers shared with serial logging
debug.cpp         Logging backend integration
```

---

### Layer 2 — Environment Dashboard

Responsibilities:

* Machine overview;
* Environment discovery;
* Module auto-detection;
* Slot registration.

Example:

```text
dashboard_machine.h/.cpp
```

---

### Layer 3 — Module Views

Responsibilities:

* Module-specific visualization;
* Direct access to monitored data;
* Detailed inspection screens.

Examples:

```text
dashboard_drv.h/.cpp
dashboard_input.h/.cpp
dashboard_vbat.h/.cpp
```

---

### Layer 4 — Initialization Logs

One-shot startup diagnostics.

Initialization traces should progressively migrate toward dedicated dashboard views whenever equivalent runtime monitoring becomes available.

---

## Build Flag

The dashboard is compiled only when:

```cpp
-D DEBUG_DASHBOARD
```

is enabled.

All public APIs expose no-op inline stubs when disabled, ensuring zero runtime overhead.

---

## FreeRTOS Task

The dashboard runs in its own dedicated task.

Characteristics:

* Pinned to **Core 0**;
* Priority **1**;
* Started after hardware initialization;
* Started after the operator pause block.

The canonical startup location is:

```text
src/machines/init/init.cpp
```

This ensures that the first render occurs only after the system is fully initialized.

---

## Rendering Ownership

The dashboard owns terminal rendering.

A dedicated render mutex protects frame integrity.

Serial diagnostics may coexist with dashboard rendering, but must never corrupt the displayed interface.

---

## Runtime Events

Runtime notifications must be published through:

```cpp
dashboard_push_event(...)
```

Examples include:

* RunLevel changes;
* Failsafe triggers;
* Exceptional conditions;
* Operator notifications.

The event ring buffer is the only runtime notification path.

Application code should never manipulate dashboard views directly.

---

## Integration Rules

### No dashboard code in application logic

Application files should remain dashboard-agnostic.

Files such as:

* `main.cpp`
* `loop()`
* domain modules

must not contain dashboard-specific rendering logic.

---

### Single compilation gate

Use only:

```cpp
-D DEBUG_DASHBOARD
```

Avoid introducing local compilation switches.

---

### Layer discipline

Responsibilities must remain separated:

* Layer 1: dashboard infrastructure;
* Layer 2: environment overview;
* Layer 3: module views;
* Layer 4: startup diagnostics.

---

### Uniform detail layouts

Conditional detail sections must preserve identical row counts.

Use placeholders such as:

```text
---
N/C
```

instead of hiding rows.

This avoids layout shifts during navigation.

---

## Debug vs Dashboard

Debug logging answers:

> "What is the software doing internally?"

The dashboard answers:

> "What is the system doing right now?"

Together, both systems provide complete visibility throughout startup, validation and operation.
