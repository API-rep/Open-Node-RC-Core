

## 7. Serial Debug Output Formatting

### 7.1 Log System API (`debug.h`)

The active logging system is based on templates (`log_impl<Level, ModuleEnabled>`). No preprocessor guards are needed around the code: all dead branches are removed at compile time via `if constexpr`.

**Generic wrappers** (always active, filtered by level only):

```cpp
log_err(fmt, ...);
log_warn(fmt, ...);
log_info(fmt, ...);
log_dbg(fmt, ...);
```

**Module-filtered wrappers** (also filtered by a module flag, removed from binary when the flag is not defined):

```
hw_log_*(...)      → active when -D DEBUG_HW     (or DEBUG_ALL)
input_log_*(...)   → active when -D DEBUG_INPUT  (or DEBUG_ALL)
sys_log_*(...)     → active when -D DEBUG_SYSTEM (or DEBUG_ALL)
combus_log_*(...)  → active when -D DEBUG_COMBUS (or DEBUG_ALL)
```

**Levels**: `LogNone=0`, `LogError=1`, `LogWarn=2`, `LogInfo=3`, `LogDebug=4`. Active threshold via `-D LOG_LEVEL=3` (default = 3 = info).

- Use `hw_log_*` / `sys_log_*` / etc. for all module-related traces.
- Use generic wrappers `log_err` / `log_info` only for outputs not attached to a module.
- Prefer human-readable text, then machine-grep friendly when relevant.
- This section is evolving: add concrete rules as debug modules converge.

### 7.2 Line Prefix

Each debug line must start with a fixed module prefix:

```
[<MODULE>] <message>
```

- `<MODULE>`: short uppercase identifier (examples: `INPUT`, `COMBUS`, `HW`, `SYSTEM`).
- Sub-module tagging drops the external module when the context is already established by indentation or a parent header line:
  - Top-level line: `[HW] Hardware init start`
  - Sub-level line (indented): `[DRV] DC drivers config check ... OK` (no `[HW]` repetition)
  - Use `[MODULE][SUBMODULE]` only when the line appears isolated without parent context (for example, error lines that can be grepped out of context).
- Keep a consistent style across the project when possible (prefer `[MODULE][SUBMODULE]` for isolated lines).
- Do not include `<STAGE>` in each line; stage context is provided by the introduction / sequence header.
- Severity (`INFO` / `WARN` / `ERROR`) is conveyed by ANSI color when `SerialAnsi` is active:
  - default color: `INFO`
  - yellow (`\033[33m`): `WARN`
  - red (`\033[31m`): `ERROR`
- If terminal color is not available (`SerialAnsi=0`), use explicit tags in the message body (`[WARN]`, `[ERROR]`).
- Keep module tags stable and uppercase to simplify serial filtering and grep.

Examples:

```
[INPUT] PS4 controller setup started
[HW][DRV] Driver wakeup sequence started
[HW] Servo count exceeds recommended limit   (yellow tag)
[COMBUS] Invalid channel index               (red tag)
```

### 7.3 Log Call Formatting

Each log call must fit on **a single line** — never split the format string and its arguments across multiple lines.
This applies regardless of line length; log call readability takes precedence over column limits.

```cpp
// correct
sys_log_warn("[VBAT] \"%s\" low! %.2f V < %.2f V\n", vBatSense->cfg[idx].infoName, vBatSense->state[idx].voltage, cutoff);

// wrong — split across lines
sys_log_warn("[VBAT] \"%s\" low! %.2f V < %.2f V\n",
  vBatSense->cfg[idx].infoName, vBatSense->state[idx].voltage, cutoff);
```

### 7.4 Recommended Sub-module Map (first draft)

| Module | Recommended Sub-modules | Typical Usage |
|---|---|---|
| `INPUT` | `PS4`, `MAP`, `WATCHDOG` | device reading, input mapping, signal loss / failsafe |
| `COMBUS` | `AN`, `DG`, `SYNC` | analog bus, digital bus, sync/reset paths |
| `HW` | `DRV`, `SRV`, `CFG` | DC drivers, servos, hardware config checks |
| `SYSTEM` | `BOOT`, `STATE`, `LOOP` | startup sequence, runlevel transitions, runtime snapshots |

- Keep sub-module tags short, uppercase, and stable over time.
- If a new sub-module is introduced, add it here to maintain consistent nomenclature.

### 7.5 Hybrid Output (normal mode only)

- For now, only define the output format for normal mode.
- Defer super-verbose rules to a later iteration.
- Keep field order stable for readability:
  1. title line
  2. board/parent
  3. config
  4. pins
  5. max speed
  6. Com channel
- `Mode` must include a readable name plus the numeric value (example: `TWO_WAY_NEUTRAL_CENTER (1)`).
- `Parent` is optional and only displayed if present.
- Inherited values must be marked with ` [INHERITED]` (and can be grayed out when color is available).
- For DRV init attachment lines, prefer a readable sentence format over a compact key/value block.
  - Preferred: `[DRV] DRV_0 attached to pin 32 at 16000Hz frequency`
  - Clone suffix: only add ` (clone mode)`
  - Keep optional hardware detail lines (enable/sleep states) out of the init flow; reserve them for a final / verbose block.
- For LEDC fade helper installation (`ledc_fade_func_install`), install it only once per runtime (static state kept) to avoid `fade function already installed` error spam.

Example (normal mode):

```
[HW][DRV] DC_DEV #6 - trailer rear left motor
  > Board Port: M-3B (ID:6) | Parent: DC_DEV #1 (CLONE)
  > Config: Freq:16000 Hz [INHERITED] | PolInv:YES [INHERITED] | Mode:TWO_WAY_NEUTRAL_CENTER (1)
  > Pins: PWM:0 BRK:15 EN:33 SLP:25 FLT:34
  > Max Speed FW: 100.0% | BK: 100.0%
  > Com Ch: THROTTLE (ID:1)
```

---



// EOF coding-conventions.md
