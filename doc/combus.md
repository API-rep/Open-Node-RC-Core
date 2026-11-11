# ComBus

## From RC Channels to the ComBus

Anyone coming from classic RC knows channels. A radio sends six, eight, maybe twelve of them. Each channel carries one value, a stick position or a switch, and its meaning is decided once and for all: channel 1 steers, channel 2 throttles, channel 5 raises the bucket. The receiver does not know what a channel means; it only drives the servo plugged into it. Want a new function? Find a free channel, if one is left, and wire it.

The **ComBus** keeps the idea of channels, and removes the limits around it.

* **Channels have names, not numbers.** `STEERING_BUS`, `BRAKE_BUS`, `KEY_BTN`: a channel says what it is.
* **Two families instead of one.** **Analog** channels carry continuous values (0 to 65535): a stick, a speed, a battery voltage. **Digital** channels carry "on" or "off" (true or false): a button, a light, a fault.
* **As many channels as the machine needs.** A dumper truck and an excavator do not need the same channels; each machine type declares its own set.
* **The meaning travels with the channel.** Its direction, its safe value, who may write it: everything is declared with the channel, not left to the wiring.

A classic radio is a fixed pipe of anonymous values. The ComBus is a configurable register of named values.

---

## A Shared Register

Inside a machine firmware, the ComBus is not only a way to talk to a remote: it is **the shared state of the whole system**.

Each module reads what it needs and writes what it produces. The input module writes the stick positions, the motion module reads them and writes the motor commands, the lights read the run level, the dashboard reads everything. No module talks to another directly: they all meet on the ComBus, which becomes the single source of truth of the machine.

This is where the ComBus is used the most:

* by the **modules**: inputs, motors, lights, battery, debug dashboard;
* by the **processor chains**: small processing blocks that read channels, transform them and write others (ramps, inertia, gearbox, cruise control). Chained together, they build the behaviour of a machine without changing the firmware;
* by the **safety**: the failsafe faults and the run level are channels like the others, so every board of a machine sees them;
* by the **links**: between the boards and sub-boards of a machine, and between a remote, a machine and its trailer, the same register is synchronised.

---

## Declared, Not Programmed

Channels are not written in C++. Each one is declared in a small configuration file, a `.cb` file, next to what it describes: the profile of a machine type, a core module, a coupler.

At build time, the **ComBus Builder** reads these files and generates everything the firmware needs: the identifiers, the arrays, and a fingerprint of each set of channels. Consistency and compatibility are checked twice: at build time by the builder, and at runtime before two firmwares exchange anything.

---

## Three Layers

Not every value has to travel everywhere. Each channel belongs to a layer that sets how far it goes:

* **BASE**: the generic channel set of a machine type (truck, excavator, tractor...), used as the "norm" for node communication (internal and external);
* **SYSTEM**: the specific channels of one node, owned by its system, defined for a custom machine configuration;
* **BOARD**: the channels private to one board or sub-board firmware, for its internal use.

A remote only needs to know the `BASE` channels of the machine it drives; the boards of a truck share their `SYSTEM` channels.

---

## Learn More

* [ComBus internal](../src/core/system/combus/README.md): the register in detail, its layers, its access rules and what a channel declares.
* [Nodes](nodes.md): what a node is made of, and what its boards share.
* [Linking Nodes](linking_nodes.md): how nodes are linked, and what keeps those links safe.
* [`.cb` syntax](configuration/cb_syntax.md): declaring channels.
* [ComBus Builder](../scripts/combus_builder/README.md): how the channels become code.
* [ComBus protocol](../src/core/system/combus/protocol/README.md): how the ComBus travels between boards and nodes.
* [Processors](../src/core/system/combus/processors/~README.md): the processing blocks and their chains.
* [Failsafe module](../src/core/system/failsafe/failsafe_module.md): the safety chains.
