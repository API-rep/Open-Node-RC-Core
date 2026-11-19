# ComBus Transmission

The ComBus is a data structure, shared first by the modules of a single board, then by the boards of a node, and between nodes (a remote, a machine, a trailer). This folder holds the transmission protocol that carries it, designed to evolve and to adapt easily.

## The Concept: Declare, Then Choose

Modern hardware is more and more flexible in how its communication ports are configured: on a microcontroller, the same pair of GPIO can now host a UART, an I2C or a CAN controller. To avoid a tangle of crossed configurations, a board **declares its possible ports**, as a catalog. In parallel, the node **declares the links it may need**, without saying over what they run. Then the configuration of each board **pairs** a port with a link, and the pair becomes a working transport interface.

```text
Ports of the board        Links of the node
(what the hardware has)   (what may travel)

  USB                       one-way extension ──┐
  COM/GPIO ──┐              two-way extension   │
  ...        │              ...                 │
             ▼                                  ▼
      ┌────────────────────────────────────────────────┐
      │  Configuration of the board, one line:         │
      │  one-way extension on COM/GPIO, as TX          │
      └────────────────────────────────────────────────┘
```

What it brings:

- **Generic boards.** A board describes what it has, not what it does. The same board serves another machine, or another role, without touching its description.
- **Pluggable transports.** Every port is used through the same few calls, whatever its protocol (UART, ESP-NOW...).
- **A change is one line.** Moving a link to another port, or from a wire to a radio, only changes the configuration.
- **A new kind of port is easy to add.** It only needs its own transport module; the rest of transmission do not change.

## The Layers

The ports and their transports are the ground floor. On top of them sits the protocol stack, and on top of the protocol, the ComBus itself. Like the TCP/IP network stack, the transmission is built as three layered blocks, each one only talking to the one below:

```text
                ┌──────────────────────────────────────┐
  Data          │  ComBus: the channels of the modules │
                └──────────────────┬───────────────────┘
                ┌──────────────────┴───────────────────┐
  Protocol      │  Contract       (handshake)          │
                │  Pairing        (coupling)           │
                │  Frame                               │
                │  Transmission   (TX, RX)             │
                └──────────────────┬───────────────────┘
                                   │  link: maps, starts, watches
                ┌──────────────────┴───────────────────┐
  Hardware      │  Transport      (ComPort)            │
                │  Port           (UART, ...)          │
                └──────────────────────────────────────┘
```

**Data.** The [ComBus](../README.md) holds the channels that the modules read and write. It knows nothing of the transmission stack, not even that it can travel through it.

**Protocol.** It carries the channels, whatever the communication hardware:

- the **contract** checks that both ends have compatible firmwares and share the same channels, before any data is applied ([handshake](handshake/README.md));
- the **pairing** makes sure that a coupled link talks to the element actually hitched to it, before the link opens ([coupling](coupling/README.md));
- the **frame** turns the channels into bytes ready to send, and the bytes received back into channels ([frame](frame/README.md));
- the **transmission** decides what leaves and what arrives on each link, and exchanges the bytes with the hardware (`combus_tx.*`, `combus_rx.*`, [below](#sending-and-receiving-the-combus)).

**Hardware.** It moves the bytes through the ports of the board ([the hardware](../../hw/README.md)):

- the **transport** is reached through a **`ComPort`**: the same few calls (`write()`, `readByte()`, `available()`), whatever the hardware behind it;
- the **port** is set up at boot by the module of its kind (UART today, ESP-NOW to come), which keeps the details of the hardware behind these calls.

Behind the scenes, a [link](link/README.md) joins the **protocol** and the **hardware** for each connection, so that nothing is hard wired in the code.
- As a **mapper**, it follows the configuration and puts a logical link on an available port.
- As an **orchestrator**, it has that port set up and starts the protocol on it (contract, TX, RX), then watches the connection and reports its loss to the failsafe.

## Sending and Receiving the ComBus

Two modules do the actual work, on every loop cycle: `combus_tx` sends, `combus_rx` receives. Each one serves all the links of the board in turn, every link at its own rate. Neither of them ever waits: when nothing is due, or nothing has arrived, it moves on, so the rest of the loop is never held up.

### One Loop Cycle

1. **Reception**: `combus_rx_update()` reads the bytes of each link and routes them to the right decoder.
2. **Watch**: `combus_link_supervisor_update()` follows the coupling and detects the loss of a link.
3. **Processing** of the machine.
4. **Transmission**: `combus_tx_update()` sends what is due on each link.

### Sending

On each link, `combus_tx_update()` first sends the handshakes due: an ACK to answer, the burst of the start, the periodic request. Then, when the period of the link has elapsed, it encodes a data frame and sends it.

A two-way end sends no data as long as its contract is not established: the other end could not use it. See [the contract](handshake/README.md).

### Receiving

On each link, `combus_rx_update()` moves the available bytes into a ring buffer, then reads it frame by frame. The header of each frame says which decoder takes it: data, handshake or coupling ([telling frames apart](frame/README.md#telling-frames-apart)).

Two guards protect the bus:

- **A corrupted frame costs one byte**: the receiver drops it and looks for the start of the next frame, without swallowing a good one.
- **Old bytes are never applied**: when the loop comes back later than the loss timeout of a link, the bytes waiting are at least that old. They are dropped, and the link is lost by its silence rather than fed with stale values.

A data frame received under an established contract goes to the bus; the others are ignored.

## Going Further

- [Linking nodes](../../../../../doc/linking_nodes.md): the concepts behind the links
- [The `both_or` merge](both_or.md): a fault shared by several boards, without ping-pong
- [The UART workflow](link/uart_workflow.md): the init chain, from the board table to `loop()`
- [The remote](../../../../remotes/README.md): the firmware of the remote and its link to the machine
