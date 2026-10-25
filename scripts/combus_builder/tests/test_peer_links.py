"""Links between nodes: computed channel set, wire order, tables, MD5 (link rework, step 3.1a).

Rules validated by the owner (2026-10-25), on two fictitious types
(src/core/config/machines/test_tractor/, test_trailer/):
  - a node declares the TYPES it accepts as peers: -D COMBUS_PEER_<TYPE>;
  - the channel set of a link is computed, never written: the BASE (next)
    channels of the same name in both types, keeping only those that flow
    (one side emits, the other receives); two downlink: "dead channel"
    warning; two uplink: error unless *_or (digital only);
  - a `downlink_pt` side emits on the link (it relays what it received);
  - common frame sorted canonically on what both types share: analog then
    digital, stable before candidate, then the name (NOT the theme);
  - same name = same type and same default value, else build error;
  - MD5 of the link symmetric: both nodes, built apart, get the same one;
  - each node has a table from its ComBus to the common frame.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.generator import generate
from scripts.combus_builder.links import (
    LinkError,
    compute_link_set,
    link_md5,
    load_type_base_channels,
    peer_types,
)

SRC = REPO_ROOT / "src"


def _ctx(*flags):
    return BuildContext(buildroot=SRC, defines=frozenset(flags), defines_with_value={},
                        cppdefines_source="override")


def _ch(id_, type_="analog", direction="downlink", theme="core", value=None, candidate=None):
    raw = {"id": id_, "infoName": id_.lower(), "type": type_, "scope": "BASE",
           "theme": theme, "direction": direction}
    if value is not None:
        raw["value"] = value
    if candidate is not None:
        raw["candidate"] = candidate
    return raw


def _canon(*raws):
    parsed = [(Path(f"ch{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


@pytest.fixture(scope="module")
def tractor():
    return load_type_base_channels(SRC, "test_tractor")


@pytest.fixture(scope="module")
def trailer():
    return load_type_base_channels(SRC, "test_trailer")


@pytest.fixture(scope="module")
def link(tractor, trailer):
    return compute_link_set("test_tractor", tractor, "test_trailer", trailer)


# =============================================================================
# Peers
# =============================================================================

def test_peer_types_from_flags():
    assert peer_types(_ctx("COMBUS_PEER_TEST_TRAILER", "MACHINE_TYPE_TEST_TRACTOR")) == ["test_trailer"]


def test_unknown_peer_type_is_an_error():
    with pytest.raises(LinkError, match="no_such_type"):
        load_type_base_channels(SRC, "no_such_type")


# =============================================================================
# Rows 1 to 7: the link tractor - trailer
# =============================================================================

def test_row1_link_set_and_wire_order(link):
    analog = [c.id for c in link.channels if c.type == "analog"]
    digital = [c.id for c in link.channels if c.type == "digital"]
    assert analog == ["TEST_BRAKE_BUS", "TEST_TILT_ANGLE"]
    assert digital == ["TEST_COUPLED_FAULT", "TEST_LIGHTS"]


def test_row2_row3_channels_known_on_one_side_only_are_absent(link):
    ids = {c.id for c in link.channels}
    assert "TEST_THROTTLE_BUS" not in ids      # the trailer does not know it (and it is not relayed)
    assert "TEST_AXLE_LOAD" not in ids         # the tractor does not know it


def test_row4_flow_on_the_link(link):
    flow = {c.id: c.flow for c in link.channels}
    assert flow == {"TEST_BRAKE_BUS": "to_peer",       # downlink_pt: the tractor relays it
                    "TEST_TILT_ANGLE": "from_peer",
                    "TEST_COUPLED_FAULT": "or",
                    "TEST_LIGHTS": "to_peer"}


def test_row6_md5_is_symmetric(tractor, trailer):
    a = compute_link_set("test_tractor", tractor, "test_trailer", trailer)
    b = compute_link_set("test_trailer", trailer, "test_tractor", tractor)
    assert link_md5(a) == link_md5(b)
    assert [c.id for c in a.channels] == [c.id for c in b.channels]


def test_row5_row7_generated_table_and_build_log(tmp_path, tractor, capsys):
    generate(tractor, _ctx("MACHINE_TYPE_TEST_TRACTOR", "COMBUS_PEER_TEST_TRAILER"), tmp_path)
    out = capsys.readouterr().out
    text = (tmp_path / "combus_peer_links.h").read_text(encoding="utf-8")
    assert "AnalogComBusID::TEST_BRAKE_BUS, AnalogComBusID::TEST_TILT_ANGLE" in text
    assert "DigitalComBusID::TEST_COUPLED_FAULT, DigitalComBusID::TEST_LIGHTS" in text
    assert "[combus_builder] set LINK test_trailer  md5=" in out
    assert "A2+D2" in out


def test_no_peer_still_writes_an_empty_links_header(tmp_path, tractor):
    generate(tractor, _ctx("MACHINE_TYPE_TEST_TRACTOR"), tmp_path)
    assert (tmp_path / "combus_peer_links.h").is_file()


# =============================================================================
# Rows 8 to 13: rules of the computation
# =============================================================================

def test_row8_same_name_different_type_is_an_error():
    a = _canon(_ch("X"))
    b = _canon(_ch("X", type_="digital"))
    with pytest.raises(LinkError, match="X"):
        compute_link_set("ta", a, "tb", b)


def test_row9_same_name_different_default_value_is_an_error():
    a = _canon(_ch("X", direction="downlink_pt", value="CbusMinVal"))
    b = _canon(_ch("X", value="CbusMaxVal"))
    with pytest.raises(LinkError, match="X"):
        compute_link_set("ta", a, "tb", b)


def test_row10_two_receivers_is_a_dead_channel():
    a = _canon(_ch("HORN"))
    b = _canon(_ch("HORN"))
    link = compute_link_set("ta", a, "tb", b)
    assert link.channels == []
    assert any("HORN" in w for w in link.warnings)


def test_row11_two_emitters_is_an_error():
    a = _canon(_ch("X", direction="uplink"))
    b = _canon(_ch("X", direction="uplink"))
    with pytest.raises(LinkError, match="X"):
        compute_link_set("ta", a, "tb", b)


def test_row12_theme_does_not_change_order_nor_md5():
    a1 = _canon(_ch("AA", direction="uplink", theme="core"), _ch("BB", direction="uplink", theme="light"))
    a2 = _canon(_ch("AA", direction="uplink", theme="light"), _ch("BB", direction="uplink", theme="core"))
    b = _canon(_ch("AA"), _ch("BB"))
    l1, l2 = compute_link_set("ta", a1, "tb", b), compute_link_set("ta", a2, "tb", b)
    assert [c.id for c in l1.channels] == [c.id for c in l2.channels] == ["AA", "BB"]
    assert link_md5(l1) == link_md5(l2)


def test_row13_candidates_follow_the_stable_channels_and_leave_the_stable_md5():
    a = _canon(_ch("ZZ", direction="uplink"), _ch("AA", direction="uplink", candidate=True))
    b = _canon(_ch("ZZ"), _ch("AA", candidate=True))
    link = compute_link_set("ta", a, "tb", b)
    assert [c.id for c in link.channels] == ["ZZ", "AA_NEXT"]
    only_stable = compute_link_set("ta", _canon(_ch("ZZ", direction="uplink")), "tb", _canon(_ch("ZZ")))
    assert link_md5(link, stable=True) == link_md5(only_stable)
    assert link_md5(link) != link_md5(only_stable)


def test_two_relays_facing_each_other_is_an_error():
    """Both sides downlink_pt: which one is the source? Refused until the graph check of 3.2."""
    a = _canon(_ch("X", direction="downlink_pt"))
    b = _canon(_ch("X", direction="downlink_pt"))
    with pytest.raises(LinkError, match="X"):
        compute_link_set("ta", a, "tb", b)
