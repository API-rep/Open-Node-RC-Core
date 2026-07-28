# Dispatcher Coding Conventions

Dispatcher files are the unique entry point selecting one implementation among several interchangeable backends.

Their purpose is **selection only**. They shall not contain implementation logic.

---

## 1. Dispatcher Pattern

Dispatchers shall follow the same structure.

```cpp
#pragma once

#if defined(FEATURE_BACKEND_A)

  #include "backend_A.h"

// #elif defined(FEATURE_BACKEND_B)
//   #include "backend_B.h"

#elif defined(FEATURE_NOP)

  #include "nop.h"

#else

  #error "No FEATURE_xxx backend selected."

#endif
```

Every dispatcher shall contain at least one commented `#elif` branch illustrating how to register an additional backend. This serves both as documentation and as a maintenance template.

---

## 2. Build Flags

Dispatcher selection shall use **independent boolean build flags**. These flags are provided to the compiler through the command line or the `platformio.ini` configuration.

Example:

```text
-D INPUT_PS4_DS4_BT
-D INPUT_MODULE_NOP
-D MACHINE_VOLVO_A60_H_BRUDER
```

Numeric comparisons shall not be used.

Forbidden:

```cpp
#if INPUT_MODULE == PS4_DS4_BT
```

Unknown identifiers are silently replaced by `0` by the C preprocessor, potentially producing incorrect matches without any compilation error.

Backend selection flags are intended **only** for dispatcher files.

If the selected backend exposes an optional feature, it shall define a dedicated capability flag for consumer code.

Example:

```cpp
// lipo.h

#if defined(VBAT_SENSE_LIPO)

#define HAS_VBAT_SENSING

...

#endif
```

Application code shall test **capabilities**, not backend implementations.

Preferred:

```cpp
#ifdef HAS_VBAT_SENSING
```

instead of:

```cpp
#if defined(VBAT_SENSE_LIPO)
```

Capability flags (`HAS_xxx`) shall always be defined by the backend itself, never by the dispatcher.

---

## 3. Backend Contract

Every backend selected by a dispatcher shall expose exactly the same public interface.

Consumers shall never know which backend is active and shall treat the data provided by the dispatcher as an opaque interface.

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

A nop backend is a valid backend exposing exactly the same public interface as every other implementation while intentionally providing no functional behaviour.

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

Application code shall include **only the dispatcher**.

Example:

```cpp
#include <config/inputs/inputs.h>
```

and **never**:

```cpp
#include "PS4_dualshock.h"
#include "nop.h"
```

Capability flags (`HAS_xxx`) are defined by the selected backend. Therefore, any source file testing a capability shall first include the corresponding dispatcher.

Correct:

```cpp
#include <config/vbat/vbat.h>

#ifdef HAS_VBAT_SENSING
...
#endif
```

Incorrect:

```cpp
#ifdef HAS_VBAT_SENSING
...
#endif
```

without first including the dispatcher, following #ifdef statement will fail due to HAS_VBAT_SENSING missing definition.

---

## 8. Extending a Dispatcher

Adding a new backend shall require only four operations:

1. Create the new backend header and source files.
2. Add a dedicated boolean build flag.
3. Register a new `#elif defined(...)` branch in the dispatcher.
4. Add the corresponding build flag in `platformio.ini`.

No existing backend should require modification.
