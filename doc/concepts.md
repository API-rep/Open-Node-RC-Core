# Open RC Node Architecture

This document provides a high-level overview of how Open RC Node is structured. It focuses on concepts rather than implementation details.

---

## Nodes

Open RC Node is built around the concept of nodes.

A node is any element that participates in the system such as :

* Remote controllers,
* Machine controllers,
* Dedicated extension modules,
* Future network services.

Nodes can evolve independently while remaining part of the same ecosystem.

As the project grows, new node types can be introduced without fundamentally changing the architecture.

---

## ComBus — The Backbone of the System

At the heart of Open RC Node lies **ComBus**, the communication model around which the entire architecture is built.

ComBus is used both for communication **between nodes** and **within a node itself**.

Whether information travels from a remote to a machine, between internal subsystems of the same controller, or between two independent machines, the same communication principles apply.

A remote stick value, battery monitor, motor controller, lighting system, sound generator ... all rely on the same shared language: ComBus.

Unlike traditional RC systems built around a fixed number of predefined channels, ComBus provides a richer and far more flexible representation of information.

It carries commands, measurements, states and events using structures that adapt to the needs of each system rather than forcing the system to adapt to predefined limitations.

```text
Remote
    ↓
ComBus
    ↓
Machine
    ↓
ComBus
    ↓
Extension Modules
```

Because every participant speaks the same language, nodes can do more than simply exchange values: they can understand each other's role and cooperate.

A remote can discover and control a machine, a machine can interact with dedicated modules and machines themselves may collaborate directly.

For example, a truck connecting to a trailer could automatically exchange information with it, coordinating lighting, tipping functions or other shared behaviors without requiring dedicated point-to-point integrations.

ComBus therefore serves as more than a transport mechanism.

It is the common foundation that allows Open RC Node to build distributed, configurable and evolving RC systems free from the fixed communication models and channel limitations commonly found in traditional RC architectures.

---

## Configuration First

If nodes are expected to cooperate, they need a shared understanding of themselves and their environment.

Configuration provides that common reference.

It defines how machines, remotes and modules are described, how they expose their capabilities, and how they interact with the rest of the system.

This principle extends far beyond communication.

Open RC Node relies extensively on configuration to describe machines, boards and behaviors. During startup, these definitions are parsed by initialization routines to assemble the final runtime environment.

In practice, the firmware is largely built from configuration.

Machine capabilities, board layouts, processor chains and communication structures are all derived from a set of editable definitions rather than hardcoded application logic.

This approach allows users to adapt the system to their needs without modifying the underlying codebase. In many cases, creating a new setup simply means editing a few configuration files.

The trade-off is that rich configurations can sometimes become intimidating. Their expressive power often relies on structured C definitions that may appear unfamiliar at first glance.

Fortunately, users increasingly do not have to navigate this complexity alone. With good documentation and the help of modern AI tools, describing a system in plain language can often be enough to generate or refine the corresponding configuration.

The objective is not to hide how the system works, but to make its flexibility more approachable and accessible.

Complexity is not eliminated: it is transformed into definitions that are easier to reuse, share and evolve.

---

## Open by Design

Open RC Node embraces both open-source software and open hardware principles.

The project is designed to be explored, modified and extended by its users.

Its architecture intentionally favors modularity and abstraction layers, allowing new capabilities to be introduced without rewriting the entire system.

New communication methods, hardware modules, machine definitions and configuration models can be integrated progressively as the project evolves.

Today, Open RC Node primarily targets Arduino-compatible ESP32 platforms. This choice provides an accessible and widely available foundation for makers while benefiting from a mature ecosystem of tools and libraries.

However, the concepts themselves remain independent from any specific technology.

Open RC Node is an active hobby project.

The foundations are established and the overall direction is clear, but experimentation and real-world use continue to shape its implementation.

This document describes the intended architecture rather than a frozen specification.

As the community, ideas and technologies evolve, the architecture is expected to evolve alongside them.
