#!/usr/bin/env python3
"""
combus_builder/links.py — links between nodes (link rework, step 3.1a).

A node declares the machine TYPES it accepts as peers (`-D COMBUS_PEER_<TYPE>`).
For each peer type, the builder reads the BASE channels of that type apart
(the "second reading": they never enter the local ComBus) and computes the
channel set of the link:

  - the BASE channels (next: candidates included) of the same name in both
    types, keeping only those that flow: one side emits, the other receives;
  - a side emits on the link when its direction is `uplink`, `both`, or
    `downlink_pt` (it relays what it received from another link);
  - two receivers: "dead channel" warning, not in the set; two emitters:
    error, unless `*_or` (digital only); two relays facing each other
    (`downlink_pt` on both sides): error until the graph check of 3.2;
  - same name = same type and same default value, else error;
  - the common frame is sorted on what both types share: analog then
    digital, stable before candidate, then the name (never the theme);
  - the MD5 of the link is symmetric: both nodes, built apart, get the same.

Each node then has a table from its ComBus to the common frame
(combus_peer_links.h, emitted by generator.generate()).
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass, field
from pathlib import Path

from .canon import canonize
from .canon.channels import ChannelDefinition
from .parser import parse_yaml_file

PEER_FLAG_PREFIX = "COMBUS_PEER_"

_UP, _DOWN = "uplink", "downlink"


class LinkError(Exception):
    """A link between two node types cannot be built (contract conflict)."""


@dataclass(frozen=True)
class LinkChannel:
    """One channel of the common frame of a link."""
    id: str
    type: str                 # "analog" | "digital"
    candidate: bool
    value: str                # rendered default value (C++ literal), the same on both sides
    flow: str                 # "to_peer" | "from_peer" | "both" | "or"
    local_token: str          # surface direction of the local type (for the MD5)
    peer_token: str           # surface direction of the peer type


@dataclass
class LinkSet:
    """The channel set of the link local type <-> peer type, in wire order."""
    local_type: str
    peer_type: str
    channels: list[LinkChannel] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)


# =============================================================================
# Peers and second reading
# =============================================================================

def peer_types(ctx) -> list[str]:
    """Machine types declared as peers by `-D COMBUS_PEER_<TYPE>`, as directory names, sorted."""
    names = set(ctx.defines) | set(getattr(ctx, "defines_with_value", {}) or {})
    return sorted(n[len(PEER_FLAG_PREFIX):].lower() for n in names if n.startswith(PEER_FLAG_PREFIX))


def load_type_base_channels(buildroot: Path, type_dir: str) -> list[ChannelDefinition]:
    """Read the BASE channels of a machine type apart (never merged into the local ComBus)."""
    folder = Path(buildroot) / "core" / "config" / "machines" / type_dir
    if not folder.is_dir():
        raise LinkError(f"peer type {type_dir!r}: no profile folder {folder}")
    parsed = [parse_yaml_file(p) for p in sorted(folder.rglob("*.cb"))]
    return [ch for ch in canonize(parsed).canonical_definitions if ch.scope == "BASE"]


# =============================================================================
# Link set
# =============================================================================

def _token(ch: ChannelDefinition) -> str:
    """Surface direction token of a canonical channel (downlink_pt, both_or, ...)."""
    d = ch.direction | ch.direction_or
    base = {frozenset({_UP}): "uplink", frozenset({_DOWN}): "downlink",
            frozenset({_UP, _DOWN}): "both"}.get(frozenset(d), "none")
    if ch.direction_or:
        return base + "_or"
    return base + ("_pt" if ch.passthrough else "")


def _emits(ch: ChannelDefinition) -> bool:
    return _UP in ch.direction or (ch.passthrough and _DOWN in ch.direction)


def _receives(ch: ChannelDefinition) -> bool:
    return _DOWN in ch.direction


def _flow(a: ChannelDefinition, b: ChannelDefinition, warnings: list[str]) -> str | None:
    """Flow of one channel on the link a <-> b, None if it does not flow (warning), LinkError if refused."""
    cid = a.id
    if a.direction_or or b.direction_or:
        ta, tb = _token(a), _token(b)
        if (ta, tb) in {("both_or", "both_or"), ("uplink_or", "downlink_or"), ("downlink_or", "uplink_or")}:
            return "or" if ta == "both_or" else ("to_peer" if ta == "uplink_or" else "from_peer")
        raise LinkError(f"channel {cid!r}: OR directions do not match ({ta} / {tb})")

    a_to_b = _emits(a) and _receives(b)
    b_to_a = _emits(b) and _receives(a)
    if a_to_b and b_to_a:
        if a.direction == b.direction == frozenset({_UP, _DOWN}) and not (a.passthrough or b.passthrough):
            return "both"
        raise LinkError(f"channel {cid!r}: both sides emit and receive ({_token(a)} / {_token(b)}); "
                        f"two relays facing each other are refused until the graph check (3.2)")
    if a_to_b:
        return "to_peer"
    if b_to_a:
        return "from_peer"
    if _emits(a) and _emits(b):
        raise LinkError(f"channel {cid!r}: two emitters on the link ({_token(a)} / {_token(b)})")
    warnings.append(f"channel {cid!r}: nobody emits it to a receiver on this link "
                    f"({_token(a)} / {_token(b)}): dead channel, left out")
    return None


def compute_link_set(local_type: str, local_chs, peer_type: str, peer_chs) -> LinkSet:
    """Channel set of the link between two node types, in wire order."""
    from .generator import _render_value

    local = {ch.id: ch for ch in local_chs if ch.scope == "BASE"}
    peer = {ch.id: ch for ch in peer_chs if ch.scope == "BASE"}
    link = LinkSet(local_type=local_type, peer_type=peer_type)

    for cid in sorted(set(local) & set(peer)):
        a, b = local[cid], peer[cid]
        if a.type != b.type:
            raise LinkError(f"channel {cid!r}: {a.type} in {local_type}, {b.type} in {peer_type}")
        va, vb = _render_value(a, a.type), _render_value(b, b.type)
        if va != vb:
            raise LinkError(f"channel {cid!r}: default value {va} in {local_type}, {vb} in {peer_type}")
        flow = _flow(a, b, link.warnings)
        if flow is None:
            continue
        link.channels.append(LinkChannel(id=cid, type=a.type, candidate=a.candidate or b.candidate,
                                         value=va, flow=flow,
                                         local_token=_token(a), peer_token=_token(b)))

    link.channels.sort(key=lambda c: (0 if c.type == "analog" else 1, c.candidate, c.id))
    return link


def link_md5(link: LinkSet, stable: bool = False) -> str:
    """MD5 of the link, the same whichever side computes it (stable: without the candidates)."""
    types = sorted([link.local_type, link.peer_type])
    entries = []
    for c in link.channels:
        if stable and c.candidate:
            continue
        dirs = {link.local_type: c.local_token, link.peer_type: c.peer_token}
        entries.append({"id": c.id, "type": c.type, "candidate": c.candidate, "value": c.value,
                        "dirs": [dirs[t] for t in types]})
    payload = {"link": types, "channels": entries}
    return hashlib.md5(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()


# =============================================================================
# C++ rendering
# =============================================================================

def _cpp_ident(type_dir: str) -> str:
    return "".join(part.capitalize() for part in type_dir.split("_"))


def render_peer_links_header(links: list[LinkSet], ids_header: str = "combus_ids.h") -> str:
    """combus_peer_links.h: per peer type, the MD5 and the table ComBus -> common frame."""
    body = [
        "/******************************************************************************",
        " * GENERATED FILE - DO NOT EDIT.",
        " *",
        " * Generated by scripts/combus_builder/links.py (link rework, step 3.1).",
        " * Links between nodes: for each peer type declared by -D COMBUS_PEER_<TYPE>,",
        " * the common frame of the link (computed channel set, wire order) and the",
        " * table from this node's ComBus to that frame.",
        " ******************************************************************************/",
        "",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        f'#include "{ids_header}"',
        "",
        "namespace combus {",
        "namespace peer {",
        "",
    ]
    if not links:
        body.append("// No peer type declared for this node.")
        body.append("")
    for link in links:
        ns = _cpp_ident(link.peer_type)
        analog = [c for c in link.channels if c.type == "analog"]
        digital = [c for c in link.channels if c.type == "digital"]
        md5 = bytes.fromhex(link_md5(link))
        md5_stable = bytes.fromhex(link_md5(link, stable=True))
        body.append(f"/// Link with a {link.peer_type} node.")
        body.append(f"namespace {ns} {{")
        body.append(f"static constexpr uint8_t kMd5[16]       = {{ {', '.join(f'0x{x:02X}u' for x in md5)} }};")
        body.append(f"static constexpr uint8_t kMd5Stable[16] = {{ {', '.join(f'0x{x:02X}u' for x in md5_stable)} }};")
        body.append(f"static constexpr uint8_t kAnalogCount        = {len(analog)}u;")
        body.append(f"static constexpr uint8_t kDigitalCount       = {len(digital)}u;")
        body.append(f"static constexpr uint8_t kStableAnalogCount  = {sum(1 for c in analog if not c.candidate)}u;")
        body.append(f"static constexpr uint8_t kStableDigitalCount = {sum(1 for c in digital if not c.candidate)}u;")
        body.append("/// Wire position -> local channel ID.")
        body.append(f"static constexpr AnalogComBusID kAnalog[] = {{ "
                    f"{', '.join(f'AnalogComBusID::{c.id}' for c in analog) or 'AnalogComBusID::CH_COUNT'} }};")
        body.append(f"static constexpr DigitalComBusID kDigital[] = {{ "
                    f"{', '.join(f'DigitalComBusID::{c.id}' for c in digital) or 'DigitalComBusID::CH_COUNT'} }};")
        body.append("/// Wire position -> this node receives it on this link (true) or sends it (false).")
        body.append(f"static constexpr bool kAnalogRx[] = {{ "
                    f"{', '.join('true' if c.flow in ('from_peer', 'both', 'or') else 'false' for c in analog) or 'false'} }};")
        body.append(f"static constexpr bool kDigitalRx[] = {{ "
                    f"{', '.join('true' if c.flow in ('from_peer', 'both', 'or') else 'false' for c in digital) or 'false'} }};")
        body.append(f"}} // namespace {ns}")
        body.append("")
    body += ["} // namespace peer", "} // namespace combus", "", "// EOF"]
    return "\n".join(body) + "\n"
