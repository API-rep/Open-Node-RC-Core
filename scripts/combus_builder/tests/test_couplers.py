"""Couplers: the norm of a link between nodes (link rework, step 3.4a).

Rules validated by the owner (2026-10-31):
  - T1 a coupler is a folder src/core/config/couplers/<name>/ of ordinary
    BASE channels; directions are those of the attachment (uplink: from the
    attachment to the vehicle, downlink: from the vehicle to the attachment,
    both_or: shared fault); no `_pt` (relaying is the business of a node);
    `value` is the safe value;
  - T2 a node declares its couplers with -D COMBUS_COUPLER_<NAME>: 0 to n on
    a vehicle, exactly 1 on an attachment; IS_MACHINE / IS_REMOTE with a
    coupler is refused explicitly, never ignored; COMBUS_PEER_<TYPE> is
    removed (refused explicitly);
  - T3 full contract: the frame is every channel of the coupler; the ComBus
    of the coupler merges into the ComBus of the node: a channel of the
    coupler unknown to the machine type is injected at its safe value
    (mirrored direction on the vehicle);
  - T4 a channel of the same name in the machine type agrees on type, value
    and zeroAt; its direction must flow (attachment: as in the coupler;
    vehicle: mirrored, `_pt` allowed); a channel that would not flow is an
    error (no "dead channel" in a full contract);
  - T5 / T6 the MD5 of the link carries the coupler only, plus the role:
    each side announces MD5(coupler + its role) and expects MD5(coupler + the
    other role), so two vehicles (or two attachments) linked by mistake
    refuse each other;
  - T7 the generated header has one namespace per coupler;
  - T8 the names of the coupler are the norm (no renaming);
  - T9 the coupler files never enter the ComBus by discovery: they are read
    apart, and only for the couplers declared by the node.
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
    coupler_names,
    inject_coupler_channels,
    link_md5,
    load_coupler_channels,
    node_nature,
)
from scripts.combus_builder.parser import discover_config_files  # noqa: E402

SRC = REPO_ROOT / "src"
V, A = "IS_VEHICLE", "IS_ATTACHMENT"
HITCH = "COMBUS_COUPLER_TEST_HITCH"


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
    local = inject_coupler_channels(_canon(*local_raws) if local_raws else [], coupler, nature)
    return compute_coupler_link(name, coupler, local, nature)


@pytest.fixture(scope="module")
def hitch():
    return load_coupler_channels(SRC, "test_hitch")


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
    assert sorted(c.id for c in hitch) == ["TEST_AXLE_LOAD", "TEST_BRAKE_BUS", "TEST_COUPLED_FAIL",
                                           "TEST_LIGHTS", "TEST_TILT_ANGLE"]


def test_t1_unknown_coupler_is_an_error():
    with pytest.raises(LinkError, match="no_such_coupler"):
        load_coupler_channels(SRC, "no_such_coupler")


def test_row12_pt_in_a_coupler_is_an_error(tmp_path):
    folder = tmp_path / "core" / "config" / "couplers" / "bad"
    folder.mkdir(parents=True)
    (folder / "bad.cb").write_text(
        "channels:\n- id: X\n  infoName: x\n  type: analog\n  scope: BASE\n  theme: core\n"
        "  direction: downlink_pt\n  value: CbusNeutral\n", encoding="utf-8")
    with pytest.raises(LinkError, match="_pt"):
        load_coupler_channels(tmp_path, "bad")


def test_t1_a_coupler_channel_is_base_only(tmp_path):
    folder = tmp_path / "core" / "config" / "couplers" / "bad"
    folder.mkdir(parents=True)
    (folder / "bad.cb").write_text(
        "channels:\n- id: X\n  infoName: x\n  type: digital\n  scope: BOARD\n  theme: core\n"
        "  value: low\n", encoding="utf-8")
    with pytest.raises(LinkError, match="BASE"):
        load_coupler_channels(tmp_path, "bad")


@pytest.mark.parametrize("machine_type", [None, "test_tractor"])
def test_t9_discovery_never_loads_a_coupler(machine_type):
    files = discover_config_files(SRC, machine_type=machine_type)
    assert not [p for p in files if "couplers" in p.parts]


# =============================================================================
# T2: declaration and nature
# =============================================================================

def test_t2_couplers_from_flags():
    assert coupler_names(_ctx(HITCH, "COMBUS_COUPLER_FIFTH_WHEEL", "IS_VEHICLE")) == ["fifth_wheel", "test_hitch"]


@pytest.mark.parametrize("defines,nature", [
    ({V}, V),
    ({V, HITCH}, V),
    ({V, HITCH, "COMBUS_COUPLER_FIFTH_WHEEL"}, V),
    ({A, HITCH}, A),
    ({"IS_MACHINE"}, "IS_MACHINE"),
    ({"IS_REMOTE"}, "IS_REMOTE"),
])
def test_row1_t2_accepted_natures(defines, nature):
    assert node_nature(defines) == nature


@pytest.mark.parametrize("defines,match", [
    ({A}, "exactly one coupler"),                                        # row 2
    ({A, HITCH, "COMBUS_COUPLER_FIFTH_WHEEL"}, "exactly one coupler"),   # row 2
    ({"IS_MACHINE", HITCH}, "IS_MACHINE"),                               # row 3
    ({"IS_REMOTE", HITCH}, "IS_REMOTE"),                                 # row 3
    ({V, "COMBUS_PEER_TEST_TRAILER"}, "COMBUS_PEER_"),                   # row 14: removed, never ignored
])
def test_row2_row3_row14_refused_natures(defines, match):
    with pytest.raises(LinkError, match=match):
        node_nature(defines)


# =============================================================================
# T3: full contract and injection
# =============================================================================

def test_row1_the_frame_is_the_full_coupler(hitch, tractor):
    link = compute_coupler_link("test_hitch", hitch, inject_coupler_channels(tractor, hitch, V), V)
    assert [c.id for c in link.channels if c.type == "analog"] == ["TEST_AXLE_LOAD", "TEST_BRAKE_BUS",
                                                                   "TEST_TILT_ANGLE"]
    assert [c.id for c in link.channels if c.type == "digital"] == ["TEST_COUPLED_FAIL", "TEST_LIGHTS"]


def test_row4_injection_on_the_vehicle_is_mirrored(hitch, tractor):
    injected = {c.id: c for c in inject_coupler_channels(tractor, hitch, V)}
    axle = injected["TEST_AXLE_LOAD"]                     # unknown to the tractor
    assert axle.direction == frozenset({"downlink"})      # the attachment emits, the vehicle receives
    assert axle.value == "CbusNeutral"
    assert axle.scope == "BASE"
    assert injected["TEST_BRAKE_BUS"].passthrough         # known: the declaration of the tractor is kept


def test_row4_injection_on_the_attachment_keeps_the_coupler_direction():
    injected = {c.id: c for c in inject_coupler_channels([], _canon(_ch("X", "downlink"),
                                                                    _ch("Y", "uplink")), A)}
    assert injected["X"].direction == frozenset({"downlink"})
    assert injected["Y"].direction == frozenset({"uplink"})


def test_row4_injection_mirrors_or_directions():
    coupler = _canon(_ch("F", "uplink_or", type_="digital"), _ch("G", "both_or", type_="digital"))
    injected = {c.id: c for c in inject_coupler_channels([], coupler, V)}
    assert injected["F"].direction_or == frozenset({"downlink"})
    assert injected["G"].direction_or == frozenset({"uplink", "downlink"})


def test_row4_injected_channel_reaches_the_combus_of_the_node(tmp_path, tractor):
    generate(tractor, _ctx(V, "MACHINE_TYPE_TEST_TRACTOR", HITCH), tmp_path)
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "TEST_AXLE_LOAD" in ids


def test_t3_without_coupler_nothing_is_injected(tmp_path, tractor):
    generate(tractor, _ctx(V, "MACHINE_TYPE_TEST_TRACTOR"), tmp_path)
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "TEST_AXLE_LOAD" not in ids
    assert (tmp_path / "combus_peer_links.h").is_file()   # always written, empty


# =============================================================================
# T4: agreement with the machine type
# =============================================================================

def test_row5_same_name_different_type_is_an_error():
    with pytest.raises(LinkError, match="X"):
        _link([_ch("X", "downlink")], [_ch("X", "uplink", type_="digital")])


def test_row6_same_name_different_value_is_an_error():
    with pytest.raises(LinkError, match="X"):
        _link([_ch("X", "downlink")], [_ch("X", "uplink", value="CbusMaxVal")])


def test_row6_same_name_different_zero_at_is_an_error():
    with pytest.raises(LinkError, match="zeroAt"):
        _link([_ch("X", "downlink", zero_at="neutral")], [_ch("X", "uplink")])


def test_row7_the_vehicle_relays_a_channel_of_the_coupler():
    assert _flow(_link([_ch("BRAKE", "downlink")], [_ch("BRAKE", "downlink_pt")]), "BRAKE") == "to_peer"


def test_row7_the_attachment_receives_what_the_vehicle_relays():
    assert _flow(_link([_ch("BRAKE", "downlink")], [_ch("BRAKE", "downlink")], nature=A), "BRAKE") == "from_peer"


def test_row8_two_emitters_is_an_error():
    with pytest.raises(LinkError, match="TILT"):
        _link([_ch("TILT", "uplink")], [_ch("TILT", "uplink")])


def test_row8b_a_channel_that_does_not_flow_is_an_error():
    """The vehicle receives BRAKE but does not relay it: nobody emits it on the link."""
    with pytest.raises(LinkError, match="BRAKE"):
        _link([_ch("BRAKE", "downlink")], [_ch("BRAKE", "downlink")])


def test_row4_flow_of_the_test_link(hitch, tractor):
    link = compute_coupler_link("test_hitch", hitch, inject_coupler_channels(tractor, hitch, V), V)
    assert {c.id: c.flow for c in link.channels} == {
        "TEST_BRAKE_BUS": "to_peer", "TEST_TILT_ANGLE": "from_peer", "TEST_AXLE_LOAD": "from_peer",
        "TEST_COUPLED_FAIL": "or", "TEST_LIGHTS": "to_peer"}


# =============================================================================
# T5 / T6: MD5 of the coupler and roles
# =============================================================================

def test_row9_two_machines_carrying_the_coupler_get_the_same_md5(hitch, tractor):
    other_vehicle = _canon(_ch("TEST_BRAKE_BUS", "uplink"), _ch("TEST_TILT_ANGLE", "downlink"),
                           _ch("UNRELATED", "downlink"))
    a = compute_coupler_link("test_hitch", hitch, inject_coupler_channels(tractor, hitch, V), V)
    b = compute_coupler_link("test_hitch", hitch, inject_coupler_channels(other_vehicle, hitch, V), V)
    assert link_md5(a) == link_md5(b)


def test_row10_a_channel_added_to_the_coupler_changes_the_md5():
    assert link_md5(_link([_ch("X", "downlink")], [])) != \
        link_md5(_link([_ch("X", "downlink"), _ch("Y", "uplink")], []))


def test_row11_each_side_expects_the_md5_of_the_other_role(hitch, tractor, trailer):
    v = compute_coupler_link("test_hitch", hitch, inject_coupler_channels(tractor, hitch, V), V)
    a = compute_coupler_link("test_hitch", hitch, inject_coupler_channels(trailer, hitch, A), A)
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
    links = [_link([_ch("TILT", "uplink")], [_ch("TILT", "downlink")], name="r"),
             _link([_ch("TILT", "uplink")], [_ch("TILT", "downlink")], name="d")]
    with pytest.raises(LinkError, match="TILT"):
        check_single_source(links)


def test_row13_the_vehicle_relays_from_one_attachment_to_the_other():
    """Kept from step 3.2b: one coupler is the source, the vehicle passes it on to the other."""
    lr = _link([_ch("TILT", "uplink")], [_ch("TILT", "downlink_pt")], name="r")
    ld = _link([_ch("TILT", "downlink")], [_ch("TILT", "downlink_pt")], name="d")
    check_single_source([lr, ld])
    assert (_flow(lr, "TILT"), _flow(ld, "TILT")) == ("from_peer", "to_peer")


def test_row13_both_or_is_the_merge_not_two_sources():
    """Kept from step 3.2b."""
    links = [_link([_ch("F", "both_or", type_="digital")], [_ch("F", "both_or", type_="digital")], name=n)
             for n in ("r", "d")]
    check_single_source(links)


def test_row13_through_the_generator(tmp_path, monkeypatch):
    couplers = {"test_r": _canon(_ch("TILT", "uplink")), "test_d": _canon(_ch("TILT", "uplink"))}
    monkeypatch.setattr(links_module, "load_coupler_channels", lambda root, name: couplers[name])
    with pytest.raises(GeneratorError, match="TILT"):
        generate(_canon(_ch("TILT", "downlink")),
                 _ctx(V, "MACHINE_TYPE_TEST_TRACTOR", "COMBUS_COUPLER_TEST_R", "COMBUS_COUPLER_TEST_D"),
                 tmp_path)


# =============================================================================
# T7: generated header and build log
# =============================================================================

def test_row1_t7_generated_header_and_build_log(tmp_path, tractor, capsys):
    generate(tractor, _ctx(V, "MACHINE_TYPE_TEST_TRACTOR", HITCH), tmp_path)
    out = capsys.readouterr().out
    text = (tmp_path / "combus_peer_links.h").read_text(encoding="utf-8")
    assert "namespace TestHitch {" in text
    assert ("static_cast<uint8_t>(AnalogComBusID::TEST_AXLE_LOAD), "
            "static_cast<uint8_t>(AnalogComBusID::TEST_BRAKE_BUS), "
            "static_cast<uint8_t>(AnalogComBusID::TEST_TILT_ANGLE)") in text
    assert '.coupler   = "test_hitch",' in text
    assert ".announced = { .stable = kAnnouncedMd5, .next = kAnnouncedMd5Next }," in text
    assert ".expected  = { .stable = kExpectedMd5,  .next = kExpectedMd5Next }," in text
    assert ".ids = kAnalog,  .rx = kAnalogRx }," in text
    assert "[combus_builder] set LINK test_hitch (vehicle)  md5=" in out
    assert "A3+D2" in out
