# Coding Conventions (C/C++)

This document defines the semantic and structural rules for writing C/C++ code in this project.
It focuses purely on C++ logic, programming preferences, and naming choices that require human engineering and cannot be guessed by automated tools.

### Scope
- **Naming Rules:** Exact casing and nomenclature for variables, classes, and functions.
- **Architectural Preferences:** Compilation constraints, include ordering, and safety rules.
- **Code Logic:** Best practices for writing robust, maintainable, and uniform code.



## Language

- All comments, Doxygen documentation, and log messages are written in **English**.


## 1. Naming

| Element | Convention | Example |
|---|---|---|
| Classes / types | `PascalCase` | `TemplateModule`, `TemplateConfig` |
| `enum` types | `PascalCase` | `TemplateMode` |
| `enum` values | `UPPER_SNAKE_CASE` | `MODE_A`, `MODE_B` |
| Free functions (C module API) | `snake_case` | `pin_reg_init`, `combus_tx_init` |
| Class methods | `lowerCamelCase` | `setDuty`, `isEnabled` |
| `#define` macros | `UPPER_SNAKE_CASE` | `MAX_RETRIES` |
| `constexpr` constants | `PascalCase` | `kTractionRamp` |
| Structure fields / variables | `lowerCamelCase` | `pinRegEntry`, `pinEntryCursor` |

- Variable names should be explicit and readable, except for short loop indices (`i`, `j`, `k`).

## 2. File Structure



### Include Order

1. Corresponding header (in a `.cpp`).
2. Project headers.
3. External / library headers.
4. Standard C/C++ headers.

### Function Decomposition

Complex functions should be divided into logical steps representing distinct responsibilities.
Use nested steps only when a major phase benefits from additional decomposition.

```
+++
Include Dependencies

Source files should include their corresponding header first.

This helps detect missing dependencies and prevents accidental reliance on transitive includes.

+++
### Debug Logging
-> renvoi vers la doc/readme debug
Use subsystem-specific wrappers whenever a message belongs to an identified subsystem.

Examples:
- hw_log_*
- input_log_*
- sys_log_*
- combus_log_*

Use generic wrappers only for messages that do not belong to a specific subsystem.