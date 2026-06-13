# Definitions (`defs/`)

This directory contains shared definitions used throughout Open RC Node.

It groups enums, constants and lightweight types that describe common definitions reused by multiple modules and projects.

Typical contents include:

* system states and operating modes;
* machine and environment identifiers;
* hardware and protocol classifications;
* device roles and component categories;
* communication-related identifiers.

Definitions stored here should remain independent from runtime state and implementation details.

As a rule of thumb:

* `defs/` describes **what something represents**;
* `struct/` describes **how data is organized and stored**.

If a type is used to identify, classify or describe a concept shared across the project, it probably belongs in `defs/`.
