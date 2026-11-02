#!/usr/bin/env python3
"""
combus_builder/links.py — links between nodes (link rework, steps 3.1a to 3.5).

The contract of a link is a COUPLER, the norm of the link, like a standard
plug (src/core/config/couplers/<name>/*.cb).  A node is a machine (IS_MACHINE)
or a remote (IS_REMOTE); a machine declares, per coupler, its side
(step 3.5):

  - `-D HAS_COUPLER_SOCKET_<NAME>`: it has the socket, it carries (0 to n);
  - `-D HAS_COUPLER_PLUG_<NAME>`: it has the plug, it is carried (0 or 1).

The role of a node on a link is the side of its coupler, never a nature of
the node (a towed trommel is carried when hitched, stand-alone otherwise).
The builder reads the coupler apart (it never enters the ComBus by discovery)
and:

  - full contract: the frame of the link is every channel of the coupler;
  - a coupler is a brick of the machine: its directions are those of the
    socket side, the hub of the star (machine-centric, as any .cb of the
    machine: `downlink_pt` relayed to the plug side, `downlink` received from
    it, `uplink` its own value sent, `both_or` shared fault);
  - injection: the ComBus of the coupler merges into the ComBus of the node;
    a channel of the coupler unknown to the machine type is added at its safe
    value (an absent function stays safe): as written on the socket side,
    its side of the plug on the plug side (see _plug_view);
  - star (step 3.2b): on the plug side, `downlink_pt` only redistributes in
    its node, it receives on the link; on the socket side, `downlink_pt`
    receives where the plug side emits (that link is its source) and emits
    where the plug side receives;
  - a channel of the same name in the machine type agrees on type, value and
    zeroAt, and must flow (one side emits, the other receives): two emitters,
    or nobody emitting, is an error; one source per channel on the node
    (check_single_source, both_or excepted);
  - the common frame is sorted: analog then digital, stable before candidate,
    then the name (never the theme);
  - passive coupler (step 3.4d): a coupler without link channel, only its
    local channels (a tool coupled by hand, a tiltable bucket): a local
    instance of the coupler, no link, no ComPeerLink; on the socket side only;
  - local channels (step 3.4e): a channel of the coupler that requires the
    flag of one side of this coupler (HAS_COUPLER_SOCKET_<NAME> or
    HAS_COUPLER_PLUG_<NAME>) belongs to that side only (the button, the
    latched command, an engaged switch...): injected as written (the filter
    of `requires` keeps it on its side only), never on the link (neither in
    the frame nor in the MD5); its name starts with the name of the coupler
    (several couplers on one machine never clash); it is BOARD, or SYSTEM
    when it comes from another board of the node, never BASE (an input from
    the remote belongs to the input contract of the machine type); a link
    channel is BASE;
  - machine type (step 3.6): <type>.mtype lists the couplers the type supports;
    an instance needs its support; the remote link carries only what the remote
    sends (the inputs of the type, the commands the supported couplers relay),
    with its own MD5: the same with or without the coupler; names are generic
    and shared between couplers (one channel, no more bytes), local channels
    are prefixed;
  - the MD5 of the link carries the coupler and the side: each side announces
    MD5(coupler + its side) and expects MD5(coupler + the other side), so two
    sockets (or two plugs) linked by mistake refuse each other.

Each node then has a table from its ComBus to the common frame
(combus_peer_links.h, emitted by generator.generate()).
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass, field, replace
from pathlib import Path

import yaml

from .canon import canonize
from .canon.channels import ChannelDefinition
from .parser import parse_yaml_file

SOCKET_FLAG_PREFIX = "HAS_COUPLER_SOCKET_"   # step 3.5: the side that carries (0 to n)
PLUG_FLAG_PREFIX = "HAS_COUPLER_PLUG_"       # step 3.5: the side that is carried (0 or 1)
REMOVED_PEER_FLAG_PREFIX = "COMBUS_PEER_"    # step 3.4a: replaced by the couplers, refused explicitly

# Category of a node (step 3.5): exactly one per env.  A machine is a compiled
# node that is driven; a remote is a node that drives (chantier 4).  The place
# of a machine in the star is the side of each of its couplers, not a nature.
NODE_CATEGORIES = ("IS_MACHINE", "IS_REMOTE")

SOCKET, PLUG = "socket", "plug"               # the two sides of a coupler, the role of a node on its link
REMOTE_LINK_PREFIX = "remote_"                # step 3.6: the link machine <-> remote, per machine type

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
    token: str                # surface direction in the coupler, socket view (for the MD5)
    zero_at_neutral: bool = False  # step S3-4: logical zero at CbusNeutral, the same on both sides


@dataclass
class LinkSet:
    """The channel set of a link through a coupler, in wire order."""
    coupler: str
    role: str                 # SOCKET | PLUG: the side of this node on the link
    channels: list[LinkChannel] = field(default_factory=list)


# =============================================================================
# Category, sides and second reading
# =============================================================================

def _define_names(ctx_or_defines) -> set[str]:
    if hasattr(ctx_or_defines, "defines"):
        return set(ctx_or_defines.defines) | set(getattr(ctx_or_defines, "defines_with_value", {}) or {})
    return set(ctx_or_defines)


def coupler_sides(ctx_or_defines) -> dict[str, str]:
    """Couplers declared by the node and their side ({name: SOCKET | PLUG}), names as directory names.

    LinkError when a coupler is declared on both sides (a node never carries
    itself) or when the node has more than one plug (it is carried by one at
    a time).
    """
    names = _define_names(ctx_or_defines)
    sockets = {n[len(SOCKET_FLAG_PREFIX):].lower() for n in names if n.startswith(SOCKET_FLAG_PREFIX)}
    plugs = {n[len(PLUG_FLAG_PREFIX):].lower() for n in names if n.startswith(PLUG_FLAG_PREFIX)}
    both = sorted(sockets & plugs)
    if both:
        raise LinkError(f"coupler {both[0]!r} declared on both sides: a node never carries itself")
    if len(plugs) > 1:
        raise LinkError(f"several plugs ({', '.join(sorted(plugs))}): a node is carried by one coupler at a time")
    sides = {name: SOCKET for name in sockets}
    sides.update({name: PLUG for name in plugs})
    return dict(sorted(sides.items()))


def node_category(defines) -> str:
    """Category of the node (IS_MACHINE or IS_REMOTE), checked against its couplers; LinkError if wrong."""
    names = _define_names(defines)
    removed = sorted(d for d in names if d.startswith(REMOVED_PEER_FLAG_PREFIX))
    if removed:
        raise LinkError(f"{', '.join(removed)}: COMBUS_PEER_<TYPE> is removed (link rework 3.4a), "
                        f"declare the side of the coupler (-D {SOCKET_FLAG_PREFIX}<NAME> or {PLUG_FLAG_PREFIX}<NAME>)")
    found = sorted(n for n in NODE_CATEGORIES if n in names)
    if not found:
        raise LinkError(f"no category set (expected exactly one of {', '.join(NODE_CATEGORIES)})")
    if len(found) > 1:
        raise LinkError(f"several categories set ({', '.join(found)}): a node is a machine or a remote")
    category = found[0]
    sides = coupler_sides(names)
    if category == "IS_REMOTE" and sides:
        raise LinkError(f"IS_REMOTE carries no coupler ({', '.join(sides)}): a remote drives, it is not coupled")
    return category


def load_type_base_channels(buildroot: Path, type_dir: str) -> list[ChannelDefinition]:
    """Read the BASE channels of a machine type apart (tests and diagnostics)."""
    folder = Path(buildroot) / "core" / "config" / "machines" / type_dir
    if not folder.is_dir():
        raise LinkError(f"machine type {type_dir!r}: no profile folder {folder}")
    parsed = [parse_yaml_file(p) for p in sorted(folder.rglob("*.cb"))]
    return [ch for ch in canonize(parsed).canonical_definitions if ch.scope == "BASE"]


def _side_flags(name: str) -> frozenset[str]:
    return frozenset({SOCKET_FLAG_PREFIX + name.upper(), PLUG_FLAG_PREFIX + name.upper()})


def is_local_channel(ch: ChannelDefinition, name: str) -> bool:
    """A local channel of the coupler `name`: it requires the flag of one of its sides, never on the link."""
    return bool(ch.requires & _side_flags(name))


def is_passive_coupler(coupler_chs, name: str) -> bool:
    """A passive coupler (step 3.4d): no link channel, only local channels."""
    return all(is_local_channel(ch, name) for ch in coupler_chs)


def load_coupler_channels(buildroot: Path, name: str) -> list[ChannelDefinition]:
    """Read the channels of a coupler apart: link channels BASE, local channels prefixed by the coupler."""
    folder = Path(buildroot) / "core" / "config" / "couplers" / name
    if not folder.is_dir():
        raise LinkError(f"coupler {name!r}: no folder {folder}")
    parsed = [parse_yaml_file(p) for p in sorted(folder.rglob("*.cb"))]
    channels = canonize(parsed).canonical_definitions
    prefix = name.upper() + "_"
    for ch in channels:
        sides = ch.requires & _side_flags(name)
        if len(sides) > 1:
            raise LinkError(f"coupler {name!r}: channel {ch.id!r} requires both sides, a local channel "
                            f"belongs to one side")
        if sides:
            if not ch.id.startswith(prefix):
                raise LinkError(f"coupler {name!r}: local channel {ch.id!r} must start with {prefix} "
                                f"(several couplers on one machine never clash)")
            if ch.scope not in ("BOARD", "SYSTEM"):
                raise LinkError(f"coupler {name!r}: local channel {ch.id!r} is {ch.scope}, a local channel "
                                f"is BOARD or SYSTEM (an input from the remote belongs to the machine type)")
            continue
        if ch.scope != "BASE":
            raise LinkError(f"coupler {name!r}: channel {ch.id!r} is {ch.scope}, a link channel is BASE "
                            f"(a local channel requires {SOCKET_FLAG_PREFIX}{name.upper()} or "
                            f"{PLUG_FLAG_PREFIX}{name.upper()})")
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


def _plug_view(ch: ChannelDefinition) -> ChannelDefinition:
    """A channel of a coupler (written from the socket side) seen from the plug side.

    The socket side emits it (`uplink`, or `downlink_pt`: relayed): the plug
    side receives it (`downlink`).  The socket side receives it (`downlink`):
    the plug side emits it (`uplink`).  `both` stays; the OR directions swap.
    """
    if ch.direction_or:
        seen = replace(ch, direction_or=frozenset(_SWAP[d] for d in ch.direction_or))
    elif ch.direction == frozenset({_UP, _DOWN}):
        seen = replace(ch, passthrough=False)
    elif _UP in ch.direction or ch.passthrough:
        seen = replace(ch, direction=frozenset({_DOWN}), passthrough=False)
    elif _DOWN in ch.direction:
        seen = replace(ch, direction=frozenset({_UP}))
    else:
        seen = ch
    raw = dict(ch.raw)
    raw["direction"] = _token(seen)
    return replace(seen, raw=raw)


def inject_coupler_channels(channels, coupler_chs, name: str, side: str) -> list[ChannelDefinition]:
    """The channels of the node, plus the channels of the coupler `name` it does not declare.

    The ComBus of the coupler merges into the ComBus of the node: a channel
    unknown to the machine type is added at its safe value, nobody writes it
    (an absent function stays safe).  As written on the socket side, its side
    of the plug on the plug side; a local channel as written.  A channel the
    node declares is kept as declared (compute_coupler_link checks the
    agreement).
    """
    known = {ch.id for ch in channels}
    added = [ch if (side == SOCKET or is_local_channel(ch, name)) else _plug_view(ch)
             for ch in coupler_chs if ch.id not in known]
    return list(channels) + added


# =============================================================================
# Remote (chantier 4)
# =============================================================================

def _remote_view(ch: ChannelDefinition) -> ChannelDefinition:
    """A BASE channel of the machine type (machine view) seen from the remote that drives it.

    The machine receives it (`downlink`, or `downlink_pt`: received then
    relayed): the remote sends it (`uplink`).  The machine sends it
    (`uplink`): the remote receives it (`downlink`, a feedback).  `both`
    stays; the OR directions swap.
    """
    if ch.direction_or:
        seen = replace(ch, direction_or=frozenset(_SWAP[d] for d in ch.direction_or))
    elif ch.direction == frozenset({_UP, _DOWN}):
        seen = replace(ch, passthrough=False)
    elif _DOWN in ch.direction:
        seen = replace(ch, direction=frozenset({_UP}), passthrough=False)
    elif _UP in ch.direction:
        seen = replace(ch, direction=frozenset({_DOWN}))
    else:
        seen = ch
    raw = dict(ch.raw)
    raw["direction"] = _token(seen)
    return replace(seen, raw=raw)


def remote_channels(channels, machine_type: str) -> list[ChannelDefinition]:
    """The ComBus of a remote driving `machine_type` (chantier 4, R2).

    Every BASE channel (the contract of the machine type) seen from the
    remote (_remote_view); the SYSTEM and BOARD channels of the machine type
    stay inside the machine; the other channels (core, remote) are kept.
    The contract of the link is its own MD5 (compute_remote_link, step 3.6).
    """
    from .machine_type import is_machine_type_path

    out = []
    for ch in channels:
        if ch.scope == "BASE":
            out.append(_remote_view(ch))
        elif not is_machine_type_path(ch.source_path, [machine_type]):
            out.append(ch)
    return out


# =============================================================================
# Machine type configuration and contract with the remote (step 3.6)
# =============================================================================

@dataclass(frozen=True)
class MachineTypeConfig:
    """The couplers a machine type supports (<type>.mtype), part of its contract."""
    socket: tuple[str, ...] = ()
    plug: tuple[str, ...] = ()


def load_machine_type_config(buildroot: Path, type_dir: str) -> MachineTypeConfig:
    """Read core/config/machines/<type>/<type>.mtype; no file: no coupler supported."""
    path = Path(buildroot) / "core" / "config" / "machines" / type_dir / f"{type_dir}.mtype"
    if not path.is_file():
        return MachineTypeConfig()
    data = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    unknown = set(data) - {"couplers"}
    if unknown:
        raise LinkError(f"{path.name}: unknown field(s) {sorted(unknown)}")
    couplers = data.get("couplers") or {}
    unknown = set(couplers) - {SOCKET, PLUG}
    if unknown:
        raise LinkError(f"{path.name}: couplers: unknown side(s) {sorted(unknown)}")
    return MachineTypeConfig(socket=tuple(couplers.get(SOCKET) or ()), plug=tuple(couplers.get(PLUG) or ()))


def _plug_sourced(coupler_chs, name: str) -> list[ChannelDefinition]:
    """Link channels the plug side sends (`downlink` written from the socket side)."""
    return [c for c in coupler_chs if not is_local_channel(c, name) and not c.direction_or
            and c.direction == frozenset({_DOWN}) and not c.passthrough]


def _relayed(coupler_chs, name: str) -> list[ChannelDefinition]:
    """Link channels the socket side relays from the remote (`downlink_pt`)."""
    return [c for c in coupler_chs if not is_local_channel(c, name) and c.passthrough]


def _delivered_to_plug(coupler_chs, name: str) -> list[ChannelDefinition]:
    """Link channels a plug side receives (`downlink_pt` and `uplink` written from the socket side)."""
    return [c for c in coupler_chs if not is_local_channel(c, name) and not c.direction_or
            and (c.passthrough or c.direction == frozenset({_UP}))]


def check_supported_couplers(type_base, supported: dict) -> None:
    """Generic names, one meaning; one source: LinkError otherwise.

    The same name in two couplers is one channel: same type, value and zeroAt.
    A channel of the machine type comes from the remote: a coupler that says
    the plug side sends it gives it two sources.
    """
    from .generator import _render_value

    seen: dict[str, tuple[str, ChannelDefinition]] = {}
    for name, chs in supported.items():
        for c in chs:
            if is_local_channel(c, name):
                continue
            if c.id in seen:
                other, o = seen[c.id]
                if (o.type, _render_value(o, o.type), o.zero_at_neutral) != \
                        (c.type, _render_value(c, c.type), c.zero_at_neutral):
                    raise LinkError(f"channel {c.id!r}: couplers {other!r} and {name!r} disagree "
                                    f"(same name, one channel: same type, value and zeroAt)")
            else:
                seen[c.id] = (name, c)

    from_remote = {c.id for c in type_base if c.scope == "BASE" and c.direction == frozenset({_DOWN})}
    for name, chs in supported.items():
        for c in _plug_sourced(chs, name):
            if c.id in from_remote:
                raise LinkError(f"channel {c.id!r}: two sources, the remote (machine type) and the plug "
                                f"side of the coupler {name!r}")


def remote_commands(type_base, supported: dict, plugs: dict | None = None) -> list[ChannelDefinition]:
    """What the remote sends, machine view: the only channels of the remote link.

    The inputs of the machine type, and the commands the supported couplers
    relay (one channel per name); never what a plug side sends, nor what the
    plug of this type receives (one source per channel; who commands a carried
    machine left stand-alone is an open question, WIP Remote firmware).  The
    same with or without the coupler: a supported coupler is part of the contract.
    """
    from_plug = {c.id for name, chs in supported.items() for c in _plug_sourced(chs, name)}
    from_plug |= {c.id for name, chs in (plugs or {}).items() for c in _delivered_to_plug(chs, name)}
    commands: dict[str, ChannelDefinition] = {}
    for c in type_base:
        if c.scope == "BASE" and c.direction == frozenset({_DOWN}) and c.id not in from_plug:
            commands[c.id] = c
    for name, chs in supported.items():
        for c in _relayed(chs, name):
            if c.id not in from_plug:
                commands.setdefault(c.id, c)
    return [commands[cid] for cid in sorted(commands)]


def compute_remote_link(machine_type: str, commands, side: str) -> LinkSet:
    """The link machine (SOCKET) <-> remote (PLUG): the commands, sent by the remote."""
    from .generator import _render_value

    link = LinkSet(coupler=REMOTE_LINK_PREFIX + machine_type, role=side)
    for c in commands:
        link.channels.append(LinkChannel(id=c.id, type=c.type, candidate=c.candidate,
                                         value=_render_value(c, c.type),
                                         flow="from_peer" if side == SOCKET else "to_peer",
                                         token=_token(c), zero_at_neutral=c.zero_at_neutral))
    link.channels.sort(key=lambda c: (0 if c.type == "analog" else 1, c.candidate, c.id))
    return link


# =============================================================================
# Link set
# =============================================================================

def _flow(a: ChannelDefinition, b: ChannelDefinition, local_is_socket: bool) -> str:
    """Flow of one channel on the link a (local) <-> b (peer, in its own view); LinkError if it does not flow.

    Step 3.2b, star: on the plug side, `_pt` only redistributes in its node (a
    plain receiver on the link); on the socket side, `_pt` receives where the
    plug side emits (that link is its source) and emits where the plug side
    receives.
    """
    cid = a.id
    if a.direction_or or b.direction_or:
        ta, tb = _token(a), _token(b)
        if (ta, tb) in {("both_or", "both_or"), ("uplink_or", "downlink_or"), ("downlink_or", "uplink_or")}:
            return "or" if ta == "both_or" else ("to_peer" if ta == "uplink_or" else "from_peer")
        raise LinkError(f"channel {cid!r}: OR directions do not match ({ta} / {tb})")

    v, t = (a, b) if local_is_socket else (b, a)   # v: the socket side, t: the plug side
    v_up, v_down = _UP in v.direction, _DOWN in v.direction
    t_up, t_down = _UP in t.direction, _DOWN in t.direction

    if v_up and t_up:
        if v.direction == t.direction == frozenset({_UP, _DOWN}) and not (v.passthrough or t.passthrough):
            return "both"
        raise LinkError(f"channel {cid!r}: two emitters on the link ({_token(a)} / {_token(b)})")
    if t_up and v_down:
        to_socket = True                        # this link is the source of the socket side
    elif t_down and (v_up or (v.passthrough and v_down)):
        to_socket = False                       # the socket side emits (its own value, or relayed)
    else:
        raise LinkError(f"channel {cid!r}: nobody emits it to a receiver on this link "
                        f"({_token(a)} / {_token(b)}): every channel of a coupler must flow")
    return "from_peer" if to_socket == local_is_socket else "to_peer"


def compute_coupler_link(name: str, coupler_chs, local_chs, side: str) -> LinkSet:
    """Channel set of the link through a coupler, in wire order: the full coupler.

    local_chs: the BASE channels of the node, coupler channels injected
    (inject_coupler_channels); side: SOCKET or PLUG, the side of this node.
    """
    from .generator import _render_value

    if side not in (SOCKET, PLUG):
        raise LinkError(f"coupler {name!r}: unknown side {side!r}")
    local_is_socket = side == SOCKET
    local = {ch.id: ch for ch in local_chs if ch.scope == "BASE"}
    link = LinkSet(coupler=name, role=side)

    for c in sorted((ch for ch in coupler_chs if not is_local_channel(ch, name)), key=lambda ch: ch.id):
        a = local.get(c.id)
        if a is None:
            raise LinkError(f"coupler {name!r}: channel {c.id!r} missing from the node (not injected)")
        b = _plug_view(c) if local_is_socket else c   # the peer, in its own view
        if a.type != c.type:
            raise LinkError(f"channel {c.id!r}: {a.type} in the node, {c.type} in the coupler {name!r}")
        va, vc = _render_value(a, a.type), _render_value(c, c.type)
        if va != vc:
            raise LinkError(f"channel {c.id!r}: default value {va} in the node, {vc} in the coupler {name!r}")
        if a.zero_at_neutral != c.zero_at_neutral:
            raise LinkError(f"channel {c.id!r}: zeroAt differs between the node and the coupler {name!r} "
                            f"(the two ends would read the same raw value differently)")
        link.channels.append(LinkChannel(id=c.id, type=c.type, candidate=c.candidate, value=vc,
                                         flow=_flow(a, b, local_is_socket), token=_token(c),
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
    """MD5 of the link: the coupler and a side (stable: without the candidates).

    peer=False: the MD5 this node announces (its own side); peer=True: the MD5
    it expects from the other end (the other side).
    """
    role = link.role if not peer else (PLUG if link.role == SOCKET else SOCKET)
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
        " * Links between nodes: for each coupler of the node (-D HAS_COUPLER_SOCKET_<NAME> or _PLUG_<NAME>),",
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
        body.append(f"/// Link through the {link.coupler} coupler, this node on the {link.role} side.")
        body.append(f"namespace {ns} {{")
        body.append("/// MD5 announced by this node (its side), and expected from the other end (the other side).")
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
        body.append(f"    .role      = ComLinkRole::{link.role.upper()},")
        body.append("    .announced = { .stable = kAnnouncedMd5, .next = kAnnouncedMd5Next },")
        body.append("    .expected  = { .stable = kExpectedMd5,  .next = kExpectedMd5Next },")
        body.append("    .analog    = { .count = kAnalogCount,  .stableCount = kStableAnalogCount,  "
                    ".ids = kAnalog,  .rx = kAnalogRx },")
        body.append("    .digital   = { .count = kDigitalCount, .stableCount = kStableDigitalCount, "
                    ".ids = kDigital, .rx = kDigitalRx },")
        body.append("};")
        body.append(f"}} // namespace {ns}")
        body.append("")
    for link in links:
        if link.coupler.startswith(REMOTE_LINK_PREFIX):
            body.append("/// The link machine <-> remote of this node, whatever the machine type.")
            body.append(f"static constexpr const ComPeerLink& kRemoteLink = {_cpp_ident(link.coupler)}::kLink;")
            body.append("")
    body += ["} // namespace peer", "} // namespace combus", "", "// EOF"]
    return "\n".join(body) + "\n"
