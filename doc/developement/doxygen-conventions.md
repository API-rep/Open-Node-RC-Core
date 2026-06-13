# Doxygen Conventions

This document describes the internal Doxygen documentation conventions of the project, with a particular focus on the difference in content and depth between `.h` files (caller perspective) and `.cpp` files (maintainer perspective).

---

## 1. File Header

- Every file starts with a short Doxygen block.
- `@brief` is optional: when omitted, Doxygen uses the first sentence/paragraph as the brief.
- `@details` is only used when it adds useful information.

```c
/******************************************************************************
 * @file template_module.h
 * Short module purpose.
 *
 * @details Optional: architecture role, constraints, usage scope.
 *****************************************************************************/
```

---

## 2. Doxygen Policy by File Type

### `.h` Files

- Use major sections to separate main parts.
- Use `///` for one-line API documentation or small declaration blocks.
- Use `///<` for members and inline getters/setters.
- Use `/** ... */` for class documentation or main declarations.

### `.cpp` Files

- Use `/** ... */` to describe behaviors of non-trivial functions (`@details`, `@param`, `@return` when relevant).
- Keep internal comments only where logic requires clarification.
- Leave **one empty line** between the Doxygen block and the code it documents.
- Between the end of a code block and the next Doxygen block:
  - **2 empty lines** by default.
  - **3 empty lines** if the previous code block exceeds 40 lines.

---

## 3. `@brief` Writing Style — Role First, Detail After

Every `@brief` (in `.h` or `.cpp`) follows the same pattern:

```
<Role noun/phrase>. <One-sentence explanation of what it contributes in context.>
```

Rules:

- Start with a **noun or short nominal phrase** naming the role, not a verb.
  - ✓ `Upper travel limit selector. Use software margin when configured, otherwise hw hard stop.`
  - ✗ `Returns the upper limit.`
- Only one sentence after the period — explain *why* this element exists in the module context, not just *what it does*.
- No technical jargon that a non-native English speaker would need to look up (`clamp` is acceptable, `saturate` is not).
- Fit on **a single line** — if not possible, the `@brief` is too long.

Examples:

```cpp
/** @brief Upper travel limit selector. Use software margin when configured, otherwise hw hard stop. */
/** @brief Lower travel limit selector. Use software margin when configured, otherwise hw hard stop. */
/** @brief Value clamper. Keep @p val within [@p lo, @p hi] to stay within travel limits. */
```

Anti-patterns to avoid:

```cpp
/** @brief Effective upper limit — margin when present, otherwise hw. */   // ✗ no role name, passive
/** @brief Returns the clamped value. */                                    // ✗ verb first, no context
/** @brief Saturate @p val within range. */                                 // ✗ jargon (saturate)
```

---

## 4. Doxygen Depth: `.h` vs `.cpp`

A function is documented **twice**: once for the caller, once for the maintainer.

### In `.h` — Caller Perspective (what and why)

- `@brief` on one line: role + purpose in one sentence.
- `@details` optional: algorithm selection table, pointer rules, mode matrix — anything that helps the caller configure the function correctly without reading the implementation.
- `@param` / `@return`: meaning of each parameter for the caller; units, null rules, ownership.
- No code structure, no step sequence, no internal variable names.

```cpp
/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Algorithm selection by pointer pattern (ramp XOR gear+inertia):
 *
 *   Mode         | ramp  | gear  | inertia
 *   -------------|-------|-------|--------
 *   Simple ramp  |  set  |  null |  null
 *   Traction     |  null |  set  |  set
 *
 *   Value checks: hw->maxHwVal > minHwVal, band within effective limits, ...
 *
 * @param cfg  Config to validate. Must not be null.
 * @return     True when valid. False + fatal log on first violation.
 */
bool motion_check(const MotionConfig* cfg);
```

### In `.cpp` — Maintainer Perspective (how and why it works)

- Same `@brief` as in `.h` — this keeps both files self-contained.
- `@details` **required for non-trivial functions**: numbered step sequence matching the `// N.` comments in the body. Reading this block alone should give a complete mental map of the function without opening the code.
- Technical details go here: pointer arithmetic, timing model, state machine transitions, sound engine hooks, etc.
- Goal: navigate the code at a glance, even for someone unfamiliar with the module.

```cpp
/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Checks in order, stops on the first failure:
 *   1. hw and band are non-null (mandatory for all modes).
 *   2. Algorithm coherence: ramp XOR (gear + inertia).
 *   3. hw->maxHwVal > hw->minHwVal.
 *   4. margin (when present) within hw limits, max > min.
 *   5. dead-band within the effective limits (margin or hw).
 */
bool motion_check(const MotionConfig* cfg)
{
	// ...
}
```

The numbered list reflects the `// N.` comments inside the function body — the reader can switch from documentation to code without friction.

---

## 5. `@brief` + `@details` for Sequential Functions

When a function is a sequential pipeline, the `@brief` stays on one line (role first) and the numbered sequence goes in `@details`:

```cpp
/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Checks in order, stops on the first failure:
 *   1. hw and band are non-null (mandatory for all modes).
 *   2. Algorithm coherence: ramp XOR (gear + inertia).
 *   3. hw->maxHwVal > hw->minHwVal.
 *   4. margin (when present) within hw limits, max > min.
 *   5. dead-band within the effective limits (margin or hw).
 */
```

---

## 6. `@details` in File Header (`.h` only) — Module Overview

The `@details` at file level (top of a `.h`) describes the module as a whole: its theme, design intent, architecture, and key concepts. This is the entry point for anyone opening the file for the first time.

- Focus on **responsibilities and relationships between components**, not implementation details.
- Structure in short prose paragraphs, one per topic, in natural reading order:
  1. **Paragraph 1 — Purpose / Theme**: what this module does and why it exists.
  2. **Paragraph 2 — Architecture / Concepts**: key types, ownership rules, patterns used (pointer pattern, registry, state machine...).
  3. **Paragraph 3 — Usage / Constraints**: who calls what, in what order, lifecycle rules.

Rules:

- Always use `` `backticks` `` for identifiers (functions, fields, types, arrays).
- No bullet lists — write prose sentences.
- Do not describe *how* the code works line by line; describe *what role it plays* and *who depends on it*.

Example:

```cpp
/**
 * @details `uart_com_init()` claims one entry in the static `ports[]` registry
 *   and fills its metadata, then returns a `NodeCom*` pointing into that entry.
 *
 *   Each `NodeCom` embeds a `ctx` pointer back to its `UartCtx`, so the
 *   port callbacks can retrieve the correct `HardwareSerial*` at runtime.
 *
 *   Because the registry is statically allocated, the returned `NodeCom*`
 *   remains valid for the lifetime of the program.
 */
```

---

## 7. Language

- All Doxygen documentation is written in **English**.

+++ (à ajouter/vérifier si présent)

### Header vs Source Documentation

Header documentation should describe the public contract:

* purpose;
* parameters;
* return values;
* usage expectations.

Source documentation should explain implementation choices and non-obvious behaviors.


// EOF doxygen-conventions.md
