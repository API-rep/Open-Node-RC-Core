# Documentation Guidelines

The Open RC Node documentation is organized around three complementary levels.

## 1. Project Documentation

Located in the `doc/` directory.

Purpose:

* Explain concepts.
* Present the ecosystem.
* Guide new users for their first project.
* Describe workflows and common use cases.

These documents should remain technology-agnostic whenever possible and focus on understanding rather than implementation details.

Examples:

* ecosystem.md
* build_a_system.md
* architecture.md

---

## 2. Component Documentation

Located directly inside the corresponding source directories as `README.md` files.

Purpose:

* Describe the role of a subsystem.
* Explain architecture and responsibilities.
* Document configuration options.
* Provide implementation-specific information.

Examples:

* src/core/system/combus/README.md
* src/machines/README.md
* src/remotes/README.md

Documentation should remain as close as possible to the code it describes.

---

## 3. Source Code

The source code remains the ultimate reference for implementation details.

Documentation should explain intent, architecture and usage, but should avoid duplicating information already obvious from the code.

---

## General Principles

* Keep documentation close to the related code.
* Prefer multiple focused documents over a single large document.
* Document concepts before implementation details.
* Favor examples whenever possible.
* Keep documentation accessible to both humans and AI assistants.
* Update documentation alongside architectural changes.

## 4. Naming convention

## Work-In-Progress Documents

Temporary documentation under active development or validation process should use the following naming convention:

```text
~document_name.md