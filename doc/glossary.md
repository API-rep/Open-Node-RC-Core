# Glossary

Words used throughout the project.

| Word | Meaning |
|---|---|
| **node** | any component of the RC ecosystem with a compiled firmware and its own ComBus |
| **board** | one microcontroller and its firmware; a node has one or more boards |
| **machine** | a node that is driven (`IS_MACHINE`): first level (the remote connects to it) or carried (reached through what carries it, or directly when stand-alone) |
| **remote** | a node that drives (`IS_REMOTE`) |
| **coupler** | the norm of a link between a socket and a plug (its folder `src/core/config/couplers/<name>/`) |
| **socket** | the side that carries: `HAS_COUPLER_SOCKET_<NAME>` (0 to n per node) |
| **plug** | the side that is carried: `HAS_COUPLER_PLUG_<NAME>` (0 or 1 per node) |
| **tool** | an element carried without a node: a passive coupler, socket only |
| **coupled / stand-alone** | the role at a given time, read in the state of the coupling (a towed trommel is coupled when hitched, stand-alone otherwise) |
| **layer** | the scope of a channel: `BASE` (between nodes), `SYSTEM` (inside a node), `BOARD` (inside a board) |
| **safe value** | the value of a channel when nobody writes it, or when its source is lost |
