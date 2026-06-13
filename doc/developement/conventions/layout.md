# File Layout and Structural Architecture

This document defines the macro-visual organization and skeleton blueprints for files based on their functional role.
It provides the mandatory layout blocks and structural order that developers must follow when creating new files.

### Scope
- **File Blueprints:** Specific structural patterns for Umbrella, Classic C++, and Config files.
- **Block Layout & Separators:** Mandatory sequence of sections, major block markers, and file termination tags.
- **Structural Spacing:** Vertical empty lines required between major structural blocks or sections.

# 1. Header and File Layout

## Header File Blueprint (`.h`)

Header files should follow this general structure:

1. `#pragma once`
2. Includes
3. Major declaration sections
4. Type and class declarations
5. Public API declarations
6. EOF marker

## Source File Blueprint (`.cpp`)

Source files should follow this general structure:

1. Includes
2. Major implementation sections
3. Function implementations
4. Empty lines between functions
5. EOF marker


# 2. Règles de composition (à traduire)

### Vertical Spacing

- **1 empty line** after include blocks.
- **1 additional empty line** before each major section block (`// ===`).
- **1 empty line** before class access sections (`public:`, `private:`).
- **2 empty lines** between the end of a code block and the+ next Doxygen block (3 if the previous block exceeds 40 lines).
- **1 to 2 empty lines** between function implementations in a `.cpp`.


### Step Layout

- Step comments are indented one level deeper than the code they describe.
- Nested steps follow the same indentation rules.
- Only one level of nested steps is allowed.