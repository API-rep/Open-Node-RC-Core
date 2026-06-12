# Communication Protocol Layer

The protocol layer provides transport-independent communication services used throughout Open RC Node.

Its primary role is to decouple application logic from the underlying communication technology. Whether data is transported over UART, ESP-NOW, Bluetooth or another medium, higher-level components interact through a common interface.

This abstraction allows ComBus services to remain independent from hardware-specific implementations.

---

## Architecture Overview

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
        ├── ESP-NOW (future)
        ├── Bluetooth (future)
        └── Other transports
```

---

## Components

### NodeCom

`NodeCom` is the generic communication interface used throughout the system.

It exposes a small set of transport-independent operations through a function pointer table:

- Write data
- Read data
- Check data availability

Higher-level services never interact directly with UART, Bluetooth or other hardware interfaces. They communicate exclusively through a `NodeCom` instance.

---

### ComBus TX

Responsible for:

- Frame generation
- Packet serialization
- Transport-independent transmission

The transmitter operates exclusively through a `NodeCom` interface.

---

### ComBus RX

Responsible for:

- Frame reception
- Packet decoding
- Validation
- Dispatching received data

The receiver operates exclusively through a `NodeCom` interface.

---

## Design Rules

### Single Initialization Rule

A communication port must only be initialized once.

```cpp
NodeCom* com = uart_com_init(...);
```

Ownership of the port is assigned during initialization.

Attempting to initialize an already claimed port is considered a configuration error.

---

### Transport Independence

Communication services must never depend on transport-specific APIs.

Good:

```cpp
combus_tx_init(nodeCom);
```

Bad:

```cpp
Serial.begin(...);
combus_tx_init(...);
```

Transport configuration belongs exclusively to the communication adapter layer.

---

### Port Ownership Guard

The UART implementation maintains a static port allocation table.

If a port is already claimed:

- initialization fails;
- a fatal error is reported;
- no duplicate access is permitted.

This prevents multiple services from unintentionally sharing the same hardware interface.

---

## Adding a New Transport

Supporting a new communication medium only requires a new adapter implementing the `NodeCom` interface.

Examples:

```text
ports/espnow_com/
ports/bluetooth_com/
ports/canbus_com/
```

Required operations:

- write()
- readByte()
- available()

Once exposed through a `NodeCom` instance, existing ComBus services can use the new transport without modification.

---

## Design Goals

- Transport-agnostic communication
- Clear separation between hardware and application layers
- Reusable communication services
- Support for multiple transport technologies
- Safe ownership of hardware communication resources