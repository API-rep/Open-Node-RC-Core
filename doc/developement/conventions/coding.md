# Coding Conventions (C/C++)

This document defines the semantic rules for writing C/C++ code in this project.

It focuses on programming practices, naming conventions and implementation choices that require human judgement and cannot be enforced by formatting tools alone.

### Scope

* **Naming Rules:** Exact casing and nomenclature for variables, classes and functions.
* **Programming Conventions:** Preferred ways of expressing logic and decomposing code.
* **File Categories:** Entry points to file-type specific conventions.
* **Implementation Guidance:** Rules intended to improve readability, maintainability and consistency.

---

## 1. Naming

| Element                       | Convention         | Example                            |
| ----------------------------- | ------------------ | ---------------------------------- |
| Classes / types               | `PascalCase`       | `TemplateModule`, `TemplateConfig` |
| `enum` types                  | `PascalCase`       | `TemplateMode`                     |
| `enum` values                 | `UPPER_SNAKE_CASE` | `MODE_A`, `MODE_B`                 |
| Free functions (C module API) | `snake_case`       | `pin_reg_init`, `combus_tx_init`   |
| Class methods                 | `lowerCamelCase`   | `setDuty`, `isEnabled`             |
| `#define` macros              | `UPPER_SNAKE_CASE` | `FEATURE_SIMULATION`               |
| `constexpr` constants         | `PascalCase`       | `SamplingDepth`, `AdcRefVoltage`   |
| Structure fields / variables  | `lowerCamelCase`   | `pinRegEntry`, `lastTickMs`        |

Guidelines:

* Prefer explicit names whenever practical.
* Short names such as `i`, `j` and `k` are acceptable for simple loop indices.
* These conventions take precedence over examples found elsewhere in the repository.

---

## 2. General Coding Principles

### Fixed-Width Types

Prefer architecture-independent types such as `uint8_t`, `uint16_t`, etc. instead of processor-dependent types such as `int`, `long` or `unsigned`.

Fixed-width types guarantee consistent layouts and value ranges across supported platforms.

---

### Meaningful Default Values

Provide default initialization only when a safe and meaningful state exists.

Typical examples include:

```cpp
bool enabled = false;
Foo* ptr = nullptr;
uint8_t count = 0;
```

Avoid arbitrary defaults that do not represent a valid initial state.

---

### Explicit Sentinel Values

Prefer explicit sentinel values over undocumented conventions.

Common project conventions include:

* `UNDEFINED` → valid type not configured;
* `NOT_YET_SET` → value not yet determined;
* `nullptr` → reference not assigned;
* `COUNT` → number of indexable entries.

---

### Compile-Time Constants

Avoid `#define` for typed constants.

Use `constexpr` instead.

Preferred usages include:

* numeric limits;
* protocol values;
* default thresholds;
* configuration defaults.

Example:

```cpp
constexpr int8_t StickMinVal = -127;
constexpr int8_t StickMaxVal = 127;
```

`#define` should be reserved for build-time feature selection driven by the build system.

Typical examples include:

```cpp
#if defined(FEATURE_SIMULATION)
    ...
#endif
```

or compiler flags passed through `-D`.

---

## 3. File Categories

Different file categories follow additional conventions specific to their purpose.

Refer to the dedicated documents when working within these domains.

### Structures

Describe shared data and relationships.

Used throughout configurations, communication contracts, dashboards, simulations and Runtime Engines.

* Structure concepts → `README.md`
* Structure conventions → `coding_structures.md`

### Configuration Files

Describe static definitions assembled to build a system.

* Configuration conventions → `coding_configs.md`

### Runtime Engines

Introduce localized execution behaviour operating on existing structures.

They combine shared data definitions with execution logic while preserving explicit ownership, memory placement and execution flow.

* Runtime Engine concepts → `runners.md`
* Runtime Engine conventions → `coding_runners.md`

### Umbrella Files

Select and assemble project variants through compile-time dispatch.

* Umbrella conventions → `coding_umbrella.md`

---

## 4. Logical Decomposition

Non-trivial functions should be decomposed into explicit logical steps when doing so improves readability and maintainability.

Step comments are intended to expose the reasoning flow of the implementation rather than describe obvious operations.

Prefer:

* meaningful phases;
* clear transitions between responsibilities;
* small coherent processing stages.

Avoid excessive decomposition when it does not improve understanding.

Refer to the layout guidelines for the visual representation of steps.

---

## 5. General Recommendations

* Prefer clarity over cleverness.
* Keep functions focused on a single responsibility whenever practical.
* Favor explicit code over implicit side effects.
* Avoid unnecessary abstraction.
* Write code intended to be understood by both humans and tooling.

---

## 6. Design Philosophy

The project follows a data-first approach.

The preferred progression is:

```text
Describe the data
        ↓
Instantiate the data
        ↓
Adapt behaviour through dynamic configuration
        ↓
Execute only what must be executed
```

Shared structures form the foundation of the system.

Configuration assembles those structures into concrete instances.

Runtime Engines are introduced only when execution behaviour becomes necessary.

Execution is therefore treated as the final layer added to bring data to life, rather than the starting point of the design.

---

## 7. Debug Facilities

A dedicated debug infrastructure exists for initialization diagnostics and runtime monitoring.

When diagnostics are required, use the project's debug facilities rather than introducing ad-hoc `Serial.print()` statements.

Refer to the dedicated debug documentation for logging conventions, APIs and dashboard integration.

// EOF coding.md
