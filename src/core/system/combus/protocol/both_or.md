# The `both_or` Merge — Anti Ping-Pong

How a `both_or` channel (for example `FAILSAFE_HARD`) is shared between boards: the rule, the mechanics, an example, and what would happen without it.

## 1. The Rule

A `both_or` channel is a value that all boards share. **Any true value, local to the board or coming from a remote board, makes the channel true** on the board. For `FAILSAFE_HARD`: one board at fault is enough to put the whole node in failsafe, and the failsafe falls back when no board is at fault any more.

Two traps must be avoided:

- **overwriting**: if the received frame overwrote the value of the board, the local computation (the hard failsafe chain) would erase it on the next cycle, or the other way round;
- **ping-pong**: if a board sent back to another the value it has just received from it, each would keep the other's value alive and a `true` would never fall back.

## 2. The Mechanics

For each `both_or` channel, each board keeps **three separate things**:

| Term | What it is | Where |
|---|---|---|
| the **host** value | what the code of the board writes itself (the hard failsafe chain recomputes it on every cycle, starting from `false`: the *clear*) | `DigitalComBus::bothOr.local` |
| the **cache** of the inputs | the last value received from **each link**, one per link | the receive snapshot of the link (`combus_rx_snapshot`) |
| the **merged** value | what every consumer reads (run level, outputs, dashboard) | `DigitalComBus::value` |

Four rules:

1. **Merge**: `value = host OR (cache of the live links)`, re-evaluated on every cycle (`com_rx_update`) and on every local write (`combus_set_digital`). A received frame never overwrites the value: it only updates the cache of its link.
2. **Sending without echo**: on link L, the board sends `host OR (cache of the links OTHER than L)`. It never sends back to L what L has sent to it. With a single link, it simply sends its host value.
3. **A board that never wrote the channel does not contribute**: its host term is `false` (`bothOr.written`). A board that receives `FAILSAFE_HARD` by diffusion without computing it keeps its default value (`fault`, true) in the slot; without this rule, it would send it to everyone, and the machine would stay in failsafe from startup. It only relays.
4. **A lost link no longer counts**: its last received value is stale. Safety is then ensured by the link loss contributor (`BOARD_LINK_FAIL` / `NODE_LINK_FAIL`, see [the failsafe module](../../failsafe/failsafe_module.md)).

As formulas, for a board B linked to links L1…Ln:

```text
host(B)        = bothOr.local if bothOr.written, else false
value(B)       = host(B) OR cache(Li)              for all live Li
sent(B → Lk)   = host(B) OR cache(Li)              for all live Li, i ≠ k
```

## 3. Example

Three boards of a node, linked to the main board M. E (motor extension) computes its own `FAILSAFE_HARD` (for example a driver overheating). S (another extension) does not compute it: it receives it by diffusion.

```text
      E ──── L1 ──── M ──── L2 ──── S
```

| Time | Event | E (host → value) | M (host → value) | S (value) | M sends on L1 | M sends on L2 |
|---|---|---|---|---|---|---|
| t0 | all is well | 0 → **0** | 0 → **0** | **0** | 0 | 0 |
| t1 | E overheats | 1 → **1** | 0, receives 1 from L1 → **1** | receives 1 from M → **1** | 0 OR (L2: 0) = **0** | 0 OR (L1: 1) = **1** |
| t2 | E cools down | 0 → **0** | 0, receives 0 from L1 → **0** | receives 0 → **0** | 0 | 0 |

At t1, M goes into `FAILSAFE_HARD`; its run level chain writes `RUNLEVEL = HALT`, which is diffused to E and S. At t2, the fault falls back by itself (leaving the `HALT` run level still takes a reboot).

**Without sending without echo**, at t1 M would send `1` back to E; at t2, E has no cause any more but receives `1` from M, which receives `1` from E: the ping-pong keeps the fault alive forever. **If S contributed its default value**, it would send `1` permanently and M would never leave failsafe.

## 4. In the Code

| What | Where |
|---|---|
| fields `bothOr.enabled`, `bothOr.local`, `bothOr.written`, `bothOr.remote` | `src/core/system/combus/combus_defs.h` (`BothOrMerge`, in `DigitalComBus`) |
| local write: keeps the host apart, recomputes the value | `combus_set_digital()` → `combus_link_set_digital()`, `combus_access.cpp` |
| host term, update of the merged cache | `combus_both_or_local()`, `combus_both_or_set_remote()`, `combus_access.h` |
| a frame does not overwrite a `both_or` | `combus_frame_apply()`, `frame/combus_frame.cpp` |
| OR of the live links, with one link excluded | `combus_rx_peer_or()`, `combus_rx.cpp` |
| merge on every cycle | `com_rx_update()`, `src/core/system/com/com_manager.cpp` |
| sending without echo, link by link | `combus_tx_update()`, `combus_tx.cpp` (values passed to `combus_frame_encode()`) |
| loss timeout of the receiving links | `combus_links_watch()` → `combus_rx_set_link_timeout()` |

Tests: Group L of `test/test_combus_loopback` (merge, back to `false`, silent peer, no echo, board that never writes).

## 5. Limits

- Only the digital `both_or` channels are merged. `uplink_or` and `downlink_or` stay declarative: they are ORs between several producers, which the diffusion refuses today (two `uplink` = build error).
- The loss of a link is handled by the link loss contributors, not by the merge.
