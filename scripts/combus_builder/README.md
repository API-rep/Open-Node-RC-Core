# ComBus Builder — Developer Documentation

For whoever modifies the builder. To write a `.cb` file, see the [`.cb` syntax](../../doc/configuration/cb_syntax.md). For the frame on the wire, see the [data frame](../../src/core/system/combus/protocol/frame/README.md).

References to the code give the file and the function, not the line number.

## 1. Purpose

The builder reads YAML `.cb` files and generates the C++ headers and sources of the ComBus: identifier enumerations, channel arrays, bus instance, MD5 fingerprints, tables of the links between nodes. It replaces the manual declaration of the channels, for a simpler end-user experience.

Two entry points, outside the package:

| Entry | Role |
|---|---|
| `scripts/combus_scons_hook.py` | PlatformIO hook, declared by `extra_scripts = pre:scripts/combus_scons_hook.py` in the `[env]` section of `platformio.ini`. The normal path. |
| `scripts/combus_builder.py` | Stand-alone CLI. Single board only: no node detection or diffusion; the machine type filter follows the context, as in the hook. Options: `--project-root`, `--out-dir`, `--define` (repeatable, form `"-D FLAG"` or `"-D KEY=VAL"`; a token without `-D` is not read). |

## 2. Pipeline

```text
platformio.ini (flags)            .cb / .cbch files
        │                                  │
   flags.py (BuildContext)            parser.py (discovery + YAML)
        │                                  │
        │                       [node mode] diffusion.py (SYSTEM between boards)
        │                                  │
        │                           canon/ (validation, merge, sort)
        └──────────────┬───────────────────┘
                  generator.py  (remote, couplers, requires, views, IDs, C++)
                       │
              md5.py, links.py  (fingerprints per view, links between nodes)
                       │
              combus_generated/ folder
```

Exact order in the hook (`_generate_combus_action`): discovery and parsing once for the whole node, diffusion, then `canonize()` and `generate()` board by board.

### Design Principles

- **Discovery and validation are separate.** The parser finds the files and only checks that the YAML is a non-empty mapping (`parse_yaml_file`). The semantic validation of the channels is done further on, in `canon/channels.py`. The extension does not decide the content: `canon` reads the `channels:` key whatever the extension (`extract_channels_sections`).
- **What a `.cb` describes.** A `.cb` says what exists and how it is exposed (identifier, type, scope, direction, activation, safe value). The business logic (thresholds, computations, processors) stays in C++.
- **The flags come from the real build.** The builder does not reimplement PlatformIO: it reads `env["CPPDEFINES"]`, with `build_flags` as a fallback and an `override_cppdefines` parameter for the tests (`acquire_build_context`). The source used is traced in `BuildContext.cppdefines_source`. For the boards of a node that are not compiled, `node_context.py` reads the configuration through the PlatformIO API (`ProjectConfig`).
- **No silent resolution.** Duplicate `id`, unknown key, `channels: null`, direction conflict: explicit error, never "the last one wins".
- **An `id` is unique across the whole project**, whatever its type, scope or theme (`ChannelConflictError`, `merge_channels`). Two definitions of the same `id` with different `value`s raise `ChannelValueConflictError`.
- **The sort is global**, after reading all the files: the order of the files has no meaning.
- **The default value is the safe value.** A `.cb` must write `value:` for each channel (`parse_yaml_file`); a failsafe alarm declares `value: fault`; an analog channel that is not at `CbusNeutral` is reported in the build log.

### `requires`: the Channel Only Exists If Its Flags Are Defined

- **Node mode** (diffusion): a declaration whose `requires` is not satisfied for a board is **dropped** for that board (`_is_active_for_board`, `diffusion.py`).
- **Single-board mode**: `generate()` calls `resolve_requires` (`generator.py`), which applies the same rule: a channel with a missing flag is not generated. It is listed in the build log (`[combus_builder] requires: channel 'X' not generated (flag not defined: ...)`), so never dropped silently. Tests: `test_generator.py` (`test_resolve_requires_*`, `test_generate_requires_missing_skips_and_logs`).
- A `BASE` channel cannot have `requires`: the frame of a machine type is fixed.

