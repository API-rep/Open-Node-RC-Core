# Link Workflow

How the links of a machine are set up, from the table of the board to `loop()`, and what runs on every cycle. What is specific to a kind of port is in the document of its adapter: [UART](../../../hw/transport/uart_port.md).

---

## 1. The Four Layers

```text
┌──────────────────────────────────────────────────────────────────────────┐
│  MACHINE                                                                 │
│    com_init()  ──►  board_links_init()  ──►  combus_links_init()         │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  PROTOCOL                                                                │
│    combus_link_start(), combus_link_supervisor_start()                   │
│    combus_rx_update(), combus_link_supervisor_update(),                  │
│    combus_tx_update()                                                    │
│    combus_frame_encode() / combus_frame_decode() / combus_frame_apply()  │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  TRANSPORT (ComPort)                                                     │
│    the adapter of the kind of port: UART today (uart_port_init())        │
│    ──► ComPort* (write / readByte / available)                           │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  PHYSICAL                                                                │
│    the peripheral and its pins, described by the ComPortCfg              │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Where a Link Is Declared

| What | Where |
|---|---|
| the logical links of the node (`ComDev`) and their protocol (`ComProtocolCfg`) | `machines/config/machines/<machine>/board/com_dev.h` |
| the ports of the board (`ComPortCfg`) | `comPorts[]`, `envCfg.cpp` of the board |
| the links of the board (`ComDevMap`: a `ComDev` on a port, with the role of this end) | `comLinks[]`, `envCfg.h` of the board |

On the Volvo A60H, the main board wires the extension link (`kComDevExtUni`) on its `COM/GPIO` port (UART2), transmit only. To wire it both ways, use `kComDevExtBi` with `LinkEnd::TX_RX`.

---

## 3. Initialisation

`setup()` calls `machine_init()` (`src/machines/init/init.cpp`):

| Step | Function | What it does for the link |
|---|---|---|
| 1 | `sys_init()` | pin registry (`pin_reg_init`), then the pools of the transports (UART: `hw_init_com()`), then the debug serial claims its slot |
| 2 | `hw_init()` | the hardware peripherals (drivers, servos, battery) |
| 3 | `input_init()` | the physical input sources |
| 4 | `output_init()` → `com_init()` → `board_links_init()` | the links of the board, see below |
| 5 | boot run level | `RUNLEVEL` is written to its startup value |

`board_links_init()` (`src/machines/init/com/board_links_init.cpp`) owns the storage of the links (TX state, RX state, runtime, `ComBusLink`, one slot per entry of `comLinks[]`) and calls `combus_links_init()` (`combus_link_setup.cpp`):

1. **`combus_link_check()`**, for each link, before taking any port: it refuses a description that cannot work (the list is in [the links](README.md#at-boot)), and checks the load of the line ([UART](../../../hw/transport/uart_port.md#line-load)). A refused link stays inactive and is logged; the others start anyway.
2. **Setting up the port**: `setupPort()` calls the adapter of the kind of port, which gives the `ComPort` of the link. Only the pins of the directions the end uses are taken, and a port already taken is refused.
3. **`combus_link_build()`**: fills the `ComBusLink`: layer from the scope, frame sizes from the matching view, TX and RX from the role of the end, the table of the link if any.
4. **TX and RX pools** registered, then **`combus_link_start()`** for each link: the handshake context (its contract), then the TX side and the RX side.
5. **`combus_link_supervisor_start()`**: the links are handed to the supervisor; a coupled link starts off.

Nothing is allocated: all the storage is static and belongs to the caller.

---

## 4. One Loop Cycle

```text
loop()
 ├─ sys_manager_update(comBus)            src/machines/system/sys_manager.cpp
 │    ├─ com_rx_update(bus)               src/core/system/com/com_manager.cpp
 │    │    ├─ combus_rx_update()          read the bytes, decode, apply the frames,
 │    │    │                              then both_or: host value OR the live links
 │    │    └─ combus_link_supervisor_update(bus)
 │    │                                   coupling, loss, safe values,
 │    │                                   NODE_LINK_FAIL / BOARD_LINK_FAIL
 │    ├─ input_update, vbat_update
 │    └─ failsafe chains (hard, then soft)
 ├─ processor chains and run level state machine
 └─ output_update(comBus)                 src/core/system/output/output_manager.cpp
      └─ com_tx_update(bus)
           └─ combus_tx_update(&bus)      handshakes due, then a data frame when its time has come
```

Each link keeps its own timer: `combus_tx_update()` never blocks, and does nothing without a sending link.

### Inside the Reception

On each link, `combus_rx_update()` moves the available bytes into a ring buffer, then reads it frame by frame:

- **A corrupted frame costs one byte**: the receiver drops the first byte and looks for the start of the next frame, without swallowing a good one.
- **A late pass drops its backlog**: when the loop comes back later than the loss timeout of a link, the bytes waiting are at least that old. They are dropped, with a warning in the log, and the link is lost by its silence rather than fed with stale values.

---

## 5. Files

| Role | File |
|---|---|
| Links of the board | `src/machines/init/com/board_links_init.cpp` |
| Communication init | `src/machines/init/com/com_init.cpp` |
| Pools of the transports | `src/machines/init/hw/hw_init_com.cpp` |
| Check, set up, build, start | `src/core/system/combus/protocol/link/combus_link_setup.cpp` |
| Supervision, loss, safe values | `src/core/system/combus/protocol/link/combus_link_supervisor.cpp` |
| Sending | `src/core/system/combus/protocol/combus_tx.cpp` |
| Reception | `src/core/system/combus/protocol/combus_rx.cpp` |
| Transport adapters | `src/core/system/hw/transport/` (UART: `uart_port.cpp`) |
| Reception and sending in the loop | `src/core/system/com/com_manager.cpp` |

---

## 6. Going Further

- [Links](README.md): describing, starting and supervising a link
- [Data frame](../frame/README.md): the format, the path of a frame
- [Handshake](../handshake/README.md): the contract of a link
- [UART port](../../../hw/transport/uart_port.md): the UART adapter, its buffers and the line load
- [The `both_or` merge](../both_or.md): the anti ping-pong between boards
