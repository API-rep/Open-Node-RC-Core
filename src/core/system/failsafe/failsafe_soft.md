# Soft Failsafe Chain (`FAILSAFE_SOFT`)

This document describes the soft chain. The reference of the module (both chains, the names, the rules) is [failsafe_module.md](failsafe_module.md).

---

## 1. Role

The soft chain aggregates the **light** faults, today losses of command: the local remote no longer answers, or a link to another node is down. Its reaction is to put the machine on hold, run level `IDLE`, and not the full stop `HALT` of the hard chain.

**Why two chains**: losing the remote for a moment (switching to another machine, going out of range) must not trigger the full safety sequence. The machine stops and waits.

**Resuming: manual.** When the fault disappears, nothing is written: the machine stays in `IDLE`, and the operator restarts it with the key (Triangle). The same happens when the controller is first connected after startup.

---

## 2. Architecture

```text
LOCAL_REMOTE_FAIL ──┐
                    ├──► reset + OR (cb_or_fn) ──► FAILSAFE_SOFT
NODE_LINK_FAIL ─────┘                                   │
                                                        ▼
                                             run level chain (rising edge)
                                                        │
                                                        ▼
                                                 RunLevel::IDLE
```

The hard chain always comes first (see [failsafe_module.md](failsafe_module.md) §6).

---

## 3. Contributors

| Contributor | Fact | Written by | Flag |
|---|---|---|---|
| `LOCAL_REMOTE_FAIL` | the local remote no longer answers (PS4 controller today) | `input_update()`, `src/machines/system/input/input_update.cpp`, when the backend is connected (`INPUT_PS4_DS4_BT`) | `HAS_LOCAL_REMOTE_FAILSAFE` |
| `NODE_LINK_FAIL` | a receiving link, to another node, is not alive (never seen included) | `com_rx_update()`, `src/core/system/com/com_manager.cpp` | `HAS_NODE_LINK_FAILSAFE` |

Both are `BOARD` channels (private to the board that carries the source), of the failsafe group, `value: fault`. The chain re-arms them to `true` after reading: their instant value is not the state of the link, read `FAILSAFE_SOFT`.

`NODE_LINK_FAIL` is fed by the links to another node (`INTER_NODE` scope). A link never seen counts at once: no startup grace, unlike `BOARD_LINK_FAIL`.

---

## 4. Chain and Aggregate

- **Code**: `src/core/system/failsafe/failsafe_soft_chain.cpp`, `kFailsafeSoftChain[]`, run by `failsafe_soft_update()` in `sys_manager_update()`, after the hard chain and before the state machine.
- **Shape**: like the hard chain. A reset (aggregate to 0), then one `cb_or_fn` per contributor, each under its flag.
- **Aggregate**: `FAILSAFE_SOFT` (`failsafe_soft.cb`), `SYSTEM`, `both_or`, shared by all the boards of the node.
- **Activation**: `HAS_FAILSAFE`, like the whole module. Without a contributor, the aggregate is `false` on every cycle.

---

## 5. Reaction

In the run level chain (`kRunlevelProcs[]`, `src/machines/config/machines/volvo_A60H_bruder/combus/processors/input/runlevel_config.h`), after the hard chain:

```cpp
static constexpr CbRunlevelCfg kFailsafeSoftRunlevelCfg {
    .high  = RunLevel::IDLE,        // FAILSAFE_SOFT 0→1: IDLE
    .low   = std::nullopt,          // FAILSAFE_SOFT 1→0: nothing, resuming goes through the key
    .claim = true,                  // blocks the key for this cycle
};
```

If the machine is already in `HALT`, this step does not run: holding `HALT`, at the head of the chain, has made a *claim*. Only a reboot leaves `HALT`.

The processor is `cb_runlevel_once_fn`: it only acts on the **rising edge**. The soft fault does not "hold" the run level, unlike the hard chain. Since the key commands come from the remote, they cannot arrive anyway while the remote is lost.

---

## 6. Readers

| Reader | File | Use |
|---|---|---|
| run level chain | `runlevel_config.h` | `IDLE` on a rising edge |
| machine dashboard | `src/machines/system/debug/dashboard_machine.cpp` | remote connected indicator |
| inputs dashboard | `src/machines/system/debug/dashboard_input.cpp` | driven bus indicator |

---

## 7. Quick Reference

| Concept | Where |
|---|---|
| Aggregate | `src/core/system/failsafe/failsafe_soft.cb` |
| Chain | `src/core/system/failsafe/failsafe_soft_chain.cpp` |
| Contributors | `src/core/config/inputs/local_remote_fail.cb`, `src/core/system/com/node_link_fail.cb` |
| Reaction | `.../processors/input/runlevel_config.h` |
| Execution order | `src/machines/system/sys_manager.cpp` |
