# Open RC Node

Open RC Node is an open-source project aiming to create a more flexible, modular and modern RC environment for DIY projects.

The idea was born during simple family play sessions. Like many RC enthusiasts, we quickly found ourselves managing multiple vehicles, multiple controllers and a growing collection of independent systems.

Traditional RC systems generally follow a simple model: one controller is paired with one or more vehicles, with limited interaction between the different devices.

This approach works perfectly for many use cases, but it quickly reaches its limits when several users need to share and operate the same fleet of machines.

Open RC Node explores a different approach by treating all equipment as components of a shared RC ecosystem.

Controllers, vehicles, electronic boards, extension modules and software services can communicate within a common architecture while keeping the flexibility and creativity that make DIY projects so appealing.

Open RC Node does not aim to reinvent RC hobbying. Its goal is to provide a common architecture that allows multiple devices, users and services to collaborate within the same environment, free from many of the limitations traditionally imposed by conventional RC systems.

---

## Designed for Makers

Open RC Node is first and foremost a project created by an enthusiast for enthusiasts.

It is intended for people who enjoy understanding, modifying and building their own systems.

The goal is not to provide a turnkey solution, but rather a toolbox that allows users to design RC equipment tailored to their own needs.

Basic knowledge of electronics, embedded systems and software configuration is still required. However, the architecture is designed to minimize custom development by relying on fully editable configuration files, naturally compatible with modern assistance tools, including generative AI.

The project's core concepts and components are progressively documented throughout the repository to make Open RC Node easier to discover, understand and extend.

---

## ComBus: The Backbone of the System

At the heart of the project lies **ComBus**, a communication protocol specifically designed for Open RC Node and around which the entire architecture is built.

It is used both for communication between devices and for communication between internal components of a single node.

A battery monitoring module, motor controller, operating mode manager, controller or vehicle all rely on the same communication mechanisms: ComBus.

It serves as the backbone of the project, transporting information, commands and status data between all system components. Fully configurable, it adapts its structure to the needs of each device and network architecture, without being constrained by the communication models or channel limitations commonly found in traditional RC systems.

---

## A Configuration-Driven Architecture

One of the project's main goals is to favor configuration over custom development.

Devices are described through configuration files that define:

* ComBus configuration;
* machine definitions;
* hardware configuration;
* software modules;
* operating modes;
* general system behavior.

The idea is that a new vehicle, controller or extension board can be created primarily through assembly and configuration of existing components.

---

## An Open and Scalable Architecture

Open RC Node currently runs on ESP32 hardware and its Arduino ecosystem, but the architecture itself is not tied to any specific microcontroller. Any platform capable of running the system's core components could theoretically host an Open RC Node implementation.

As a result, ESP-NOW is currently a natural candidate for wireless communication between nodes, but the architecture is designed to remain as independent as possible from the underlying transport layer.

Likewise, control sources can take many forms:

* custom controllers;
* gamepads;
* web interfaces;
* or any device capable of producing or consuming ComBus messages.

This flexibility makes it possible to build anything from a simple RC vehicle to more advanced systems involving multiple machines, multiple operators and different levels of automation.

---

## Project Status

Open RC Node is currently under active development. The foundations of the architecture are in place and the project's main concepts are now clearly established.

The project is developed during the author's free time as an ongoing hobby and experimentation effort. Modern AI tools play an important role in this journey, assisting with design, documentation and the exploration of new ideas.

However, significant work, testing and iteration will still be required before reaching a fully mature and complete ecosystem.

The project continues to evolve through practical experimentation, with the goal of building a coherent, extensible and enjoyable RC environment for DIY enthusiasts.

---

## Documentation

Project documentation is organized around a central entry point:

* **[General Documentation](docs/README.md)**

There you will find Open RC Node's core concepts as well as links to the various thematic documentation sections.

Most components, modules and subsystems also include their own README files, documenting their purpose, behavior and interfaces as close to the code as possible.