## 3. Module Map

| Module | Role |
|---|---|
| `parser.py` | Searches recursively for `.cb` and `.cbch` under `PROJECT_SRC_DIR` (default `src/`), skips `.pio`, `.git`, `__pycache__`, `node_modules` (`discover_config_files`). Loads the YAML (`safe_load`), requires a mapping at the root. Never enters `core/config/couplers/`. |
| `machine_type.py` | Detects the machine type through a single `MACHINE_TYPE_*` flag of `MACHINE_TYPE_FLAGS`. Two flags: `AmbiguousMachineTypeError`. Filters the `.cb` files under `core/config/machines/<other_type>/`. |
| `flags.py` | `BuildContext`: the `-D` flags seen by the compiler (`defines`, `defines_with_value`). Used to resolve `requires`. |
| `node_context.py` | Node detection from the env names `<root>_<board_id>` (`parse_env_hierarchy`). Static resolution of the context of each board, without compiling. |
| `diffusion.py` | Diffusion of the `SYSTEM` channels between the boards of a node (§5). |
| `canon/channels.py` | Vocabulary and validation of the channels, detection of duplicate `id`s, canonical sort. |
| `canon/__init__.py` | `canonize()`: extraction of `channels:`, merge, sort. |
| `generator.py` | View of the remote, injection of the couplers, resolution of `requires`, selection of the three views, ID assignment, C++ rendering. |
| `md5.py` | MD5 fingerprint of each view and the related headers. |
| `links.py` | Links between nodes: couplers, link with the remote, `combus_peer_links.h` (§6). |
| `coherence.py` | Checks that the `BuildContext` matches `pio run -t idedata` (integration test). |

Outside the package: `scripts/combus_scons_hook.py`, `scripts/combus_builder.py`, `scripts/combus_builder_dump_ctx.py`, `scripts/combus_test_add_generated_source.py`.

## 4. Node, Board and Root Env ("Mother") Model

- A **node** is a machine or a remote. It carries one or more **boards**.
- Detection: an env `E` is the root env of the node if at least one env `E_<id>` exists; an env `E_<id>` is a board of `E`. Without a sub-env, the build is in single-board mode (`parse_env_hierarchy`).
- The **root env** (called "mother" in the code, `board_id = None`) only carries the common configuration of the node (the shared `build_flags`). It is **not** a board: no circuit matches it. The main board of the machine (for example `<root>_mainboard`) is a sub-env, so a real board. The root env does not take part in the diffusion and produces no file in node mode: `diffuse_system_channels` excludes it and the hook skips the `board_id is None` entry. Compiling it is refused with an explicit message ("is the root env of a node ... Build one of its boards instead").
- In single-board mode, `board_id` is `None`: a single set of files, without a prefix.
- The `HAS_RUNLEVEL_MANAGER` flag designates the board that owns the `RUNLEVEL` channel ("mainboard by convention", `runlevel.cb`), not the root env.

In `platformio.ini`, the test node `test_node` (boards `test_node_mainboard` and `test_node_extboard`) exercises node mode; the other envs are single-board.

## 5. SYSTEM Diffusion Between Boards

Input: all the active `SYSTEM` declarations per board (a declaration is active if its `requires` is satisfied for that board). Rules, per `id` (docstring of `diffusion.py`), with `n_up`, `n_down`, `n_both`, `n_none` the number of active boards in each direction:

| Case | Result |
|---|---|
| nothing active | channel ignored |
| `both` with `uplink` or `downlink` | `DiffusionConflictError` (fatal) |
| `both` alone | the absent boards receive `both`, or `both_or` if a board declared it; the `none` boards stay `none` |
| `n_up >= 2` | `DiffusionConflictError` (fatal) |
| `n_up == 1` | the absent or `downlink` boards become/stay `downlink` |
| `n_up == 0`, `n_down >= 1` | "dead channel" warning, the absent boards get `none` |
| only `none` | no diffusion, neither warning nor error |

