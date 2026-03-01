# Code Style Synthesis (Project + PwmBroker)

## 1) Goal
This style merges:
- project readability and section layout from `doc/Code formating template`
- library-oriented API clarity and Doxygen quality from `PwmBroker`

The objective is a clean, stable, educational style for both app code and reusable modules.

---

## 2) Core rules

### File header
- Use a short Doxygen block at top of each file.
- `@brief` is optional: when omitted, Doxygen uses the first sentence/paragraph as brief.
- Use `@details` only when useful.

### Sections
- Keep major sections with 80-char separators:
  - `// =============================================================================`
  - `// X. SECTION TITLE (CONSTANTS AND ENUMS, CLASS DEFINITION, etc.)`
  - `// =============================================================================`
- Keep one empty line between a section header block and the first code line.
- Keep one additional empty line before each 3-line major section block (`// ===`) to make section breaks more visible.
- Keep extra empty lines between large declaration blocks (enum/struct/class) when it improves readability.

### Local steps
- Use local step comments as:
  - `// --- Step description ---`
- If several steps are present, number them:
  - `// --- 1. First step description ---`
  - `// --- 2. Second step description ---`
- One sub-step depth is allowed for long logic:
  - `// --- 1. First step description ---`
  - `// --- 1.1 First sub-step description ---`
- For local step comments (`// --- ... ---`), always apply one additional indentation level relative to the next code line.
  - Example: if the next code line starts at 2 tabs, the step comment starts at 3 tabs.
  - This rule is mandatory for all local step comments in `.cpp` files.

### Language
- All comments and docs in English.

### Doxygen policy
- In `.h`:
  - use major sections to separate main parts
  - use `///` for one-line API docs or short declaration blocks
  - use `///<` for members and inline getters/setters
  - use `/** ... */` for class-level and main declaration docs
- In `.cpp`:
  - use `/** ... */` for non-trivial function behavior (`@details`, `@param`, `@return` when relevant)
  - keep internal comments only where logic needs clarification
  - keep one empty line between a Doxygen block and the code it documents
  - between end of a code block and the next Doxygen block:
    - use 2 empty lines by default
    - use 3 empty lines if the preceding code block exceeds 40 lines

### Naming and layout
- Class/type names: PascalCase
- Enum type names: PascalCase
- Enum values: UPPER_SNAKE_CASE
- Methods/functions: lowerCamelCase
- `#define` preprocessor macros: UPPER_SNAKE_CASE
- `constexpr` compile-time constants: PascalCase
- Variable names: explicit and readable (except short loop indices)

### Spacing and indentation
- Use tabs for block indentation in templates and reference code.
- Apply the same regular indentation rules inside preprocessor blocks (`#if`, `#elif`, `#else`, `#endif`) as in normal function/code blocks.
- Indent the preprocessor directives themselves (`#if`, `#elif`, `#else`, `#endif`) relative to their nesting level — the whole directive including `#` is indented with tabs, exactly like regular `if`/`else` keywords:
  ```cpp
  #ifdef OUTER
  	#if INNER == VALUE
  		#include "something.h"
  	#else
  		#error "Unknown value."
  	#endif
  #endif // OUTER
  ```
- For regular comment lines (`//`, `///`) placed before declarations/steps, use one additional tab relative to the code block below:
  - `/// ...` before enum/struct/method declarations
  - `// --- ... ---` before local logic steps
- For inline end-of-line comments (`//`, `///<`) placed after code on the same line:
  - always leave **at least 2 spaces** between the end of the code and the `//` or `///<`:
    ```cpp
    float vPin = ...;  // voltage at GPIO pin (V)        ✓
    bool  isLow;       ///< True when voltage is low.    ✓
    float vPin = ...;// wrong — no space                ✗
    ```
- For local step comments specifically, this means:
  - code line at indentation `N`
  - `// --- ... ---` comment at indentation `N + 1`
  - never keep `// --- ... ---` at the same indentation as the code line it describes
