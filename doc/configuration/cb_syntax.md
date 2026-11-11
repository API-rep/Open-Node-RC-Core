# The `.cb` File — Writing ComBus Channels

A `.cb` file declares ComBus channels. It is a small YAML file read at build time by the [ComBus Builder](../../scripts/combus_builder/README.md) to generate the node firmware.

This page tells you how to write one. How the builder works inside is described in its own README.

---

## An Example

```yaml
channels:
  - id: VBAT_FAIL                          # the name of the channel, unique in the project
    infoName: "VBAT failsafe contributor"  # a readable label, for the logs and the dashboard
    type: digital                          # analog (a value) or digital (true / false)
    scope: SYSTEM                          # how far it goes: BASE, SYSTEM or BOARD
    theme: failsafe                        # its functional group
    direction: uplink_or                   # this board produces it (see below)
    value: fault                           # its safe value
    requires: [HAS_FAILSAFE, HAS_VBAT_FAILSAFE]   # it only exists with these build flags
```

A file can declare several channels, one after the other under `channels:`.

---

## Where to Put It

Anywhere under `src/`, next to what it describes: a core module, the profile of a machine type (`core/config/machines/<type>/`), a coupler (`core/config/couplers/<name>/`). The builder finds it on its own.

* The files of a machine type are only read when the firmware is built for that type (`-D MACHINE_TYPE_<TYPE>`).
* The files of a coupler are only read when the node declares that coupler.

---

## The Fields

| Field | Required | What to write |
|---|---|---|
| `id` | yes | the name of the channel, in capitals: `STEERING_BUS`, `KEY_BTN`. Unique in the whole project |
| `infoName` | yes | a short readable label |
| `type` | yes | `analog` (0 to 65535) or `digital` (true / false) |
| `scope` | yes | `BASE`, `SYSTEM` or `BOARD` (see below) |
| `theme` | yes | one of `core`, `failsafe`, `vbat`, `light`, `input`, `sim`, `coupler`, `remote` |
| `direction` | yes, except on `BOARD` | who sends and who receives (see below) |
| `value` | yes | the safe value (see below) |
| `requires` | no | build flags without which the channel does not exist |
| `zeroAt` | no | analog only: where the logical zero sits (see below) |
| `candidate` | no | `true` for a `BASE` channel on trial (see below) |

### `scope`

* `BASE`: part of the machine type, shared with the remote and with the other nodes;
* `SYSTEM`: shared by the boards of the node;
* `BOARD`: private to one board.

### `direction`

Written from the point of view of the board that declares the channel:

| My board… | I write |
|---|---|
| produces and shares the value | `uplink` |
| receives the value | `downlink` |
| exchanges it both ways | `both` |
| keeps it to itself | `none` |
| shares an alarm that every board may raise | `both_or` |
| receives it and passes it on to the other boards of its node (`BASE` only) | `downlink_pt` |

* Declare a `SYSTEM` channel once, on the board that produces it: the other boards of the node receive it automatically.
* A channel has a single producer: two boards declaring `uplink` for the same channel is an error.
* `uplink_or` and `downlink_or` also exist, for digital channels written by several sources.

### `value`: the Safe Value

The value of the channel when nobody writes it, and the one it takes back when its source is lost:

* analog: `CbusNeutral` (`CbusMinVal`, `CbusMaxVal` or an integer are accepted, with a warning);
* digital: `low`: every function and processor is written so that low (off) is its safe state;
* an alarm of the `failsafe` theme: `fault` (true, so that a missing value means a fault).

### `requires`

A list of build flags, **all required**: `requires: [HAS_FAILSAFE, HAS_VBAT_FAILSAFE]`. Without them, the channel is not generated, and the build log says so. Not allowed on a `BASE` channel: the channels of a machine type are fixed.

### `zeroAt`

Analog only: fixes the logical zero of the value. `start` (the default) puts it at 0; `neutral` puts it at `CbusNeutral`, so that the value read by the application can be negative (a gear, a steering position).

### `candidate`

`candidate: true` marks a `BASE` channel on trial: it travels only between nodes that both have it, and the others keep working without it. Write its final name; the builder adds `_NEXT` until it is promoted.

---

## When the Build Fails

| The build says | What to do |
|---|---|
| ``no `value:` `` | add `value:`, the safe value of the channel |
| `` `value: high` is refused `` | a true safe value is for an alarm: use `fault` in the `failsafe` theme, or `low` |
| `` `theme` must be one of ... `` | use a theme of the list, or ask for a new one in the builder |
| `channel id 'X' is defined in two files` | an `id` is unique: rename one, or remove the duplicate |
| `channel BASE 'X' declares requires=...` | remove `requires`, or move the channel to `SYSTEM` or `BOARD` |
| a conflict between two `uplink` | only one board produces a channel: the others declare `downlink`, or nothing |
| `unknown field(s)` | a typo in a field name |

---

## Going Further

* [ComBus](../combus.md): what the ComBus is, its layers.
* [ComBus Builder in depth](../../scripts/combus_builder/README.md): how the files are found and read, the diffusion between boards, the fingerprints, the generated files.
