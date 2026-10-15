# Open RC Node Documentation

Welcome to the Open RC Node documentation.

This section provides an overview of the project's documentation and serves as the central entry point to the various resources available throughout the repository.

Open RC Node is built around a small set of reusable concepts that can be assembled to create a wide variety of RC systems, from simple vehicles to more advanced multi-node environments.

The goal of this documentation is not to describe every implementation detail, but to help users, makers and developers understand how the different parts of the ecosystem fit together.

---

## Documentation Guide

### New to Open RC Node?

If you are discovering Open RC Node for the first time, this is the recommended starting point.

This section explains how nodes interact within the ecosystem and how Open RC Node differs from traditional RC architectures.

Topics include:

* Node concepts and roles
* Shared machine fleets
* Multi-user environments
* Remote-to-machine interactions
* Network discovery mechanisms
* Typical communication flows

Future documentation will also include diagrams and animations illustrating how remotes, machines and services exchange information across the network.

* [Open RC Node Ecosystem](ecosystem.md)

---

### Building a System

Ready to build your own Open RC Node setup?

This section covers the practical aspects of creating and deploying a system.

#### Quick Start

* [Building an Open RC Node System](building_a_system.md) — Step-by-step guide

#### Hardware

* Available boards
* Supported peripherals
* Motor control
* Servo outputs
* Lighting
* Sound modules
* Extension capabilities

#### Development Environment

* Visual Studio Code
* PlatformIO installation
* Repository setup
* Building and uploading firmware

#### First Project

* Creating a machine
* Creating a remote
* Uploading firmware
* Testing communications
* Debugging tools
* Dashboard overview

---

### Need a Custom Configuration?

One of the key strengths of Open RC Node is its configuration-driven architecture.

Most systems can be created and customized through configuration files without modifying application logic.

Topics include:

* Machine configuration
* Remote configuration
* ComBus configuration
* Module configuration
* Operating modes
* Configuration examples and reference

See [Configuration files](configuration/README.md) — syntax reference for each configuration file format (ComBus channels `.cb`).

---

### Want to Contribute?

If you plan to contribute to Open RC Node or understand its internal architecture, start here.

Topics include:

* ComBus
* [Core Architecture](core_architecture.md)
* [Board Architecture](board_architecture.md) - Hardware design and responsabity
* Modules
* Internal services
* Software architecture
* [Development Guidelines](developement/README.md)

See also:

* [Documentation Guidelines](documentation_guidelines.md) — How we structure documentation
* [IA Guidelines](ia-guidelines.md) — Guidelines for AI-assisted development

---

### By Subject

Quick index to the documents that sit next to the code. Start from the subject, then follow the links of the document you land on.

**ComBus (communication bus)**

* [ComBus overview](../src/core/system/combus/README.md) — what the bus is and how modules use it
* [ComBus v2 migration document](~combus_v2.md) — layering, directions, builder section (work in progress)
* [Protocol layer](../src/core/system/combus/protocol/~README.md) — transport-independent services
* [Frame and handshake format](../src/core/system/combus/protocol/frame/combus_frame_protocol.md) — wire format, handshake, MD5 contract
* [UART workflow](../src/core/system/combus/protocol/uart_workflow.md) — how the UART link is set up
* [ComBus Builder](../scripts/combus_builder/README.md) — developer README of the generator (pipeline, node mode, tests)
* [Builder open points](combus_builder_handoff.md) — what remains to check or decide
* [`.cb` syntax](configuration/cb_syntax.md) — writing channel definitions

**ComBus processors**

* [Processors inventory](../src/core/system/combus/processors/~README.md) and [future intentions](../src/core/system/combus/processors/COMBUS_PROCESSORS_ROADMAP.md)
* [Processor modules convention](../src/core/system/combus/processors/modules/README.md)
* [Ramp runtime calculation guide](archives/ramp_runtime_explanation.md) (archive)

**Safety and link health**

* [Failsafe module](../src/core/system/failsafe/failsafe_module.md)
* [REMOTE_LINK_LOST chain](../src/core/system/inputs/remote_link_lost.md)

**Machines and configuration**

* [Machine configuration system](../src/machines/config/~README.md)
* [Machine family configuration](../src/core/config/machines/README.md)
* [Shared data definitions (`include/`)](../include/README.md)

**Debug tools**

* [Debug architecture](../src/core/system/debug/~README.md), [logging](../src/core/system/debug/logging/~README.md) and [dashboard](../src/core/system/debug/dashboard/~README.md)

**Tests**

* [PlatformIO test setup](../test/README_pio_test_setup.md)
* Builder tests: see the "Lancer les tests" section of the [ComBus Builder README](../scripts/combus_builder/README.md)

---

### Project Roadmap & Evolution

Track the project's evolution and upcoming features.

* [Roadmap](roadmap.md) — Current development direction
* [Deferred Features](deferred_features.md) — Backlog and future improvements

Open RC Node is an evolving project. Documentation will continue to grow alongside the architecture and implementation.
