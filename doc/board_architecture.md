# Board Architecture — Category, Board Configuration, Functions and Peripherals

This document defines the build-flag contract that describes what a given firmware *is*: which kind of node it belongs to, which board configuration it compiles, which functions it runs, and how fixed-function hardware modules attach to it.

Refer to [core_architecture.md](core_architecture.md) for the repository-level organization this document builds on, and to [nodes.md](nodes.md) for what a node and its boards are.

---

## 1. Node Category

*Which kind of node is this?*

* **`IS_MACHINE`** — a node that is driven (`src/machines/`)
* **`IS_REMOTE`** — a node that drives (`src/remotes/`)

Exactly one per firmware: the ComBus Builder hook checks it (`links.node_category()`).

---

## 2. Board Configuration

*Which configuration of the machine does this firmware compile?*

* **`IS_MAINBOARD`** — the main board of the machine: `config/machines/<machine>/board/mainboard/`
* **`IS_EXT_BOARD`** — an extension board of the machine: `config/machines/<machine>/ext_board/`

Then `-D BOARD_<NAME>` selects the hardware of that board (pins, ports, drivers), for example `BOARD_ESP32_8M_6S`.

These two flags only choose a configuration tree. They no longer give a ComBus role: every board is a peer of its node's ComBus, and owns the channels of the modules it hosts. No extension board configuration exists yet.

---

## 3. Functions of a Board

*What does this board run?*

What a board does is given by functional flags, not by its configuration:

| Flag | The board |
|---|---|
| `HAS_INPUT_PROC` | runs the input chains (buttons → states, run level) |
| `HAS_SIM_PROC` | runs the simulation chains |
| `HAS_RUNLEVEL_MANAGER` | owns the `RUNLEVEL` channel (one board per machine, the main board by convention) |
| `HAS_COUPLER_SOCKET_<NAME>`, `HAS_COUPLER_PLUG_<NAME>` | carries a side of a coupler |
| `HAS_<SOURCE>_FAILSAFE` | writes a failsafe contributor |

A `SYSTEM` channel produced by one of these functions is declared with its flag in `requires`, and reaches the other boards of the node by diffusion.

---

## 4. Peripherals — Fixed-Function Plug-In Modules

A further category exists below "board": hardware with a fixed function acting as a plug-and-play device. Typically a specialized IC such as an RF receiver or an IO expander.

* **Board** — has its own MCU, runs its own compiled firmware, participates in the ComBus of its node.
* **Peripheral** — fixed-function hardware module, reusable by any board.

Peripherals will follow a dedicated prefix-flag pattern, distinct from the per-domain flags such as `INPUT_*`:

```cpp
#define PERIPH_<NAME>
```

Peripherals are intentionally kept fixed-function, to avoid growing their own sub-configuration system. The `PERIPH_` prefix is reserved: no peripheral uses it yet.

---

## 5. Flag Naming Convention Summary

| Flag | Axis | Set in platformio.ini |
|---|---|---|
| `IS_MACHINE` / `IS_REMOTE` | node category | yes, exactly one |
| `IS_MAINBOARD` / `IS_EXT_BOARD` | board configuration tree | yes, on a machine board |
| `BOARD_<NAME>` | board hardware | yes |
| `HAS_<FUNCTION>` | functions of the board | yes |
| `PERIPH_<NAME>` | fixed-function peripheral (reserved) | — |