- Relative indentation rule:
  - if next code line starts at `N` tabs, comment line starts at `N + 1` tabs
  - example: code at `2` tabs -> comment at `3` tabs
  - **edge case `N=0`**: file-scope declarations (free functions, template functions) are at 0 tabs, so their `///` doc comment is at 1 tab:
    ```cpp
    	/// This comment is at 1 tab — the template below is at 0 tabs.
    template<typename... Args>
    inline void my_func(const char* fmt, Args... args) { ... }
    ```
- Keep major section separators unindented at file scope:
  - `// =============================================================================`
  - `// X. SECTION TITLE`
  - `// =============================================================================`
- Do not mix tab + spaces before emphasis comments.
- It is acceptable to visually emphasize comments more than the code line they document.
- Keep vertical spacing explicit:
  - 1 empty line after include blocks
  - 1 additional empty line before each major 3-line section block
  - 1 empty line before class/struct access sections (`public:`, `private:`)
  - 2 empty lines between end of code and next Doxygen block (3 if previous block > 40 lines)
  - 1 to 2 empty lines between function implementations in `.cpp`

### Include order
1. Matching header
2. Project headers
3. External/library headers
4. C/C++ standard headers

### End of file
- Keep a final marker comment:
  - `// EOF <file>`

---

## 3) Header template checklist (`.h`)
- File Doxygen block
- `#pragma once`
- Includes
- Major sections
- Readable blank lines between declaration blocks
- Type/class declarations
- Public API docs with `///`
- Members docs with `///<`
- EOF marker

## 4) Source template checklist (`.cpp`)
- File Doxygen block
- Matching header included first
- Major sections by topic
- Function bodies indented with tabs
- Non-trivial functions documented with `/** ... */`
- Local step comments (`// --- ... ---`)
- Readable blank lines between functions
- EOF marker

---

## 5) Practical convention for this repo
- Keep existing machine/core section style.
- Keep richer `@details` for reusable libraries and complex logic.
- Prefer concise comments for obvious code and detailed notes for non-trivial behavior.

---

## 6) Reference templates
Use:
- `doc/template_module.h`
- `doc/template_module.cpp`

---

## 7) Debug serial output formatting

### 7.0 Log system API (`debug.h`)

The active log system is template-based (`log_impl<Level, ModuleEnabled>`). No preprocessor guards around code — all dead branches are removed at compile time via `if constexpr`.

**Generic wrappers** (always active, level-filtered only):
```cpp
log_err(fmt, ...);   log_warn(fmt, ...);   log_info(fmt, ...);   log_dbg(fmt, ...);
```

**Module-gated wrappers** (also filtered by module flag, stripped from binary when flag not set):
```
hw_log_*(...)      → active when -D DEBUG_HW     (or DEBUG_ALL)
input_log_*(...)   → active when -D DEBUG_INPUT  (or DEBUG_ALL)
sys_log_*(...)     → active when -D DEBUG_SYSTEM (or DEBUG_ALL)
combus_log_(*)...  → active when -D DEBUG_COMBUS (or DEBUG_ALL)
```

**Level constants** (`LogNone=0`, `LogError=1`, `LogWarn=2`, `LogInfo=3`, `LogDebug=4`). Active threshold via `-D LOG_LEVEL=3` (default = 3 = info).

- Use `hw_log_*` / `sys_log_*` etc. for all module-scoped traces.
- Use generic `log_err` / `log_info` etc. only for outputs not tied to a module.
- Keep outputs human-readable first, then machine-grep friendly when practical.
- Keep this section evolutive: add concrete formatting rules as debug modules converge.

### 7.1 Line prefix convention (first concrete rule)
- Every debug line should start with a fixed module prefix:
  - `[<MODULE>] <message>`
- `<MODULE>`: short uppercase module identifier (examples: `INPUT`, `COMBUS`, `HW`, `SYSTEM`).
- Sub-module tagging drops the outer module bracket when the context is already established by indentation or a parent header line:
  - top-level line: `[HW] Hardware init start`
  - sub-level line (indented): `[DRV] DC drivers config check ... OK`  (no `[HW]` repetition)
  - use `[MODULE][SUBMODULE]` only when the line appears standalone without a parent context (e.g. error lines that may be grepped out of context)