The same `id` declared twice on the same board raises `DiffusionSystemConflictError` before any other rule. The `*_or` variants follow the rule of their family (`uplink_or` counts as `uplink`, etc.); `both` and `both_or` on the same `id`: accepted with a "divergence" warning; `both_or` mixed with `uplink`/`downlink`: fatal error (`_resolve_group`).

The diffused copy takes the fields of the original declaration (`theme`, `type`, `infoName`, `value`...) but **not its `requires`**: a board that receives the channel by diffusion always has it, even if the flags required by the source board are not defined there. The OR policy is not copied either, except for `both_or` (`uplink_or` is received as `downlink`).

The `BASE` channels are not diffused between boards: their direction inside the node is given by `downlink_pt` (§6).

Why diffuse: the `SYSTEM` ComBus must be identical on all the boards of a node. A consumer module on board B must be able to read a channel produced only by a module active on board A, without enabling that module on every board. A channel that must stay private to a board is declared `BOARD`: the diffusion only handles the `SYSTEM` scope, and the `BOARD` declarations are only kept on the boards where their `requires` is satisfied.

Each board receives its own files (§8), including the identifier files: there is no "single set of IDs per node".

Why the `SYSTEM` fingerprint is canonised (`ONE_WAY`): the diffusion swaps `uplink` and `downlink` between boards by construction. Without canonisation, two correctly configured boards would have different `SYSTEM` MD5s as soon as an asymmetric channel exists (a guaranteed false disagreement).

## 6. Scopes, ID Assignment and Views

Three scopes (`scope` field of a channel), from the widest to the narrowest:

| Scope | Role |
|---|---|
| `BASE` | Standard profile of the machine type: channels that every compatible node shares, even without a network link. It is the contract the handshake checks by MD5. |
| `SYSTEM` | Channels of this node (a machine or a remote), shared between its boards. |
| `BOARD` | Channels internal to a board, never shared. |

The old commits and the stashes use the former names: `REMOTE` for `BASE`, `LOCAL` for `SYSTEM`, `SYSTEM` for `BOARD`.

Three views, with the same ID space (`select_views`):

| View | Content | Files emitted |
|---|---|---|
| `combus` | BASE + SYSTEM + BOARD | `combus.h`, `combus.cpp`, `combus_ids.h` |
| `combus_system` | BASE + SYSTEM | `combus_system_ids.h` only (`_NO_EMIT_RUNTIME_VIEWS`) |
| `combus_base` | BASE | `combus_base_ids.h` only |

Two distinct orders, not to be confused:

- `_VIEW_SCOPE_ORDER` (`generator.py`): BASE 0, SYSTEM 1, BOARD 2. Used to **number the IDs**, in `_select_view` only.
- `SCOPE_ORDER` (`canon/channels.py`): SYSTEM 0, BASE 1, BOARD 2. Used for the **canonical sort** `(scope, type, theme, id)` (`ChannelDefinition.sort_key`, `canonize_channels`).

Assignment rule (`_select_view`): the `analog` and `digital` channels are sorted separately, by `(scope, candidate, theme, id)` with the `_VIEW_SCOPE_ORDER` order. The candidates (`candidate: true`, `BASE` only) therefore come after the stable `BASE` channels of their type: adding one moves no stable `BASE` ID. The `.cb` gives the final name, and `validate_channel` adds `_NEXT` (reserved suffix, refused if written by hand); `merge_channels` refuses a candidate whose final name is already taken. Each type has its own counter, starting at 0. The `BASE` channels therefore have the lowest IDs of their type, then `SYSTEM`, then `BOARD`. A channel keeps the same ID in all the views where it appears. The `combus_system` and `combus_base` views are thus prefixes of `combus`.

What each view is for (docstring of `md5.py`, `combus_link_start()` on the C++ side):

- `combus_base`: the base profile of the machine type (what is validated between nodes by the handshake);
- `combus_system`: the alignment base between the boards of a node. Two boards can carry different `BOARD` channels without their `SYSTEM` contracts diverging, because this view does not include `BOARD`;
- `combus`: the full view of the board (local reference).

