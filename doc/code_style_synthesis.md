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
- Methods/functions: lowerCamelCase
- Constants/macros: UPPER_SNAKE_CASE
- Variable names: explicit and readable (except short loop indices)

### Spacing and indentation
- Use tabs for block indentation in templates and reference code.
- For regular comment lines (`//`, `///`) placed before declarations/steps, use one additional tab relative to the code block below:
  - `/// ...` before enum/struct/method declarations
  - `// --- ... ---` before local logic steps
- Relative indentation rule:
  - if next code line starts at `N` tabs, comment line starts at `N + 1` tabs
  - example: code at `2` tabs -> comment at `3` tabs
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
