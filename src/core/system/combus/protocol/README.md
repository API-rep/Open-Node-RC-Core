# ComBus Protocol

This folder carries the ComBus channels between two boards or two nodes, without knowing anything about the physical medium. It answers three questions: what goes on the wire, do both ends speak the same language, and is the link alive?

## Overview

```text
Machine modules
        │  read and write the channels
        ▼
      ComBus
        │
        ▼
    protocol/        describes the links, sends, receives, watches
        │
        ▼
     NodeCom         write and read interface
        │
        ├── UART
        └── ESP-NOW (to come)
```

## Who Does What

| Element | Role |
|---|---|
| [`frame/`](frame/README.md) | The data frame: encode, decode, apply to the bus |
| [`handshake/`](handshake/README.md) | The contract of the link: do both ends have the same MD5? |
| [`coupling/`](coupling/README.md) | The pairing: who is hitched to whom? |
| `combus_tx.*` | The transmission of each link, at its own rate |
| `combus_rx.*` | The reception of each link: routes the bytes to the right decoder |
| `combus_links.*` | Builds the links from their description, then watches them |
| `combus_protocol.*` | The structures that describe a link, and its initialisation |
| `combus_peer_link_defs.h` | The types of a link between nodes: table and contract, filled by the builder |

## Describing a Link

A link is declared, not programmed:

- `ComDev` describes the link: its scope, direction, activation and rate.
- `ComPort` describes the physical port of the board.
- `ComDevMap` wires a `ComDev` on a `ComPort`, and gives the role of this end (`TX`, `RX` or `TX_RX`).

At startup, `combus_links_init()` validates each `ComDevMap`, opens the port, which gives a `NodeCom`, then starts the link. All storage belongs to the caller and stays static: nothing is allocated.

## The Two Scopes

| Scope of the link | Connects | Channels carried |
|---|---|---|
| `INTER_NODE` | two nodes (remote, trailer...) | `BASE` |
| `INTRA_NODE` | the boards of one node | `SYSTEM`, which includes `BASE` |

`BOARD` channels stay on their board: they never travel.

A link between nodes may also have **its own table** (`ComPeerLink`): this is the case of a link through a coupler and of the link with the remote. It then carries exactly the channels of that table.

## One Loop Cycle

1. **Reception**: `combus_rx_update()` reads the bytes and routes them to the right decoder.
2. **Watch**: `combus_links_update()` follows the coupling and detects the loss of a link. A loss is published in `NODE_LINK_FAIL` or `BOARD_LINK_FAIL`, depending on the scope.
3. **Processing** of the machine.
4. **Transmission**: `combus_tx_update()` sends the handshakes due, then a data frame when its time has come.

## Telling Frames Apart

All frames start with the same header. Two bytes are enough to tell them apart:

| `seq` | `flags` | Frame | Handled by |
|---|---|---|---|
| 1 to 255 | 0 | data | `frame/` |
| 0 | coupling bit set | coupling | `coupling/` |
| 0 | other | handshake | `handshake/` |

## The Rules

- **Transport independence**: the protocol only talks to `NodeCom`, never to `Serial` or another peripheral.
- **One port, one owner**: opening a port already taken is a configuration error, reported and refused.
- **An invalid frame costs only one byte**: the receiver drops that first byte and looks for the start of the next frame, without swallowing a good one.

## Adding a Transport

An adapter that provides a `NodeCom` is enough: `write()`, `readByte()`, `available()`. The rest of the protocol does not change.

## Going Further

- [The data frame](frame/README.md)
- [The handshake](handshake/README.md)
- [Coupling and pairing](coupling/README.md)
- [The `both_or` merge](both_or.md): the anti ping-pong between boards
- [The UART workflow](uart_workflow.md): the init chain, from the build flag to `loop()`
- [The builder](../../../../../scripts/combus_builder/README.md): how the IDs and tables are generated