The order of the IDs is a contract: changing the order changes the protocol without changing the names. The MD5 covers what makes the contract of each channel (not only the ID), so a change of `direction` or of default value at a constant ID is detected.

`CH_COUNT` ends each enumeration. `View.wire_end` (number of `BASE` channels) exists on the Python side only: on the C++ side, the dimensions of the frame are the `CH_COUNT` of the enumerations of the `BASE` view.

### Direction Inside the Node

Each generated channel carries two directions, grouped in `.flow` (`ChanFlow`): `.betweenNodes` (on a link between nodes: the one of the `.cb`, or the one of the diffusion for a `SYSTEM`) and `.betweenBoards` (on a link inside the node), computed by `node_direction()` (`generator.py`). For a `BASE`, only `downlink_pt` travels inside the node: `uplink` on the board connected to another node (`is_connected_board()`, flags `INTER_NODE_FLAGS`), `downlink` elsewhere; `both_or` keeps its meaning. `check_single_connected_board()` refuses, in node mode, several connected boards (called by the hook). The `passthrough` flag of the canon is not part of the MD5.

| Declared (`BASE`) | Connected board | Other boards |
|---|---|---|
| `downlink_pt` | `uplink` (redistributes) | `downlink` (receive) |
| `downlink` | `none` (uses it alone) | `none` |
| `uplink`, `both`, `none` | `none` | `none` |
| `both_or` | `both_or` (OR merge) | `both_or` |

`both_pt` is refused (several sources by nature), `_pt` is refused on `SYSTEM` (already diffused). On reception, a channel is only written if its direction for the link includes `downlink`.

### Links Between Nodes: the Couplers

The contract of a link is a **coupler**, the norm of the link, like a standard plug: `src/core/config/couplers/<name>/*.cb`, ordinary `BASE` channels. The machine type declares the couplers it supports in `machines/<type>/<type>.mtype` (`load_machine_type_config`): it is the condition of any instance. The coupler is a **brick of the machine**: it is written **from the point of view of the socket** (the side that carries, the hub of the star), like any `.cb` of the machine (`downlink_pt`: received and relayed to the plug, `downlink`: received from the plug, `uplink`: its own value sent, `both_or`: shared fault), `value` = the safe value. A machine (`IS_MACHINE`) declares the **side** of each of its couplers (`coupler_sides()`): `-D HAS_COUPLER_SOCKET_<NAME>` (the socket: it carries, 0 to n) or `-D HAS_COUPLER_PLUG_<NAME>` (the plug: it is carried, 0 or 1); the role of a node on a link is that side, never a nature of the node. A coupler on both sides, two plugs, or a remote (`IS_REMOTE`) with a coupler are **refused explicitly** (`node_category()`), like the former `-D COMBUS_PEER_<TYPE>`.

