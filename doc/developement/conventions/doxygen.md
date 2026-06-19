# Doxygen Conventions

This document defines the Doxygen documentation conventions used throughout the project.

Formatting rules and object appearance are defined in `formatting.md`.

Placement and spacing rules are defined in `layout.md`.

---

## 1. General Principles

Doxygen documentation shall prioritise:

* readability;
* usefulness to the intended reader;
* concise wording;
* consistency across modules.

All Doxygen documentation is written in English.

Documentation should provide context that the code alone does not immediately convey.

---

## 2. File Documentation

Every source and header file shall start with a Doxygen file banner.

`@file` identifies the file and shall contain only the file name.

`@brief` provides a short description of the file.

`@details` describes the module as a whole when additional context is useful.

Typical topics include:

* module purpose;
* architectural role;
* ownership rules;
* lifecycle constraints;
* relationships between major components.

Implementation details shall not appear in file-level documentation.

Multi-line `@details` paragraphs should visually indent continuation lines.

Example:

```cpp
/*!
 * @file ESP32_8M_6S.h
 * @brief ESP32 motherboard configuration.
 *
 * @details Defines the hardware resources available on this board revision,
 *   including motor drivers, servo outputs, sensing channels and communication
 *   interfaces.
 */
```

### File-Level TODO

A `TODO:` section may appear in the file banner.

Use it only for planned module evolution.

Typical examples include:

* structural refactoring;
* future subsystem additions;
* architectural improvements.

Avoid implementation-specific TODOs in file banners.

Example:

```cpp
/*!
 * TODO:
 *   - Add sensing descriptors for future hardware revisions.
 *   - Migrate extension ports to the common descriptor registry.
 */
```

---

## 3. Header vs Source Documentation

Documentation serves different audiences depending on file type.

### Header Files (`.h`)

Header documentation describes the public contract.

Focus on:

* purpose;
* usage expectations;
* parameter meaning;
* return values;
* ownership rules;
* configuration matrices;
* lifecycle constraints.

The goal is to allow callers to use the API correctly without reading the implementation.

### Source Files (`.cpp`)

Source documentation describes implementation intent.

Focus on:

* algorithm flow;
* implementation choices;
* state transitions;
* timing considerations;
* non-obvious behaviours;
* maintenance guidance.

The goal is to allow maintainers to understand the implementation quickly.

---

## 4. `@brief` Writing Style

Every `@brief` follows the same principle:

```text
Role. One-sentence explanation of its contribution in context.
```

Rules:

* Start with a noun or short nominal phrase.
* Avoid verb-first descriptions.
* Explain why the element exists, not merely what it does.
* Avoid unnecessary jargon.
* Keep the entire `@brief` on a single line whenever practical.

Examples:

```cpp
/** @brief Upper travel limit selector. Use software margin when configured, otherwise hw hard stop. */

/** @brief Value clamper. Keep @p val within [@p lo, @p hi] to stay within travel limits. */
```

Avoid:

```cpp
/** @brief Returns the clamped value. */

/** @brief Saturate @p val within range. */
```

---

## 5. `@details` Usage

`@details` is expected by default.

It provides the information required to understand the documented element beyond its short description.

Typical contents include:

* rationale;
* operating modes;
* ownership expectations;
* constraints;
* decision rules;
* architectural context.

Usage constraints belong in `@details`, not in `@brief`.

`@details` may remain concise when little additional explanation is required.

Documentation depth shall remain proportional to complexity.

Typical guidance:

* trivial declarations → short descriptions;
* small helpers → concise `@details`;
* public APIs → complete caller-oriented documentation;
* non-trivial implementations → richer maintainer-oriented documentation.

The objective is to maximise clarity while avoiding documentation noise.

---

## 6. Sequential Functions

Functions implementing a sequence of logical steps shall reflect this organisation in their documentation.

`@details` should describe the sequence using the same numbering as the implementation comments.

