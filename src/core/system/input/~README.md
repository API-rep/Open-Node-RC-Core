# Command Sources

*Draft.* A machine can be driven by the remote of the project, by a controller plugged into the machine, tomorrow by an RC receiver. Seen from the machine, they all do the same thing: they bring commands. This folder holds what they share, so that each of them is only a small module, and the machine never needs to know which one drives.

The design and its history are in [WIP - Local remote](<../../../../doc/WIP - Local remote.md>).

---

## The Concept

* **The inputs of the machine type** (`*_STICK`, `*_BTN`) are where the commands enter: the commands as the machine receives them, before its processing chains. They are `BOARD` channels, declared by the machine type under `requires: [HAS_REMOTE]`: they only exist on the board that acquires the commands, and never travel between boards.
* **One source is one module**: the controller, the remote, a receiver. Each module says whether its source is alive, and its **mapper** writes the inputs from what the source gives.
* **One source alive drives.** When none is alive, or when several are, no source drives: the inputs go back to their safe value (stick at neutral, button released) and the loss is raised (`NODE_LINK_FAIL`). Two sources alive at once never drive the machine: nobody can know which one the driver holds.
* **The outputs do not change.** The chains of the board turn the inputs into the commands of the machine type (`STEERING_BUS`, `THROTTLE_BUS`...), which the other boards of the node receive as before.

```text
  PS4 controller ─► backend ─► reframer ─► mapper ─┐
                                                   ├─► base: one source alive ─► *_STICK, *_BTN ─► input chains ─► *_BUS
  remote ═link═► snapshot of the link ─► mapper ───┘
```

---

## The Base

The base (`remote_source.*`) asks each source, on every cycle, whether it is alive, then lets the only one alive write the inputs. It has no state of its own: the caller owns the sources (at most `kRemoteSourceMaxCount`) and their watch.

A source, for the base, is two calls (`RemoteSourceDev`):

* `poll()`: read the source, say whether it is alive. Called for every source, on every cycle.
* `apply()`: write the inputs. Called only for the source that drives, right after.

A source that is not alive keeps its hands off the inputs: the base writes their safe value.

**The loss.** On a board that compiles a source module, the base writes `NODE_LINK_FAIL`: no source, or several. It also folds in the loss of the other links to another node (`combus_links_lost()`). On any other board, the link supervisor writes it, as before.

**In the loop**, the base runs right after the reception, before the input chains:

```text
com_rx_update()  ──►  input_sources_update()  ──►  battery, failsafe chains, input chains...
  the links             the base: one source
  are read              writes the inputs
```

---

## A Module: Backend, Reframer, Mapper

The generic module (`remote_module.*`) is made of three layers:

| Layer | Role |
|---|---|
| **Backend** | reads the device and writes the channels of the source, already scaled to the bus; says whether the source is connected |
| **Reframer** | optional: a chain of the `source` stage (deadband, centre, expo), stateless processors only, run after each read that succeeded |
| **Mapper** | a table, from the channels of the source to the inputs of the type |

**The channels of a source** are `BOARD` channels of the `source` theme, under the flag of the module (`PS4_LX`, `PS4_CROSS`...). Their names never end with `_STICK` or `_BTN`, which are kept for the inputs of the type.

**The read rhythm.** A module reads its source once per period, at most `kRemoteSourceMaxHz` (100 Hz: a period of at least 10 ms, checked at build time by `remote_period_ok()`). The reframer runs once per sample, never between two: its cost follows the rhythm of the source, not the speed of the loop.

**The mapper** is a configuration, a simple switchboard: one row per input, `{ channel of the source, input, inversion }`. The scaling belongs to the backend, the framing to the reframer, the behaviour of the machine to its chains.