- **Remote**: an `IS_REMOTE` node drives a machine type (`-D MACHINE_TYPE_<TYPE>`); its ComBus is the `BASE` view of that type **mirrored** (`remote_channels()`: what the machine receives, the remote sends), without the `SYSTEM` and `BOARD` channels of the type; without `MACHINE_TYPE_*`, error. The machine ↔ remote link has its own table and its own MD5 (`compute_remote_link()`, `combus::peer::kRemoteLink`): it only carries what the remote sends (the inputs of the type and the commands the supported couplers relay), the same with or without the coupler. As any link, its MD5 carries the side: the machine is the socket, the remote the plug.
- **Passive coupler**: a coupler without a link channel, only local channels (a tool coupled by hand): a local instance, without a link nor a `ComPeerLink` (`is_passive_coupler()`, log `set COUPLER <name> (passive)`), on the socket side only.
- **Local channels**: a channel of the coupler that requires the flag of one of its sides (`requires: [HAS_COUPLER_SOCKET_<NAME>]` or `[HAS_COUPLER_PLUG_<NAME>]`) belongs to that side only (`is_local_channel()`): injected as is, filtered by `requires`, never on the link (neither frame nor link MD5); it is `BOARD` (or `SYSTEM` if it comes from another board of the node, never `BASE`), with the `coupler` theme, and its name starts with the name of the coupler (`FRONT_COUPLER_CMD`: several couplers on a machine never clash). An input from the remote (the hitch button) belongs to the input contract of the machine type, not to the coupler. The folder of the coupler may carry its chain (`<name>_chain.cpp`, guarded by the flag of its side, `HAS_COUPLER_SOCKET_<NAME>`), plugged through `CouplingDev::chain`.
- **Read apart** (`load_coupler_channels()`): the discovery of the `.cb` files never enters `core/config/couplers/` (`parser._is_coupler_path()`); only the declared couplers are read.
- **Injection** (`inject_coupler_channels()`, called by `generate()` before the views): the ComBus of the coupler **merges** into the one of the node; a channel of the coupler unknown to the machine type is added at its safe value (an absent function stays safe): as written on the socket, and its side of the coupler on the plug (`_plug_view()`: what the socket sends, the plug receives, and the other way round). Adding a channel to a coupler therefore changes the ComBus of every node that carries it: a **breaking** change.
- **Set of the link** (`compute_coupler_link()`): **all** of the coupler (full contract). A channel of the same name in the machine type must have the same type, the same value and the same `zeroAt`, and must **flow**: two senders, or nobody sending, is an error. On the socket, `downlink_pt` relays (it sends to the plug, or receives where the plug sends); one source per channel on the node (`check_single_source()`, `both_or` excepted).
- **Common frame**: analog then digital, stable before candidates, then the name (never the theme).
- **MD5 of the coupler and of the side** (`link_md5()`): it only covers the coupler (the same for every machine that carries it) and a side; each side announces MD5(coupler + its side) and expects MD5(coupler + the other side) (`peer=True`), so two sockets, or two plugs, linked by mistake refuse each other.
- `combus_peer_links.h` (always written, empty without a coupler or a remote): per link, the announced MD5s (`kAnnouncedMd5`, `kAnnouncedMd5Next`) and the expected ones (`kExpectedMd5`, `kExpectedMd5Next`), the channel counts, the **table** position on the wire → local ID (`kAnalog[]`, `kDigital[]`) and what this node receives (`kAnalogRx[]`, `kDigitalRx[]`).

The build log shows one line per link: `[combus_builder] set LINK <coupler> (<side>)  md5=<32 hex>  A3+D2`. The test coupler `test_coupler` and the test types `test_tractor` and `test_trailer` (channels prefixed `TEST_`) are used by the tests (`test_couplers.py`, `test_remote_contract.py`).

**Machine type filter**: with a type, the core and the profile of that type are loaded; **without a type, no type profile**: neighbouring types share names on purpose, like `BRAKE_BUS` in a tractor and in its trailer, and would clash (`filter_files_by_machine_type`).

## 7. MD5

