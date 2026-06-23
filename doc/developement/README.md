## Purpose

These conventions exist to keep Open RC Node consistent, readable and easy to evolve.

They aim to provide:

* clear and predictable code organization;
* readable APIs and self-explanatory interfaces;
* documentation that stays close to the implementation;
* patterns that support long-term maintenance;
* a development style that works well for both human contributors and AI-assisted workflows.

The objective is not to enforce arbitrary rules, but to establish a common language that helps contributors understand, review and extend the project with confidence.

---

## Convention Categories

The development conventions are intentionally split into several focused documents.

The same concept may appear in multiple places, but each document addresses a different aspect of it:

* **Formatting** defines how things look.
* **Layout** defines where things are placed and how they are organized.
* **Coding** defines why and when a construct should be used.
* **Doxygen** defines how it should be explained to future readers.

This separation avoids mixing concerns and keeps each document focused on a single question:

### Core Conventions

| Question | Reference |
|----------|-----------|
| What does it look like? | [Formatting Items](conventions/formating.md) |
| Where does it go? | [Layout Rules](conventions/layout.md) |
| Why is it written this way? | [Coding Standards](conventions/coding.md) |
| How should it be documented? | [Doxygen Conventions](conventions/doxygen.md) |

### Specialized Topics

* [Coding Configurations](conventions/coding_configs.md) — Configuration-specific patterns
* [Coding Runners](conventions/coding_runners.md) — Runner implementation guidelines
* [Coding Structures](conventions/coding_structures.md) — Data structure conventions
* [Runners](conventions/runners.md) — Runner system documentation

---

## Related Resources

* [Main Documentation](../README.md) — Project documentation entry point
* [IA Guidelines](../ia-guidelines.md) — Quick reference for AI-assisted development
