# Formatting Guidelines

## 1. General Formatting Rules

### 1.1. Indentation
- Use tabulations for indentation, including in preprocessor blocks.

### 1.2. Line Length
- Lines should not exceed 80-100 characters for readability, except for debug log lines which should not be split.

### 1.3. Comments
- Use `//` for single-line comments.
- Use `/* ... */` for multi-line comments.
- Use `///` for Doxygen comments in `.h` files.
- Use `///<` for Doxygen comments for struct members in `.h` files.
- Use `/** ... */` for detailed Doxygen comments in `.cpp` files.

### 1.4. Sections
- Use `// ===` for section separators, with 80 characters.
- Section titles should be in uppercase.

### 1.5. Step Comments
- Use `// N. Step description` for step comments, with a single space after `//`.
- **Single-line comments should not have a step number.**

### 1.6. File Headers
- Use `/* ... */` for file headers, with a line of asterisks at 80 characters.
- Include `@file` and `@brief` in the header.

### 1.7. Vertical Spacing

A "block" consists of: Doxygen documentation + its associated code (function, class, struct, etc.).

**Within a block** (Doxygen → code): **1 empty line**
```cpp
/**
 * @brief Description.
 */

void function() {  // 1 empty line between Doxygen and its code
```

**Between blocks**: **2 empty lines**
```cpp
void function() {
    // code
}


/**
 * @brief Next function.  // 2 empty lines after previous block ends
 */
```

**Section separators** (`// ===`) are part of the block that follows them.

Complete rules:
- **1 empty line** after include blocks.
- **1 empty line** between Doxygen documentation and the code it documents (same block).
- **2 empty lines** between the end of one block and the start of the next block.
- **1 empty line** before class access sections (`public:`, `private:`).
- **1 to 2 empty lines** between function implementations in a `.cpp`.

### 1.8. End of File
- Use `// EOF <file>` at the end of each file.

## 2. Code Formatting

### 2.1. Naming Conventions
- Follow the naming conventions defined in `coding-conventions.md`.

### 2.2. Function and Method Definitions
- Use `///` for function documentation in `.h` files.
- Use `/** ... */` for detailed function documentation in `.cpp` files.

### 2.3. Struct and Enum Definitions
- Use `///<` for struct member documentation in `.h` files.
- Use `/** ... */` for detailed struct and enum documentation in `.cpp` files.

### 2.4. Preprocessor Directives
- Use tabulations for indentation in preprocessor blocks.

### 2.5. Debug Logs
- Debug logs should not be split across multiple lines.

## 3. Examples

### 3.1. File Header Example
```c
/*
 * @file module.h
 * @brief Brief description of the module.
 */
```

### 3.2. Section Example
```c
// === SECTION TITLE ===
```

### 3.3. Step Comment Example
```c
// Validate configuration
// Apply polarity inversion when configured
```

### 3.4. Single-line Comment Example
```c
// Optional cleanup sequence
```

### 3.5. End of File Example
```c
// EOF module.h
```

## 4. Checklist

- [ ] File header with `/* ... */` and `@file` and `@brief`.
- [ ] `#pragma once` in `.h` files.
- [ ] Sections with `// ===` and uppercase titles.
- [ ] Tabulations for indentation.
- [ ] Step comments with `// N. Step description` and a single space.
- [ ] Single-line comments without step numbers.
- [ ] `// EOF <file>` at the end of each file.