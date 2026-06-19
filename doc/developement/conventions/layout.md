# Layout Rules

This document defines how formatting elements are assembled and organised within files.

If `formatting.md` defines the visual vocabulary, this document defines the rules used to compose it, similarly to paragraphs and tabulations in a word processor.

This document intentionally avoids semantic C/C++ decisions. Those belong to `coding-conventions.md`.

Whenever practical, these rules should remain simple and compatible with automated formatting tools.

---

## 1. General Principles

The layout of a file shall prioritise:

* consistency;
* readability;
* predictable navigation;
* minimal visual noise.

---

## 2. Global Spacing

* Indentation uses tabs.
* Maximum line length is 120 columns.

Blank lines shall be used consistently:

* One blank line separates a Doxygen block from the code it documents.
* Two blank lines separate major logical blocks (e.g. functions, declarations, Doxygen+code groups).
* At least one blank line shall appear before and after a section separator.
* One blank line shall precede a code comment introducing a logical step.
* No blank line separates such comments from the code they describe.

Refer to the example files for representative spacing patterns.

---

## 3. Header File Layout (`.h`)

Recommended order:

```text
{file_banner}

#pragma once

{system_includes}
{project_includes}

{forward_declarations}

{declarations}

{eof_marker}
```

---

## 4. Source File Layout (`.cpp`)

Recommended order:

```text
{file_banner}

{system_includes}
{project_includes}

{definitions}

{eof_marker}
```

Source files should mirror the semantic order of their associated headers whenever practical.

---

## 5. Section Separators

Rules:

* Section separators delimit major themes within a file.
* At least one blank line shall appear before and after a separator.
* In practice, two blank lines will often precede a separator because the previous logical block already ends with its own separation.
* Consecutive separators are forbidden.
* Numbering is strongly encouraged when a natural progression exists.
* Numbering remains optional.

Visual form:

```cpp
// =============================================================================
// 1. CONFIGURATION
// =============================================================================
```

Refer to the example files for typical usage patterns.

---

## 6. Code Comments

Rules:

* Code comments immediately precede the code they describe.
* No blank line separates a comment from its associated code.
* One blank line shall precede a logical step comment.
* Sub-steps are limited to one nesting level.
* Step numbering may be omitted when a function naturally consists of a single logical phase.
* Step comments shall be indented exactly one tab deeper than the code block they describe.

Example:

```cpp
	  // Read current state
	readState();
```

Step identifiers may be referenced by Doxygen documentation describing algorithm flow.

Refer to the example files for complete step layouts.

---

## 7. Documentation Placement

Rules:

* Documentation immediately precedes its target.
* No unrelated code may separate them.
* Rich documentation uses Doxygen blocks.
* Compact Doxygen blocks may be used when extensive documentation would reduce readability.

Refer to `doxygen-conventions.md`.

---

## 8. Include Organisation

Rules:

* Separate system and project includes.
* Include annotations are encouraged whenever they improve understanding.
* Include comments explain purpose rather than repeating filenames.

Example:

```cpp
#include <PS4Controller.h>  // DualShock 4 Bluetooth protocol support.
```

---

## 9. Vertical Alignment

Vertical alignment is encouraged whenever it improves scanability.

Typical candidates include:

* structure members;
* container fields;
* enum values;
* designated initialisers.

Examples:

```cpp
const char* infoName;
uint8_t     count;
State*      state;
```

```cpp
UNDEFINED = 0,
STARTING  = 1,
RUNNING   = 2,
```

Alignment decisions shall favour readability over mechanical consistency.

---

## 10. Configuration Arrays

Rules:

* Preserve enum declaration order.
* Group entries according to the associated enum structure.
* Entry identification comments are encouraged.
* Compact and expanded entry layouts may coexist within the same file when readability benefits.

Example:

```cpp
// LX_STICK
{
	.infoName    = "Left X stick",
	.type        = RemoteComp::ANALOG_STICK,
	.minVal      = StickMinVal,
	.maxVal      = StickMaxVal,
	.isInverted  = false
},

// LY_STICK
{
	.infoName    = "Left Y stick",
	.type        = RemoteComp::ANALOG_STICK,
	.minVal      = StickMinVal,
	.maxVal      = StickMaxVal,
	.isInverted  = false
},
```

---

## 11. Enum Grouping

Long enumerations may be subdivided using enum group separators.

Example:

```cpp
// --- Traction (0x10–0x1F) ---
TRACT_WHEEL = 0x10,
TRACT_TRACK = 0x11,
```

Use only when meaningful categories improve readability.

---

## 12. Debug Log Placement

Debug statements follow the indentation level of the surrounding scope.

Example:

```cpp
if (ready)
{
	LOG_DEBUG("[HW] Initialised.");
}
```

Refer to `debug-logging.md`.

---

## 13. EOF Marker Placement

The EOF marker shall be the final non-empty line of the file.

No additional content may follow it.

Example:

```cpp
// EOF module.cpp
```
