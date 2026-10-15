# IA Guidelines

Single entry point for every AI agent working on the Open RC Node project.

An agent-specific file (for example `CLAUDE.md` at the repository root) holds the instructions that only concern that agent, then points here. This file holds what applies to **all** AI agents, and points to the human-readable documents of each topic.

---

## 1. Reading Order

1. This file (rules for AI agents, sections 2 to 5).
2. [Documentation Guide](README.md) — find the topic you work on (see "By Subject").
3. Before writing or changing C/C++: the conventions below.
4. Before writing documentation: [Documentation Guidelines](documentation_guidelines.md).

---

## 2. Conventions Quick Reference

| Document | Purpose |
|----------|---------|
| [Development Guidelines](developement/README.md) | Overview of the conventions |
| [Layout Rules](developement/conventions/layout.md) | How to organize code within files |
| [Coding Standards](developement/conventions/coding.md) | Semantic rules, naming, debug facilities (§7) |
| [Formatting Items](developement/conventions/formating.md) | Visual appearance of code elements |
| [Doxygen Conventions](developement/conventions/doxygen.md) | Documentation standards |

### Configuration-Specific Guidelines

* [Coding Configurations](developement/conventions/coding_configs.md)
* [Coding Runners](developement/conventions/coding_runners.md)
* [Coding Structures](developement/conventions/coding_structures.md)
* [Coding Dispatcher](developement/conventions/coding_dispatcher.md)
* [Runners](developement/conventions/runners.md)

### Templates

Reference files showing a compliant rendering: [`developement/template/`](developement/template/) (`module.h`, `module.cpp`, `code.h`, `code.cpp`). Read them before creating a new file.

---

## 3. AI-Specific Rules

These rules concern the behavior of an AI agent only; they are not repeated in the convention documents.

### 3.1 Do Not Invent

* Do not invent function, type, macro or constant names that do not already exist in the modified file or its included headers.
* Do not invent log sub-modules that are not listed in [Coding Standards](developement/conventions/coding.md) (§7, Debug Facilities).
* Do not invent `enum` values or `struct` fields: use the ones already defined.
* If a file or a symbol cannot be found, say so instead of guessing.

### 3.2 Preserve Existing Style

* When modifying an existing file, adopt the style already in place (tabs, naming, comment style, section order).
* Do not convert correctly-styled code to another valid style.
* Do not reorganize a file, and do not reformat code you do not touch, without an explicit request.
* Make minimal, focused edits.

### 3.3 Priority in Case of Conflict

1. Style already present in the modified file.
2. Formatting, Layout and Coding conventions for the base rules.
3. Doxygen conventions for documentation.
4. This file for AI behavior.

### 3.4 Ambiguity Cases

* **Unknown naming**: choose the closest convention in [Coding Standards](developement/conventions/coding.md) §1; inside an existing module, follow the module's style.
* **Line length**: debug log lines are never split. Otherwise prefer readability, without massively exceeding 80-100 characters.
* **Useful or superfluous comment**: if the code is obvious, add no comment. If the behavior is non-trivial, add a step comment or `@details`.
* **Undocumented existing function**: document it according to the [Doxygen Conventions](developement/conventions/doxygen.md). A trivial function needs a `@brief` only.

### 3.5 Generated Files

* Never write code without a Doxygen header block.
* Never leave an empty section without its section marker.
* Always end with `// EOF <file>`.

### 3.6 Acceptance of AI-Generated Code

An AI may introduce new internal mechanisms when they simplify a local implementation. These mechanisms must not change the fundamental concepts of the project without explicit validation. The shared concepts (structures, definitions, cfg/state separation, memory organization) take priority over abstractions proposed automatically.

---

## 4. Simplified Checklist

Before validating a modification:

- [ ] Header `/* ... */` at 80 characters with `@file` and `@brief`.
- [ ] `#pragma once` in `.h` files.
- [ ] Sections `// ===` at 80 characters if the file contains them.
- [ ] Tabs for indentation.
- [ ] Correct `@brief`: role name first, one line.
- [ ] No useless documentation duplication.
- [ ] No invented information.
- [ ] Existing file style respected.
- [ ] `// EOF <file>` at end of file.

---

## 5. Session Rules

### Commit Messages

```
<type>(<scope>): <short description>
```

Types: `feat` / `fix` / `refactor` / `chore` / `docs` / `test`. Scope: the subsystem (`combus`, `hook`, `builder`, `input`, `vbat`, `debug`, `motion`...). Description under 72 characters, lowercase, no final period. This is the format used in `git log`.

### Debug Flags

Use only the shared flags: `DEBUG_INPUT`, `DEBUG_HW`, `DEBUG_SYSTEM`, `DEBUG_COMBUS`, `DEBUG_ALL`. No ad-hoc flag per module. They are centralized in `src/core/system/debug/debug.h`.

### `static_assert` Placement

* Constants of a single header: in that header.
* Constants crossing two distinct include chains: in the init `.cpp` where both chains are resolved.

### End of Session

1. Propose a commit message (format above).
2. If the last build ended with exit 0, say so: the firmware can be uploaded directly.
3. PS4 library check (below).

### PS4_Controller_Host — Periodic Check

The fork `API-rep/PS4_Controller_Host` @ `b58a05d` fixes the Bluetooth init for IDF 4.4. The project is **locked on IDF 4.4** (`espressif32@6.7.0`): the SPP code of `ps4.c` is incompatible with IDF 5.x. At the end of a session, or before a platform upgrade, check whether the upstream `pablomarquez76/PS4_Controller_Host` supports IDF 5.x in `ps4.c` / `ps4_spp.c`. If it does, evaluate a platform migration.

---

## Entry Points

* [Main Documentation](README.md) — Start here for the project overview and the subject index
* [Development Guidelines](developement/README.md) — Development conventions overview
