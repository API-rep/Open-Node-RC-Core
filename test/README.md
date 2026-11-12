# Tests

The project is tested at three levels:

| Level | Where it runs | What it checks |
|---|---|---|
| [ComBus Builder](../scripts/combus_builder/README.md#11-running-the-tests) | the PC, with pytest | the generation of the ComBus from the `.cb` files |
| [Unity suites](#unity-suites) | one board | the firmware modules, a link looped back on the board itself |
| [Two-board bench](#two-board-bench) | two boards, driven from the PC | real links between two boards |

This page covers the two levels on the boards.

---

## Unity Suites

Unity suites are compiled with a selection of the project sources, uploaded to the board and run there, the results coming back on the serial port. This section tells which suites exist, how to run them, and how to write a new one without falling into the traps of PlatformIO 6.

### The Suites

One suite is one folder under `test/` and one env of the same name in `platformio.ini`.

| Suite | What it tests | Wiring |
|---|---|---|
| `test_combus_loopback` | the frame codec, the UART loopback, link health, the contract, modes, filters | TX↔RX jumper on the UART of the link |
| `test_peer_link` | a link between two nodes through the generated tables, coupling, pairing | same jumper |
| `test_remote_link` | the link of a remote to the machine it drives | same jumper |
| `test_runlevel` | the runlevel chain of the Volvo, `HALT` held until a reboot | none |

With the jumper, the board talks to itself: what it sends on TX comes back on RX, so a single board plays both ends of a link. The pins are those of the link in the configuration of the test env.

### Running a Suite

```bash
# Compile, upload and run on the board
pio test -e test_<name> --upload-port <port> --test-port <port>

# Compile only (no board plugged in)
pio test -e test_<name> --without-uploading --without-testing

# List the discovered suites
pio test -e test_<name> --list-tests
```

If the `pio` command is not available, `python -m platformio` does the same.

### Adding a Suite

Start from this template:

```ini
[env:test_<name>]
test_framework   = unity
test_filter      = test_<name>
test_build_src   = yes                       ; DO NOT FORGET
extra_scripts    =
    pre:scripts/combus_scons_hook.py
    pre:scripts/combus_test_add_generated_source.py
build_src_filter = -<*> +<the/sources/**>
build_flags      = ${env.build_flags} -D THE_FLAGS -I src -I include
```

Each line is there for a reason (PlatformIO 6.1.19):

* **The folder starts with `test_`.** PlatformIO silently ignores any folder under `test/` whose name does not: no error, just "Nothing to build".
* **`test_filter` is exactly the name of the folder**, without a glob: `test_combus_loopback`, not `combus_loopback/*`.
* **`test_build_src = yes`.** It is off by default: without it, the sources of `src/` are not compiled, and the link fails on dozens of `undefined reference`, even with a correct `build_src_filter`.
* **The second extra script.** The suites use the channel arrays (`AnalogComBusArray`, `DigitalComBusArray`) defined in the generated `combus.cpp`. The ComBus hook generates the file; `combus_test_add_generated_source.py` adds it to the sources of the test.
* **`build_src_filter` on a single line.** The multi-line syntax does not work for this option.
* **`<folder/**>` is not recursive**: it only takes the files directly under `<folder/>`. To take the subfolders too, write `<folder/**/*.cpp>` (see the `hw/` filter of `test_combus_loopback`).
* **The flags describe a node**, like a real board: its category (`IS_MACHINE`, `IS_REMOTE`), its machine type, and the `HAS_*` flags its channels require. Without them, the ComBus hook refuses the build, or does not generate the channels the test reads.

A suite whose sources include `<config/config.h>` can carry a minimal stub of it in its own folder (`test/test_combus_loopback/config/config.h`): the test runner puts the folder of the suite on the include path.

### When It Goes Wrong

| Symptom | Cause |
|---|---|
| "Nothing to build" | the folder does not start with `test_`, or `test_filter` does not match it |
| dozens of `undefined reference` at link time | `test_build_src = yes` is missing, or a source is missing from `build_src_filter` |
| `AnalogComBusArray` undefined | the second extra script is missing |
| `TypeError: unsupported operand type(s) for -: 'float' and 'NoneType'` | an internal SCons error on a first library install (corrupted cache): delete the cache below and run again |

```powershell
Remove-Item "$env:LOCALAPPDATA\Temp\PlatformIO\build\Open_Node_RC_Core\test_<name>" -Recurse -Force
Remove-Item ".pio\libdeps\test_<name>" -Recurse -Force
```

---

## Two-Board Bench

*Being written.* A Unity run drives a single board; a real link needs two, driven at the same time. The bench is a Python script, `test/two_boards/bench.py`, that flashes two boards wired crossed (TX of one to RX of the other, both ways), drives them over their USB serial and prints one PASS / FAIL line per check. The firmware of the bench is in `src/bench/`, one env per role in `platformio.ini` ("TWO-BOARD BENCH" section).

This section will be completed when the bench is finished.
