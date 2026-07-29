# Board Architecture — Roles, Peripherals, and Ecosystem Category

This document defines the vocabulary and build-flag contract used to
describe what a given firmware actually *is*: which part of the
ecosystem it belongs to, which physical role it plays within that part,
and how fixed-function hardware modules attach to it.

Refer to `core_architecture.md` for the repository-level organization
this document builds on top of.

---

## 1. Two Independent Axes

Every firmware build resolves two independent questions, each governed
by its own pair of mutually-exclusive `IS_*` flags.

### 1.1 Ecosystem Category

*Which kind of node is this?*

* **`IS_MACHINE`** — defines a `machine` node sub project
* **`IS_REMOTE`** — defines a `remote` node sub project

This is the axis already documented in `core_architecture.md`
(`src/machines/`, `src/remotes/`, ...). It answers "what does this
firmware control" — a vehicle, or a handheld controller.

### 1.2 Board Role

*What is this firmware's position within its own ecosystem category?*

* **`IS_MAINBOARD`** — the decision-making board for its node. Hosts
  the authoritative ComBus instance for that node. Exactly one
  mainboard per node.
* **`IS_EXT_BOARD`** — a child board attached to a mainboard, with its
  own compiled firmware, its own MCU, participating in the same node's
  ComBus as a secondary participant rather than the authority.

This axis is orthogonal to ecosystem category. A sound module is the
canonical example: depending on the concrete vehicle, the exact same
sound board could conceivably be wired as the machine's mainboard or
as an extension board — the role is a contract decision, not a
hardware limitation of the board itself.

### Combined Contract

A firmware always resolves both axes: its node type in the ecosystem
and its board role in the node.

---

## 2. Peripherals — Fixed-Function Plug-In Modules

A third category exists below "board": hardware with a fixed function
acting as plug-and-play devices. Typically a specialized IC such as
RF receiver, IO expander, etc.

* **Board** — has its own MCU, runs its own compiled firmware, carries
  `IS_MAINBOARD`/`IS_EXT_BOARD`, participates in ComBus as a node.
* **Peripheral** — fixed-function hardware module, reusable by any 
  ecosystem board regardless of its role.

Peripherals follow a dedicated prefix-flag pattern (PERIPH_), distinct
from per-domain flags.

```cpp
#define PERIPH_<NAME>
```

Naming: the `PERIPH_` prefix is reserved for this category, distinct
from `IS_MAINBOARD`/`IS_EXT_BOARD` and from
per-domain flags such as `INPUT_*` (see `coding_umbrella.md`).
Peripherals are intentionally kept fixed-function to avoid growing
their own sub-configuration system.

---

## 3. Flag Naming Convention Summary

| Prefix | Axis | Set directly in platformio.ini? |
|---|---|---|
| `IS_MACHINE` / `IS_REMOTE` | Ecosystem category | Yes |
| `IS_MAINBOARD` / `IS_EXT_BOARD` | Board role | Yes |
| `PERIPH_<NAME>` | Fixed-function peripheral selection | Yes |

// EOF board_architecture.md