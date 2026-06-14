# Basic Structure Conventions

This document defines the conventions used for simple data structures in Open RC Node.

It focuses on lightweight structures whose primary role is to organize data. More advanced patterns such as configuration/state containers are documented separately.

Refer to `include/struct/README.md` for subsystem-level structure design patterns.

---

## 1. Structures Are Primarily Data Containers

Basic structures exist to group related pieces of information together.

They should remain lightweight and focus on describing data rather than implementing behavior.

Prefer:

```cpp
struct Position {
    int16_t x;
    int16_t y;
};
```

Avoid turning simple structures into miniature modules containing substantial processing logic.

---

## 2. Lightweight Helpers Are Acceptable

Small helper functions deriving information directly from the stored fields are acceptable.

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

Helpers involving substantial processing, external interactions, ownership management, or subsystem behavior should instead be implemented outside of the structure entirely.

---

## General Philosophy

Basic structures should remain simple.

They describe what exists rather than how it operates.

When additional responsibilities emerge—runtime ownership, configuration aggregation, dispatch behavior, subsystem interactions—they should evolve into higher-level patterns documented elsewhere in the project.

Refer to `include/struct/README.md` for subsystem-level structure patterns such as configuration/state separation and top-level containers.
