# Building an Open RC Node System

This document covers the practical aspects of building, configuring and
deploying an Open RC Node system.

For the conceptual contracts behind the flags used here (ecosystem
category, board role, domain dispatchers), refer to `board_architecture.md`
and `development/conventions/coding_dispatcher.md`.

Topics include:

- Hardware selection
- Development environment setup
- PlatformIO configuration
- Firmware build process
- Configuration files
- Upload and testing procedures

---

## Project Layout — Environments as Sub-Projects

An Open RC Node repository builds several distinct firmwares from the same
source tree — a machine, its extension boards, remote controllers — each as
its own PlatformIO environment (`[env:xxx]`). Every environment compiles a
different subset of `src/` (via `build_src_filter`) with a different set of
`-D` flags.

Three environment families exist today:

```text
[env]                     Global defaults inherited by every environment
                          (platform, framework, shared lib_deps, shared
                          build_flags).

[env:machines]            Base config shared by every machine instance
                          (IS_MACHINE, src/machines/ on the include path).

[env:<instance>]          extends [env:machines]
                          Params common to one concrete vehicle (e.g.
                          volvo_A60H_bruder) shared by both its mainboard
                          and its extension board: input module, battery
                          profile, and every other instance-specific flag
                          that doesn't depend on board role.

[env:<instance>:MAINBOARD] extends [env:<instance>]
                          Board-role-specific config for that instance's
                          mainboard: IS_MAINBOARD, hardware, modules.

[env:<instance>:EXT_BOARD] extends [env:<instance>]
                          Board-role-specific config for that instance's
                          extension board (e.g. its sound module):
                          IS_EXT_BOARD, hardware, modules.

[env:remotes]              Base config shared by every remote controller.
                          Same scheme: [env:<remote_instance>], then
                          :MAINBOARD / :EXT_BOARD as needed.
```

`extends` gives each concrete environment the shared base flags without
retyping them, and lets a single edit to the base propagate to every
instance built from it.

### Sharing Identity Across Paired Environments

A machine and its paired extension board (e.g. a sound module) must agree
on which vehicle they belong to — both need `MACHINE_MY_MACHINE` to
resolve the same ComBus layout. Rather than duplicating that flag in two
environment blocks (and risking them drifting apart), declare it once in a
dedicated section and reference it from both:

```ini
; Machine identity shared between the machine node and its paired sound node.
[volvo_A60H_id]
build_flags =
    -D MACHINE_VOLVO_A60_H_BRUDER

[env:volvo_A60H_bruder]
extends = env:machines
build_flags =
    ${env:machines.build_flags}
    ${volvo_A60H_id.build_flags}
    ...

[env:sound_node_volvo]
extends = env:sound_node_base
build_flags =
    ${env:sound_node_base.build_flags}
    ${volvo_A60H_id.build_flags}
    ...
```

This is the general pattern for any flag that must stay identical across
two or more paired environments: one section holding the shared
`build_flags`, referenced by `${section.build_flags}` from each consumer —
never copy-pasted.

---

## Build Flags

### Vehicle Instance Selection

Selects the concrete vehicle instance to build — an assertion flag, set
directly in `platformio.ini`.

Example:

```ini
-D MACHINE_VOLVO_A60_H_BRUDER
```

`MACHINE` identifies one specific hardware build (this Volvo A60H Bruder
conversion). It is distinct from `MACHINE_TYPE_*`, which identifies the
*vehicle class* (dumper truck, excavator, ...) that instance belongs to.
`MACHINE_TYPE_*` is **not** a build flag — it is declared once in the
instance's own top-level header (e.g. `volvo_A60H_bruder.h`), since it is a
property of the instance, derived once the instance is known, not a
separate build-time choice. See `coding_dispatcher.md` for how
`MACHINE_TYPE_*` drives the type-level ComBus dispatcher
(`machine_type.h` → `<machine_type>_config.h` → `combus_ids_base.h`).


---

### Ecosystem Category

```ini
-D IS_MACHINE
-D IS_REMOTE
```

Mutually exclusive. Identifies which part of the ecosystem this firmware
belongs to — a vehicle, or a handheld controller. See
`board_architecture.md`.

---

### Board Role

```ini
-D IS_MAINBOARD
-D IS_EXT_BOARD
```

Mutually exclusive, orthogonal to the ecosystem category above. Identifies
this firmware's position within its own node: the decision-making board
hosting the authoritative ComBus (`IS_MAINBOARD`), or a child board with its
own compiled firmware, participating in the same node's ComBus as a
secondary participant (`IS_EXT_BOARD`).

This mirrors the `VBAT_LIPO` / `HAS_VBAT_SENSING` split documented in
`coding_dispatcher.md` — a selection flag set once at the point of
selection, and a derived presence flag (`HAS_*`) computed in exactly one
place and consumed everywhere else.

#### `IS_*` vs `HAS_*` Convention

Every new `IS_*`, `PERIPH_*`, or domain-specific selection flag introduced
in the project should clearly indicate, in one line, which architectural
axis it belongs to and whether it is a build-time assertion.

`HAS_*` flags say what a board has or does. Most of them are the functions
of the board, set in `platformio.ini` (see `board_architecture.md`). A few
are derived by the selected backend header, included through its dispatcher
during compilation (`HAS_VBAT_SENSING`, for instance).

**Rules:**

- A function of the board is a `HAS_*` flag in `platformio.ini`.
- A capability derived by a backend is never set in `platformio.ini`: any
  file evaluating it must first include the corresponding dispatcher header.

---

### Main Board Selection

Selected par -D BOARD_* flags

```ini
-D -D BOARD_ESP32_8M_6S
```

---

### Domain Dispatcher Flags

Some subsystems select among several mutually-exclusive backends at
compile time, following the umbrella/dispatcher pattern documented in
`coding_dispatcher.md`. Each backend owns its own dedicated flag, prefixed by
its domain.

```ini
-D VBAT_LIPO              ; selects the LiPo battery sensing backend
; -D VBAT_NONE            ; alternative: no battery sensing
```

### Command Sources

The board that acquires the commands declares it, then the sources it may read. Several sources can be built together: only one alive at a time drives the machine.

```ini
-D HAS_REMOTE             ; this board acquires the commands (one board per machine)
-D HAS_REMOTE_PS4         ; a PS4 controller plugged into the machine
-D HAS_REMOTE_NODE        ; the remote of the project, through its link
```

See the [command sources](../src/core/system/input/~README.md).

### Peripheral Selection

Fixed-function hardware modules with no MCU of their own (e.g. an RF
receiver module) are selected the same way, under the `PERIPH_` prefix:

```ini
-D PERIPH_<NAME>
```

See `board_architecture.md` for the distinction between a board and a
peripheral.

---

Additional build flags and examples will be documented here as the project
evolves.

// EOF ~building_a_system.md