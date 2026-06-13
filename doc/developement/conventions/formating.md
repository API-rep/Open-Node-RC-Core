# Code Formatting and Style Guidelines

This document defines the micro-visual style and cosmetic rules applied to the code text.
These non-semantic guidelines ensure pixel-perfect consistency across the repository and are designed to be strictly enforced by automated formatting tools (e.g., Clang-Format).

### Scope
- **Indentation & Whitespace:** Characters used (tabs/spaces), margins, and spaces around operators or braces.
- **Line Length Limits:** Rules regarding maximum column width and single-line execution blocks.
- **Inline Elements Style:** Spaces inside comments, around keywords, and bracket positioning.



### Step Comments

Visual markers used to identify logical phases within a function.

Examples:
- Simple step: `// 1. Step description`
- Nested step: `// 1.1 Sub-step description`

### Major Sections (à retravailler comme "step comments")

Major sections group declarations or implementations by topic.
They should be used to separate large functional areas and improve navigation within the file.
ex :
`// =============================================================================`
`// 1. MAJOR SECTION`
`// =============================================================================`

### EOF Marker

Every source and header file should terminate with an EOF marker.

Example:
// EOF module.cpp4

### Debug Log Call Formatting

Debug Log calls should remain on a single source line.

Avoid splitting format strings and arguments across multiple lines.
+ ref vers le readme 
 
## Step Comment Formatting - The "Step Comment" Object

A "Step Comment" is a standardized visual marker used to segment code logic. It must strictly comply with the following formatting rules:

- **Syntax**: It must start with two slashes followed by exactly one space, the step number, and a single space before the text (e.g., `// N. Step description`).
- **Nesting Limits**: Only one level of sub-steps is authorized, using the dot notation (e.g., `// N.M Sub-step description`).
- **Optional Numbering**: If a function contains only one single logical block, the step number prefix is optional.

- **Inline Spacing**: Always insert a single space character immediately after the comment slashes (e.g., `// N. Step`).
- **Relative Indentation**: Step comments must be indented exactly one tab further than the code line immediately following them.