- One fingerprint per view: MD5 of a canonical JSON (sorted keys, compact separators) (`canonical_bytes`, `compute_view_hash`).
- Inputs per channel, in wire order (the order of the IDs): `id`, `type`, `scope`, `direction`, `direction_or`, `value` in the form of the emitted C++ literal (an absent value and its explicit default value give the same hash), and `zeroAt` when it is `neutral` (`_view_channel_canonical`). `theme` and `infoName` are not part of it: a debug label, or a theme that does not move the channel on the wire, does not break the contract. `requires` is not part of it (its effect is the presence of the channel in the view).
- The build log shows one line per set: `[combus_builder] set BASE    md5=<32 hex>  A9+D9`, then `set NEXT` (with the number of candidates) and `set SYSTEM`, followed by `[<board>]` in node mode (`_print_set_hashes`).
- The code of the builder (`scripts/combus_builder/**/*.py` except the tests) and the hook are part of the sources of the generation: modifying them regenerates the files, like a `.cb`.
- `combus_system` view only: `ONE_WAY` canonisation. `uplink`, `downlink`, `uplink_or` and `downlink_or` give the same state; `both`, `both_or` and `none` stay distinct (`_canonical_direction_for_hash`). This is what lets two boards of a node have opposite roles without disagreeing.
- `combus_base` and `combus` views: raw directions, `uplink` and `uplink_or` give different fingerprints.
- `_canonical_direction_for_hash` is reserved to the hash, never used for generation.
- `BASE` set: two fingerprints. **Stable**: the view without its candidates (`stable_view`), emitted as `kCombusBaseComBusMd5`. **Next**: the whole view (stable channels then candidates), emitted as `kCombusBaseComBusMd5Next`. Without a candidate, they are equal (the `candidate` flag is not hashed). The same header emits the number of channels of the stable set per type (`kCombusBaseStableAnalogCount`, `kCombusBaseStableDigitalCount`): the size of the frames of a `BASE` link outside next mode. The `combus_system` view contains the candidates: two boards of a node with different candidates do not have the same `SYSTEM` MD5.
- The hash therefore changes when one of these fields changes, when a channel enters or leaves a view (through `requires`), or when the order of ID assignment changes.
- Propagation by the scope of the modified channel (a consequence of the composition of the views, §6): a `BASE` channel changes the three fingerprints; a `SYSTEM` channel changes `combus_system` and `combus`, not `combus_base`; a `BOARD` channel only changes `combus`.
- Headers emitted: `combus_wire_common.h` (common constants `kProjectVersionMajor`, `kProjectVersionMinor`, `kProjectVersionDev`, `kCombusHandshakeWirePayloadLen`) then one header per view (`k<View>ComBusMd5[16]` and its hexadecimal form), which includes the first one. The constants are `static constexpr`: the `xtensa-esp32-elf-g++` toolchain rejects `inline constexpr` at namespace scope in some configurations.
- Project version: it is not part of the hash. The builder reads `project_version.h` (root of the repository, `read_project_version()`) and writes `MAJOR`, `MINOR` and `DEV` into `combus_wire_common.h`, with `kCombusHandshakeWirePayloadLen = 19` (16 bytes of MD5 and the three versions). The file must define `PROJECT_VERSION_MAJOR`, `PROJECT_VERSION_MINOR` and `PROJECT_VERSION_DEV` as `= <n>u;`, each between 0 and 255; otherwise the build stops (`FATAL`, no silent default value). The hook declares the file as a source: modifying it regenerates the files. Without a version given (direct call of `generate()` in the tests), `md5.py` keeps 0, 1 and 0.

## 8. Generated Outputs

Single board: the 10 files of `EXPECTED_ARTIFACTS` (`combus_scons_hook.py`): `combus.cpp`, `combus.h`, `combus_ids.h`, `combus_system_ids.h`, `combus_system_md5.h`, `combus_md5.h`, `combus_base_ids.h`, `combus_base_md5.h`, `combus_wire_common.h`, `combus_peer_links.h`.

Node mode: the compiled board gets the **unprefixed** names (the ones the C++ code includes); every **other** board of the node gets nine files prefixed by its `board_id` (`combus_<id>.h`, `combus_<id>.cpp`, `combus_<id>_ids.h`, `combus_system_<id>_ids.h`, `combus_base_<id>_ids.h`, `combus_<id>_md5.h`, `combus_system_<id>_md5.h`, `combus_base_<id>_md5.h`, `combus_peer_links_<id>.h`); `combus_wire_common.h` is not prefixed (`_expected_artifacts_for_node`). The C++ identifiers are not prefixed. With two boards, `test_node_mainboard` thus publishes 19 files (10 unprefixed and 9 for `extboard`). The `test_node_extboard` env is not meant to be compiled (only resolved statically).

Output folder (`_default_out_dir`):

- `Path(PROJECT_BUILD_DIR)/<env>/"combus_generated"` if `PROJECT_BUILD_DIR` is defined;
- otherwise `.pio/build/<env>/combus_generated`.

**One folder per env**: SCons decides to regenerate env by env, and two envs may have different sets of channels (through `requires`). A shared folder would let an env compile with the files written by another one. The folder is not cleaned by `-t clean`: to force the regeneration of an env, delete `<build_dir>/<env>/combus_generated`. `scripts/combus_test_add_generated_source.py` follows the same rule. Do not override `PLATFORMIO_BUILD_DIR` with a path inside the project: the "è" of "Modèlisme" breaks the link step (the `build_dir` of `platformio.ini` already puts the build outside the project for that reason).

