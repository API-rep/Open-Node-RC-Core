# Coding Conventions (C/C++)

This document defines the semantic rules for writing C/C++ code in this project.

It focuses on programming practices, naming conventions and implementation choices that require human judgement and cannot be enforced by formatting tools alone.

### Scope

- **Naming Rules:** Exact casing and nomenclature for variables, classes and functions.
- **Programming Conventions:** Preferred ways of expressing logic and decomposing code.
- **File Organization:** High-level organization principles for different file categories.
- **Implementation Guidance:** Rules intended to improve readability, maintainability and consistency.

---

## 1. Naming

| Element                        | Convention          | Example                            |
| ------------------------------ | ------------------- | ---------------------------------- |
| Classes / types                | `PascalCase`        | `TemplateModule`, `TemplateConfig` |
| `enum` types                   | `PascalCase`        | `TemplateMode`                     |
| `enum` values                  | `UPPER_SNAKE_CASE`  | `MODE_A`, `MODE_B`                 |
| Free functions (C module API)  | `snake_case`        | `pin_reg_init`, `combus_tx_init`   |
| Class methods                  | `lowerCamelCase`    | `setDuty`, `isEnabled`             |
| `#define` macros               | `UPPER_SNAKE_CASE`  | `MAX_RETRIES`                      |
| `constexpr` constants          | `PascalCase`        | `SamplingDepth`, `AdcRefVoltage`   |
| Structure fields / variables   | `lowerCamelCase`    | `pinRegEntry`, `lastTickMs`        |

- Variable names should be explicit whenever practical.
- Short names such as `i`, `j` and `k` are acceptable for simple loop indices.
- Naming conventions defined here take precedence over examples found elsewhere in the repository.

---

## 2. File Organization

Different file categories serve different purposes and should remain focused on their intended responsibility.

### 2.1 Definition Files

Definition files describe shared concepts and identifiers.

Typical contents include:

- enums;
- aliases;
- constants;
- lightweight compile-time definitions.

Examples:

```text
include/defs/
```

---

### 2.2 Structure Files

Structure files define shared data layouts.

Typical contents include:

- configuration structures;
- runtime state structures;
- container structures.

Examples:

```text
include/struct/
```

Refer to `include/struct/README.md` for structure-specific design guidelines.

---

### 2.3 Configuration Files

Configuration files describe a particular environment without implementing behavior.

Typical contents include:

- board descriptions;
- machine declarations;
- feature selection;
- static wiring definitions.

Examples:

```text
src/*/config/
```

---

### 2.4 Runtime Modules

Runtime modules implement behavior.

Typical contents include:

- initialization routines;
- processing logic;
- drivers;
- services;
- module APIs.

Examples:

```text
src/core/system/
src/machines/system/
```

---

### 2.5 Umbrella Files

Umbrella files expose a simplified entry point by gathering and dispatching lower-level definitions.

Typical contents include:

- aggregated includes;
- configuration selectors;
- environment dispatch;
- compatibility layers.

Examples:

```text
defs.h
struct.h
config.h
```

---

## 3. Logical Decomposition

Non-trivial functions should be decomposed into explicit logical steps when doing so improves readability and maintainability.

Step comments are intended to expose the reasoning flow of the implementation rather than describe obvious operations.

Prefer:

- meaningful phases;
- clear transitions between responsibilities;
- small coherent processing stages.

Avoid excessive decomposition when it does not improve understanding.

Refer to the layout guidelines for the visual representation of steps.

---

## 4. General Recommendations

- Prefer clarity over cleverness.
- Keep functions focused on a single responsibility whenever practical.
- Favor explicit code over implicit side effects.
- Avoid unnecessary abstraction.
- Write code intended to be understood by both humans and tooling.

---

## 5. Compile-Time Definitions

- Prefer `constexpr` over `#define` for typed compile-time constants.
- Favor compile-time evaluation whenever possible.
- Keep definitions strongly typed.
- Reserve macros for cases where language constructs cannot be used.

---

## 6. Debug Facilities

A dedicated debug infrastructure exists for initialization diagnostics and runtime monitoring.

When diagnostics are required, use the project's debug facilities rather than introducing ad-hoc `Serial.print()` statements.

Refer to the dedicated debug documentation for logging conventions, APIs and dashboard integration.