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
- **Pluggable transports.** Every port is used through the same few calls, a standard interface, whatever its protocol (UART, ESP-NOW...).
- **A change stays small.** Moving a link to another port, or from a wire to a radio, only changes a few lines of the configuration.
- **A new kind of port is easy to add.** It only needs its own transport module; the rest of the transmission does not change.

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
- the **transmission** decides what leaves and what arrives on each link, and exchanges the bytes with the hardware (`combus_tx.*`, `combus_rx.*`, [below](#receiving-and-sending-the-combus)).

**Hardware.** It moves the bytes through the ports of the board ([the hardware](../../hw/README.md)):

- the **transport** is reached through a **`ComPort`**: the same few calls (`write()`, `readByte()`, `available()`), whatever the hardware behind it;
- the **port** is set up at boot by the module of its kind (UART today, ESP-NOW to come), which keeps the details of the hardware behind these calls.

Behind the scenes, the [link manager](link/README.md) ties the **protocol** to the **hardware**, connection by connection, at two moments:

- **At boot, the setup** follows the configuration: it puts each logical link on its port, has the port set up, and starts the protocol on it (contract, TX, RX).
- **While it runs, the supervisor** follows each connection and reports its loss to the failsafe.

This way, nothing is hard wired in the code, and the transmission stack stays modular.

## Receiving and Sending the ComBus

On every loop cycle, the [`com_manager`](../../com/com_manager.h) calls the transmission twice: at the beginning of the loop, to receive and supervise the links, and at the end, to send.

```text
  ┌────────────────────────────────────────┐
  │                RECEIVE                 │◄──┐
  │   - read the links                     │   │ loop()
  │   - apply the frames                   │   │
  │   ┌────────────────────────────────┐   │   │
  │   │        LINK SUPERVISOR         │   │   │
  │   │   - follow each link's health  │   │   │
  │   └────────────────────────────────┘   │   │
  └───────────────────┬────────────────────┘   │
                      ▼                        │
  ┌───────────────────┴────────────────────┐   │
  │       PROCESSING OF THE MACHINE        │   │
  └───────────────────┬────────────────────┘   │
                      ▼                        │
  ┌───────────────────┴────────────────────┐   │
  │                  SEND                  │   │
  │   - send what is due on each link      │───┘
  └────────────────────────────────────────┘
```

Each pass processes all the links of the board in turn, every link at its own rate, and never waits: when nothing is due, or nothing has arrived, it moves on, so the rest of the loop is never held up.

### Receiving

On each link, `combus_rx_update()` simply reads all the bytes waiting on the port, and cuts them into frames. Each frame goes to its decoder: data, [handshake](handshake/README.md) or [coupling](coupling/README.md).

Before any value reaches the bus, a few health checks:

- **A damaged frame is dropped**, and the reading goes on with the next one.
- **Stale bytes are dropped**: after a long stall of the loop, a link is lost by its silence rather than fed with old values.
- **No data without a contract**: data frames are only applied once both ends agree on their channels ([handshake](handshake/README.md)).

Once every link is read, the **link supervisor** (`combus_link_supervisor_update()`) brings the state of each link up to date: it restores the safe values of a link that stops being alive, and reports its loss to the failsafe module.

### Sending

On each link, `combus_tx_update()` sends two kinds of frames:

- **Handshakes**, at the start and then regularly, for as long as the link runs. They check that both ends have compatible firmwares and the same set of ComBus channels.
- **Data frames**, at the rate of the link.

If they agree, the data flows. If not, the data is held back until they do.

## Going Further

- [Linking nodes](../../../../../doc/linking_nodes.md): the concepts behind the links
- [The link manager](link/README.md): describing, setting up and supervising a link
- [The hardware](../../hw/README.md): the ports and their transports
- [The handshake](handshake/README.md): what both ends check, and when
- [The failsafe](../../failsafe/failsafe_module.md): what happens when a link is lost