The CLI writes by default into `<root>/out/combus_generated`.

## 9. SCons Hook

- A single deferred `env.Command`: the generation happens at build time, when `CPPDEFINES` is complete. `varlist=["CPPDEFINES", "CPPPATH", "LIBDEPS"]`. Sources of the command: the `.cb` and `.cbch` files found by `rglob` under the source folder, the code of the builder and `project_version.h`.
- Why `pre:` and a deferred command: in `pre:`, `CPPDEFINES` is not populated yet; in `post:`, the list of sources is already frozen. By only registering a SCons command when the hook loads, the generation runs later with the full environment, before the compilation of the files that include the generated headers. The variables of the `varlist` enter the SCons signature: a change of flags runs the generation again, an identical build does not. An `AddPostAction` would be too late to generate headers (`scripts/combus_builder_dump_ctx.py` uses it legitimately, see §11).
- The parser resolves its source folder in this order (`resolve_buildroot`): `env["PROJECT_SRC_DIR"]`, then `env["PROJECT_DIR"]/src`, then `project_root/src` (CLI mode), then `cwd/src`. The hook passes `env`, the CLI passes `project_root`.
- Anti-stale: writing into `<folder>.new`, completeness check (`_check_staging_complete`), then publication (`_publish`). Any error returns a non-zero code to SCons.
- `COMBUS_BUILDER_SKIP` (`1`, `true`, `yes`, `on`): the action becomes a no-op that only checks the presence of the `EXPECTED_ARTIFACTS`, not of the per-board files (`_skip_requested`, `_skip_action`). It is an explicit workaround: it does not check that the files match the flags, the `.cb` files nor the version of the builder.
- On a generation failure, the build stops before any compilation (non-zero return code of the action); the old artefacts stay on disk but are not used by this build. The publication `rmtree` then `os.replace` (`_publish`) leaves a short window without a folder: if the process is killed at that moment, the next build fails on a missing header rather than using an inconsistent set.
- A machine type is deduced once for the whole node; if a board resolves another one, the build stops. The hook logs `machine-type filter active` when a type is active.
- Every env of a machine carries its `-D MACHINE_TYPE_*` in `platformio.ini`. The `excavator` and `loader` types are declared in `MACHINE_TYPE_FLAGS` but have no `.cb` file.

## 10. Limits and Not Implemented

- **OR merge in C++**: done for `both_or` (`src/core/system/combus/protocol/both_or.md`), state in `DigitalComBus.bothOr` (`BothOrMerge`); `uplink_or` and `downlink_or` stay declarative.
- **Real build with two physical boards**: not tested yet.
- **YAML to `platformio.ini` script**: planned, not written. It must produce a root env (common configuration) and one sub-env per board, single board included, with `HAS_RUNLEVEL_MANAGER` on a single board per machine.
- **`chains:`**: no canoniser yet (`TODO(ComBus processors)` in `canon/__init__.py`).
- The C++ code includes the generated files directly: `#include <combus.h>`, `#include <combus_ids.h>`, `#include <combus_base_ids.h>`, or `src/core/config/machines/combus_types.h`, which does `#include <combus_ids.h>` (the hook adds the generated folder to the include path). `combus.h` is unique in the project: the `combus_init()` API is in `combus_init.h`.

## 11. Running the Tests

Windows cp1252 console: always `PYTHONIOENCODING=utf-8`. The whole folder runs at once, or one file at a time:

```text
set "PYTHONIOENCODING=utf-8"
python -m pytest scripts/combus_builder/tests
python -m pytest scripts/combus_builder/tests/test_nd2_diffusion.py
```

Files of `scripts/combus_builder/tests/`:

