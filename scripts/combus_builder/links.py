#!/usr/bin/env python3
"""
combus_builder/links.py — links between nodes (link rework, steps 3.1a and 3.4a).

Step 3.4a: the contract of a link is a COUPLER, the norm of the link, like a
standard plug (src/core/config/couplers/<name>/*.cb), no longer computed for
a pair of machine types.  A node declares the couplers it carries
(`-D COMBUS_COUPLER_<NAME>`): 0 to n on a vehicle, exactly 1 on an
attachment.  The builder reads the coupler apart (it never enters the
ComBus by discovery) and:

  - full contract: the frame of the link is every channel of the coupler;
  - injection: the ComBus of the coupler merges into the ComBus of the node;
    a channel of the coupler unknown to the machine type is added at its
    safe value (an absent function stays safe), with the direction of the
    coupler on the attachment and the mirrored one on the vehicle;
  - directions of a coupler are those of the attachment (uplink: to the
    vehicle, downlink: from the vehicle, both_or: shared fault); no `_pt`;
  - star "vehicle + 1" (step 3.2b): on the attachment, `downlink_pt` only
    redistributes in its node, it receives on the link; on the vehicle,
    `downlink_pt` receives where the attachment emits (that link is its
    source) and emits where the attachment receives;
  - a channel of the same name in the machine type agrees on type, value and
    zeroAt, and must flow (one side emits, the other receives): two emitters,
    or nobody emitting, is an error; one source per channel on the node
    (check_single_source, both_or excepted);
  - the common frame is sorted: analog then digital, stable before candidate,
    then the name (never the theme);
  - the MD5 of the link carries the coupler and the role: each side announces
    MD5(coupler + its role) and expects MD5(coupler + the other role), so two
    vehicles (or two attachments) linked by mistake refuse each other.

Each node then has a table from its ComBus to the common frame
(combus_peer_links.h, emitted by generator.generate()).
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass, field, replace
from pathlib import Path

from .canon import canonize
from .canon.channels import ChannelDefinition
from .parser import parse_yaml_file

COUPLER_FLAG_PREFIX = "COMBUS_COUPLER_"
REMOVED_PEER_FLAG_PREFIX = "COMBUS_PEER_"   # step 3.4a: replaced by the couplers, refused explicitly

# Nature of a node (step 3.2b): exactly one structural flag per env.  The
# vehicle is the hub of the star; an attachment has one link, to a vehicle; a
# stand-alone machine has no link (a coupling for some machines may come
# later); the remote is unchanged (chantier 4).
NODE_NATURES = ("IS_VEHICLE", "IS_ATTACHMENT", "IS_MACHINE", "IS_REMOTE")

_UP, _DOWN = "uplink", "downlink"
_SWAP = {_UP: _DOWN, _DOWN: _UP}


class LinkError(Exception):
    """A link between two nodes cannot be built (contract conflict)."""


@dataclass(frozen=True)
class LinkChannel:
    """One channel of the common frame of a link."""
    id: str
    type: str                 # "analog" | "digital"
    candidate: bool
    value: str                # rendered default value (C++ literal), the safe value of the coupler
    flow: str                 # "to_peer" | "from_peer" | "both" | "or"
    token: str                # surface direction in the coupler, attachment view (for the MD5)
    zero_at_neutral: bool = False  # step S3-4: logical zero at CbusNeutral, the same on both sides


@dataclass
class LinkSet:
    """The channel set of a link through a coupler, in wire order."""
    coupler: str
    role: str                 # "vehicle" | "attachment": the role of this node on the link
    channels: list[LinkChannel] = field(default_factory=list)


# =============================================================================
# Couplers, nature and second reading
# =============================================================================

def coupler_names(ctx) -> list[str]:
    """Couplers declared by `-D COMBUS_COUPLER_<NAME>`, as directory names, sorted."""
    names = set(ctx.defines) | set(getattr(ctx, "defines_with_value", {}) or {})
    return sorted(n[len(COUPLER_FLAG_PREFIX):].lower() for n in names if n.startswith(COUPLER_FLAG_PREFIX))


def node_nature(defines) -> str:
    """Structural flag of the node, checked against its couplers; LinkError if the env is wrong."""
    removed = sorted(d for d in defines if d.startswith(REMOVED_PEER_FLAG_PREFIX))
    if removed:
        raise LinkError(f"{', '.join(removed)}: COMBUS_PEER_<TYPE> is removed (link rework 3.4a), "
                        f"declare the coupler of the link (-D {COUPLER_FLAG_PREFIX}<NAME>)")
    found = sorted(n for n in NODE_NATURES if n in defines)
    if not found:
        raise LinkError(f"no structural flag set (expected exactly one of {', '.join(NODE_NATURES)})")
    if len(found) > 1:
        raise LinkError(f"several structural flags set ({', '.join(found)}): a node has exactly one nature")
    nature = found[0]
    couplers = sorted(d for d in defines if d.startswith(COUPLER_FLAG_PREFIX))
    if nature in ("IS_MACHINE", "IS_REMOTE") and couplers:
        raise LinkError(f"{nature} carries no coupler ({', '.join(couplers)}): only a vehicle or an "
                        f"attachment is linked (a coupling for this nature is not defined)")
    if nature == "IS_ATTACHMENT" and len(couplers) != 1:
        raise LinkError(f"IS_ATTACHMENT needs exactly one coupler, the one of its vehicle (got {len(couplers)})")
    return nature


def load_type_base_channels(buildroot: Path, type_dir: str) -> list[ChannelDefinition]:
    """Read the BASE channels of a machine type apart (tests and diagnostics)."""
    folder = Path(buildroot) / "core" / "config" / "machines" / type_dir
    if not folder.is_dir():
        raise LinkError(f"machine type {type_dir!r}: no profile folder {folder}")
    parsed = [parse_yaml_file(p) for p in sorted(folder.rglob("*.cb"))]
    return [ch for ch in canonize(parsed).canonical_definitions if ch.scope == "BASE"]


def load_coupler_channels(buildroot: Path, name: str) -> list[ChannelDefinition]:
    """Read the channels of a coupler apart: BASE only, no `_pt`."""
    folder = Path(buildroot) / "core" / "config" / "couplers" / name
    if not folder.is_dir():
        raise LinkError(f"coupler {name!r}: no folder {folder}")
    parsed = [parse_yaml_file(p) for p in sorted(folder.rglob("*.cb"))]
    channels = canonize(parsed).canonical_definitions
    for ch in channels:
        if ch.scope != "BASE":
            raise LinkError(f"coupler {name!r}: channel {ch.id!r} is {ch.scope}, a coupler is BASE only")
        if ch.passthrough:
            raise LinkError(f"coupler {name!r}: channel {ch.id!r} is {_token(ch)}: no _pt in a coupler "
                            f"(relaying is the business of a node)")
    return channels


# =============================================================================
# Injection
# =============================================================================

def _token(ch: ChannelDefinition) -> str:
    """Surface direction token of a canonical channel (downlink_pt, both_or, ...)."""
    d = ch.direction | ch.direction_or
    base = {frozenset({_UP}): "uplink", frozenset({_DOWN}): "downlink",
            frozenset({_UP, _DOWN}): "both"}.get(frozenset(d), "none")
    if ch.direction_or:
        return base + "_or"
    return base + ("_pt" if ch.passthrough else "")


def _mirror(ch: ChannelDefinition) -> ChannelDefinition:
    """A channel of a coupler seen from the vehicle: uplink and downlink swapped."""
    mirrored = replace(ch, direction=frozenset(_SWAP[d] for d in ch.direction),
                       direction_or=frozenset(_SWAP[d] for d in ch.direction_or))
    raw = dict(ch.raw)
    raw["direction"] = _token(mirrored)
    return replace(mirrored, raw=raw)


def inject_coupler_channels(channels, coupler_chs, nature: str) -> list[ChannelDefinition]:
    """The channels of the node, plus the channels of the coupler it does not declare.

    The ComBus of the coupler merges into the ComBus of the node: a channel
    unknown to the machine type is added at its safe value, nobody writes it
    (an absent function stays safe).  Direction of the coupler on the
    attachment, mirrored on the vehicle.  A channel the node declares is kept
    as declared (compute_coupler_link checks the agreement).
    """
    known = {ch.id for ch in channels}
    added = [ch if nature == "IS_ATTACHMENT" else _mirror(ch) for ch in coupler_chs if ch.id not in known]
    return list(channels) + added


# =============================================================================
# Link set
# =============================================================================

def _flow(a: ChannelDefinition, b: ChannelDefinition, local_is_vehicle: bool) -> str:
    """Flow of one channel on the link a (local) <-> b (peer, in its own view); LinkError if it does not flow.

    Step 3.2b, star "vehicle + 1": on the attachment, `_pt` only redistributes
    in its node (a plain receiver on the link); on the vehicle, `_pt` receives
    where the attachment emits (that link is its source) and emits where the
    attachment receives.
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
        raise LinkError(f"channel {cid!r}: nobody emits it to a receiver on this link "
                        f"({_token(a)} / {_token(b)}): every channel of a coupler must flow")
    return "from_peer" if to_vehicle == local_is_vehicle else "to_peer"


