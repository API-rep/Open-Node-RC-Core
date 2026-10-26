#!/usr/bin/env python3
"""
combus_builder/links.py — links between nodes (link rework, step 3.1a).

A node declares the machine TYPES it accepts as peers (`-D COMBUS_PEER_<TYPE>`).
For each peer type, the builder reads the BASE channels of that type apart
(the "second reading": they never enter the local ComBus) and computes the
channel set of the link:

  - the BASE channels (next: candidates included) of the same name in both
    types, keeping only those that flow: one side emits, the other receives;
  - star "vehicle + 1" (step 3.2b): on the attachment, `downlink_pt` only
    redistributes in its node, it receives on the link; on the vehicle,
    `downlink_pt` receives where the attachment emits (that link is its
    source) and emits where the attachment receives;
  - two receivers: "dead channel" warning, not in the set; two emitters:
    error, unless `*_or` (digital only); one source per channel on the node
    (check_single_source, both_or excepted);
  - same name = same type and same default value, else error;
  - the common frame is sorted on what both types share: analog then
    digital, stable before candidate, then the name (never the theme);
  - the MD5 of the link is symmetric: both nodes, built apart, get the same;
    it names the vehicle of the link, so two vehicles (or two attachments)
    linked by mistake disagree and the handshake refuses.

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

# Nature of a node (step 3.2b): exactly one structural flag per env.  The
# vehicle is the hub of the star; an attachment has one link, to a vehicle; a
# stand-alone machine has no peer; the remote is unchanged (chantier 4).
NODE_NATURES = ("IS_VEHICLE", "IS_ATTACHMENT", "IS_MACHINE", "IS_REMOTE")

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
    vehicle_type: str = ""    # step 3.2b: the type this side takes for the vehicle of the link (in the MD5)
    channels: list[LinkChannel] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)


# =============================================================================
# Peers and second reading
# =============================================================================

def peer_types(ctx) -> list[str]:
    """Machine types declared as peers by `-D COMBUS_PEER_<TYPE>`, as directory names, sorted."""
    names = set(ctx.defines) | set(getattr(ctx, "defines_with_value", {}) or {})
    return sorted(n[len(PEER_FLAG_PREFIX):].lower() for n in names if n.startswith(PEER_FLAG_PREFIX))


def node_nature(defines) -> str:
    """Structural flag of the node, checked against its peers; LinkError if the env is wrong."""
    found = sorted(n for n in NODE_NATURES if n in defines)
    if not found:
        raise LinkError(f"no structural flag set (expected exactly one of {', '.join(NODE_NATURES)})")
    if len(found) > 1:
        raise LinkError(f"several structural flags set ({', '.join(found)}): a node has exactly one nature")
    nature = found[0]
    peers = sorted(d for d in defines if d.startswith(PEER_FLAG_PREFIX))
    if nature in ("IS_MACHINE", "IS_REMOTE") and peers:
        raise LinkError(f"{nature} has no peer ({', '.join(peers)}): only a vehicle or an attachment is linked")
    if nature == "IS_ATTACHMENT" and len(peers) != 1:
        raise LinkError(f"IS_ATTACHMENT needs exactly one peer, its vehicle (got {len(peers)})")
    return nature


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


def _flow(a: ChannelDefinition, b: ChannelDefinition, local_is_vehicle: bool,
          warnings: list[str]) -> str | None:
    """Flow of one channel on the link a (local) <-> b (peer), None if it does not flow (warning).

    Step 3.2b, star "vehicle + 1": on the attachment, `_pt` only redistributes
    in its node (a plain receiver on the link); on the vehicle, `_pt` receives
    where the attachment emits (that link is its source) and emits where the
    attachment receives.  LinkError if refused.
    """
    cid = a.id
    if a.direction_or or b.direction_or:
        ta, tb = _token(a), _token(b)
        if (ta, tb) in {("both_or", "both_or"), ("uplink_or", "downlink_or"), ("downlink_or", "uplink_or")}:
            return "or" if ta == "both_or" else ("to_peer" if ta == "uplink_or" else "from_peer")
        raise LinkError(f"channel {cid!r}: OR directions do not match ({ta} / {tb})")

    v, t = (a, b) if local_is_vehicle else (b, a)
    v_up, v_down = _UP in v.direction, _DOWN in v.direction
    t_up, t_down = _UP in t.direction, _DOWN in t.direction

    if v_up and t_up:
        if v.direction == t.direction == frozenset({_UP, _DOWN}) and not (v.passthrough or t.passthrough):
            return "both"
        raise LinkError(f"channel {cid!r}: two emitters on the link ({_token(a)} / {_token(b)})")
    if t_up and v_down:
        to_vehicle = True                       # this link is the source of the vehicle
    elif t_down and (v_up or (v.passthrough and v_down)):
        to_vehicle = False                      # the vehicle emits (its own value, or relayed)
    else:
        warnings.append(f"channel {cid!r}: nobody emits it to a receiver on this link "
                        f"({_token(a)} / {_token(b)}): dead channel, left out")
        return None
    return "from_peer" if to_vehicle == local_is_vehicle else "to_peer"


def compute_link_set(local_type: str, local_chs, peer_type: str, peer_chs, local_nature: str) -> LinkSet:
    """Channel set of the link between two node types, in wire order.

    local_nature: IS_VEHICLE or IS_ATTACHMENT, the structural flag of the node
    built (step 3.2b); the peer is taken for the other nature.
    """
    from .generator import _render_value

    if local_nature not in ("IS_VEHICLE", "IS_ATTACHMENT"):
        raise LinkError(f"a {local_nature} node has no link with a peer type")
    local_is_vehicle = local_nature == "IS_VEHICLE"
    local = {ch.id: ch for ch in local_chs if ch.scope == "BASE"}
    peer = {ch.id: ch for ch in peer_chs if ch.scope == "BASE"}
    link = LinkSet(local_type=local_type, peer_type=peer_type,
                   vehicle_type=local_type if local_is_vehicle else peer_type)

    for cid in sorted(set(local) & set(peer)):
        a, b = local[cid], peer[cid]
        if a.type != b.type:
            raise LinkError(f"channel {cid!r}: {a.type} in {local_type}, {b.type} in {peer_type}")
        va, vb = _render_value(a, a.type), _render_value(b, b.type)
        if va != vb:
            raise LinkError(f"channel {cid!r}: default value {va} in {local_type}, {vb} in {peer_type}")
        flow = _flow(a, b, local_is_vehicle, link.warnings)
        if flow is None:
            continue
        link.channels.append(LinkChannel(id=cid, type=a.type, candidate=a.candidate or b.candidate,
                                         value=va, flow=flow,
                                         local_token=_token(a), peer_token=_token(b)))

    link.channels.sort(key=lambda c: (0 if c.type == "analog" else 1, c.candidate, c.id))
    return link


def check_single_source(links: list[LinkSet]) -> None:
    """One source per channel on the node: received on one of its links at most (both_or excepted)."""
    sources: dict[str, list[str]] = {}
    for link in links:
        for c in link.channels:
            if c.flow in ("from_peer", "both"):
                sources.setdefault(c.id, []).append(link.peer_type)
    for cid, peers in sorted(sources.items()):
        if len(peers) > 1:
            raise LinkError(f"channel {cid!r}: received from several links ({', '.join(peers)}): "
                            f"one source per channel")


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
    payload = {"link": types, "vehicle": link.vehicle_type, "channels": entries}
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
        "#include <core/system/combus/protocol/frame/combus_frame_defs.h>   // ComPeerLink",
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
        body.append(f"static constexpr uint8_t kAnalog[] = {{ "
                    f"{', '.join(f'static_cast<uint8_t>(AnalogComBusID::{c.id})' for c in analog) or '0u'} }};")
        body.append(f"static constexpr uint8_t kDigital[] = {{ "
                    f"{', '.join(f'static_cast<uint8_t>(DigitalComBusID::{c.id})' for c in digital) or '0u'} }};")
        body.append("/// Wire position -> this node receives it on this link (true) or sends it (false).")
        body.append(f"static constexpr bool kAnalogRx[] = {{ "
                    f"{', '.join('true' if c.flow in ('from_peer', 'both', 'or') else 'false' for c in analog) or 'false'} }};")
        body.append(f"static constexpr bool kDigitalRx[] = {{ "
                    f"{', '.join('true' if c.flow in ('from_peer', 'both', 'or') else 'false' for c in digital) or 'false'} }};")
        body.append("/// The link, for the ComDev of this peer (ComDev::peer).")
        body.append("static constexpr ComPeerLink kLink = {")
        body.append(f'    "{link.peer_type}", kMd5, kMd5Stable,')
        body.append("    kAnalogCount, kDigitalCount, kStableAnalogCount, kStableDigitalCount,")
        body.append("    kAnalog, kDigital, kAnalogRx, kDigitalRx")
        body.append("};")
        body.append(f"}} // namespace {ns}")
        body.append("")
    body += ["} // namespace peer", "} // namespace combus", "", "// EOF"]
    return "\n".join(body) + "\n"
