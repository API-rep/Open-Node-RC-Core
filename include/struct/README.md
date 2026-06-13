# Structure Design

This directory contains the shared structures used throughout Open RC Node.

These structures act as common contracts between the different environments of the project: machines, remotes, dashboard views, services, simulations, and future extensions. They define how data is organized and exchanged, independently from the modules that process it.

The goal is to keep these definitions lightweight, explicit, and reusable. Shared structures should describe *what exists* and *how pieces relate to each other*, without embedding implementation details or processing logic whenever possible.

The following patterns represent the preferred way of designing structures within Open RC Node.

---

## Configuration–State Separation

Subsystems should explicitly separate immutable definitions from mutable runtime data.

A subsystem is generally decomposed into three layers:

* **Configuration (`cfg`)**
  Immutable definitions describing how the subsystem behaves. Configuration is typically provided by board files, machine definitions, presets, or other shared references, and usually resides in flash memory.

* **Runtime State (`state`)**
  Mutable data evolving during execution. State is owned by the subsystem and typically resides in RAM.

* **Container (`module`)**
  Common top-level structure gathering the configuration and state structures into a single access point. The container usually stores pointers or references to these underlying structures rather than owning the data itself, exposing the complete execution context of the subsystem.

This separation improves ownership clarity, memory placement, subsystem reusability, and allows the same logic to operate on different configurations without modification.

The exact naming may vary when appropriate, but the distinction between configuration, state, and their aggregation through explicit references should remain clear.