* The machine type gives a table for each source (`core/config/machines/<type>/inputs/`, `ps4_map.cpp` for the PS4). An instance may replace it whole with its own.
* The module only reads its table in RAM. The single door is `remote_map_apply()`, at boot: it checks the whole table, then copies it at once. A table is refused when it is too long, when a row reads a channel that is not on the bus or writes a channel that is not an input of this board, or when two rows share a channel or an input. A refused table leaves the source refused, with its cause in the log: it is never alive.
* An input without a row keeps its safe value.

---

## The PS4 Controller

The PS4 module (`ps4/`, `-D HAS_REMOTE_PS4`) is a backend on top of the PS4 library, over Bluetooth. It is thin on purpose: Bluetooth is bound to leave, one radio per configuration.

On every read, the backend writes all the channels of the source (`ps4.cb`):

* a stick: to `[0..max]`, neutral at the centre, with a deadband around it;
* an analog trigger (L2, R2): to `[neutral..max]`, neutral when released, with a deadband above the rest, so that the safe value of every analog channel stays the neutral;
* a button: as read; the triggers also give `PS4_L2_PRESSED` and `PS4_R2_PRESSED`.

Not connected, every channel takes its safe value, and the source is not alive.

---

## The Remote of the Project

The remote is another node: its commands arrive through a real link (UART today, ESP-NOW to come), with everything a link between nodes has: handshake, contract, age, loss. Its module (`remote_node/`, `-D HAS_REMOTE_NODE`) does not use the generic module: the link already plays the role of the backend.

**The link of a command source writes no bus.** Its `ComDev` is marked `source`: no target bus is given to it at setup. Its frames stay in the snapshot of the link, and the module reads them:

* **alive**: the link is `ALIVE`, with a valid snapshot;
* **mapper**: the table the ComBus Builder generates for the link (`kRemoteLink`), applied to the snapshot by `combus_rx_apply_snapshot()`, with the identity of the board (`BOARD`), so that it reaches the inputs. It is the same code as the one a link uses on its target bus: only the bus and the identity differ;
* **at the loss**: the module sends every position it receives back to its safe value, the commands relayed by a coupler included. The base takes back the inputs only.

The link supervisor leaves a source link out of `combus_links_lost()` and out of its safe values: the base and the module take care of both.

---

## Configuring a Board

| Flag | Meaning |
|---|---|
| `HAS_REMOTE` | the board acquires the commands: it carries the inputs of the machine type, one board per machine |
| `HAS_REMOTE_PS4` | the PS4 module; needs `HAS_REMOTE` |
| `HAS_REMOTE_NODE` | the module of the remote of the project, with its link marked `source`; needs `HAS_REMOTE` |

The list of the inputs of the board is generated by the ComBus Builder (`kRemoteAnalogInputs`, `kRemoteDigitalInputs`, in `combus_peer_links.h`), and used by the base and by the checks of the mapper. The machine wires its sources in `src/machines/system/input/input_sources.*`, which also tells the dashboards which source drives (`input_sources_driver()`).

---

## Files

| File | Role |
|---|---|
| `remote_source.h`, `.cpp` | the base: one source alive drives, the loss |
| `remote_module.h`, `.cpp` | the generic module: backend, reframer, mapper, read rhythm |
| `remote_map.h`, `.cpp` | the mapper: the table in RAM and its checks |
| `ps4/ps4_backend.h`, `.cpp`, `ps4/ps4.cb` | the PS4 backend and the channels of its source |
| `remote_node/remote_node_source.h`, `.cpp` | the module of the remote of the project |

---

## Open Points

* **The identity of the writes** (`layerOk()`): with command sources, it only filters the links, which already write only the positions of their table. To be examined at the final audit of the link rework.
* **What the remote receives back**: today the link only carries what the remote sends. To be seen with the firmware of the remote.
* **Changing a mapper while running**: when a change is allowed, who calls the door, and where the table is kept across reboots.

## Going Further

* [WIP - Local remote](<../../../../doc/WIP - Local remote.md>): the design, what was set aside, the steps.
* [Linking nodes](../../../../doc/linking_nodes.md): the remote among the other nodes.
* [Remote](../../../remotes/README.md): the firmware of the remote.
