# The Data Frame

This folder turns the state of the ComBus into bytes, and back. The codec is pure: it knows nothing of the state of the link, of its loss, or of any particular channel. The run level and the failsafe travel in it as ordinary channels.

## The Format

| Byte | Size | Field |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` |
| 1 | 1 | `nAnalog`: number of analog channels |
| 2 | 1 | `nDigital`: number of digital channels |
| 3 | 1 | `seq`: frame counter |
| 4 | 1 | `flags`: 0 on a data frame |
| 5 | `ceil(nDigital / 8)` | digital channels, 1 bit per channel, least significant bit first |
| 5 + d | `2 × nAnalog` | analog channels, little-endian `uint16` |
| end | 2 | CRC-16/CCITT-FALSE over all the previous bytes, most significant byte first |

- The digital channels come **before** the analog ones.
- The CRC-16 (polynomial `0x1021`, initial value `0xFFFF`) is computed with a 512-byte table in flash, built at compile time. It catches every error of 1 or 2 bits, where the CRC-8 it replaced let through about 1 pair of inverted bits in 128. On the bench, 0 wrong value reached the bus out of about 17 000 corrupted frames.
- A frame is 255 bytes at most, because its length fits in one byte.
- `seq` goes from 1 to 255, then back to 1. The value 0 is reserved for the [handshake](../handshake/README.md) and the [coupling](../coupling/README.md): the encoder refuses it.

## Telling Frames Apart

All frames start with the same header. Two bytes are enough to tell them apart:

| `seq` | `flags` | Frame | Handled by |
|---|---|---|---|
| 1 to 255 | 0 | data | `frame/` |
| 0 | coupling bit set | coupling | [`coupling/`](../coupling/README.md) |
| 0 | other | handshake | [`handshake/`](../handshake/README.md) |

## Which Channels Travel

A position on the wire must designate the same channel on both sides. There are two ways to guarantee it:

- **Without a table**: the position is the identifier of the channel. The frame carries the start of the view of the scope, because the builder orders the identifiers so that `BASE` is the prefix of `SYSTEM`.
- **With a table** (`ComPeerLink`, a link between nodes): the table gives, for each position, the local channel, and says whether this node receives or sends it.

In both cases, the [handshake](../handshake/README.md) proves that both ends agree: they compare the MD5 of this set of channels.

## The Path of a Frame

```text
Sending     bus → combus_frame_encode() → bytes → NodeCom

Receiving   NodeCom → bytes → combus_frame_decode() → snapshot of the link
                                                          │
                                 combus_frame_apply()  ←──┘
                                          │
                                          ▼
                                         bus
```

**`combus_frame_decode()`** validates first, and only then writes: `SOF`, announced sizes, capacity of the link, CRC. A refused frame leaves the output untouched. The result stays indexed by position on the wire.

**`combus_frame_apply()`** writes to the bus only the channels this node receives:

- With a table, the table says so.
- Without a table, the direction of the channel on this kind of link says so (`ChanFlow`).

A channel this node produces is therefore never overwritten by its peer. The writes go through `combus_link_set_*()`, with the layer of the link, which refuses a channel outside the scope of the link: a frame never writes a `BOARD` channel.

**The `both_or` case**: these channels are never written by a frame. The received value stays in the snapshot of the link, and `com_rx_update()` merges it with the value of the board on every cycle. See [the `both_or` merge](../both_or.md).

## Files

| File | Content |
|---|---|
| `combus_frame.h`, `.cpp` | Encoding, decoding, applying to the bus, CRC |
| `combus_frame_defs.h` | The structures: layout, header, decoded frame |

## Going Further

- [ComBus transmission](../README.md): the overview
- [Handshake](../handshake/README.md): the contract of the link
- [The builder](../../../../../../scripts/combus_builder/README.md): how the identifiers are assigned
