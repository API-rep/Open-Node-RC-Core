# UART Port

The UART adapter sets up a UART port of the ESP32 and drives it behind a `ComPort`. It is the only code of the ComBus transmission that knows `HardwareSerial`.

## Declaring a UART Port

A board declares a UART port with `ComPortCfg::makeUart()` and its `UartPortCfg` (see [the hardware](../README.md#declaring-a-port)). The index of the port is the ESP32 peripheral:

| Index | Serial | Usual role |
|---|---|---|
| 0 | `Serial` | USB, debug serial |
| 1 | `Serial1` | |
| 2 | `Serial2` | extension port |

## Setting Up

All the UART ports of the program share one pool of slots (`UartCtx`), owned by the machine and registered once at boot: `hw_init_com()` calls `uart_port_register_pool()` from `sys_init()`. The debug serial claims its slot first.

A link then sets up its port with `uart_port_init(serial, rate, txPin, rxPin, name, &pinReg)`, which returns the `ComPort` of the link:

- **Only the pins of the end are taken.** The pin of a direction the end does not use is passed as `-1`, and is not reserved.
- **A port is claimed once.** A second claim of the same port is refused, and so is any claim once the pool is full.
- **The driver gets a transmit ring** of 512 bytes before `begin()`. A write copies the frame and returns at once, instead of waiting until the whole frame is in the 128-byte FIFO of the hardware.

## Reading and Writing

- **Writing**: a 226-byte frame took 20.6 ms to write without the transmit ring, 0.19 ms with it. The ring does not save an overloaded line: once it is full, the writes block again.
- **Reading**: the adapter reads the driver in blocks of 64 bytes, then serves them one by one. Byte by byte, the driver is slow once its buffer is full: after a stall of the loop, 380 bytes took 130 ms instead of 2.7 ms.

## Line Load

At boot, a link is checked against the line that carries it. A transmitting end on a UART sends its full frame at its frame rate, plus a handshake request and its ACK every 250 ms, 10 bits per byte (start and stop). `combus_link_load_pct()` gives this load in % of the line rate:

```text
load = (frameLen × txHz + 2 × 26 × 1000 / 250) × 10 × 100 / rate
```

| Load | At boot |
|---|---|
| up to 70 % | the link starts |
| above 70 % | the link starts, with a warning in the log |
| above 100 % | the link is refused: the line cannot carry it |

The margin is measured: at 94 % of a 19 200 baud line, the transmit ring fills and a write already blocks the loop for about 40 ms. The real configurations run at 115 200 baud and 50 Hz, about 20 %. The check only covers a UART: another kind of port counts as 0 for now.

## Files

| Role | File |
|---|---|
| The adapter | `uart_port.h`, `uart_port.cpp` |
| The pool of the machine | `src/machines/init/hw/hw_init_com.cpp` |
| The line load | `combus_link_load_pct()`, `src/core/system/combus/protocol/link/combus_link_setup.cpp` |

## Going Further

- [The hardware](../README.md): the ports, the adapters and the `ComPort`
- [The link manager](../../combus/protocol/link/README.md#at-boot-the-setup): where the port is set up