| File | Tests |
|---|---|
| `test_canon.py` | channel validation, merge, canonical sort |
| `test_value.py`, `test_safe_values.py`, `test_fault_value.py`, `test_neutral_value.py` | `value` field: validation, C++ rendering, safe value, `fault`, `CbusNeutral` |
| `test_zero_at.py` | `zeroAt` field of an analog channel |
| `test_flags.py`, `test_coherence.py` | `BuildContext`, coherence with `idedata` |
| `test_machine_type_filter.py` | machine type filter |
| `test_signature_consistency.py` | `env=` / `project_root=` signatures of `discover_and_parse` |
| `test_nd1_integration.py`, `test_nd1_multiboard.py` | node, boards, per-board generation |
| `test_nd2_diffusion.py`, `test_nd6_both_or.py` | `SYSTEM` diffusion, `both_or` |
| `test_mother_not_a_board.py` | the root env ("mother") is not a board |
| `test_node_category.py` | node category (`IS_MACHINE`, `IS_REMOTE`) and sides of the couplers |
| `test_node_direction.py` | direction inside the node, `downlink_pt` |
| `test_generator.py` | views, IDs, C++ rendering, compilation |
| `test_md5.py` | fingerprints, `ONE_WAY` canonisation |
| `test_candidates.py` | candidate channels, stable and next MD5, `DEV` field |
| `test_project_version.py` | reading `project_version.h` |
| `test_couplers.py` | couplers: set, order, tables, MD5 per side, local channels, passive |
| `test_remote_view.py`, `test_remote_contract.py` | mirror view of the remote, contract of the link with it |
| `test_dumper_profile.py` | the real channel set of the `dumper_truck` |
| `test_hook.py` | helper functions of the SCons hook (without SCons) |
| `test_stress.py` | property tests on random `.cb` files (see below) |
| `_cpp_compiler.py` | search for a C++ compiler (a helper, not a test) |

### Stress Tests (`test_stress.py`)

Each test draws random but valid `.cb` declarations, with a fixed seed: a failure is reproducible. Invariants checked:

- identical output between two runs and whatever the order of the files and of the channels;
- ID numbering: per type, from 0 to n-1, BASE then SYSTEM then BOARD; a channel keeps its ID in all the views; the views are prefixes;
- MD5 matrix: changing the default value of a `BASE` channel changes the three fingerprints, `SYSTEM` two, `BOARD` only one; `infoName`, a default value written explicitly and a satisfied `requires` change nothing; `uplink` / `downlink` stay distinct on the `BASE` view;
- any duplicate `id` is refused;
- absurd values in each field: only the validation exception is allowed;
- random YAML files: only `ParseError` is allowed;
- `SYSTEM` diffusion (2 to 5 boards, root env present or not) compared with an oracle written independently from the rules of `diffusion.py`: same directions, same errors, same warnings.

Settings: `COMBUS_STRESS_SEEDS` (number of seeds per test, 25 by default) and `COMBUS_STRESS_SEED0` (first seed, to replay a failure).

```text
set "COMBUS_STRESS_SEEDS=1500"
python -m pytest scripts/combus_builder/tests/test_stress.py
```

A run with 1500 seeds is 15 000 cases, about 2 minutes.

The compilation tests use `g++`, `clang++` or, failing that, the PlatformIO toolchain `xtensa-esp32-elf-g++` (`-fsyntax-only`); without a compiler they are skipped instead of failing.

Coherence tools with PlatformIO:

- `python -m scripts.combus_builder.coherence <env>...` compares the `BuildContext` acquired by the builder with the real definitions returned by `pio run -t idedata` (`--no-strict` to only report; without an argument, all the envs of `platformio.ini`).
- `scripts/combus_builder_dump_ctx.py` is an end-of-build hook that writes `combus_ctx.json` into the build folder, to compare the context of the real build with `idedata` through two separate `pio` invocations. It is referenced neither by `platformio.ini` nor by the main hook; it is used by the integration tests.
- The heavy integration tests of `test_flags.py` only run with `COMBUS_INTEGRATION=1`.

A test must fail through `assert` or through an exception. A test that does `return False` passes silently under pytest. To check that a test "bites", make the code fail on purpose (in memory or with a temporary change) and see the failure. `scripts` has no `__init__.py` (namespace package): when in doubt about the origin of an import, print `module.__file__`.
