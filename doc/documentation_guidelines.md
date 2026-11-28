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

* core_architecture.md
* build_a_system.md
* concepts.md

---

## 2. Component Documentation

Located directly inside the corresponding source directories as `README.md` files.

Purpose:

* Describe the role of a subsystem.
* Explain architecture and responsibilities.
* Document configuration options.
* State its rules and invariants, and the decisions behind them.

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

## 4. What to Write, What to Leave to the Code

The code can be read at any time, by a person or by an AI. A document is only worth keeping up by hand for what the code does not say. One test, sentence by sentence:

* **Could it be found again by reading the code?** Then it is not written: it would drift at the next change, and a stale document misleads more than a missing one.
* **Would it be lost if it were deleted?** Then it is written.

**Write:**

* **The intent**: what a module must do, what it promises. The document states the intent, the code implements it: when they differ, the code has a bug, not a new truth.
* **The concepts and the vocabulary**: what a word means in the project (a layer, an input, a source).
* **The rules and invariants**: what must stay true, and why.
* **The decisions**: the choice made, the paths set aside and the reason. While a work site is open, they live in its WIP; at its closure, the document keeps the decision and its reason, not the history.
* **The user documentation**: how to declare, configure and build, with examples of what the user writes (a `.cb`, a build flag).
* **What experience taught**: the pitfalls of the tools, the traps met on the boards.

**Leave to the code:**

* **Copies of code**: structures, enums, function bodies, formulas. Name them; never copy them.
* **Lists that follow the code**: files, functions, symbols, generated names, tables from a concept to its file.
* **Step-by-step descriptions** that follow the code line by line.
* **Values taken from the code**: sizes, thresholds, timings. Name the constant if the reader needs it, never its value.
* **Inventories**: the processors of a folder, the tests of a suite, the readers of a channel.

**Allowed:**

* A pointer to the folder or to the entry point of a module, in one line.
* The values of an enum that name a concept, with their meaning: the states of a link, the levels of a fault. They are vocabulary, not a copy of the enum.
* A short code example when it shows what the reader writes, or illustrates a convention. It is taken from living code, so that it does not outlive what it shows.

---

## General Principles

* Keep documentation close to the related code.
* Prefer multiple focused documents over a single large document.
* Document concepts before implementation details.
* Favor examples whenever possible.
* Keep documentation accessible to both humans and AI assistants.
* Update documentation alongside architectural changes.

## 5. Naming Convention

## Work-In-Progress Documents

Temporary documentation under active development or validation process should use the following naming convention:

```text
~document_name.md