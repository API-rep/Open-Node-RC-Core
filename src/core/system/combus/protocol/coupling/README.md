# Coupling and Pairing

A link through a coupler only exists when two elements are hitched (`LinkActivation::ON_COUPLING`). When several machines are in range, the question is **who is hitched to whom**. This folder answers it before opening the link.

This document describes the mechanics. For the principle (coupler, socket, plug, what happens when hitching), start with [Linking nodes](../../../../../../doc/linking_nodes.md).

Vocabulary: the **socket** (`SOCKET`) is the side that carries, the **plug** (`PLUG`) the side that is carried.

## The Idea

The trigger of the coupler, for example a hitch sensor, rises on both sides at the same moment. Each end therefore measures the **age of its hitching**, and two machines really hitched together announce almost the same one. Pairing relies on this agreement: the socket asks several times, and only the answers whose age matches count.

## The Coupling Frame

It is separate from the handshake: it does not make a link alive and does not touch the contract. 20 bytes long, it uses the `seq == 0` slot, marked by the coupling bit of `flags`:

| Byte | Size | Field |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` |
| 1, 2 | 2 | `nAnalog` = 0, `nDigital` = 0 |
| 3 | 1 | `seq` = 0 |
| 4 | 1 | `flags`: bit 2 = coupling |
| 5 | 1 | `msg`: 1 = request, 2 = answer |
| 6 | 1 | `round`: round number |
| 7 | 1 | `addr.type`: 0 = none, 1 = MAC48 |
| 8 to 13 | 6 | `addr.bytes`: the address (MAC of the ESP32) |
| 14, 15 | 2 | `hitchAgeMs`: age of the hitching in ms, saturated at 65 535, little-endian |
| 16, 17 | 2 | `features`: function mask, reserved, 0 for now |
| 18, 19 | 2 | CRC-16/CCITT-FALSE over the previous bytes, as in the [data frame](../frame/README.md) |

A firmware that does not know this frame takes it for a handshake of the wrong length: the CRC fails, it drops one byte and resynchronises, without touching the contract.

## Pairing

```text
         trigger true for engageMs
   DETACHED ───────────────────▶ PAIRING
       ▲                           │
       │                           ├── one candidate, enough rounds ──────▶ COUPLED
       │                           ├── two candidates ────────────────────▶ CONFLICT
       │                           └── not enough answers (socket) ───────▶ FAILED
       │
       └── trigger false for releaseMs, from any state
```

| State | The link |
|---|---|
| `DETACHED` | is off |
| `PAIRING` | only decodes coupling frames, any other byte is dropped |
| `COUPLED` | works normally |
| `CONFLICT` | stays off until the trigger falls and rises again |
| `FAILED` | stays off; the socket retries as long as the trigger stays true |

**Socket side**: it sends 5 requests, 75 to 125 ms apart, then listens for a last period of 100 ms. An answer counts if its age matches, within `ageToleranceMs`.

- No identity, or fewer than 4 rounds for the only one heard: `FAILED`, retried 3 times every 2 s.
- Two identities: `CONFLICT`, never retried.
- A single identity over at least 4 rounds: `COUPLED`.

**Plug side**: it listens and answers every request whose age matches, after a delay drawn between 0 and 10 ms so that two plugs do not answer over each other.

- A single socket over at least 4 requests: `COUPLED`.
- Two sockets: `CONFLICT`, and it no longer answers.
- Fewer requests: it starts listening again.

A trigger that falls for less than `releaseMs` is only logged: a loose contact does not unhitch.

## Once Coupled

The address of the peer is kept and handed to the transport (`NodeCom::setPeer`). Coupling frames from another identity are counted. On a wire, this address has no effect; it will filter the senders with ESP-NOW.

The link then starts like the others: [handshake](../handshake/README.md), then data.

## Loss

A coupled link has its own failsafe contributor, named by its `CouplingDev` (`fail`, for example `TRAILER_LINK_FAIL`):

- healthy while unhitched, and during pairing;
- at fault when hitched without a healthy link: pairing in `CONFLICT` or `FAILED`, or a paired link lost after the grace counted from pairing.

A coupled link never counts in `NODE_LINK_FAIL`: the machine puts its contributor in the chain of its choice.

## The Passive Coupler

A coupler without a node on the other side, such as a tool, has nobody to talk to. Its trigger is its presence detector: `COUPLED` once the detector has settled, without pairing (`combus_pairing_update_passive()`).

## Files

| File | Content |
|---|---|
| `combus_coupling.h`, `.cpp` | The coupling frame: encode, decode, hand over to pairing |
| `combus_pairing.h`, `.cpp` | The state machine of each end |
| `combus_coupling_watch.h`, `.cpp` | Applies the pairing state to the link, writes the contributors of the coupler, follows the passive couplers |

The frame does not depend on pairing: pairing registers with it to receive the frames.

## Going Further

- [ComBus transmission](../README.md): the overview
- [Links](../link/README.md): how a link is described and watched
- [Handshake](../handshake/README.md): what follows pairing
- [Failsafe module](../../../failsafe/failsafe_module.md): where the contributor of a coupler goes
