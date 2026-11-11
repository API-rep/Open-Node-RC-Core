# Linking Nodes

A machine rarely lives alone: a remote drives it, a truck tows a trailer, an excavator carries a bucket.

This document explains how Open RC Node links [nodes](nodes.md) together, and what keeps those links safe. It focuses on concepts; the mechanics live in the documents listed at the end.

---

## Two Kinds of Links

* **Inside a node**, the main board and its sub-boards share the channels of their node (the `SYSTEM` layer, which includes `BASE`). See [Inside a Node](nodes.md#inside-a-node).
* **Between nodes**, a remote and its machine, a truck and its trailer exchange the channels of the machine type (the `BASE` layer).

This document focuses on this second kind of link.

```text
        Remote
          │  link between nodes
          ▼
        Truck
          │  link between nodes, through a coupler
          ▼
        Trailer
```

---

## One Link, One Set of Channels

Each node keeps its own ComBus, fixed at build time. It already contains the optional channels of the nodes it can be linked to.

Two channels are matched when they have the **same name**. A channel the other end does not know is simply not part of the link. This is why channel names are a **standard**, shared by the machine types and the couplers: the same function must have the same name everywhere.

---

## Couplers

A **coupler** is the interface of a link between two machines, like a standard trailer plug: it says which channels travel through it, and in which direction.

Two machines that declare the same coupler can be hitched without knowing each other in advance: a trailer works with any truck that has the same socket.

A coupler has two sides:

* the **socket** carries: the truck, the excavator with its bucket. A node may have several sockets;
* the **plug** is carried: the trailer, the trommel. A node has one plug at most.

A **tool**, such as a bucket, has no node: there is nobody to talk to. Detecting its presence is enough; such a specific coupler is called **passive**.

The machine type declares the couplers it supports, at build time. A machine can only fit a coupler its type supports.

### The Star

Links form a star: a node may have several sockets, so several partners around it. The plug side never relays a value to another node, which avoids sending loops.

---

## Hitching

Hitching follows four steps, each one secured:

1. **Detection**: both sides see that they are hitched.
2. **Recognition**: each side makes sure who its partner is, even with several machines in range.
3. **Agreement**: both sides check that they follow the same coupler, each on its own side.
4. **Exchange**: the channels of the coupler travel.

When unhitched, the link goes off. A short loss of contact does not unhitch.

---

## The Remote

The link between a remote and its machine needs no hitching: it is on as soon as both are powered. It has its own set of channels: only what the remote sends. The remote sends inputs; the machine computes its own state.

Today a remote drives a single machine, chosen at build time. Driving several machines, and switching from one to another, is planned.

See [Remote](../src/remotes/README.md).

---

## Agreeing Before Exchanging

Before any data, both ends of a link agree on a **contract**, a fingerprint of the channels of the link. Nothing is applied until they agree.

The other part of the contract is software compatibility: only a major version update breaks the compatibility between two nodes.

See [Handshake](../src/core/system/combus/protocol/handshake/README.md).

---

## Sharing Channels Safely

Each side declares what it sends and what it receives. A channel has one sender across all the linked nodes; a value can be relayed from one link to another, and a shared fault can be raised by every end.

Every channel also has a **safe value**. When a link stops being alive, what the node received through it returns to that value: a machine never keeps obeying a command that no longer arrives. The loss itself is reported as a fact; the failsafe chains decide what it means.

See [ComBus](../src/core/system/combus/README.md), [Links](../src/core/system/combus/protocol/link/README.md) and [Failsafe module](../src/core/system/failsafe/failsafe_module.md).

---

## Going Further

* [ComBus](../src/core/system/combus/README.md): the shared register, its layers and its channels.
* [ComBus protocol](../src/core/system/combus/protocol/README.md): how links are described, sent, received and watched.
* [Links](../src/core/system/combus/protocol/link/README.md): declaring, starting and watching a link.
* [Handshake](../src/core/system/combus/protocol/handshake/README.md): the contract of a link, its states and its modes.
* [Coupling and pairing](../src/core/system/combus/protocol/coupling/README.md): the frame, the state machine, the timings.
* [Failsafe module](../src/core/system/failsafe/failsafe_module.md): the two chains and their contributors.
* [Remote](../src/remotes/README.md): the firmware of the remote.
