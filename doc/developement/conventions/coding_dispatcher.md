# Dispatcher Coding Conventions

Dispatcher files are the unique entry point selecting one implementation among several interchangeable backends, typically definition and configuration files.

The purpose of a dispatcher is **selection only**. It shall not contain implementation logic.

---

## 1. Dispatcher Pattern

Dispatchers shall follow the same structure.

```cpp
#pragma once

#ifndef FEATURE_FLAG
  #error "No feature selected."
#endif

#if defined(FEATURE_BACKEND_A)

  #include "backend_A.h"

// #elif defined(FEATURE_BACKEND_B)
//   #include "backend_B.h"

#elif defined(FEATURE_NONE)

  #include "nop.h"

#else

  #error "Unsupported feature configuration."

#endif

// Shared interface declarations...
```

---

## 2. Boolean Build Flags

Dispatcher selection shall use **independent boolean build flags**. These flags are provided to the compiler through the command line or the `platformio.ini` configuration.

Example:

```text
-D INPUT_PS4_DS4_BT
-D INPUT_MODULE_NONE
```

Numeric comparisons shall not be used.

Forbidden:

```cpp
-D INPUT_MODULE == PS4_DS4_BT
```

This pattern introduces a silent C preprocessor bug by replacing unknown identifiers with `0`, potentially producing incorrect matches without any compilation error.

---

## 3. Backend Contract

Every backend selected by a dispatcher shall expose exactly the same public interface.

Consumers shall never know which backend is active and shall treat the data provided by the dispatcher as an opaque interface, regardless of which backend has been selected.

Conditional compilation in consumer code shall be avoided whenever possible.

---

## 4. Backend Compilation Guards

Backend implementation files shall be protected by the same build flag used by the dispatcher.

Both the header **and** the source file shall be guarded.

Example:

```cpp
#if defined(INPUT_PS4_DS4_BT)

// Backend declarations or implementation

#endif
```

This rule serves two purposes:

* **Safety**: prevents accidental compilation of multiple backends if a backend file is included directly or compiled unintentionally.
* **Lightweight builds**: ensures that unused backend code, lookup tables and static data are completely excluded from the final binary.

The dispatcher remains responsible for backend selection, while each backend protects itself against accidental misuse.

---

## 5. "nop" Backends

Optional subsystems should provide a dedicated **nop backend** rather than special-case code inside the dispatcher.

A nop backend represents a valid implementation exposing exactly the same public interface as every other backend while intentionally providing no functional behaviour.

Typical example:

```text
/core/config/inputs/
```

The dispatcher simply selects the nop backend like any other implementation.

---

## 6. Dispatcher Scope

Dispatchers should remain intentionally small.

They are limited to:

* backend selection;
* shared declarations common to every backend;
* interface composition structures when required.

Any backend-specific data, configuration tables or implementation details belong exclusively to the backend files.

---

## 7. Include Policy

Application code shall include only the dispatcher as the single entry point.

Example:

```cpp
#include <config/inputs/inputs.h>
```

and **never**:

```cpp
#include "PS4_dualshock.h"
#include "nop.h"
```

The dispatcher is the public contract.

Backend headers remain private implementation details.

---

## 8. Extending a Dispatcher

Adding a new backend shall require only four operations:

1. Create the new backend header and source files.
2. Add a dedicated boolean build flag.
3. Register a new `#elif defined(...)` branch in the dispatcher.
4. Add the corresponding build flag in `platformio.ini`.

No existing backend should require modification.
