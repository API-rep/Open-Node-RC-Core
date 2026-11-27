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
enum DrvDev {
    STEERING = 0,
    CABIN_LEFT_MOTOR,
    CABIN_RIGHT_MOTOR,
    // ...
    DC_DRV_COUNT
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
extern DcDevice dcDevArray[DC_DRV_COUNT];
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
DcDevice dcDevArray[DC_DRV_COUNT] = {

    // STEERING
    {
        .ID         = STEERING,
        .infoName   = "steering actuators",
        .drvPort    = &drvPortArray[DRV_PORT_1A],
        .DevType    = DcDevType::DC_MOTOR,
        .usage      = DevUsage::STEER_MOTOR,
        .signal     = DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER,
        .comChannel = AnalogComBusID::STEERING_BUS,
        .pwmFreq    = M_DEF_PWM_FREQ,
        .polInv     = true
    },

    // CABIN_LEFT_MOTOR
    {
        .ID         = CABIN_LEFT_MOTOR,
        .infoName   = "cabin left motor",
        .drvPort    = &drvPortArray[DRV_PORT_1B],
        .DevType    = DcDevType::DC_MOTOR,
        .usage      = DevUsage::TRACT_WHEEL,
        .signal     = DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER,
        .comChannel = AnalogComBusID::ESC_SPEED_BUS,
        .pwmFreq    = M_DEF_PWM_FREQ,
        .polInv     = true
    },

    // CABIN_RIGHT_MOTOR: a clone, only the overrides are declared
    {
        .ID       = CABIN_RIGHT_MOTOR,
        .infoName = "cabin right motor",
        .drvPort  = &drvPortArray[DRV_PORT_2B],
        .polInv   = false,
        .parentID = CABIN_LEFT_MOTOR
    },

    // ...
};
```

The source file therefore becomes the readable inventory of the resources declared in the header.

---

## 6. Top-Level Descriptors

Each configuration domain should expose a top-level descriptor aggregating its resources.

Examples include:

- board descriptors;
- sensing containers;
- machine descriptors.

Top-level descriptors provide a single entry point to subsystem configuration.

They should reference lower-level descriptor arrays rather than duplicate their contents.

Whenever possible, they should remain lightweight and declarative.

Example:

```cpp
inline constexpr EnvCfg machine {
    .infoName    = kVehicleName,
    .dcDev       = dcDevArray,
    .dcDevCount  = DC_DRV_COUNT,
    .srvDev      = SrvDevArray,
    .srvDevCount = SRV_COUNT,
    // ...
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
static_assert(VBatCutoffVoltage >= 2.5f,
              "VBatCutoffVoltage below 2.5 V/cell — risk of irreversible LiPo cell damage");
```

Assertions that merely restate obvious implementation details should be avoided.