# ComBus Processors — Roadmap & Inventory

ComBus processors are reusable data transformation blocks used throughout Open RC Node.

They operate on ComBus channels and can be assembled into processing chains to create complex behaviors without embedding machine-specific logic directly into firmware code.

A processor typically:

* Reads a ComBus value
* Optionally observes other ComBus channels
* Applies a transformation or decision
* Forwards the result to the next stage
* Optionally writes additional side-channel outputs

Processors can therefore be seen as the building blocks used to create machine behavior.

---

## Why Processors Exist

Open RC Node is heavily configuration-driven.

Rather than implementing vehicle-specific logic directly in application code, machine behavior is built by assembling reusable processor chains.

This approach allows the same processors to be reused across multiple machine classes while keeping the resulting behavior fully configurable.

Typical use cases include:

* Input conditioning
* Motion simulation
* Signal filtering
* Virtual gearboxes
* Cruise control
* Safety systems
* Data routing
* Hardware command generation

---

## Processing Model

A processor chain receives data from a ComBus channel and progressively transforms it through multiple stages.

```text
ComBus Channel
       │
       ▼
[ Processor A ]
       │
       ▼
[ Processor B ]
       │
       ▼
[ Processor C ]
       │
       ▼
ComBus Channel
```

Processors may:

* Modify the current value
* Observe the current value without modifying it
* Read additional ComBus channels
* Generate side-channel outputs
* React to machine state
* Use static or dynamic configuration

Chains can write back to the same channel or produce entirely new channels depending on the desired behavior.

---

## Typical Example

A throttle input may pass through multiple processors before reaching the final hardware output:

```text
Remote Input
      │
      ▼
THROTTLE_BUS
      │
      ▼
Ramp Processor
      │
      ▼
Brake Processor
      │
      ▼
Cruise Processor
      │
      ▼
Gear Processor
      │
      ▼
ESC_SPEED_BUS
```

Each processor contributes a small, isolated responsibility while the complete chain creates the final machine behavior.

---

## Configuration Driven

Processor chains are defined through configuration files.

The long-term objective is to allow machine behaviors to be assembled primarily through configuration rather than custom code.

New machine types should therefore be able to reuse existing processors while combining them differently to achieve their desired behavior.

---

## Processing Pipeline Vision

ComBus processors are intended to become the central data transformation framework used throughout Open RC Node.

The long-term architecture is based on three distinct processing stages:

```text
Input Device
      │
      ▼
Input Formatting
      │
      ▼
ComBus Processing
      │
      ▼
Output Formatting
      │
      ▼
Hardware Driver
```

### 1. Input Formatting

The first stage is responsible for normalizing raw input devices before they enter the machine logic.

Examples:

* Remote controls
* Joysticks
* Switches
* Sensors
* Future network-based inputs

The objective is to convert device-specific values into the standard Open RC Node channel representation.

This stage is device-oriented and should be configured at the input-device level rather than inside machine configurations.

For a source of commands with a fixed protocol (a controller or a receiver plugged into the machine), this stage is the **reframer** of the source: a chain of the `source` stage, called by the module of the source right after each read of the device, never by the main loop. It therefore runs once per fresh sample, before any chain of the machine.

A chain of the `source` stage holds **stateless processors only**: their output depends on the pipeline value, their configuration and their secondary input of this pass, nothing else (no `state`, no `millis()`, no `static`). Scaling, deadband, centering, inversion and expo belong here; ramps and inertia belong to the simulation chains. It writes the channels of its source only. Rule and inventory: `doc/WIP - ComBus processors.md` §2.9.

The associated channel conventions and encoding rules are documented separately in the internal channel specification.

---

### 2. ComBus Processing

The second stage contains the processor chains described in this document.

This is where machine behavior is assembled through reusable processing blocks.

Typical responsibilities include:

* Motion simulation
* Inertia
* Gearboxes
* Cruise control
* Safety systems
* Data routing
* State management

---

### 3. Output Formatting

The final stage converts internal ComBus values into formats expected by hardware drivers and external libraries.

Examples:

* Motor drivers
* Servo libraries
* Sound engines
* Lighting systems

This layer isolates hardware-specific conventions from the internal ComBus representation and allows the same machine logic to be reused across different hardware implementations.

---

## Future Direction — Audio Processing

The processor framework is not limited to machine control.

A future evolution of the sound subsystem may reuse the same processor architecture to build configurable audio processing chains.

Potential applications include:

* Engine sound synthesis
* Audio filtering
* Mixer stages
* Sound effects
* Dynamic audio behaviors driven by ComBus events

The objective would be to extend the configuration-driven philosophy already used for machine behavior to the audio domain.

---

## Current Processor Categories

* Base
* Math
* Input
* Motion
* Gear

---

Dernière mise à jour : 2026-06-10 (fin de saison 2026).