- Keep one project-wide style when possible (prefer `[MODULE][SUBMODULE]` for standalone lines).
- Do not include `<STAGE>` in each line; stage context is provided by the sequence intro/header.
- Severity (`INFO` / `WARN` / `ERROR`) is conveyed by ANSI color when `SerialAnsi` is enabled:
  - normal/default color: `INFO`
  - yellow (`\033[33m`): `WARN`
  - red (`\033[31m`): `ERROR`
- If terminal color is unavailable (`SerialAnsi=0`), use explicit fallback tags in message body (`[WARN]`, `[ERROR]`).
- Keep module tags stable and uppercase to simplify serial filtering and grep.

Examples:
- `[INPUT] PS4 controller setup started`
- `[HW][DRV] Driver wakeup sequence started`
- `[HW - DRV] Driver wakeup sequence started`
- `[HW] Servo count exceeds recommended limit` (yellow module tag)
- `[COMBUS] Invalid channel index` (red module tag)

### 7.2 Log call formatting
- Every log call must fit on a **single line** — never split format string and arguments across lines.
- Applies regardless of line length; readability of the log call takes precedence over column limits.

```cpp
// correct
sys_log_warn("[VBAT] \"%s\" low! %.2f V < %.2f V\n", vBatSense->cfg[idx].infoName, vBatSense->state[idx].voltage, cutoff);

// wrong — split across lines
sys_log_warn("[VBAT] \"%s\" low! %.2f V < %.2f V\n",
  vBatSense->cfg[idx].infoName, vBatSense->state[idx].voltage, cutoff);
```

### 7.3 Recommended sub-module map (first draft)
- Use these tags as default references when available in the codebase.

| Module | Recommended sub-modules | Typical usage |
|---|---|---|
| `INPUT` | `PS4`, `MAP`, `WATCHDOG` | device read, input mapping, input loss/failsafe |
| `COMBUS` | `AN`, `DG`, `SYNC` | analog bus, digital bus, sync/reset paths |
| `HW` | `DRV`, `SRV`, `CFG` | DC drivers, servos, hardware config checks |
| `SYSTEM` | `BOOT`, `STATE`, `LOOP` | startup sequence, runlevel transitions, runtime snapshots |

- Keep sub-module tags short, uppercase, and stable over time.
- If a new sub-module is introduced, add it here to keep naming consistent across modules.

### 7.3 Hybrid output (normal mode only)
- For now, only define the normal mode output format.
- Defer super-verbose formatting rules to a later iteration.
- Keep field order stable for readability:
  1. title line
  2. board/parent
  3. config
  4. pins
  5. max speed
  6. Com channel
- `Mode` must include a readable name plus numeric value (example: `TWO_WAY_NEUTRAL_CENTER (1)`).
- `Parent` is optional and printed only when present.
- Inherited values should be marked with ` [INHERITED]` (and may be gray when color output is available).
- For DRV init attach lines, prefer human-readable sentence format over compact key/value blocks.
  - Preferred: `[DRV] DRV_0 attached to pin 32 at 16000Hz frequency`
  - Clone-specific suffix: append only ` (clone mode)`
  - Keep optional hardware detail lines (enable/sleep pin states) out of init flow; reserve them for final summary/verbose blocks.
- For LEDC fade helper setup (`ledc_fade_func_install`), install once per runtime (guarded static state) to avoid repeated `fade function already installed` error spam.

Example (normal mode):

`[HW][DRV] DC_DEV #6 - trailer rear left motor`
`  > Board Port: M-3B (ID:6) | Parent: DC_DEV #1 (CLONE)`
`  > Config: Freq:16000 Hz [INHERITED] | PolInv:YES [INHERITED] | Mode:TWO_WAY_NEUTRAL_CENTER (1)`
`  > Pins: PWM:0 BRK:15 EN:33 SLP:25 FLT:34`
`  > Max Speed FW: 100.0% | BK: 100.0%`
`  > Com Ch: THROTTLE (ID:1)`
