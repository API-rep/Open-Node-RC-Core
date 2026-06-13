# Debug Logging System

The debug logging system provides textual diagnostics through the serial interface.

It is primarily intended for developers during bring-up, initialization and troubleshooting.

Unlike the dashboard, which exposes runtime state to the operator, debug logs explain what the software is doing internally.

---

## Philosophy

Open RC Node debugging is split into two complementary systems:

* **Initialization Logging**
* **Runtime Dashboard**

This document focuses on initialization logging and developer diagnostics.

Initialization logs remain available throughout runtime but are normally silent once startup is complete.

---

## Logging API

The logging backend relies on templates and compile-time filtering.

Dead branches are removed by the compiler using `if constexpr`, eliminating the need for explicit preprocessor guards around log calls.

---

### Generic Wrappers

Use these wrappers for messages without subsystem ownership.

```cpp
log_err(...);
log_warn(...);
log_info(...);
log_dbg(...);
```

---

### Module Wrappers

Use subsystem wrappers whenever possible.

```cpp
hw_log_err(...);
hw_log_warn(...);
hw_log_info(...);
hw_log_dbg(...);

input_log_*(...);

sys_log_*(...);

combus_log_*(...);
```

---

## Build Flags

Subsystem diagnostics are controlled through shared flags.

```text
DEBUG_HW
DEBUG_INPUT
DEBUG_SYSTEM
DEBUG_COMBUS
DEBUG_ALL
```

Global verbosity is controlled through:

```text
LOG_LEVEL
```

Available levels:

```text
LogNone
LogError
LogWarn
LogInfo
LogDebug
```

---

## Logging Rules

* Prefer subsystem wrappers whenever ownership is clear.
* Reserve generic wrappers for cross-cutting messages.
* Prefer concise, human-readable wording.
* Include technical details only when they improve diagnostics.
* Avoid excessive runtime verbosity.

---

## Prefix Convention

Every log line should start with a stable module identifier.

Format:

```text
[MODULE] Message
```

Examples:

```text
[INPUT] PS4 controller setup started
[HW] Servo initialization complete
[COMBUS] Invalid channel index
[SYSTEM] RunLevel changed to READY
```

---

### Sub-modules

Additional context may be provided through sub-modules.

Format:

```text
[MODULE][SUBMODULE] Message
```

Examples:

```text
[HW][DRV] Driver wakeup sequence started
[INPUT][WATCHDOG] Signal timeout detected
```

Sub-module names should remain:

* Short;
* Uppercase;
* Stable over time.

---

## Severity

ANSI colors should be preferred when terminal support is available.

Recommended conventions:

* Default color: informational messages;
* Yellow: warnings;
* Red: errors.

Fallback textual markers may be used:

```text
[WARN]
[ERROR]
```

---

## Recommended Sub-modules

### INPUT

* PS4
* MAP
* WATCHDOG

### COMBUS

* AN
* DG
* SYNC

### HW

* DRV
* SRV
* CFG

### SYSTEM

* BOOT
* STATE
* LOOP

This registry exists to maintain a consistent vocabulary throughout the project.

---

## Initialization Output Patterns

Initialization traces should follow a predictable structure.

Recommended field order:

1. Title;
2. Parent or board information;
3. Configuration;
4. Pins;
5. Runtime limits;
6. Associated ComBus channels.

Inherited values should be explicitly marked.

Example:

```text
[HW][DRV] DC_DEV #6 - trailer rear left motor
  > Board Port: M-3B (ID:6) | Parent: DC_DEV #1 (CLONE)
  > Config: Freq:16000 Hz [INHERITED]
  > Pins: PWM:0 BRK:15 EN:33 SLP:25 FLT:34
  > Max Speed FW:100.0% | BK:100.0%
  > Com Ch: THROTTLE (ID:1)
```

---

## Dashboard Interaction

Debug logs and the dashboard coexist but serve different purposes.

Debug logging owns:

* Startup diagnostics;
* Developer traces;
* Internal explanations.

The dashboard owns:

* Runtime monitoring;
* Operator visibility;
* Interactive inspection.

Both systems should evolve together while remaining decoupled from application logic.
