# Coding Conventions (C/C++)

This document defines the semantic rules for writing C/C++ code in this project.

It focuses on programming practices, naming conventions, and implementation choices that require human judgement and cannot be enforced by formatting tools alone.

### Scope

* **Naming Rules:** Exact casing and nomenclature for variables, classes, and functions.
* **Programming Conventions:** Preferred ways of expressing logic and decomposing code.
* **Architectural Preferences:** Include ordering and general coding practices.
* **Implementation Guidance:** Rules intended to improve readability, maintainability, and consistency.

---

## 1. Naming

| Element                       | Convention         | Example                            |
| ----------------------------- | ------------------ | ---------------------------------- |
| Classes / types               | `PascalCase`       | `TemplateModule`, `TemplateConfig` |
| `enum` types                  | `PascalCase`       | `TemplateMode`                     |
| `enum` values                 | `UPPER_SNAKE_CASE` | `MODE_A`, `MODE_B`                 |
| Free functions (C module API) | `snake_case`       | `pin_reg_init`, `combus_tx_init`   |
| Class methods                 | `lowerCamelCase`   | `setDuty`, `isEnabled`             |
| `#define` macros              | `UPPER_SNAKE_CASE` | `MAX_RETRIES`                      |
| `constexpr` constants         | `PascalCase`       | `kTractionRamp`                    |
| Structure fields / variables  | `lowerCamelCase`   | `pinRegEntry`, `pinEntryCursor`    |

* Variable names should be explicit and self-descriptive whenever practical.
* Short names such as `i`, `j`, and `k` are acceptable for simple loop indices.
* Naming conventions defined in this document take precedence over examples found elsewhere in the repository.

---

## 2. Include Order

Source files should organize includes in the following order:

1. Corresponding header file;
2. Project headers;
3. External or third-party library headers;
4. Standard C/C++ headers.

Example:

```cpp
#include "module.h"

#include "core/system/debug/debug.h"
#include "core/system/combus/combus.h"

#include <Arduino.h>

#include <cstring>
#include <cstdint>
```

---

## 3. Shared Structures

Structures located under `include/struct` and follow additional design principles specific to Open RC Node.
Design for a separation between configuration, runtime state, and their aggregation through top-level containers.

Refer to `include/struct/README.md` for the complete structure design guidelines.



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

+++ (en vrac, à remanier)
### Constants & Flash Storage
- **Compile-Time Constants**: Global constants and configuration values fixed at compile time must strictly use the `constexpr` keyword instead of `#define` macros.
- **Flash Memory Optimization**: For numeric values, configurations, and lookup tables, `constexpr` must be preferred to guarantee that compiler evaluations happen at compile time and that data is stored directly in Flash memory rather than RAM.

---

## 5. Debug Logging

A dedicated debug infrastructure exists for initialization diagnostics and runtime monitoring.

When diagnostics are required, use the project's debug facilities rather than introducing ad-hoc `Serial.print()` statements.

Refer to the Debug documentation for logging conventions, available APIs, and dashboard integration.
