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

# 1.2 Board Role — *deprecated*

> **Status: deprecated.** The `IS_MAINBOARD` / `IS_EXT_BOARD` axis 
described below no longer reflects the architecture. Kept here for
historical context only, until the remaining code references are migrated.

`IS_EXT_BOARD` was originally introduced to integrate `sound_module`
as a secondary board attached to a mainboard's authoritative ComBus.

This model no longer holds.

Any board with compiled firmware is simply a **BOARD**: a peer
participant on its node's ComBus, capable of hosting any module — motors,
sound, inputs, or others — rather than having a distinguished "mainboard"
or "extension" role.

A node's ComBus no longer has a single authoritative owner.

Each board implicitly owns the ComBus channels for whichever modules
it hosts, while sharing the rest of the node's ComBus like any other
participant.

There is therefore no longer a board-role axis to resolve.

A firmware only resolves its node type in the ecosystem (§1.1);
board role is no longer a separate contract.

Code still referencing `IS_MAINBOARD` / `IS_EXT_BOARD` — for example
`combus_scons_hook.py`'s structural-flags check — should be flagged
`// deprecated` and migrated to drop the distinction.


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