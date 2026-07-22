# ComBus — Communication Bus

ComBus is the central communication backbone of Open RC Node. It provides the common language used by all nodes and subsystems within the ecosystem.

Unlike traditional RC systems built around a fixed number of predefined channels, ComBus offers a richer and more flexible representation of information. It serves both as a transport mechanism **between nodes** and as a shared data model **within a single node**.

---

## Design Philosophy

### A Common Language

A remote stick value, battery monitor, motor controller, lighting system, sound generator … all rely on the same shared language: ComBus.

This unified approach allows nodes to cooperate far beyond simple value exchange. Because every participant understands the same data structures, they can discover each other's role, interact with dedicated modules and cooperate directly.

For example, a truck connecting to a trailer could automatically exchange information, coordinating lighting, tipping functions or other shared behaviors without requiring dedicated point-to-point integrations.

ComBus therefore serves as more than a transport mechanism. It is the foundation that allows Open RC Node to build distributed, configurable and evolving RC systems.

---

## Architecture Layers (ComBus v2)

The usage of ComBus has progressively grown beyond its initial role as a simple link between remote and machine. ComBus v2 formalizes the different usage levels already introduced in the current architecture:

```text
Inter‑node     ← exchanges between distinct nodes
                (remote ↔ machine, machine ↔ sound)

Inter‑device   ← exchanges between boards within the same system
                (main board ↔ extensions)

Intra‑device   ← local data shared within a single firmware
                (VBAT, RunLevel, simulated states, intermediate values…)
```

### Hard Rule: Channel Scope

Each channel belongs explicitly to one of these layers, which determines its visibility and propagation mechanisms.

This separation must be reflected in ComBus structures, frame transport and configuration:

* **Inter‑node** data defines external exchanges;
* **Inter‑device** data structures hardware extensions;
* **Intra‑device** data remains private to the firmware.

---

## Channel Ownership

ComBus implements a granular ownership system that prevents accidental interference between subsystems.

Each channel declares an authoritative writer — a `ChanOwner` composed of two nibbles:

* **Node Group** (bits 7‑4) — the physical node that natively owns the channel;
* **Process Role** (bits 3‑0) — the specific process authorized to write.

### Access Rules

Access is verified by the `combus_access` module before any write operation:

* `NONE` (0x00) — channel unclaimed; write denied for all callers.
* `ANY`  (0xFF) — unrestricted; any caller may write.
* **Wildcard‑group** (`GRP == GRP_ANY, PROC != PROC_ANY`) — any node's matching process.
* **Local domain** (`GRP == this node's group`) — full byte match required.
* **Foreign domain** (`GRP != this node's group`) — `PROC_BRIDGE` only (UART RX bridge).

### Common Owner Values

```cpp
// Machine node
MACHINE_SYSTEM = GRP_MACHINE | PROC_SYSTEM
MACHINE_INPUT  = GRP_MACHINE | PROC_INPUT
MACHINE_VBAT   = GRP_MACHINE | PROC_VBAT
MACHINE_BRIDGE = GRP_MACHINE | PROC_BRIDGE

// Sound node
SOUND_SYSTEM   = GRP_SOUND   | PROC_SYSTEM
SOUND_BRIDGE   = GRP_SOUND   | PROC_BRIDGE

// Remote controller
REMOTE_SYSTEM  = GRP_REMOTE  | PROC_SYSTEM
REMOTE_INPUT   = GRP_REMOTE  | PROC_INPUT

// Extension board
EXT_SYSTEM     = GRP_EXT     | PROC_SYSTEM

// Wildcard‑group owners
VBAT_MON       = GRP_ANY     | PROC_VBAT    // any node's battery monitor
```

---

## Data Structure

ComBus is defined by three core structures:

### Analog Channel

```cpp
typedef struct {
  const char* infoName;      // short description
  uint16_t    value;         // current value (0‑65535)
  ChanOwner   owner;         // authoritative writer
} AnalogComBus;
```

Analog channels represent continuous values. Their 16‑bit range provides ample resolution for control signals, sensor readings and intermediate computations.

**Important:** Changing the `uint16_t` size for `AnalogComBus` would break assumptions across the system. Some subsystems depend on this specific width.

### Digital Channel

```cpp
typedef struct {
  const char* infoName;      // short description
  bool        value;         // current state (true/false)
  ChanOwner   owner;         // authoritative writer
} DigitalComBus;
```

Digital channels represent binary states — buttons, switches, flags and discrete events.

