# ComBus — Communication Bus

The **ComBus** (*Communication Bus*) is the central data structure of Open RC Node.

It was designed to provide a common access point for the different modules of a system, allowing them to organize, share, and synchronize functional information.

Rather than having each module own and exchange its own data with other components, the ComBus acts as a **shared functional register** containing the current state of the system.

Each module reads and updates the information it needs through this common register.

The ComBus therefore serves as a **single source of truth**, allowing system components to collaborate without direct dependencies.

Over time, this approach has made the ComBus one of the fundamental concepts of Open RC Node.

---

# Register Structure

The ComBus consists of a collection of identified and typed channels.

Each channel represents a piece of functional information within the system:

- commands;
- states;
- measurements;
- computed values;
- system information.

Two main channel families are available:

- **Analog**: continuous values;
- **Digital**: binary states.

Modules do not own the data they use.

Instead, they publish the information they produce to the ComBus and read the information they need to perform their own processing.

Each channel has a logical owner (**Owner**) responsible for updating its value, ensuring the consistency of shared data.

---

# ComBus Set

A single register containing every possible function supported by Open RC Node would quickly become too large and difficult to maintain.

Each machine family therefore defines its own **ComBus Set**: a specialized register definition adapted to a particular type of machine.

It defines:

- the available channels;
- their type;
- their meaning;
- their intended use.

It provides both the common foundation used to build a machine configuration and the **functional contract** shared by all components of a system.

---

# Configuration and Standardization

Beyond the ComBus Set, additional configuration layers make it possible to define:

- the available functions;
- the configuration of a specific machine;
- the implementation of its behavior.

This approach makes it possible to create machine variants while preserving a common and reusable foundation.

The standardization of ComBus Sets also made it possible to use the same definition as the basis for communication between multiple nodes.

A remote controller, a machine, or any other node can therefore share the same ComBus Set and manipulate the same functional information.

The ComBus is **not a communication protocol**. It only defines the structure and organization of shared data.

Synchronization between multiple ComBus instances is handled by communication modules. These modules select the channels to transmit, encapsulate them using the protocol appropriate for the underlying transport, then update the local ComBus instance with the received data.

As a result, the same ComBus structure can be synchronized over different communication media (UART, ESP-NOW, Wi-Fi, Bluetooth...) and at different architectural levels (**Inter-node**, **Inter-device**, or **Intra-device**) in a completely transparent manner.

Communication is therefore just one of the many uses of the ComBus.

---

# Channel Layers

The ComBus organizes channels into three distinct layers, each with a specific propagation scope:

| Layer | Scope | Description |
|---|---|---|
| **REMOTE** | Inter-node | Data exchanged between distinct nodes (remote↔machine, machine↔sound) |
| **LOCAL** | Intra-node | Data shared between all boards of the same system |
| **SYSTEM** | Intra-device | Private data within a single board's firmware |

## Layer Definitions

### REMOTE (Inter-node)
- **Purpose**: Universal commands known to all nodes
- **Example**: Steering, throttle, basic vehicle controls
- **Characteristics**:
  - Minimal service set for each vehicle type
  - Remote controllers send commands blindly
  - Standardized across vehicle variants
  - Transmitted between nodes via communication bridges

### LOCAL (Intra-node)  
- **Purpose**: Vehicle-specific features shared within a system
- **Example**: Special lighting sequences, dump bed controls
- **Characteristics**:
  - Includes all REMOTE channels automatically
  - Specific to vehicle model (MAN vs VOLVO features)
  - Shared between main board and extension boards
  - Never transmitted to remote controllers

### SYSTEM (Intra-device)
- **Purpose**: Private data for internal board management
- **Example**: Intermediate values between processors, internal states
- **Characteristics**:
  - Never shared between boards
  - Used for communication between processors on same board
  - No reason to propagate beyond local firmware

## Access Rules

Each layer defines who may write to its channels:

1. **SYSTEM** → Only the local firmware may write
2. **LOCAL** → Local firmware + extension boards (same system)
3. **REMOTE** → Any source (local + extensions + remote nodes)

## Configuration Example

```cpp
// REMOTE channels (inter-node)
{ .infoName = "steering", .value = CbusNeutral, .layer = ChanLayer::REMOTE },
{ .infoName = "throttle", .value = CbusNeutral, .layer = ChanLayer::REMOTE },

// LOCAL channels (intra-node)  
{ .infoName = "man_light_sequence", .value = 0u, .layer = ChanLayer::LOCAL },
{ .infoName = "dump_bed_control", .value = CbusNeutral, .layer = ChanLayer::LOCAL },

// SYSTEM channels (intra-device)
{ .infoName = "throttle_ramp_state", .value = 0u, .layer = ChanLayer::SYSTEM },
{ .infoName = "gear_shift_timer", .value = 0u, .layer = ChanLayer::SYSTEM },
```

## Layer Inheritance

The ComBus follows a natural hierarchy:
- **SYSTEM** data can be promoted to **LOCAL** if needed
- **LOCAL** data includes all **REMOTE** channels
- **REMOTE** represents the minimal universal set

This layered approach ensures clean separation of concerns while maintaining flexibility for future expansions.

---

# ComBus Processors

**ComBus Processors** (`cbProc`) add functional processing around the register.

A processor:

1. reads one or more channels;
2. applies a transformation;
3. writes the result to other channels.

Processors can be chained together to build complex behaviors without modifying the main firmware logic.

Driven by configuration files, they make machine behavior easily customizable.

Typical applications include:

- engine simulation;
- inertia;
- command ramps;
- dynamic limiting;
- sensor processing.

The final machine behavior is therefore built by combining:

```text
ComBus Set
    +
Configuration
    +
ComBus Processors
    =
Machine Behavior
```