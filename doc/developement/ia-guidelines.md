# AI Guidelines

Mandatory startup file for any AI agent generating or modifying code, comments, or documentation in this project.

**Principle**: This file summarizes what an AI must and must not do. It systematically refers to detailed documents for formatting, code, and Doxygen rules. The only content specific to this file is AI agent behavior instructions.

---

## 1. Document Hierarchy

Read and apply documents in this order:

1. **`formatting.md`** — everything visual: headers, sections, spacing, indentation, comments, end of file.
2. **`coding-conventions.md`** — everything C/C++ code: naming, step breakdown, file structure, debug logs.
3. **`doxygen-conventions.md`** — everything Doxygen documentation: `@brief`, `@details`, `.h` vs `.cpp` focus.
4. **`ia-guidelines.md`** (this file) — expected AI agent behavior and ambiguity resolution.

---

## 2. Absolute Requirements (Non-Negotiable)

These points are not detailed here; they are developed in the documents above. An AI agent applies them systematically.

> For details and examples, see `formatting.md`, `coding-conventions.md`, and `doxygen-conventions.md`.

---

## 3. AI-Specific Rules

These rules do not appear in other documents. They exclusively concern AI agent behavior.

### 3.1 Do Not Invent

- Do not invent function, type, macro, or constant names that do not already exist in the modified file or its included headers.
- Do not invent log sub-modules not listed in `coding-conventions.md` §7.4.
- Do not invent `enum` values or `struct` fields: use those already defined.

### 3.2 Preserve Existing Style

- When modifying an existing file, adopt the style already in place (tabs, naming, comment style, section order).
- Do not convert existing correctly-styled code to another style, even if the latter is also valid.
- Do not reorganize a file without explicit request.

### 3.3 Priority in Case of Conflict

If two rules seem contradictory, apply this priority order:

1. Style already present in the modified file.
2. `formatting.md` and `coding-conventions.md` for base rules.
3. `doxygen-conventions.md` for documentation.
4. This file (`ia-guidelines.md`) for AI behavior guidelines.

### 3.4 Ambiguity Cases

- **Unknown naming**: choose the closest convention defined in `coding-conventions.md` §1; if the context is an existing module, follow the module's style.
- **Line length**: debug log lines are never split (see `coding-conventions.md` §7.3). For the rest, prefer readability to a strict limit, without massively exceeding 80-100 characters.
- **Useful or superfluous comment**: if the code is obvious, do not add a comment. If the behavior is non-trivial, add a step comment or `@details`.
- **Documentation for an existing undocumented function**: document according to `doxygen-conventions.md`. If the function is trivial, a `@brief` is sufficient; if it is non-trivial, add `@details` and relevant `@param`/`@return`.

### 3.5 Generated Files

- Never write code without a Doxygen header block.
- Never leave empty sections without an appropriate section marker.
- Always end with `// EOF <file>`.

---

## 4. Simplified AI Checklist

Before validating a modification, check at minimum:

- [ ] Header `/* ... */` at 80 characters with `@file` and `@brief`.
- [ ] `#pragma once` in `.h` files.
- [ ] Sections `// ===` at 80 characters if the file contains them.
- [ ] Tabs for indentation.
- [ ] Correct `@brief`: role name first, one line.
- [ ] No useless documentation duplication.
- [ ] No invented information.
- [ ] Existing file style respected.
- [ ] `// EOF <file>` at end of file.

> For complete details, see:
> - `formatting.md` §1, §2, §5, §8
> - `coding-conventions.md` §1, §2, §3
> - `doxygen-conventions.md` §3, §4

---

## 5. Visual Reference Examples

Complete templates are in the same folder:

- `template_module.h`
- `template_module.cpp`

These files illustrate compliant rendering. An AI agent can refer to them to understand the concrete application of rules.

---

## 6. What to Avoid

- Do not repeat in a file rules already present in `formatting.md`, `coding-conventions.md`, or `doxygen-conventions.md`.
- Do not add generic sections not specific to AI in this file.
- Do not ignore the style already in place in an existing file.

// EOF ia-guidelines.md
