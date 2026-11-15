# Failsafe Module

This document is the reference of the failsafe module: its two chains (hard and soft), its naming conventions and its rules. The soft chain has its own document, [failsafe_soft.md](failsafe_soft.md).

---

## 1. Two Severities, Two Chains

The module distinguishes two severities of fault. Each one has its aggregate and its reaction:

| Chain | Aggregate | Reaction (run level) | Example of cause |
|---|---|---|---|
| **hard** | `FAILSAFE_HARD` | `HALT`: motors stopped, put to sleep, disabled; only a reboot leaves it | low battery, loss of a link between boards |
| **soft** | `FAILSAFE_SOFT` | `IDLE`: on hold; resuming takes the key | loss of the remote, loss of a link to another node |

The aggregate states the **severity**, not the reaction: the reaction may change without any renaming. The hard chain always wins over the soft one (§6).

The whole module is enabled by `HAS_FAILSAFE` (set by `[env:machines]`). Without it, there is no chain, no aggregate and no contributor.

---

## 2. Naming Conventions

| Element | Form | Examples |
|---|---|---|
| aggregate | `FAILSAFE_<SEVERITY>` | `FAILSAFE_HARD`, `FAILSAFE_SOFT` |
| contributor | `<SOURCE>_FAIL` | `VBAT_FAIL`, `BOARD_LINK_FAIL`, `NODE_LINK_FAIL`, `LOCAL_REMOTE_FAIL` |
| flag of a contributor | `HAS_<SOURCE>_FAILSAFE` | `HAS_VBAT_FAILSAFE`, `HAS_BOARD_LINK_FAILSAFE` |
| flag of the module | `HAS_FAILSAFE` | — |

All these channels belong to the **`failsafe`** theme: the **failsafe group**. They are alarms, "true = fault". Their safe value is therefore `true`, declared `value: fault` in the `.cb` (§4).

---

## 3. Contributors

A contributor states a **fact** ("the battery is failing", "a link between boards is down"), without any severity. The chain that reads it decides the severity.

| Contributor | Fact | Written by | Flag | Chain today |
|---|---|---|---|---|
| `VBAT_FAIL` | battery below the threshold | `vbat_update()` (`src/core/system/vbat/vbat.cpp`) | `HAS_VBAT_FAILSAFE` | hard |
| `BOARD_LINK_FAIL` | a receiving link, between boards of the node, is lost | `com_rx_update()` (`src/core/system/com/com_manager.cpp`) | `HAS_BOARD_LINK_FAILSAFE` | hard |
| `NODE_LINK_FAIL` | a receiving link, to another node, is lost | `com_rx_update()` | `HAS_NODE_LINK_FAILSAFE` | soft |
| `LOCAL_REMOTE_FAIL` | the local remote (PS4 controller; tomorrow a PPM/SBUS receiver) no longer answers | `input_update()` (`src/machines/system/input/input_update.cpp`) | `HAS_LOCAL_REMOTE_FAILSAFE` | soft |

A contributor requires `HAS_FAILSAFE` and **its own flag**, never the flag of a chain: it stays neutral and **pluggable**.

**Pluggable means movable, never duplicated.** A contributor is read by **a single chain**: the chain re-arms it to `true` after reading (§4), so a second chain would read a false fault. To change the severity of a contributor, move its line from one chain to the other.

**A coupled link** (`ON_COUPLING`) has its own contributor, named by its `CouplingDev` (`fail`, for example `TRAILER_LINK_FAIL`): healthy while unhitched and during pairing; at fault when hitched without a healthy link: pairing in conflict (two candidates) or failed (too few answers), or a paired link lost after the grace counted from pairing. A coupled link never counts in `NODE_LINK_FAIL`: the machine puts this contributor in the chain of its choice.

`BOARD_LINK_FAIL` and `NODE_LINK_FAIL` are fed by the **scope** of the link: a link between boards counts as lost if it is silent, or never seen once its startup grace (`startupGraceMs`) is over; a link to another node counts as soon as it is not alive, never seen included. There is no per-link loss policy: the severity is decided by placing the contributor in a chain.

---

## 4. Proof of Life: `fault` by Default and Re-arming

Each alarm is `true` (fault) by default: `value: fault` in its `.cb`. The builder checks it: a digital channel of the `failsafe` theme must declare `fault`, and `fault` is refused anywhere else.

On every cycle:

```text
source module  → writes its contributor to false (healthy) if all is well
chain          → reads the contributor, ORs it into the aggregate,
                 then puts it back to true (cb_or_fn: "consume + guard")
```

If the source module stops writing, its contributor stays `true`: the fault is raised. The contributor is both a health state and a **proof of life**, without timeout or timestamp. Consequence: the instant value of a contributor on the bus is not the health state; read the aggregate.

---

## 5. The Chains

Both chains have the same shape: a reset first (`cb_reset_fn`, aggregate to 0 = healthy), then one `cb_or_fn` per contributor (input = output = the contributor). The aggregate becomes `true` as soon as a contributor is at fault, for the cycle.

