# UART Port

The UART adapter sets up a UART port of the controller and drives it behind a `ComPort`: everything specific to a UART, `HardwareSerial` included, stays inside it.

## Declaring a UART Port

A board declares a UART port with `ComPortCfg::makeUart()` (see [the hardware](../README.md#declaring-a-port)). Its index is the UART peripheral of the controller: `Serial`, `Serial1` or `Serial2`. The first one is usually the USB and the debug serial.

## Setting Up

All the UART ports of the program share one pool of slots, owned by the machine and registered once at boot, from `hw_init_com()`. When the debug is on, the debug serial claims its slot first.

A link then sets up its port with `uart_port_init()`, which returns the `ComPort` of the link:

- **Only the pins in use are taken.** The others are left free, for the next modules to claim.
- **A port is claimed once.** A second claim of the same port is refused, and so is any claim once the pool is full.
- **The driver gets a transmit buffer** before it starts.

## Reading and Writing

Both go through a buffer, so that the main loop never waits on the line:

- **Writing** copies the frame into the transmit buffer and returns at once. Only an overloaded line, which fills the buffer, can block it again.
- **Reading** takes the bytes from the driver in blocks, much faster than one by one.

## Line Load

At boot, a link is checked against the line that carries it: what a transmitting end sends, its frames and its handshakes, is weighed against the line rate (`combus_link_load_pct()`). Two limits:

- close to the line rate, the link starts with a **warning** in the log;
- above the line rate, the link is refused with an **error**: the line cannot carry it.

The margin of the warning is measured: near the full load, the transmit buffer fills, and the writes already block the loop. The check only covers a UART: another kind of port counts as unloaded for now.

## Going Further

- [The hardware](../README.md): the ports, the adapters and the `ComPort`
- [The link manager](../../combus/protocol/link/README.md#at-boot-the-setup): where the port is set up
