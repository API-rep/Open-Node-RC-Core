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

# Data Scope

Not all information has the same visibility.

The ComBus distinguishes three propagation levels:

| Level | Purpose |
|---|---|
| **Inter-node** | Communication between multiple nodes |
| **Inter-device** | Communication between multiple boards within the same system |
| **Intra-device** | Internal data within a firmware |

Examples:

```text
Inter-node
Remote Controller ↔ Machine


Inter-device
Main Board ↔ Expansion Board


Intra-device
Motion Simulation ↔ Sound Module
```

Each channel explicitly belongs to one layer.

This layer defines how far a piece of information may propagate.

Local data remains local until it is explicitly published to a higher propagation level.

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