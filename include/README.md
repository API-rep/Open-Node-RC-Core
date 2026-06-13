# Include Directory

This directory contains the shared data definitions used throughout Open RC Node.

Unlike implementation-specific headers located next to their corresponding modules, the files placed under `include/` define common contracts that may be consumed by multiple parts of the system.

The goal is to provide a stable and centralized reference for types, structures and constants that describe the RC environment itself.


## Contents

* `defs/` Shared definitions, enums and common identifiers used across the project. See [`defs/README.md`](defs/README.md) for details and placement guidelines.

* `struct/` Shared data structures describing configurations, runtime states and containers. See [`struct/README.md`](struct/README.md) for organization principles and ownership patterns.

* `const.h`
  Global constants and project-wide aliases used by multiple modules.


## Design Philosophy

Only place a header in `include/` when it represents a reusable contract shared by multiple modules. Implementation details, private helpers and module-specific logic should remain close to the code that owns them.

Keeping common definitions centralized helps maintain consistency across the project while reducing duplication and coupling.