| | Hard chain | Soft chain |
|---|---|---|
| code | `src/core/system/failsafe/failsafe_chain.cpp` (`kFailsafeChain[]`) | `src/core/system/failsafe/failsafe_soft_chain.cpp` (`kFailsafeSoftChain[]`) |
| run by | `failsafe_update()` | `failsafe_soft_update()` |
| contributors | `VBAT_FAIL`, `BOARD_LINK_FAIL` | `LOCAL_REMOTE_FAIL`, `NODE_LINK_FAIL` |

Each contributor is in an `#if defined(HAS_<SOURCE>_FAILSAFE)` block. A chain without a contributor writes `false` on every cycle.

**Execution order** (`sys_manager_update()`, `src/machines/system/sys_manager.cpp`): reception (`com_rx_update`, which writes the link contributors), inputs (`input_update`), battery (`vbat_update`), hard chain, soft chain. All the sources have therefore written before the chains, and both aggregates are fresh before the run level chain and the state machine of `main.cpp`. The startup value of the aggregates never reaches the state machine.

---

## 6. Reaction: the Run Level Chain

The module knows neither run levels nor machines. The reaction lives in the run level chain of the machine, `kRunlevelProcs[]` (`src/machines/config/machines/volvo_A60H_bruder/combus/processors/input/runlevel_config.h`), in this order:

1. **holding `HALT`** (`cb_runlevel_hold_fn`): if the run level is already `HALT`, it makes a *claim*, which skips all the following steps;
2. **`FAILSAFE_HARD`** (`cb_runlevel_fn`, on every cycle): while it is true, writes `HALT` and makes a *claim*;
3. **`FAILSAFE_SOFT`** (`cb_runlevel_once_fn`, on a rising edge): writes `IDLE` and makes a *claim*;
4. **the key** (`KEY_ACTIVE`, on its edges): `STARTING` or `TURNING_OFF`.

`main.cpp` reacts to the run level: `case RunLevel::HALT` stops, puts to sleep and disables the motors on entry.

**`HALT` is only left by a reboot**: once the hard fault has fallen, neither the key nor the soft fault leaves it. At reboot, `init.cpp` writes `IDLE`. The hold keeps no state: it only reads the value of `RUNLEVEL`.

**Unknown value**: the `default` of the `switch` of `main.cpp` treats any value outside `RunLevel` (bug, corrupted value) as a hard fault. It writes `HALT`, which is then held.

---

## 7. Rules to Keep

- **The module** holds no business logic (battery, links, inputs): it aggregates contributors and publishes two aggregates.
- **A source module** owns its contributor and writes it on every cycle.
- **A contributor** is read by a single chain.
- **A machine** chooses its contributors (flags) and their chain (a line in the chain), and provides its reaction (run level chain).

---

## 8. Files

```text
src/core/system/failsafe/
├── failsafe.h / .cpp              // failsafe_update(): runs the hard chain
├── failsafe_chain.h / .cpp        // kFailsafeChain[]: reset + contributors of the hard chain
├── failsafe_hard.cb               // FAILSAFE_HARD (SYSTEM, both_or)
├── failsafe_soft_chain.h / .cpp   // kFailsafeSoftChain[]: soft chain
├── failsafe_soft.cb               // FAILSAFE_SOFT (SYSTEM, both_or)
└── failsafe_soft.md               // the soft chain

The contributors stay next to the module that writes them:

src/core/system/com/board_link_fail.cb      // BOARD_LINK_FAIL (BOARD), written by com_rx_update()
src/core/system/com/node_link_fail.cb       // NODE_LINK_FAIL (BOARD), written by com_rx_update()
src/core/system/vbat/vbat_fail.cb           // VBAT_FAIL (SYSTEM, uplink_or)
src/core/config/inputs/local_remote_fail.cb // LOCAL_REMOTE_FAIL (BOARD), written by input_update()
src/core/system/combus/processors/logic/cb_or.h   // cb_or_fn
```

---

## 9. Quick Reference

| Concept | Where |
|---|---|
| Holding `HALT` | `cb_runlevel_hold_fn`, `src/core/system/combus/processors/base/cb_runlevel.h` |
| Aggregates | `failsafe_hard.cb`, `failsafe_soft.cb` |
| Chains | `failsafe_chain.cpp`, `failsafe_soft_chain.cpp` |
| Link contributors | `com_rx_update()`, `src/core/system/com/com_manager.cpp` |
| Execution order | `sys_manager_update()`, `src/machines/system/sys_manager.cpp` |
| Run level reaction | `kRunlevelProcs[]`, `.../processors/input/runlevel_config.h` |
| Motor reaction | `src/machines/main.cpp`, `case RunLevel::HALT` |
| Reading | `comBus.digitalBus[DigitalComBusID::FAILSAFE_HARD].value` (or `FAILSAFE_SOFT`) |