### Main Bus Structure

```cpp
typedef struct {
  RunLevel      runLevel;           // machine run level
  ChanOwner     runLevelOwner;      // who may write runLevel
  bool          batteryIsLow;       // true when any VBAT channel reports low
  ChanOwner     battLowOwner;       // who may write batteryIsLow
  bool          isDrived;           // true when an input source refreshed the bus
  uint32_t      lastFrameMs;        // timestamp of last combus_frame_apply
  AnalogComBus* analogBus;         // analog channel array
  DigitalComBus* digitalBus;        // digital channel array
  uint32_t      analogBusMaxVal;    // maximum analog channel value
} ComBus;
```

The `isDrived` flag indicates whether at least one physical input source has refreshed the bus during the current cycle. It is pre‑cleared by `sys_manager_reset()` each loop and re‑asserted by each active source.

---

## Working with ComBus

### Reading and Writing Channels

Channels are accessed through the `combus_access` module, which enforces ownership rules:

```cpp
// Write an analog channel
combus_set_analog(&bus, DRIVE_SPEED_BUS, value, ChanOwner::MACHINE_INPUT);

// Write a digital channel
combus_set_digital(&bus, HORN, true, ChanOwner::MACHINE_INPUT);

// Read channels
uint16_t speed = combus_get_analog(&bus, DRIVE_SPEED_BUS);
bool hornActive = combus_get_digital(&bus, HORN);
```

### Processor Chains

ComBus processors are reusable data transformation blocks that operate on ComBus channels. They can be assembled into processing chains to create complex behaviors without embedding machine‑specific logic directly into firmware code.

A typical throttle input chain:

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

## Transport Protocol

ComBus can be transported over multiple mediums through a common `NodeCom` interface:

```text
Application Layer
        │
        ▼
      ComBus
        │
        ▼
 Communication Protocol
        │
        ▼
      NodeCom
        │
        ├── UART
        ├── ESP‑NOW (future)
        ├── Bluetooth (future)
        └── Other transports
```

This abstraction allows ComBus services to remain independent from hardware‑specific implementations.

### Frame Structure

ComBus frames include:

* Environment ID (`envId`) — identifies the machine layout
* Channel counts (`nAnalog`, `nDigital`)
* Rolling frame counter (`seq`)
* RunLevel (`runLevel`)
* Status flags (`flags`)
* Payload data (analog and digital channels)

Frames are encoded by `combus_frame_encode()` and decoded by `combus_frame_decode()`.

---

## Future Evolution (ComBus v2)

### Dual‑Frame Architecture

A future evolution may introduce two traffic classes:

```text
Control frame   ← real‑time commands
                  (sticks, buttons, critical states)

Config frame    ← low‑frequency parameters
                  (brightness, trims, persistent settings…)
```

This separation is comparable to the PDO/SDO approach used in CANopen.

Potential benefits:

* Reduced load on real‑time frames;
* Transport of occasional parameters without enlarging control frames;
* Better scalability for future specialized nodes.

The simplest implementation could use periodic multiplexing:

```text
Frame 1 → Control
Frame 2 → Control
Frame 3 → Control
Frame 4 → Config
```

This approach could be realized without major protocol changes, via a frame counter on the transmitter side.

### Typed Channel Groups

Another possible evolution is grouping channels by functional domains instead of keeping them in a flat namespace:

```cpp
MotionComBus motion;
LightComBus  light;
CoreComBus   core;
```

Each domain would have its own identifiers and storage, allowing a node to carry only the groups it actually needs.

Potential benefits:

* Functional groups truly optional;
* More explicit dependencies between modules;
* Elimination of unused slots in certain configurations.

This approach would however require an evolution of the transport protocol to support multiple frame types or a multiplexing mechanism. No decision has been taken at this stage; this path will only be reconsidered if multiple independent modules require truly distinct channel sets.

---

## Why ComBus Matters

ComBus is the cornerstone that enables:

* **Configuration‑driven architecture** — systems are assembled from reusable definitions;
* **Node cooperation** — remotes, machines and modules understand each other's roles;
* **Hardware abstraction** — the same logic runs on different hardware platforms;
* **Extensible behaviors** — new capabilities can be introduced without rewriting the entire system;
* **Future‑proof design** — the architecture can evolve alongside community ideas and technologies.

By providing a common language for the entire ecosystem, ComBus transforms Open RC Node from a collection of independent devices into a cooperative, configurable and evolving RC environment.