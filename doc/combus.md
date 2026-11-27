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
* by the **links**: between the boards and sub-boards of a machine, and between a remote, a machine and its trailer, the channels they share travel.

---

## Declared, Not Programmed

You do not program a channel: you **declare** it. A few lines in a small text file, a `.cb` file, give its name, its type, how far it goes and its safe value. No C++ to write.

The file lives **next to what it describes**: a machine type, a module, a coupler. At build time, the project is scanned and every `.cb` it finds is taken in, as long as its module is part of the build. Nothing to register, no central list to edit.

The same approach is being extended to the rest of the configuration: the processor chains, then the configuration of the machines and their boards.

What it brings:

* **Adding a module is simple.** A new function (lights, a winch, a sensor) comes with its own folder: its code, its `.cb`, and later its chains. Drop it into the project, and its channels appear where they are needed. A user can write an extra module without touching the core.
* **A machine is assembled, not rewritten.** Enable a module, and its channels come with it; leave it out, and they disappear.
* **Mistakes show up early.** A name used twice, a channel with no safe value, two boards claiming the same channel: the build refuses it and says why. And before two firmwares exchange anything, they check that they share the same channels.

---

## Learn More

* [ComBus internal](../src/core/system/combus/README.md): the register in detail, its layers, its access rules and what a channel declares.
* [Nodes](nodes.md): what a node is made of, and what its boards share.
* [Linking Nodes](linking_nodes.md): how nodes are linked, and what keeps those links safe.
* [`.cb` syntax](configuration/cb_syntax.md): declaring channels.
* [ComBus Builder](../scripts/combus_builder/README.md): how the channels become code.
* [ComBus transmission](../src/core/system/combus/protocol/README.md): how the ComBus travels between boards and nodes.
* [Processors](../src/core/system/combus/processors/~README.md): the processing blocks and their chains.
* [Failsafe module](../src/core/system/failsafe/failsafe_module.md): the safety chains.
