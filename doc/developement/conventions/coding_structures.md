# Basic Structure Conventions

This document defines the coding conventions used when implementing structures in Open RC Node.

It focuses on the structure definitions themselves: field organization, lightweight helpers, and readability considerations.

Subsystem decomposition patterns (`cfg`, `state`, `dynCfg`) are described in `include/struct/README.md`.

---

## 1. Structures Describe Data

Structures primarily exist to organize related pieces of information.

They should focus on describing data rather than implementing behaviour.

```cpp
struct Position {
    int16_t x;
    int16_t y;
};
```

For subsystem structures, keep each responsibility explicit.

Prefer:

```cpp
struct MotorCfg {
    uint16_t minSpeed;
    uint16_t maxSpeed;
};

struct MotorState {
    uint16_t currentSpeed;
};

struct Motor {
    const char*       name;
    uint8_t           id;

    const MotorCfg*   cfg;
    MotorState*       state;
};
```

Avoid turning structures into miniature modules containing substantial processing logic.

Execution behaviour belongs to Runtime Engines.

Refer to `include/struct/README.md` for subsystem-level design patterns.

---

## 2. Lightweight Helpers Are Acceptable

Small helper functions deriving information directly from stored fields are acceptable.

Helpers should:

* remain focused on exposing information derived from the structure;
* avoid modifying the structure state;
* remain reasonably short and self-contained;
* be declared at the end of the structure.

### Single-expression helpers

When the implementation is trivial, it may remain directly inside the structure.

Example:

```cpp
struct Range {
    uint16_t min;
    uint16_t max;

    constexpr bool contains(uint16_t value) const noexcept {
        return value >= min && value <= max;
    }
};
```

### Multi-line helpers

When a helper spans more than a single expression, keep the structure declaration concise and move the implementation immediately below it.

Prefer:

```cpp
struct Range {
    uint16_t min;
    uint16_t max;

    constexpr bool intersects(const Range& other) const noexcept;
};

// Helper implementation
constexpr bool Range::intersects(const Range& other) const noexcept {
    return (min <= other.max) && (other.min <= max);
}
```

This preserves the readability of the structure while keeping type-specific helpers close to the type they belong to.

Helpers involving substantial processing, external interactions, ownership management, execution sequencing, or subsystem behaviour should instead be implemented outside of the structure entirely.

Those responsibilities belong to Runtime Engines or standalone functions.

---

## 3. Dynamic Configuration Overrides

Some configurations may support runtime overrides through an optional `dynCfg`.

When used, `dynCfg` should:

* reuse the same structure type as `cfg`;
* reside in RAM;
* preserve the original Flash-resident configuration;
* only be introduced when runtime adaptation is genuinely required.

Small configurations may be duplicated entirely:

```cpp
// Flash
const RampCfg rampCfg = {
    .rate   = 100,
    .target = 2000,
};

// RAM
RampCfg rampDynCfg = rampCfg;
bool rampDynActive = false;
```

Effective configuration is then resolved as:

```cpp
const RampCfg* eff = rampDynActive
                   ? &rampDynCfg
                   : &rampCfg;
```

For larger configurations, prefer separating immutable and mutable parameters into distinct structures so that only the truly dynamic subset consumes RAM.

Refer to `include/struct/README.md` for the rationale and memory considerations behind this pattern.
