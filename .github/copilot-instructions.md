# Copilot Session Rules (Persistent)

These instructions are loaded for every coding session in this workspace.

## 1) Mandatory style re-check before coding
Before creating or editing C/C++ code, always read:
- `doc/code_style_synthesis.md`
- `doc/template_module.h`
- `doc/template_module.cpp`

If a generated file does not match these documents, fix formatting before finalizing.

## 2) C/C++ formatting baseline
- Respect the project section format (`// =============================================================================`).
- Keep Doxygen usage aligned with `doc/code_style_synthesis.md`.
- Keep comment indentation rules and spacing rules exactly as documented.
- Keep the final file marker: `// EOF <file>`.
- In every `.h/.cpp` file, `@file` must exactly match the real filename.

## 3) Workflow extension area
Add new persistent workflow rules here over time, for example:
- debug workflow presets
- commit message conventions
- test/build order
- release checklist

### Debug flag policy
- New debug points must use shared `DEBUG_*` flags only (`DEBUG_INPUT`, `DEBUG_HW`, `DEBUG_SYSTEM`, `DEBUG_COMBUS`, `DEBUG_ALL`).
- Do not introduce or reintroduce ad-hoc per-module flags (for example `DEBUG_HW_INIT`).
- Shared activation logic must stay centralized in `src/core/utils/debug/debug_core.h`.

### Periodic reminder note
- At appropriate moments (after a stable build/upload cycle, before large refactors, or at session wrap-up), proactively suggest refining this workflow section.
- Keep the reminder short and actionable (one concrete proposal at a time).
- When the reminder concerns debug readability, explicitly point to `doc/code_style_synthesis.md` section `7) Debug serial output formatting (WIP)`.

## 4) Safe change policy
- Prefer minimal, focused edits.
- Do not reformat unrelated code.
- Keep changes consistent with existing project style.
