# UART Workflow

How a ComBus link over UART is set up on a machine, from the table of the board to `loop()`, and what runs on every cycle. The frame and the handshake are described in their own documents: [data frame](../frame/README.md), [handshake](../handshake/README.md).

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
│    uart_port_register_pool() ──► static pool of UartCtx                  │
│    uart_port_init()          ──► ComPort* (write / readByte / available) │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  PHYSICAL                                                                │
│    HardwareSerial (Serial / Serial1 / Serial2), TX / RX pins (ComPortCfg)│
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

The UART index of a port maps to the ESP32 serial: 0 = `Serial` (USB), 1 = `Serial1`, 2 = `Serial2`.

---

## 3. Initialisation

`setup()` calls `machine_init()` (`src/machines/init/init.cpp`):

| Step | Function | What it does for the link |
|---|---|---|
| 1 | `sys_init()` | pin registry (`pin_reg_init`), then the shared UART pool (`hw_init_com()` → `uart_port_register_pool()`), then the debug serial claims its slot |
| 2 | `hw_init()` | the hardware peripherals (drivers, servos, battery) |
| 3 | `input_init()` | the physical input sources |
| 4 | `output_init()` → `com_init()` → `board_links_init()` | the links of the board, see below |
| 5 | boot run level | `RUNLEVEL` is written to its startup value |

`board_links_init()` (`src/machines/init/com/board_links_init.cpp`) owns the storage of the links (TX state, RX state, runtime, `ComBusLink`, one slot per entry of `comLinks[]`) and calls `combus_links_init()` (`combus_link_setup.cpp`):

1. **`combus_link_check()`**, for each link, before taking any port: a coupled link has its `CouplingDev` and its table; the link type and the role of the end agree; the frame rate is at most 1000 Hz and a receiving end has a loss timeout; the line rate is below the ceiling of the port; the line is not too loaded (see below); a table is used between nodes only, and fits the receive buffers. A refused link stays inactive and is logged; the others start anyway.
2. **Opening the port**: `uart_port_init(serial, rate, txPin, rxPin, name, &pinReg)` gives the `ComPort` of the link. The pin of a direction the end does not use is passed as `-1` and is not reserved. A port already taken is refused. The driver gets a 512-byte transmit ring before `begin()`: a write copies the frame and returns at once, instead of waiting until the whole frame is in the hardware FIFO.
3. **`combus_link_build()`**: fills the `ComBusLink`: layer from the scope, frame sizes from the matching view, TX and RX from the role of the end, the table of the link if any.
4. **TX and RX pools** registered, then **`combus_link_start()`** for each link: the handshake context (its contract), then the TX side and the RX side.
5. **`combus_link_supervisor_start()`**: the links are put under watch; a coupled link starts off.

Nothing is allocated: all the storage is static and belongs to the caller.

### Line Load

A transmitting end on a UART sends its full frame at its frame rate, plus a handshake request and its ACK every 250 ms, 10 bits per byte (start and stop). `combus_link_load_pct()` gives this load in % of the line rate:

```text
load = (frameLen × txHz + 2 × 26 × 1000 / 250) × 10 × 100 / rate
```

| Load | At boot |
|---|---|
| up to 70 % | the link starts |
| above 70 % | the link starts, with a warning in the log |
| above 100 % | the link is refused: the line cannot carry it |

The margin is measured: at 94 % of a 19 200 baud line, the transmit ring fills and a write already blocks the loop for about 40 ms. The real configurations run at 115 200 baud and 50 Hz, about 20 %. The check only covers a UART: another kind of port counts as 0 for now.

The transmit ring removes the blocking of large frames: a 226-byte frame took 20.6 ms to write without it, 0.19 ms with it. It does not save an overloaded line: the ring fills, and the writes block again.

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

---

## 5. Files

| Role | File |
|---|---|
| Links of the board | `src/machines/init/com/board_links_init.cpp` |
| Communication init | `src/machines/init/com/com_init.cpp` |
| UART pool | `src/machines/init/hw/hw_init_com.cpp` |
| Check, build, start | `src/core/system/combus/protocol/link/combus_link_setup.cpp` |
| Watch, loss, safe values | `src/core/system/combus/protocol/link/combus_link_supervisor.cpp` |
| Sending | `src/core/system/combus/protocol/combus_tx.cpp` |
| Reception | `src/core/system/combus/protocol/combus_rx.cpp` |
| UART transport | `src/core/system/hw/transport/uart_port.cpp` |
| Reception and sending in the loop | `src/core/system/com/com_manager.cpp` |

---

## 6. Going Further

- [Links](README.md): describing, starting and watching a link
- [Data frame](../frame/README.md): the format, the path of a frame
- [Handshake](../handshake/README.md): the contract of a link
- [The `both_or` merge](../both_or.md): the anti ping-pong between boards
