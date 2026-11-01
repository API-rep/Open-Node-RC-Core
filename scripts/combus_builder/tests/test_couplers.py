"""Couplers: the norm of a link between nodes (link rework, steps 3.4a to 3.5).

Rules validated by the owner (2026-10-31; socket view: 2026-11-01, step 3.4e;
sides: 2026-11-01, step 3.5):
  - T1 a coupler is a folder src/core/config/couplers/<name>/ of ordinary
    BASE channels, a brick of the machine: directions are those of the socket
    side, the hub of the star (machine-centric: `downlink_pt` relayed to the
    plug side, `downlink` received from it, `uplink` its own value sent,
    `both_or` shared fault); `value` is the safe value;
  - T2 a machine (IS_MACHINE) declares the side of each coupler:
    -D HAS_COUPLER_SOCKET_<NAME> (it carries, 0 to n) or
    -D HAS_COUPLER_PLUG_<NAME> (it is carried, 0 or 1); a coupler on both
    sides, two plugs, or a remote (IS_REMOTE) with a coupler is refused
    explicitly, never ignored; COMBUS_PEER_<TYPE> is removed (refused);
  - T3 full contract: the frame is every channel of the coupler; the ComBus
    of the coupler merges into the ComBus of the node: a channel of the
    coupler unknown to the machine type is injected at its safe value (as
    written on the socket side, its side of the plug on the plug side);
  - T4 a channel of the same name in the machine type agrees on type, value
    and zeroAt; its direction must flow; a channel that would not flow is an
    error (no "dead channel" in a full contract);
  - T5 / T6 the MD5 of the link carries the coupler only, plus the role:
    each side announces MD5(coupler + its role) and expects MD5(coupler + the
    other role), so two sockets (or two plugs) linked by mistake refuse each
    other;
  - T7 the generated header has one namespace per coupler;
  - T8 the names of the coupler are the norm (no renaming);
  - T9 the coupler files never enter the ComBus by discovery: they are read
    apart, and only for the couplers declared by the node;
  - L (step 3.4e, 2026-11-01) local channels: a channel of the coupler that
    requires the flag of one of its sides belongs to that side only (filtered
    by `requires`), never on the link (frame, MD5); BOARD or SYSTEM; its name
    starts with the name of the coupler; an input from the remote (the
    coupling button) belongs to the machine type, not to the coupler.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder import links as links_module  # noqa: E402
from scripts.combus_builder.canon import canonize  # noqa: E402
from scripts.combus_builder.flags import BuildContext  # noqa: E402
from scripts.combus_builder.generator import GeneratorError, generate  # noqa: E402
from scripts.combus_builder.links import (  # noqa: E402
    LinkError,
    check_single_source,
    compute_coupler_link,
    PLUG,
    SOCKET,
    coupler_sides,
    inject_coupler_channels,
    is_local_channel,
    link_md5,
    load_coupler_channels,
    node_category,
)
from scripts.combus_builder.parser import discover_config_files  # noqa: E402

SRC = REPO_ROOT / "src"
V, A = SOCKET, PLUG
M = "IS_MACHINE"
HITCH = "HAS_COUPLER_SOCKET_TEST_COUPLER"
HITCH_PLUG = "HAS_COUPLER_PLUG_TEST_COUPLER"
TOOL = "HAS_COUPLER_SOCKET_TEST_TOOL"


def _ctx(*flags):
    return BuildContext(buildroot=SRC, defines=frozenset(flags), defines_with_value={},
                        cppdefines_source="override")


def _ch(id_, direction="downlink", type_="analog", value=None, zero_at=None, candidate=None):
    raw = {"id": id_, "infoName": id_.lower(), "type": type_, "scope": "BASE", "theme": "core",
           "direction": direction,
           "value": value if value is not None else ("low" if type_ == "digital" else "CbusNeutral")}
    if zero_at is not None:
        raw["zeroAt"] = zero_at
    if candidate is not None:
        raw["candidate"] = candidate
    return raw


def _canon(*raws):
    parsed = [(Path(f"ch{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


def _flow(link, cid):
    return {c.id: c.flow for c in link.channels}.get(cid)


def _link(coupler_raws, local_raws, nature=V, name="cpl"):
    """The link of a node carrying the coupler, after injection, as the generator computes it."""
    coupler = _canon(*coupler_raws)
    local = inject_coupler_channels(_canon(*local_raws) if local_raws else [], coupler, name, nature)
    return compute_coupler_link(name, coupler, local, nature)


@pytest.fixture(scope="module")
def hitch():
    return load_coupler_channels(SRC, "test_coupler")


@pytest.fixture(scope="module")
def tractor():
    return links_module.load_type_base_channels(SRC, "test_tractor")


@pytest.fixture(scope="module")
def trailer():
    return links_module.load_type_base_channels(SRC, "test_trailer")


# =============================================================================
# T1 / T9: the coupler folder, read apart
# =============================================================================

def test_t1_the_test_coupler_is_read_apart(hitch):
    assert sorted(c.id for c in hitch if not is_local_channel(c, "test_coupler")) == [
        "TEST_AXLE_LOAD", "TEST_BRAKE_BUS", "TEST_COUPLED_FAIL", "TEST_LIGHTS", "TEST_TILT_ANGLE"]
    assert sorted(c.id for c in hitch if is_local_channel(c, "test_coupler")) == ["TEST_COUPLER_CMD", "TEST_COUPLER_COUPLED",
                                                                  "TEST_COUPLER_ENGAGED_SW"]


# =============================================================================
# L: local channels (step 3.4e)
# =============================================================================

def _coupler_folder(tmp_path, name, body):
    folder = tmp_path / "core" / "config" / "couplers" / name
    folder.mkdir(parents=True)
    (folder / f"{name}.cb").write_text("channels:\n" + body, encoding="utf-8")
    return tmp_path


def _local(id_, scope="BOARD", requires="[HAS_COUPLER_SOCKET_FRONT_COUPLER]"):
    direction = "" if scope == "BOARD" else "  direction: uplink\n"
    return (f"- id: {id_}\n  infoName: x\n  type: digital\n  scope: {scope}\n  theme: core\n"
            f"{direction}  value: low\n  requires: {requires}\n")


def test_l1_a_prefixed_board_local_channel_is_accepted(tmp_path):
    chs = load_coupler_channels(_coupler_folder(tmp_path, "front_coupler", _local("FRONT_COUPLER_CMD")), "front_coupler")
    assert is_local_channel(chs[0], "front_coupler")


def test_l2_a_local_channel_without_the_prefix_is_refused(tmp_path):
    with pytest.raises(LinkError, match="FRONT_COUPLER_"):
        load_coupler_channels(_coupler_folder(tmp_path, "front_coupler", _local("COUPLER_CMD")), "front_coupler")


def test_l3_a_local_channel_of_both_natures_is_refused(tmp_path):
    root = _coupler_folder(tmp_path, "front_coupler", _local("FRONT_COUPLER_CMD", requires="[HAS_COUPLER_SOCKET_FRONT_COUPLER, HAS_COUPLER_PLUG_FRONT_COUPLER]"))
    with pytest.raises(LinkError, match="both sides"):
        load_coupler_channels(root, "front_coupler")


def test_l4_a_local_channel_may_be_system(tmp_path):
    """The engaged switch on another board of the node: SYSTEM, still local."""
    root = _coupler_folder(tmp_path, "front_coupler", _local("FRONT_COUPLER_ENGAGED_SW", scope="SYSTEM"))
    assert is_local_channel(load_coupler_channels(root, "front_coupler")[0], "front_coupler")


def test_l4b_a_local_channel_is_never_base(tmp_path):
    """BASE with a requires is refused (the frame of a machine type is fixed): the remote input belongs to the type."""
    root = _coupler_folder(tmp_path, "front_coupler", _local("FRONT_COUPLER_BTN", scope="BASE"))
    with pytest.raises(Exception, match="FRONT_COUPLER_BTN"):
        load_coupler_channels(root, "front_coupler")


def test_l5_local_channels_on_the_vehicle_only_and_off_the_link(tmp_path, tractor, trailer):
    generate(tractor, _ctx(M, "MACHINE_TYPE_TEST_TRACTOR", HITCH), tmp_path / "v")
    ids_v = (tmp_path / "v" / "combus_ids.h").read_text(encoding="utf-8")
    assert "TEST_COUPLER_CMD" in ids_v
    assert "TEST_COUPLER_ENGAGED_SW" not in ids_v                    # its flag is not set
    links_v = (tmp_path / "v" / "combus_peer_links.h").read_text(encoding="utf-8")
    assert "TEST_COUPLER_" not in links_v                            # never on the link

    generate(tractor, _ctx(M, "MACHINE_TYPE_TEST_TRACTOR", HITCH, "HAS_TEST_COUPLER_ENGAGED_SW"), tmp_path / "g")
    assert "TEST_COUPLER_ENGAGED_SW" in (tmp_path / "g" / "combus_ids.h").read_text(encoding="utf-8")

    generate(trailer, _ctx(M, "MACHINE_TYPE_TEST_TRAILER", HITCH_PLUG), tmp_path / "a")
    assert "TEST_COUPLER_CMD" not in (tmp_path / "a" / "combus_ids.h").read_text(encoding="utf-8")


def test_p1_a_coupler_without_link_channel_is_passive(tmp_path, tractor, capsys):
    """Step 3.4d: a local instance of the coupler, no link, no ComPeerLink."""
    assert links_module.is_passive_coupler(load_coupler_channels(SRC, "test_tool"), "test_tool")
    assert not links_module.is_passive_coupler(load_coupler_channels(SRC, "test_coupler"), "test_coupler")
    generate(tractor, _ctx(M, "MACHINE_TYPE_TEST_TRACTOR", HITCH, TOOL), tmp_path)
    out = capsys.readouterr().out
    assert "[combus_builder] set COUPLER test_tool (passive)" in out
    links = (tmp_path / "combus_peer_links.h").read_text(encoding="utf-8")
    assert "namespace TestCoupler {" in links
    assert "TestTool" not in links
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "TEST_TOOL_DETECT_SW" in ids and "TEST_TOOL_ATTACHED" in ids


def test_p2_the_coupler_of_an_attachment_is_never_passive(tmp_path, trailer):
    with pytest.raises(GeneratorError, match="passive"):
        generate(trailer, _ctx(M, "MACHINE_TYPE_TEST_TRAILER", "HAS_COUPLER_PLUG_TEST_TOOL"), tmp_path)


def test_l6_local_channels_leave_the_md5_of_the_link(hitch, tractor):
    link_only = [c for c in hitch if not is_local_channel(c, "test_coupler")]
    with_local = compute_coupler_link("test_coupler", hitch, inject_coupler_channels(tractor, hitch, "test_coupler", V), V)
    without = compute_coupler_link("test_coupler", link_only, inject_coupler_channels(tractor, link_only, "test_coupler", V), V)
    assert link_md5(with_local) == link_md5(without)


def test_t1_unknown_coupler_is_an_error():
    with pytest.raises(LinkError, match="no_such_coupler"):
        load_coupler_channels(SRC, "no_such_coupler")


def test_row12_pt_in_a_coupler_is_the_relay_of_the_vehicle(tmp_path):
    """Vehicle view (3.4e): `downlink_pt` is allowed, the vehicle relays it to the attachment."""
    folder = tmp_path / "core" / "config" / "couplers" / "relay"
    folder.mkdir(parents=True)
    (folder / "relay.cb").write_text(
        "channels:\n- id: X\n  infoName: x\n  type: analog\n  scope: BASE\n  theme: core\n"
        "  direction: downlink_pt\n  value: CbusNeutral\n", encoding="utf-8")
    coupler = load_coupler_channels(tmp_path, "relay")
    assert coupler[0].passthrough


def test_t1_a_coupler_channel_is_base_only(tmp_path):
    folder = tmp_path / "core" / "config" / "couplers" / "bad"
    folder.mkdir(parents=True)
    (folder / "bad.cb").write_text(
        "channels:\n- id: X\n  infoName: x\n  type: digital\n  scope: BOARD\n  theme: core\n"
        "  value: low\n", encoding="utf-8")
    with pytest.raises(LinkError, match="a link channel is BASE"):
        load_coupler_channels(tmp_path, "bad")


@pytest.mark.parametrize("machine_type", [None, "test_tractor"])
def test_t9_discovery_never_loads_a_coupler(machine_type):
    files = discover_config_files(SRC, machine_type=machine_type)
    assert not [p for p in files if "couplers" in p.parts]


# =============================================================================
# T2: category and sides (step 3.5)
# =============================================================================

def test_t2_sides_from_flags():
    """The dolly: carried by the truck (plug), it carries the semi-trailer (socket)."""
    assert coupler_sides(_ctx(M, "HAS_COUPLER_PLUG_HITCH", "HAS_COUPLER_SOCKET_FIFTH_WHEEL")) == {
        "fifth_wheel": SOCKET, "hitch": PLUG}


@pytest.mark.parametrize("defines", [
    {M},
    {M, HITCH},
    {M, HITCH, "HAS_COUPLER_SOCKET_FIFTH_WHEEL"},
    {M, HITCH_PLUG},
    {M, "HAS_COUPLER_PLUG_HITCH", "HAS_COUPLER_SOCKET_FIFTH_WHEEL"},
])
def test_row1_t2_a_machine_with_its_sides(defines):
    assert node_category(defines) == M


def test_row1_t2_a_remote_without_coupler():
    assert node_category({"IS_REMOTE"}) == "IS_REMOTE"


@pytest.mark.parametrize("defines,match", [
    (set(), "no category"),
    ({M, "IS_REMOTE"}, "several categories"),
    ({M, HITCH, HITCH_PLUG}, "both sides"),                              # a node never carries itself
    ({M, HITCH_PLUG, "HAS_COUPLER_PLUG_FIFTH_WHEEL"}, "several plugs"),  # carried by one at a time
    ({"IS_REMOTE", HITCH}, "IS_REMOTE"),                                 # a remote is never coupled
    ({M, "COMBUS_PEER_TEST_TRAILER"}, "COMBUS_PEER_"),                   # removed, never ignored
])
def test_row2_row3_refused_categories_and_sides(defines, match):
    with pytest.raises(LinkError, match=match):
        node_category(defines)


# =============================================================================
# T3: full contract and injection
# =============================================================================

def test_row1_the_frame_is_the_full_coupler(hitch, tractor):
    link = compute_coupler_link("test_coupler", hitch, inject_coupler_channels(tractor, hitch, "test_coupler", V), V)
    assert [c.id for c in link.channels if c.type == "analog"] == ["TEST_AXLE_LOAD", "TEST_BRAKE_BUS",
                                                                   "TEST_TILT_ANGLE"]
    assert [c.id for c in link.channels if c.type == "digital"] == ["TEST_COUPLED_FAIL", "TEST_LIGHTS"]


def test_row4_injection_on_the_vehicle_is_as_written(hitch, tractor):
    injected = {c.id: c for c in inject_coupler_channels(tractor, hitch, "test_coupler", V)}
    axle = injected["TEST_AXLE_LOAD"]                     # unknown to the tractor
    assert axle.direction == frozenset({"downlink"})      # as written: the vehicle receives it
    assert axle.value == "CbusNeutral"
    assert axle.scope == "BASE"
    assert injected["TEST_BRAKE_BUS"].passthrough         # known: the declaration of the tractor is kept


def test_row4_injection_on_the_attachment_is_its_side_of_the_plug():
    coupler = _canon(_ch("RELAYED", "downlink_pt"), _ch("OWN", "uplink"), _ch("BACK", "downlink"))
    injected = {c.id: c for c in inject_coupler_channels([], coupler, "test_coupler", A)}
    assert injected["RELAYED"].direction == frozenset({"downlink"})   # the vehicle relays it: received
    assert not injected["RELAYED"].passthrough
    assert injected["OWN"].direction == frozenset({"downlink"})       # the vehicle sends it: received
    assert injected["BACK"].direction == frozenset({"uplink"})        # the vehicle receives it: sent


def test_row4_injection_mirrors_or_directions():
    coupler = _canon(_ch("F", "uplink_or", type_="digital"), _ch("G", "both_or", type_="digital"))
    injected = {c.id: c for c in inject_coupler_channels([], coupler, "test_coupler", A)}
    assert injected["F"].direction_or == frozenset({"downlink"})
    assert injected["G"].direction_or == frozenset({"uplink", "downlink"})


def test_row4_injected_channel_reaches_the_combus_of_the_node(tmp_path, tractor):
    generate(tractor, _ctx(M, "MACHINE_TYPE_TEST_TRACTOR", HITCH), tmp_path)
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "TEST_AXLE_LOAD" in ids


def test_t3_without_coupler_nothing_is_injected(tmp_path, tractor):
    generate(tractor, _ctx(M, "MACHINE_TYPE_TEST_TRACTOR"), tmp_path)
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "TEST_AXLE_LOAD" not in ids
    assert (tmp_path / "combus_peer_links.h").is_file()   # always written, empty


# =============================================================================
# T4: agreement with the machine type
# =============================================================================

def test_row5_same_name_different_type_is_an_error():
    with pytest.raises(LinkError, match="X"):
        _link([_ch("X", "uplink")], [_ch("X", "uplink", type_="digital")])


def test_row6_same_name_different_value_is_an_error():
    with pytest.raises(LinkError, match="X"):
        _link([_ch("X", "uplink")], [_ch("X", "uplink", value="CbusMaxVal")])


def test_row6_same_name_different_zero_at_is_an_error():
    with pytest.raises(LinkError, match="zeroAt"):
        _link([_ch("X", "uplink", zero_at="neutral")], [_ch("X", "uplink")])


def test_row7_the_vehicle_relays_a_channel_of_the_coupler():
    assert _flow(_link([_ch("BRAKE", "downlink_pt")], [_ch("BRAKE", "downlink_pt")]), "BRAKE") == "to_peer"


def test_row7_the_attachment_receives_what_the_vehicle_relays():
    assert _flow(_link([_ch("BRAKE", "downlink_pt")], [_ch("BRAKE", "downlink")], nature=A), "BRAKE") == "from_peer"


def test_row8_two_emitters_is_an_error():
    """The coupler says the attachment sends TILT; the vehicle sends it too."""
    with pytest.raises(LinkError, match="TILT"):
        _link([_ch("TILT", "downlink")], [_ch("TILT", "uplink")])


def test_row8b_a_channel_that_does_not_flow_is_an_error():
    """The coupler says the vehicle relays BRAKE; the vehicle receives it but does not relay it."""
    with pytest.raises(LinkError, match="BRAKE"):
        _link([_ch("BRAKE", "downlink_pt")], [_ch("BRAKE", "downlink")])


def test_row4_flow_of_the_test_link(hitch, tractor):
    link = compute_coupler_link("test_coupler", hitch, inject_coupler_channels(tractor, hitch, "test_coupler", V), V)
    assert {c.id: c.flow for c in link.channels} == {
        "TEST_BRAKE_BUS": "to_peer", "TEST_TILT_ANGLE": "from_peer", "TEST_AXLE_LOAD": "from_peer",
        "TEST_COUPLED_FAIL": "or", "TEST_LIGHTS": "to_peer"}


# =============================================================================
# T5 / T6: MD5 of the coupler and roles
# =============================================================================

def test_row9_two_machines_carrying_the_coupler_get_the_same_md5(hitch, tractor):
    other_vehicle = _canon(_ch("TEST_BRAKE_BUS", "uplink"), _ch("TEST_TILT_ANGLE", "downlink"),
                           _ch("UNRELATED", "downlink"))
    a = compute_coupler_link("test_coupler", hitch, inject_coupler_channels(tractor, hitch, "test_coupler", V), V)
    b = compute_coupler_link("test_coupler", hitch, inject_coupler_channels(other_vehicle, hitch, "test_coupler", V), V)
    assert link_md5(a) == link_md5(b)


def test_row10_a_channel_added_to_the_coupler_changes_the_md5():
    assert link_md5(_link([_ch("X", "downlink")], [])) != \
        link_md5(_link([_ch("X", "downlink"), _ch("Y", "uplink")], []))


def test_row11_each_side_expects_the_md5_of_the_other_role(hitch, tractor, trailer):
    v = compute_coupler_link("test_coupler", hitch, inject_coupler_channels(tractor, hitch, "test_coupler", V), V)
    a = compute_coupler_link("test_coupler", hitch, inject_coupler_channels(trailer, hitch, "test_coupler", A), A)
    assert link_md5(v) != link_md5(a)                               # two vehicles would not agree
    assert link_md5(v, peer=True) == link_md5(a)                    # the vehicle expects the attachment
    assert link_md5(a, peer=True) == link_md5(v)                    # and the other way round
    assert link_md5(v, stable=True, peer=True) == link_md5(a, stable=True)


def test_t5_theme_does_not_change_order_nor_md5():
    """Kept from step 3.1a."""
    def make(theme_a, theme_b):
        a, b = _ch("AA", "uplink"), _ch("BB", "uplink")
        a["theme"], b["theme"] = theme_a, theme_b
        return _link([a, b], [])
    l1, l2 = make("core", "light"), make("light", "core")
    assert [c.id for c in l1.channels] == [c.id for c in l2.channels] == ["AA", "BB"]
    assert link_md5(l1) == link_md5(l2)


def test_t5_candidates_leave_the_stable_md5():
    with_candidate = _link([_ch("ZZ", "downlink"), _ch("AA", "downlink", candidate=True)], [])
    without = _link([_ch("ZZ", "downlink")], [])
    assert [c.id for c in with_candidate.channels] == ["ZZ", "AA_NEXT"]
    assert link_md5(with_candidate, stable=True) == link_md5(without, stable=True)
    assert link_md5(with_candidate) != link_md5(without)


# =============================================================================
# Row 13: one source per channel on the vehicle (unchanged)
# =============================================================================

def test_row13_two_couplers_sources_of_the_same_channel():
    links = [_link([_ch("TILT", "downlink")], [_ch("TILT", "downlink")], name="r"),
             _link([_ch("TILT", "downlink")], [_ch("TILT", "downlink")], name="d")]
    with pytest.raises(LinkError, match="TILT"):
        check_single_source(links)


def test_row13_the_vehicle_relays_from_one_attachment_to_the_other():
    """Kept from step 3.2b: one coupler is the source, the vehicle passes it on to the other."""
    lr = _link([_ch("TILT", "downlink")], [_ch("TILT", "downlink_pt")], name="r")      # r sends it
    ld = _link([_ch("TILT", "downlink_pt")], [_ch("TILT", "downlink_pt")], name="d")   # relayed to d
    check_single_source([lr, ld])
    assert (_flow(lr, "TILT"), _flow(ld, "TILT")) == ("from_peer", "to_peer")


def test_row13_both_or_is_the_merge_not_two_sources():
    """Kept from step 3.2b."""
    links = [_link([_ch("F", "both_or", type_="digital")], [_ch("F", "both_or", type_="digital")], name=n)
             for n in ("r", "d")]
    check_single_source(links)


def test_row13_through_the_generator(tmp_path, monkeypatch):
    couplers = {"test_r": _canon(_ch("TILT", "downlink")), "test_d": _canon(_ch("TILT", "downlink"))}
    monkeypatch.setattr(links_module, "load_coupler_channels", lambda root, name: couplers[name])
    with pytest.raises(GeneratorError, match="TILT"):
        generate(_canon(_ch("TILT", "downlink")),
                 _ctx(M, "MACHINE_TYPE_TEST_TRACTOR", "HAS_COUPLER_SOCKET_TEST_R", "HAS_COUPLER_SOCKET_TEST_D"),
                 tmp_path)


# =============================================================================
# T7: generated header and build log
# =============================================================================

def test_row1_t7_generated_header_and_build_log(tmp_path, tractor, capsys):
    generate(tractor, _ctx(M, "MACHINE_TYPE_TEST_TRACTOR", HITCH), tmp_path)
    out = capsys.readouterr().out
    text = (tmp_path / "combus_peer_links.h").read_text(encoding="utf-8")
    assert "namespace TestCoupler {" in text
    assert ("static_cast<uint8_t>(AnalogComBusID::TEST_AXLE_LOAD), "
            "static_cast<uint8_t>(AnalogComBusID::TEST_BRAKE_BUS), "
            "static_cast<uint8_t>(AnalogComBusID::TEST_TILT_ANGLE)") in text
    assert '.coupler   = "test_coupler",' in text
    assert ".role      = ComLinkRole::SOCKET," in text      # step 3.4c: the role of this node
    assert ".announced = { .stable = kAnnouncedMd5, .next = kAnnouncedMd5Next }," in text
    assert ".expected  = { .stable = kExpectedMd5,  .next = kExpectedMd5Next }," in text
    assert ".ids = kAnalog,  .rx = kAnalogRx }," in text
    assert "[combus_builder] set LINK test_coupler (socket)  md5=" in out
    assert "A3+D2" in out
