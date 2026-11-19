# Hardware

This folder is what the core knows of the hardware of a board: its communication ports, its devices (`dev/`), its drivers (`drv.h`) and the registry of its pins (`pin_reg.*`).

Only the communication ports are described here for now.

## Communication Ports

### Declaring a Port

A **`ComPortCfg`** ([`com_port.h`](com_port.h)) describes one communication port of a board: its name, its kind (`ComPortType`) and the settings of that kind. It is purely descriptive and lives in flash: declaring a port does not start it.

The board lists all its ports in a table, `comPorts[]` of its `envCfg.cpp`, even those that no link uses yet. Each port is created with the factory of its kind:

```cpp
const ComPortCfg comPorts[] = {
	ComPortCfg::makeUart("USB",      { 0, Txd0Pin,   Rxd0Pin,   UartMaxBaud }),   // UART0, debug serial
	ComPortCfg::makeUart("COM/GPIO", { 2, TxdExtPin, RxdExtPin, UartMaxBaud }),   // UART2, extension port
};
```

| Kind | Settings | Fields |
|---|---|---|
| `UART` | `UartPortCfg` | `index` (peripheral: 0, 1 or 2), `txPin`, `rxPin` (-1 = not wired), `maxBaud` (highest rate of the port) |

The name is the one printed on the board, so that the logs point to the right connector. A link names the port it uses in its `ComDevMap`; a rate above `maxBaud` is refused at boot.

*To come*: the configuration of the boards will move to configuration files, like the channels in `.cb` files. The ports will then be declared there.

### Transport Adapters

A transport adapter sets up one kind of port and drives it. It is the only code that knows the medium.

| Kind | Adapter | Where |
|---|---|---|
| `UART` | `uart_port_init()` | [`transport/uart_port.h`](transport/uart_port.h) |

At boot, the link calls the adapter of its port, with the line rate of the link and only the pins its end needs: a transmit-only end does not take the RX pin. The adapter claims the port and its pins: a port claimed twice is refused. It then returns a `ComPort`.

The UART adapter reads the driver in blocks of 64 bytes, then serves them one by one. Byte by byte, the driver is slow once its buffer is full: after a stall of the loop, 380 bytes took 130 ms instead of 2.7 ms.

### ComPort

A **`ComPort`** ([`com_port.h`](com_port.h)) is the accessor of a port once set up: a small structure of functions, filled by the adapter.

| Function | Role |
|---|---|
| `write()` | send bytes |
| `readByte()` | read one byte (-1 when empty) |
| `available()` | number of bytes waiting |
| `setPeer()` | optional: the only peer to deliver frames from, on a transport with several peers |

Each link keeps its `ComPort` for the whole run, and the [ComBus transmission](../combus/protocol/README.md) goes through it alone:

- the reception reads all the bytes from it, then hands each frame to its decoder;
- the data frames, the handshakes and the coupling frames are written to it;
- the pairing sets its peer, once a coupled link is hitched, and clears it when it is unhitched.

None of them touches anything else of the hardware, so a new medium only needs a new adapter.

### Adding a Kind of Port

1. One value in `ComPortType`.
2. One settings structure, and its member in the union of `ComPortCfg`, with its factory.
3. One adapter in `transport/`, which returns a `ComPort`.
4. One case where the link sets up its port (`setupPort()` in `combus_link_setup.cpp`).

## Going Further

- [ComBus transmission](../combus/protocol/README.md): what travels over the ports
- [The links](../combus/protocol/link/README.md): how a link names its port and starts
