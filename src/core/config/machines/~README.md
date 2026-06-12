# Machine Class Configuration

Machine classes define reusable ComBus templates for a family of machines.

Their primary purpose is to provide a common channel layout that can be reused by different vehicle implementations belonging to the same category.

A machine class does not define a specific vehicle.

Instead, it defines the communication structure required for that type of machine.

Examples:

* Dumper Truck
* Excavator
* Loader

Each class defines the channels, data types and communication contracts required for normal operation.

---

## Why Machine Classes Exist

Many vehicles share similar control and simulation requirements.

Rather than redefining an entire ComBus layout for every vehicle, Open RC Node groups them into machine classes.

Vehicles belonging to the same class can therefore reuse:

* ComBus channel definitions
* Input mappings
* Motion presets
* Sound mappings

while still providing their own vehicle-specific configuration.

---

## Configuration Hierarchy

```text
src/core/config/machines/
  machine_config.h          ← machine class dispatcher
  combus_ids.h              ← ComBus ID dispatcher
  combus_types.h            ← ComBus type dispatcher

  <class>/
    <class>_config.h
    combus/
    motion/
    inputs_map/
    sound/
```

The top-level dispatchers provide a common entry point for the rest of the system.

Each machine class then exposes its own configuration domains.

This allows the framework to select the appropriate ComBus template and associated resources based on the active machine configuration.

---

## Configuration Domains

### ComBus

Reference channel definitions for the machine class.

This is the primary purpose of a machine class.

The ComBus layout defines:

* Channel identifiers
* Channel types
* Expected data flow
* Communication contracts

All compatible vehicles within the same class share the same reference ComBus structure.

---

### Motion

Optional motion presets associated with the class.

Typical examples include:

* Drive train behavior
* Steering model
* Gear management
* Vehicle simulation presets

---

### Input Mapping

Default mappings between user inputs and ComBus channels.

This allows different machine classes to expose different control schemes while maintaining a consistent architecture.

---

### Sound

Optional sound-related definitions associated with the machine class.

These resources can be consumed by dedicated sound nodes or future integrated implementations.

---

## Current Machine Classes

### Dumper Truck

Articulated haul trucks and dumpers.

Current implementation:

* Volvo A60H Bruder

---

### Excavator

Excavators and hydraulic arm machines.

Planned implementation.

---

### Loader

Wheel loaders and similar machines.

Planned implementation.

---

## Creating a New Machine Class

Creating a new machine class typically involves:

1. Creating a new class directory.
2. Defining the reference ComBus layout.
3. Defining channel identifiers and types.
4. Providing optional motion presets.
5. Providing optional input mappings.
6. Providing optional sound resources.

Machine classes should focus on defining reusable communication templates and common behavior shared by an entire category of machines.
