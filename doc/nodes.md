# Nodes

## What a Node Is

**Node** is the generic name of any component of the RC ecosystem: a machine, a remote, a trailer, a dedicated module...

* A node typically has **its own compiled firmware and its own ComBus**.
* Nodes are intended to **interact**: a remote drives a machine, a truck tows a trailer. But a node can also live alone as a simple RC machine.
* Whatever its size, a node presents itself to the others as **a single participant**, with one category declared in its firmware: a **machine** is driven (`IS_MACHINE`), a **remote** drives (`IS_REMOTE`).

---

## Inside a Node

A single main board for a node is often enough. When it is not, a multi-board architecture is available, in two forms:

* **plug-and-play boards**, plugged into a module of the main board: fixed-function hardware (an RF receiver, an IO expander) driven by that module. They have no firmware nor ComBus of their own (see the *peripherals* of [Board Architecture](board_architecture.md));
* **secondary boards**, each with its own microcontroller and firmware, that talk with the main board as peers.

```text
   ┌─────────────────────────┐
   │ Truck (one node)        │
   │                         │
   │   main board            │
   │      │                  │
   │      │ link inside      │
   │      ▼ the node         │
   │   secondary board       │
   └─────────────────────────┘
```

Any secondary board can host any module: motors, inputs, sound, lights. Each board owns and declares the channels of the modules it hosts.

---

## What the Boards Share

The boards of a node interact by sharing the `SYSTEM` channel layer, which includes the channels of the machine type (the `BASE` layer):

* a channel declared on one board reaches the others automatically: a module on one board can read a value produced on another, without being enabled there;
* the channels of the `BOARD` layer stay private to their board;
* some system channels are owned by a single board, the main board by convention: the run level (`RUNLEVEL`), for instance;
* the failsafe channels (`FAILSAFE_HARD`, the emergency stop, and `FAILSAFE_SOFT`, the hold) are shared: a fault raised on any board puts the whole node in failsafe.

---

## Going Further

* [ComBus](combus.md): the shared register and its layers.
* [Linking Nodes](linking_nodes.md): how nodes are linked together.
* [Board Architecture](board_architecture.md): the build flags that describe a firmware.
