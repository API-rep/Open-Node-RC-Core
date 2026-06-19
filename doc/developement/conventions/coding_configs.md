# Configuration Coding Conventions

This document defines the conventions used to describe configurable hardware resources, protocols and reusable subsystem instances.

The objective is to provide deterministic configuration structures that are easy to validate, iterate and maintain.

---

## 1. Configuration Descriptor Pattern

Configuration files describing hardware, protocols or configurable subsystems shall follow the **Configuration Descriptor Pattern**.

This pattern enables:

- batch processing;
- deterministic indexing;
- compile-time validation;
- support for multiple instances of the same device type;
- scalable extension of existing configurations.

Typical organisation:

```text
Header (.h)
───────────
Constants
↓
ID enum (+ COUNT)
↓
Configuration array declarations
↓
Top-level descriptor declarations
↓
Optional compile-time guards

Source (.cpp)
─────────────
Configuration array definitions
↓
Top-level descriptor definitions (if not constexpr)
↓
Optional compile-time guards
```

The header describes **what resources exist**.

The source file defines **how those resources are configured**.

---

## 2. Header vs Source Responsibilities

Configuration headers (`.h`) act as descriptors.

They declare:

- typed constants;
- ID enums;
- terminal `*_COUNT` values;
- configuration array declarations;
- top-level descriptor declarations;
- optional compile-time guards.

Configuration sources (`.cpp`) provide the actual configuration data.

They define:

- descriptor array contents;
- designated initialisers;
- board- or protocol-specific values;
- optional validation guards tied to those values.

This separation keeps declarations compact while isolating verbose configuration tables.

---

## 3. Header Contracts: COUNT and Array Declarations

Enums used to index configuration arrays shall expose a terminal `*_COUNT` entry.

Example:

```cpp
enum class AnalogInputDevID : uint8_t {
    LX_STICK,
    LY_STICK,
    RX_STICK,

    ANALOG_DEV_COUNT
};
```

The COUNT entry is part of the descriptor mechanism and shall not be treated as an ordinary enum value.

It shall be used to derive:

- array sizes;
- iteration bounds;
- compile-time checks.

Duplicating the same information through separate constants shall be avoided.

Configuration headers shall declare the associated descriptor arrays using this COUNT value.

Example:

```cpp
extern AnalogInputDev AnalogInputDevArray[
    static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)
];
```

The header therefore defines both:

- the identifier contract;
- the descriptor interface exposed to the rest of the application.

---

## 4. Source Definitions: Configuration Arrays

Configuration arrays provide the concrete implementation of the contracts declared in the header.

They should be defined in the associated source file whenever their contents are non-trivial, keeping verbose configuration data separate from the public descriptor interface.

The order of array entries is significant.

Each entry shall correspond to the enum value occupying the same ordinal position. This implicit mapping forms part of the contract between the header and its implementation.

Entry identification comments are encouraged, especially for long descriptor lists, as they improve visual matching between enum declarations and array definitions.

Fields relying on safe defaults may be omitted. Default values should represent the safest behaviour that remains logically valid for the subsystem.

Example:

```cpp
AnalogInputDev AnalogInputDevArray[
    static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)
] = {

    // LX_STICK
    {
        .infoName   = "left X axis stick",
        .type       = RemoteComp::ANALOG_STICK,
        .minVal     = StickMinVal,
        .maxVal     = StickMaxVal,
        .isInverted = false
    },

    // LY_STICK
    {
        .infoName   = "left Y axis stick",
        .type       = RemoteComp::ANALOG_STICK,
        .minVal     = StickMinVal,
        .maxVal     = StickMaxVal,
        .isInverted = false
    },

    // RX_STICK
    {
        .infoName   = "right X axis stick",
        .type       = RemoteComp::ANALOG_STICK,
        .minVal     = StickMinVal,
        .maxVal     = StickMaxVal,
        .isInverted = false
    }

    // ...
};
```

The source file therefore becomes the readable inventory of the resources declared in the header.

---

## 6. Top-Level Descriptors

Each configuration domain should expose a top-level descriptor aggregating its resources.

Examples include:

- board descriptors;
- remote descriptors;
- sensing containers;
- machine descriptors.

Top-level descriptors provide a single entry point to subsystem configuration.

They should reference lower-level descriptor arrays rather than duplicate their contents.

Whenever possible, they should remain lightweight and declarative.

Example:

```cpp
inline constexpr InputDev inputDev {
    .infoName             = "PS4 dualshock controller",
    .protocol             = RemoteProtocol::PS4_BLUETOOTH,
    .analogInputDev       = AnalogInputDevArray,
    .digitalInputDev      = digitalInputDevArray,
    .analogInputDevCount  = static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT),
    .digitalInputDevCount = static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)
};
```

---

## 7. Compile-Time Guards

`static_assert` may be used to enforce meaningful compile-time invariants.

Typical examples include:

- hardware capabilities;
- protocol limitations;
- descriptor consistency;
- resource ceilings.

Compile-time guards may appear either:

- in headers, when validating descriptor contracts;
- in sources, when validating configuration values.

Example:

```cpp
static_assert(
    static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT) <= 6u,
    "Exceeded hardware capabilities."
);
```

Assertions that merely restate obvious implementation details should be avoided.