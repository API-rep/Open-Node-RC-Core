# Formatting Items

This document defines the visual elements used throughout the repository.

It acts as a reference lexicon describing the appearance of each element, similarly to fonts and styles in a word processor.

Rules describing how these elements are assembled are defined in `layout.md`.

---

## File Banner

**Usage:** File identification and file-level documentation.

```cpp
/*!****************************************************************************
 * @file  {filename}
 * @brief {one_line_description}
 *
 * @details {extended_description}
 *****************************************************************************/
```

See `doxygen-conventions.md`.

---

## Section Separator

**Usage:** Separate major themes within a file.

```cpp
// =============================================================================
// {SECTION_TITLE}
// =============================================================================
```

or

```cpp
// =============================================================================
// {N}. {SECTION_TITLE}
// =============================================================================
```

See `layout.md`.

---

## Step Comment

**Usage:** Identify logical phases within an implementation.

```cpp
// N. Step description
// N.M Sub-step description
```

See `layout.md` and `coding-conventions.md`.

---

## Inline Comment

**Usage:** Internal implementation notes.

```cpp
// Explanation of a local implementation detail.
```

Must not replace public documentation.

---

## Block Comment

**Usage:** General multi-line explanation not intended for Doxygen.

```cpp
/*
 * Additional explanation.
 */
```

---

## Doxygen Block Comment

**Usage:** Document an object requiring rich documentation.

```cpp
/**
 * @brief {short_description}
 *
 * @details {extended_description}
 */
```

See `doxygen-conventions.md`.

---

## Compact Doxygen Block

**Usage:** Document small objects requiring concise documentation.

```cpp
/** @brief Available run levels. */
```

Typical candidates include:

* short enums;
* small structs;
* trivial functions;
* simple `constexpr` helpers.

See `doxygen-conventions.md`.

---

## Doxygen Inline Comment

**Usage:** Document declarations inline.

```cpp
uint16_t rate;      ///< Units per second.
```

Preferred for public members and enum values.

See `doxygen-conventions.md`.

---

## Include Annotation

**Usage:** Explain why a dependency is required.

```cpp
#include <foo.h>  // Provides Bar and Baz.
```

---

## Enum Group Separator

**Usage:** Visually subdivide long monolithic enumerations.

```cpp
// --- Traction (0x10–0x1F) ---
```

Used only when meaningful categories emerge.

---

## Entry Identification Comment

**Usage:** Identify configuration entries whose meaning depends on enum ordering.

```cpp
// LX_STICK
{
    ...
},
```

Commonly used in configuration arrays.

---

## EOF Marker

**Usage:** Mark the intentional end of a file.

```cpp
// EOF {filename}
```

See `layout.md`.

---

## Debug Prefix

**Usage:** Reserved logging subsystem identifiers.

```text
[HW]
[COMBUS]
[SYSTEM]
```

See `debug-logging.md`.
