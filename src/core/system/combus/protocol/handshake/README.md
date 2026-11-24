# The Handshake

Two firmwares flashed separately may not have the same ComBus. If one believes that position 3 is the brake and the other the steering, the machine does anything. The handshake detects it before the slightest movement: **as long as both ends do not agree, no data is applied.**

It is a consistency check, not a safety measure.

## The Contract

The contract of a link is the MD5 of the set of channels it carries, computed by the builder.

| Link | MD5 compared |
|---|---|
| `BASE` scope | the one of the `BASE` view |
| `SYSTEM` scope | the one of the `SYSTEM` view |
| with a table (`ComPeerLink`) | the one of the link, specific to each side |

On a link with a table, each end announces the MD5 of its own side and expects the one of the other side. Two sockets, or two plugs, linked by mistake therefore refuse each other.

The major version of the project is also part of the contract. The minor and development versions are only logged.

## The Frame

26 bytes long, it uses the reserved slot `seq == 0`:

| Byte | Size | Field |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` |
| 1, 2 | 2 | `nAnalog` = 0, `nDigital` = 0 |
| 3 | 1 | `seq` = 0 |
| 4 | 1 | `flags`: bit 0 = ACK, else request; bit 1 = NEXT, else legacy (without the "ComBus-next" channels) |
| 5 to 20 | 16 | announced MD5 |
| 21, 22, 23 | 3 | project version: major, minor, development |
| 24, 25 | 2 | CRC-16/CCITT-FALSE over the previous bytes, as in the [data frame](../frame/README.md) |

## The Sequence

A link always starts `UNKNOWN`. Each end sends a **burst** of 3 requests 100 ms apart, then one request every 250 ms, whatever the state of the contract.

```text
End A                                  End B
  │ ── request (MD5 of A) ───────────▶ │  arms an ACK, agreeing or not
  │ ◀──────────────── ACK (MD5 of B) ── │
  │  compares: A becomes STABLE        │
  │                                    │
  │ ◀──────────── request (MD5 of B) ── │
  │ ── ACK (MD5 of A) ───────────────▶ │
  │                                    │  compares: B becomes STABLE
```

To become `STABLE`, each end needs a proof:

- **A two-way end** waits for the ACK of its own request: it then knows that the other end receives it.
- **A receive-only end** is satisfied with the request of the sender.

A send-only end waits for nothing: its periodic requests let a receiver started later establish itself.

A request is always answered with an ACK; an ACK is never answered.

## The States

| | `UNKNOWN` | `STABLE` |
|---|---|---|
| Data frames received | ignored | applied |
| The link counts as alive | no | yes, if it receives |
| A two-way end sends data | no | yes |

A send-only end always sends its data.

The contract falls back to `UNKNOWN` on:

- a refused handshake: MD5 and major version are compared every time, not only at the start;
- a silence longer than the timeout of the link, after data;
- on a two-way end, no ACK for 1 s, that is 4 requests: the other end no longer receives us;
- a reset of the transport.

The link is then no longer alive, and its loss is published according to its scope (`NODE_LINK_FAIL` or `BOARD_LINK_FAIL`).

A failsafe never touches the contract: blocking the frames would block the failsafe of the other boards.

## Candidate Channels: Stable, Next, Legacy

A candidate channel is a channel added on trial, after the stable channels. It lets two firmwares of neighbouring versions talk to each other. The *stable* MD5 does not count it, the *next* MD5 does. These modes only concern `BASE` links: a `SYSTEM` link carries its candidates in its single set.

| Mode | When | Channels sent |
|---|---|---|
| **next** | both ends have the same candidates | all of them |
| **stable** | the candidates differ | the stable ones only; the candidates keep their default value |
| **legacy** | this end has no candidate | all of them: there are only stable ones |

How they agree:

- An end with candidates starts with NEXT requests: the burst, then 3 retries. It then switches to legacy requests (with the stable MD5), or as soon as a NEXT ACK does not match.
- A NEXT handshake decides **the mode, never the contract**: it cannot drop a link.
- The mode follows what the other end announces, without cutting the link. A loss brings it back to stable mode and re-arms the NEXT requests.

The candidates always follow the stable channels on the wire: a next frame read in stable mode stays right on its stable part.

## Build Switches

Reserved for debugging, they are 0 by default:

| Macro | Effect |
|---|---|
| `COMBUS_MD5_CHECK_DISABLE` | Accepts any handshake. A very visible banner reports it at startup. |
| `COMBUS_HANDSHAKE_BURST_DISABLE` | Removes the burst; the periodic requests remain. |

## Files

| File | Content |
|---|---|
| `combus_handshake.h`, `.cpp` | Constants, per-link context, shared helpers |
| `combus_handshake_rx.h`, `.cpp` | Decode a frame, compare, update the contract |
| `combus_handshake_tx.h`, `.cpp` | The burst, the periodic requests and the ACKs |

## Going Further

- [ComBus transmission](../README.md): the overview
- [Data frame](../frame/README.md): what the contract protects
- [Coupling and pairing](../coupling/README.md): the other frame of the `seq == 0` slot
- [Linking nodes](../../../../../../doc/linking_nodes.md): why both ends must agree
