# ComBus — Communication Bus

The **[ComBus](../../../../doc/combus.md)** (*Communication Bus*) is the central data structure of Open RC Node, and over time it has become one of its fundamental concepts.

It was designed to provide a common access point for the different modules of a system, allowing them to organize, share, and synchronize functional information.

The ComBus is only a data structure, **not a communication protocol**: it defines what the shared data is and how it is organized, not how it travels. The same ComBus can therefore be carried over many different means of communication (a wire, a radio link) without changing anything in the modules that use it.

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

Modules do not own the data they use, and do not exchange it with each other. Instead, they publish the information they produce to the ComBus and read the information they need to perform their own processing.

The ComBus thus acts as a **shared functional register** containing the current state of the system: its **single source of truth**, allowing the components to collaborate without direct dependencies.

---

# ComBus Set and Configuration

A single register containing every possible function supported by Open RC Node would quickly become too large and difficult to maintain.

Each machine family therefore defines its own **ComBus Set**: a specialized register definition adapted to a particular type of machine.

It defines:

- the available channels;
- their type;
- their meaning;
- their intended use.

The ComBus Set is the common foundation of every machine of that type. Further configuration levels are built on top of it:

- the configuration of a specific machine;
- the functions it enables;
- the implementation of its behavior.

This approach makes it possible to create machine variants while preserving a common and reusable foundation.

---

# Sharing Between Nodes

The ComBus Set is also the **functional contract** that lets the nodes of a system interact: a remote controller, a machine, or any other node speaks the same channels, with the same names and the same meaning. Each link between nodes carries only the part it needs.

Synchronization between multiple ComBus instances is handled by communication modules. These modules select the channels to transmit, encapsulate them using the protocol appropriate for the underlying transport, then update the local ComBus instance with the received data.

As a result, the same ComBus structure can be synchronized over different communication media (wired, wireless, RF...), between nodes as well as between the boards of a node, in a completely transparent manner.

Communication is therefore just one of the many uses of the ComBus.

---

# Channel Layers

Not every value has to travel everywhere. The ComBus organizes channels into three layers, each with its own propagation scope:

| Layer | Scope | Description |
|---|---|---|
| **BASE** | Machine type | Standard profile of the machine type: the same channels on every machine of the type, and on each of its boards |
| **SYSTEM** | Intra-node | Data shared between all boards of the same node |
| **BOARD** | Intra-device | Private data within a single board's firmware |

The layers are nested: `SYSTEM` includes all the `BASE` channels, and the ComBus of a board includes all the `SYSTEM` channels. The final ComBus of a board is the superposition of the three:

```text
BASE                    = the profile of the machine type
BASE + SYSTEM           = what the boards of a node share
BASE + SYSTEM + BOARD   = ComBus of the board
```

A layer adds channels; it never replaces a lower one.

## Layer Definitions

### BASE (Machine Type)

- **Purpose**: the profile of a machine type (truck, excavator, tractor...): the channels common to all its machines, and to all their boards.
- **Example**: the commands the machine applies, computed from its inputs: `STEERING_BUS`, `THROTTLE_BUS`, `BRAKE_BUS`.
- **Characteristics**:
  - defined once per machine type (`core/config/machines/<type>/`), identical for all its machines;
  - shared by the boards of a node;
  - never conditional: a `BASE` channel cannot depend on a build flag.

### SYSTEM (Intra-node)

- **Purpose**: the data of one node, shared by its boards.
- **Example**: computed states (`KEY_ACTIVE`, `DRIVE_STATE_BUS`), the run level, the failsafe aggregates.
- **Characteristics**:
  - shared between the main board and the extension boards;
  - never leaves the node.

### BOARD (Intra-device)

- **Purpose**: private data of one board or sub-board firmware, for its internal use.
- **Example**: intermediate values between processors, the local channels of a coupler, the inputs of the remote (`*_STICK`, `*_BTN`).
- **Characteristics**:
  - never shared between boards;
  - no reason to propagate beyond the local firmware.

## Access Rules

The layers also organize the access to the ComBus: they give write permissions according to the level of exchange:

| Layer | May be written by |
|---|---|
| **BASE** | the board itself, the other boards of its node, and other nodes |
| **SYSTEM** | the board itself and the other boards of its node |
| **BOARD** | the board itself only |

In practice, each writer has a layer of its own. The code of the board writes as `BOARD`, so it may write every channel. A link writes with the layer it carries: a link between nodes as `BASE`, a link between the boards of a node as `SYSTEM`.

Within these permissions, the `direction` of a channel works like the permissions of a shared file, seen from a link: `uplink` is read-only for the link (the board writes, the other end reads), `downlink` is writable by the link (the other end writes, the board reads), `both` is read-write, and `none` is protected: the channel never leaves the board.

---

# What a Channel Declares

Channels are not written by hand: each one is declared in a `.cb` file, and the ComBus Builder generates the identifiers, the arrays and the fingerprints at build time.

| Field | Meaning |
|---|---|
| `id` | the name of the channel, unique across the project |
| `type` | `analog` (0 to 65535) or `digital` |
| `scope` | its layer: `BASE`, `SYSTEM` or `BOARD` |
| `theme` | its functional group (`core`, `failsafe`, `light`...) |
| `direction` | which way it travels: `uplink`, `downlink`, `both`, `none`; `*_or` for a shared fault, `downlink_pt` for a relayed value |
| `value` | its **safe value**: neutral for an analog channel, `false` for a digital one, `fault` for an alarm |
| `requires` | the build flags without which the channel does not exist |
| `zeroAt` | where the logical zero of an analog channel sits: at 0, or at neutral |
| `candidate` | a `BASE` channel on trial, outside the stable contract (dev. feature) |

See the [`.cb` syntax](../../../../doc/configuration/cb_syntax.md) and the [ComBus Builder](../../../../scripts/combus_builder/README.md).

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

---

# Going Further

- [ComBus overview](../../../../doc/combus.md): the introduction, the reading path and the index of the ComBus documents
- [Linking nodes](../../../../doc/linking_nodes.md): how nodes are linked, and what keeps those links safe
- [ComBus transmission](protocol/README.md): how the ComBus travels between boards and nodes
- [Accessors](combus_access.h): reading and writing channels, logical and raw values
- [.cb syntax](../../../../doc/configuration/cb_syntax.md): declaring channels