def compute_coupler_link(name: str, coupler_chs, local_chs, nature: str) -> LinkSet:
    """Channel set of the link through a coupler, in wire order: the full coupler.

    local_chs: the BASE channels of the node, coupler channels injected
    (inject_coupler_channels); nature: IS_VEHICLE or IS_ATTACHMENT.
    """
    from .generator import _render_value

    if nature not in ("IS_VEHICLE", "IS_ATTACHMENT"):
        raise LinkError(f"a {nature} node has no link")
    local_is_vehicle = nature == "IS_VEHICLE"
    local = {ch.id: ch for ch in local_chs if ch.scope == "BASE"}
    link = LinkSet(coupler=name, role="vehicle" if local_is_vehicle else "attachment")

    for c in sorted(coupler_chs, key=lambda ch: ch.id):
        a = local.get(c.id)
        if a is None:
            raise LinkError(f"coupler {name!r}: channel {c.id!r} missing from the node (not injected)")
        b = c if local_is_vehicle else _mirror(c)   # the peer, in its own view
        if a.type != c.type:
            raise LinkError(f"channel {c.id!r}: {a.type} in the node, {c.type} in the coupler {name!r}")
        va, vc = _render_value(a, a.type), _render_value(c, c.type)
        if va != vc:
            raise LinkError(f"channel {c.id!r}: default value {va} in the node, {vc} in the coupler {name!r}")
        if a.zero_at_neutral != c.zero_at_neutral:
            raise LinkError(f"channel {c.id!r}: zeroAt differs between the node and the coupler {name!r} "
                            f"(the two ends would read the same raw value differently)")
        link.channels.append(LinkChannel(id=c.id, type=c.type, candidate=c.candidate, value=vc,
                                         flow=_flow(a, b, local_is_vehicle), token=_token(c),
                                         zero_at_neutral=c.zero_at_neutral))

    link.channels.sort(key=lambda c: (0 if c.type == "analog" else 1, c.candidate, c.id))
    return link


