# The Remote

The remote is a node like any other: it has its own ComBus and a link to the machine it drives. This folder holds its firmware (env `remotes`).

## What It Sends

A machine type (`-D MACHINE_TYPE_<TYPE>`, for example `dumper_truck`) has an **input contract**: its inputs (`*_STICK`, `*_BTN`), which exist on the board of the machine that acquires the commands. The remote has their mirror, built by the ComBus Builder: what the machine receives, the remote sends. It therefore only sends inputs (sticks, buttons), and the machine alone decides what to do with them.

On the machine, the remote is a command source like any other, such as a controller plugged into the machine: its link writes no bus, and its module applies what the link received when the remote is the source that drives. See the [command sources](../core/system/input/~README.md).

The link has its own table and its own MD5 (`combus::peer::kRemoteLink`), the same whether the machine has couplers or not. As for any link with a table, the MD5 also carries the side of each end: the machine is the socket, the remote the plug. There is no pairing: it is an ordinary link between nodes.

## How the Link Opens

```text
setup()  remote_links_init()     one link per driven machine (driven_machines.h)
loop()   remote_links_update()   reception, supervision, then sending
```

The table of the driven machines, `config/driven_machines.h`, gives for each one its name, its ComBus and its link (port, direction, protocol).

## Current Limits

- **A single driven machine.** Driving several needs one ComBus per machine, so one per link: `combus_links_init()` and `combus_tx_update()` only take one today.
- **No physical input**: the channels keep their safe value.
- **The port is hard-coded** (UART2, GPIO 17/16), until the remote has a board configuration.

These three points are marked `TODO(chantier 4)` in the code. The rest of the work is in [WIP - Remote firmware](<../../doc/WIP - Remote firmware.md>).

## Going Further

- [Linking nodes](../../doc/linking_nodes.md): the remote among the other nodes
- [The link manager](../core/system/combus/protocol/link/README.md): describing, setting up and supervising a link
- [ComBus transmission](../core/system/combus/protocol/README.md): the overview
- [ComBus Builder](../../scripts/combus_builder/README.md): the mirror view of the remote
