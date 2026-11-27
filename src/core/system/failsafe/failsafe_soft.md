# Soft Failsafe Chain (`FAILSAFE_SOFT`)

This document describes the soft chain. The reference of the module (both chains, the names, the rules) is [failsafe_module.md](failsafe_module.md).

---

## 1. Role

The soft chain aggregates the **light** faults, today losses of command: no command source drives (none alive, or several at once), or a link to another node is down. Its reaction is to put the machine on hold, run level `IDLE`, and not the full stop `HALT` of the hard chain.

**Why two chains**: losing the remote for a moment (switching to another machine, going out of range) must not trigger the full safety sequence. The machine stops and waits.

**Resuming: manual.** When the fault disappears, nothing is written: the machine stays in `IDLE`, and the operator restarts it with the key (Triangle). The same happens when the controller is first connected after startup.

---

## 2. Architecture

```text
NODE_LINK_FAIL ──► reset + OR (cb_or_fn) ──► FAILSAFE_SOFT
                                                 │
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
| `NODE_LINK_FAIL` | no command source drives (none alive, or several), or a receiving link to another node is not alive (never seen included) | on a board with a command source module (`HAS_REMOTE_PS4`, `HAS_REMOTE_NODE`): the base of the sources, `remote_sources_update()`, `src/core/system/input/remote_source.cpp`; elsewhere: `combus_link_supervisor_update()`, `src/core/system/combus/protocol/link/combus_link_supervisor.cpp` | `HAS_NODE_LINK_FAILSAFE` |

It is a `BOARD` channel (private to the board that carries the source), of the failsafe group, `value: fault`. The chain re-arms it to `true` after reading: its instant value is not the state of the commands, read `FAILSAFE_SOFT`.

A single writer per board: where the base of the sources runs, it writes the contributor and folds in the loss of the other links to another node (`combus_links_lost(INTER_NODE)`); the link of a command source is left to the base. A link never seen counts at once: no startup grace, unlike `BOARD_LINK_FAIL`. See the [command sources](../input/~README.md).

---

## 4. Chain and Aggregate

- **Code**: `src/core/system/failsafe/failsafe_soft_chain.cpp`, `kFailsafeSoftChain[]`, run by `failsafe_soft_update()` in `sys_manager_update()`, after the hard chain and before the state machine.
- **Shape**: like the hard chain. A reset (aggregate to 0), then one `cb_or_fn` per contributor, each under its flag (one contributor today).
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
| Contributor | `src/core/system/com/node_link_fail.cb` |
| Reaction | `.../processors/input/runlevel_config.h` |
| Execution order | `src/machines/system/sys_manager.cpp` |