def check_single_source(links: list[LinkSet]) -> None:
    """One source per channel on the node: received on one of its links at most (both_or excepted)."""
    sources: dict[str, list[str]] = {}
    for link in links:
        for c in link.channels:
            if c.flow in ("from_peer", "both"):
                sources.setdefault(c.id, []).append(link.coupler)
    for cid, couplers in sorted(sources.items()):
        if len(couplers) > 1:
            raise LinkError(f"channel {cid!r}: received from several links ({', '.join(couplers)}): "
                            f"one source per channel")


def link_md5(link: LinkSet, stable: bool = False, peer: bool = False) -> str:
    """MD5 of the link: the coupler and a role (stable: without the candidates).

    peer=False: the MD5 this node announces (its own role); peer=True: the MD5
    it expects from the other end (the other role).
    """
    role = link.role if not peer else ("attachment" if link.role == "vehicle" else "vehicle")
    entries = []
    for c in link.channels:
        if stable and c.candidate:
            continue
        entry = {"id": c.id, "type": c.type, "candidate": c.candidate, "value": c.value, "dir": c.token}
        if c.zero_at_neutral:                    # step S3-4: hashed only when neutral
            entry["zeroAt"] = "neutral"
        entries.append(entry)
    payload = {"coupler": link.coupler, "role": role, "channels": entries}
    return hashlib.md5(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()


# =============================================================================
# C++ rendering
# =============================================================================

def _cpp_ident(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


def _md5_array(name: str, md5_hex: str) -> str:
    return f"static constexpr uint8_t {name} = {{ {', '.join(f'0x{x:02X}u' for x in bytes.fromhex(md5_hex))} }};"


def render_peer_links_header(links: list[LinkSet], ids_header: str = "combus_ids.h") -> str:
    """combus_peer_links.h: per coupler, the MD5 and the table ComBus -> common frame."""
    body = [
        "/******************************************************************************",
        " * GENERATED FILE - DO NOT EDIT.",
        " *",
        " * Generated by scripts/combus_builder/links.py (link rework, steps 3.1 and 3.4a).",
        " * Links between nodes: for each coupler declared by -D COMBUS_COUPLER_<NAME>,",
        " * the common frame of the link (the full coupler, wire order) and the table",
        " * from this node's ComBus to that frame.",
        " ******************************************************************************/",
        "",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "#include <core/system/combus/protocol/frame/combus_peer_link_defs.h>   // ComPeerLink",
        f'#include "{ids_header}"',
        "",
        "namespace combus {",
        "namespace peer {",
        "",
    ]
    if not links:
        body.append("// No coupler declared for this node.")
        body.append("")
    for link in links:
        ns = _cpp_ident(link.coupler)
        analog = [c for c in link.channels if c.type == "analog"]
        digital = [c for c in link.channels if c.type == "digital"]
        body.append(f"/// Link through the {link.coupler} coupler, this node being the {link.role}.")
        body.append(f"namespace {ns} {{")
        body.append("/// MD5 announced by this node (its role), and expected from the other end (the other role).")
        body.append(_md5_array("kAnnouncedMd5[16]    ", link_md5(link, stable=True)))
        body.append(_md5_array("kAnnouncedMd5Next[16]", link_md5(link)))
        body.append(_md5_array("kExpectedMd5[16]     ", link_md5(link, stable=True, peer=True)))
        body.append(_md5_array("kExpectedMd5Next[16] ", link_md5(link, peer=True)))
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
        body.append("/// The link, for the ComDev of this coupler (ComDev::peer).")
        body.append("static constexpr ComPeerLink kLink = {")
        body.append(f'    .coupler   = "{link.coupler}",')
        body.append("    .announced = { .stable = kAnnouncedMd5, .next = kAnnouncedMd5Next },")
        body.append("    .expected  = { .stable = kExpectedMd5,  .next = kExpectedMd5Next },")
        body.append("    .analog    = { .count = kAnalogCount,  .stableCount = kStableAnalogCount,  "
                    ".ids = kAnalog,  .rx = kAnalogRx },")
        body.append("    .digital   = { .count = kDigitalCount, .stableCount = kStableDigitalCount, "
                    ".ids = kDigital, .rx = kDigitalRx },")
        body.append("};")
        body.append(f"}} // namespace {ns}")
        body.append("")
    body += ["} // namespace peer", "} // namespace combus", "", "// EOF"]
    return "\n".join(body) + "\n"