Example:

```cpp
/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Checks in order:
 *   1. Mandatory pointers are valid.
 *   2. Algorithm selection is coherent.
 *   3. Hardware limits are consistent.
 *   4. Margins remain within valid bounds.
 *   5. Dead-band fits within effective limits.
 */
```

The numbering should match the `// N.` comments found in the function body.

This allows readers to switch between documentation and implementation without friction.

---

## 7. `@param` vs `///<`

Both provide contextual information, but they apply to different situations.

### `@param`

Use `@param` to document function parameters as part of the function contract.

`@param` explains what the caller must know when supplying the argument.

Document:

* meaning;
* units;
* ownership;
* valid ranges;
* nullability;
* special values;
* lifetime requirements.

Do not restate what is already obvious from the type.

If removing the `@param` does not lose information beyond the function signature, the `@param` is probably unnecessary.

`@param` is mandatory for public APIs and non-trivial documented functions.

It may be omitted for trivial helpers whose meaning is self-evident.

Example:

```cpp
/**
 * @brief Write a value to an analog ComBus channel.
 *
 * @param bus     Target ComBus instance receiving the update.
 * @param ch      Channel identifier.
 * @param val     New value to write.
 * @param caller  Identity checked against channel ownership.
 *
 * @return True if the write was accepted.
 */
bool combus_set_analog(ComBus& bus,
                       AnalogComBusID ch,
                       uint16_t val,
                       ChanOwner caller);
```

### `///<`

Use `///<` to document data members and fields directly at their declaration.

Typical use cases include:

* structure members;
* class members;
* enum values;
* `constexpr` constants;
* designated initializer fields.

`///<` documents the role of the declared element itself, independently of any function contract.

Examples:

```cpp
struct VBatSenseState {
    float voltage;       ///< Last averaged battery voltage (V).
    bool  disabled;      ///< True when the channel was found unpowered at init.
};
```

```cpp
enum class DevUsage : uint8_t {
    SIG_HORN  = 0x40,    ///< Audible horn or buzzer.
    SIG_LIGHT = 0x41     ///< Lighting output.
};
```

```cpp
inline constexpr Board boardCfg {
    .infoName     = "ESP32 board",   ///< Human-readable board name.
    .drvPort      = drvPortArray,    ///< Driver descriptor array.
    .drvPortCount = DRV_PORT_COUNT   ///< Number of configured drivers.
};
```

### Choosing Between Them

Use this simple rule:

* Function argument → `@param`
* Declared field or value → `///<`

Ask yourself:

> "Am I documenting how the caller uses this value?"

→ Use `@param`.

> "Am I documenting what this declared element represents?"

→ Use `///<`.

In short:

> `@param` documents usage. `///<` documents identity.

---

## 8. Doxygen Scope

Doxygen documents entities.

Typical entities include:

* files;
* functions;
* types;
* enums;
* structures;
* variables;
* descriptor objects;
* configuration arrays.

Doxygen should not be used solely to organise the visual structure of a file.

Use layout section separators and ordinary comments for that purpose.

Avoid documentation blocks whose only role is to introduce the next section of code.

---

## 9. Identifier References

Use Doxygen references whenever they improve clarity.

Typical examples include:

* functions;
* types;
* structure members;
* arrays;
* configuration objects.

Example:

```cpp
`uart_com_init()` claims one entry in the static `ports[]` registry.
```

References should support navigation and reduce ambiguity.

---

## 10. Avoid

Avoid documentation that merely restates the code.

Examples:

```cpp
/** @brief Gets the current value. */

/** @brief Sets the enabled flag. */
```

Avoid implementation walkthroughs in headers.

Avoid duplicating information already obvious from names and types.

Avoid maintenance instructions attached to entities when the information belongs to coding conventions instead.

Documentation should explain intent, context and constraints rather than narrating the obvious.

// EOF doxygen-conventions.md
