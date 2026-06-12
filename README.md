# Open RC Node

Open RC Node is an open-source project aiming to create a more flexible, modular and modern RC environment for DIY projects.

The idea was born during simple family play sessions. Like many RC enthusiasts, we quickly found ourselves managing multiple vehicles, multiple controllers and a growing collection of independent systems. Traditional RC setups work perfectly for many situations, but they can quickly become limiting when several users need to share and operate the same fleet of machines.

Open RC Node explores a different approach by treating all equipment as components of a shared RC ecosystem.

Controllers, vehicles, electronic boards, extension modules and software services can communicate within a common architecture while preserving the flexibility and creativity that make DIY projects so appealing.

Open RC Node does not aim to reinvent RC hobbying. Its goal is to provide a common architecture that allows multiple devices, users and services to collaborate within the same environment, free from many of the limitations traditionally imposed by conventional RC systems.

---

## Designed for Makers

Open RC Node is first and foremost a project created by an enthusiast for enthusiasts.

It is intended for people who enjoy understanding, modifying and building their own systems.

The goal is not to provide a turnkey solution, but rather a toolbox that allows users to design RC equipment tailored to their own needs.

Whether you simply want to add realistic lighting to a vehicle, build a fully custom transmitter, create an autonomous extension module or design your own electronic boards from scratch, Open RC Node aims to provide the foundations to support that journey.

Basic knowledge of electronics and embedded systems is still helpful. However, the project intentionally favors configuration over custom coding, allowing users to assemble and adapt systems by editing a relatively small number of definitions rather than rewriting application logic.

The project's concepts and components are progressively documented throughout the repository, making Open RC Node easier to discover, understand and extend — both for makers and for the AI tools increasingly accompanying them.

---

## More Than Driving

Open RC Node is not only about moving vehicles from point A to point B.

The architecture makes it possible to enrich RC experiences with behaviors that would traditionally require substantial custom development.

Examples include:

* Realistic motion simulation (vehicle inertia, gearbox, cruise control...),
* Advanced lighting behaviors,
* Sound integration,
* Safety features,
* Cooperative interactions between multiple devices (trailers...).

These features are designed to remain modular and reusable, encouraging experimentation and creativity rather than one-off implementations.

---

## Open by Nature

Open RC Node embraces both open-source software and open hardware principles.

The project is designed to be explored, modified and extended by its users.

Its modular architecture and abstraction layers make it easier to integrate new communication methods, hardware modules, machine definitions and capabilities as the project evolves.

Today, Open RC Node primarily targets Arduino-compatible ESP32 platforms, benefiting from an accessible ecosystem of boards, tools and libraries.

The concepts themselves, however, remain independent from any specific technology.

Whether using custom transmitters, gamepads, web interfaces or entirely new hardware, the objective remains the same: to give makers the freedom to build the RC systems they imagine.

---

## Project Status

Open RC Node is currently under active development.

The foundations of the architecture are in place and the project's core concepts are now well established, but many ideas continue to evolve through practical experimentation.

The project is developed during the author's free time as an ongoing hobby and learning journey.

Modern AI tools play an increasingly important role in this process, assisting with design, documentation and the exploration of new ideas.

Significant work, testing and iteration still lie ahead before reaching a fully mature ecosystem, but the objective remains unchanged: building a coherent, extensible and enjoyable RC environment for DIY enthusiasts.

---

## Documentation

Project documentation is organized around a central entry point:

* **[General Documentation](doc/README.md)**

There you will find introductory guides, conceptual documentation and links to the various thematic sections of the project.

Most components, modules and subsystems also include their own `README.md` files, documenting their purpose and behavior as close to the code as possible.
