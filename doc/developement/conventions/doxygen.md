# Doxygen Conventions

This document defines the Doxygen documentation conventions used throughout the project.

Formatting rules and object appearance are defined in [`formatting.md`](formatting.md).

Placement and spacing rules are defined in [`layout.md`](layout.md).

---

## 1. General Principles

Doxygen documentation shall prioritise:

* readability;
* usefulness to the intended reader;
* concise wording;
* consistency across modules.

All Doxygen documentation is written in English.

Documentation explains intent, context and constraints: what the code alone does not immediately convey. It never restates what the names and types already say.

Doxygen documents entities: files, functions, types, structure members, enum values, constants, descriptor objects and configuration arrays. It does not organise the visual structure of a file: section separators and ordinary comments do that ([layout](layout.md), §5 and §6).

An identifier is written as in the code, so that it can be searched: a function with its parentheses (`uart_com_init()`), an array with its brackets (`ports[]`), a member with its owner when it is ambiguous (`UartCtx::rxCache`).

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

### Open Work

A file banner carries no `TODO` list. Planned evolutions and open points belong to the WIP of their work item (see `doc/WIP - Master.md`), where they are ordered and discussed.

In the code, a `TODO` marks one precise place only, and names the WIP that covers it, so that it can be found from both sides:

```cpp
		// TODO(Simulation chain): whether the processors work on the logical value.
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

The goal is to allow callers to use the API correctly without reading the implementation. A header does not walk through the implementation.

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

Each non-trivial function opens with a Doxygen block in the `.cpp`: its role in `@brief`, then in `@details` its steps, numbered as in the body (section 6), and the reasons for its choices. A reader gets the logic of a function at a glance, before reading its body. It explains how the contract of the header is carried out, without copying it. A trivial function takes a compact `///` line.

```cpp
/**
 * @brief Control frame reader. Validates the received bytes, then unpacks them.
 *
 * @details In order:
 *   1. Minimum length and SOF.
 *   2. Header, copied as is: its first two bytes are the layout of this frame.
 *   3. Declared sizes, against the 255-byte limit and the capacities in `cfg`.
 *   4. CRC over everything before it.
 *
 *   Nothing is written to `outputFrame` before the CRC passes: a refused frame leaves
 *   it untouched.
 */
```

Every statement in such a block is checked against the code when it is written: a wrong comment is worse than none.

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

It provides the information required to understand the documented element beyond its short description. Typical contents:

* **the steps**, numbered as in the body of the function (section 6);
* **the reasons for the choices**, and the obvious alternative that was rejected, when there is one;
* **the failure and absence cases**: what is left untouched when the input is refused, what `nullptr`, `std::nullopt` or `0` means, which safe value is taken;
* **the ownership and lifetime of the storage**: who allocates it, and how long it must live (static storage owned by the caller, for example);
* **the conditions of existence**: the `HAS_*` flags that enable the element, the end or side it applies to (TX or RX, socket or plug);
* **a short usage example** in `@code` ... `@endcode`, for the entry point of a module;
* **where to go next**: the module that takes over, the README of the folder.

Usage constraints belong in `@details`, not in `@brief`.

`@details` may remain concise when little additional explanation is required.

Documentation depth shall remain proportional to complexity:

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
 * @brief MotionConfig validator. Checks a configuration once, before its first use.
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

`@param` appears once, where the contract is declared: in the `.h` for a public function, never repeated in the `.cpp`; in the `.cpp` for a `static` function, which has no header.

Example:

```cpp
/**
 * @brief Transport: write the raw value of an analog channel, with the layer of a link.
 *
 * @param bus    Target ComBus.
 * @param ch     Channel.
 * @param val    New raw value.
 * @param layer  Layer of the link, checked against the layer of the channel.
 *
 * @return true if the write was accepted, false if the layer refused it.
 */

bool combus_link_set_raw_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanLayer layer);
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
