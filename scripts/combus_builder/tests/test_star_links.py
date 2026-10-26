"""Links in the star "vehicle + 1" (link rework, step 3.2b).

Rules validated by the owner (2026-10-26):
  - on an attachment, `_pt` only means "redistribute in my node": on the link
    it is a plain `downlink`, the attachment receives;
  - on the vehicle, `_pt` means "pass it on", link by link: where the
    attachment emits the channel, that link is the source and the vehicle
    receives; where the attachment receives it, the vehicle emits;
  - one source per channel on the vehicle: among its links, one at most is the
    source of a channel (both_or excepted, it is the merge), else build error;
  - the nature is in the MD5 of the link ("the vehicle of this link is X"):
    two vehicles, or two attachments, linked by mistake get different MD5s and
    the handshake refuses.

T = tractor (IS_VEHICLE), R = trailer and D = dolly (IS_ATTACHMENT).
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
    compute_link_set,
    link_md5,
)

V, A = "IS_VEHICLE", "IS_ATTACHMENT"


def _ch(id_, direction, type_="analog"):
    return {"id": id_, "infoName": id_.lower(), "type": type_, "scope": "BASE", "theme": "core",
            "direction": direction, "value": "low" if type_ == "digital" else "CbusNeutral"}


def _canon(*raws):
    parsed = [(Path(f"ch{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


def _flow(link, cid):
    return {c.id: c.flow for c in link.channels}.get(cid)


# =============================================================================
# One link
# =============================================================================

def test_row1_relay_on_both_sides_the_vehicle_emits():
    link = compute_link_set("t", _canon(_ch("BRAKE", "downlink_pt")), "r", _canon(_ch("BRAKE", "downlink_pt")), V)
    assert _flow(link, "BRAKE") == "to_peer"


def test_row2_the_attachment_side_gets_the_same_link_and_md5():
    t, r = _canon(_ch("BRAKE", "downlink_pt")), _canon(_ch("BRAKE", "downlink_pt"))
    from_t = compute_link_set("t", t, "r", r, V)
    from_r = compute_link_set("r", r, "t", t, A)
    assert _flow(from_r, "BRAKE") == "from_peer"
    assert [c.id for c in from_t.channels] == [c.id for c in from_r.channels]
    assert link_md5(from_t) == link_md5(from_r)
    assert link_md5(from_t, stable=True) == link_md5(from_r, stable=True)


@pytest.mark.parametrize("nature", [V, A])
def test_row3_two_nodes_of_the_same_nature_do_not_agree_on_the_md5(nature):
    x, y = _canon(_ch("BRAKE", "downlink_pt")), _canon(_ch("BRAKE", "downlink_pt"))
    assert link_md5(compute_link_set("x", x, "y", y, nature)) != link_md5(compute_link_set("y", y, "x", x, nature))


def test_row4_the_attachment_emits_the_link_is_the_source_of_the_vehicle():
    link = compute_link_set("t", _canon(_ch("TILT", "downlink_pt")), "r", _canon(_ch("TILT", "uplink")), V)
    assert _flow(link, "TILT") == "from_peer"


def test_row9_two_plain_receivers_is_still_a_dead_channel():
    link = compute_link_set("t", _canon(_ch("BRAKE", "downlink")), "r", _canon(_ch("BRAKE", "downlink")), V)
    assert link.channels == []
    assert any("BRAKE" in w for w in link.warnings)


# =============================================================================
# Several links on the vehicle
# =============================================================================

def _two_links(t_dir, r_dir, d_dir, type_="analog"):
    t = _canon(_ch("TILT", t_dir, type_))
    return [compute_link_set("t", t, "r", _canon(_ch("TILT", r_dir, type_)), V),
            compute_link_set("t", t, "d", _canon(_ch("TILT", d_dir, type_)), V)]


def test_row5_the_vehicle_relays_from_one_attachment_to_the_other():
    lr, ld = _two_links("downlink_pt", "uplink", "downlink")
    check_single_source([lr, ld])
    assert (_flow(lr, "TILT"), _flow(ld, "TILT")) == ("from_peer", "to_peer")


@pytest.mark.parametrize("t_dir", ["downlink_pt", "downlink"])
def test_row6_row7_two_sources_on_the_vehicle(t_dir):
    with pytest.raises(LinkError, match="TILT"):
        check_single_source(_two_links(t_dir, "uplink", "uplink"))


def test_row8_both_or_is_the_merge_not_two_sources():
    check_single_source(_two_links("both_or", "both_or", "both_or", type_="digital"))


def test_row6_through_the_generator(tmp_path, monkeypatch):
    """The build itself breaks when two peer types are sources of the same channel."""
    peers = {"test_r": _canon(_ch("TILT", "uplink")), "test_d": _canon(_ch("TILT", "uplink"))}
    monkeypatch.setattr(links_module, "load_type_base_channels", lambda root, peer: peers[peer])
    ctx = BuildContext(buildroot=REPO_ROOT / "src", cppdefines_source="override", defines_with_value={},
                       defines=frozenset({"IS_VEHICLE", "MACHINE_TYPE_TEST_TRACTOR",
                                          "COMBUS_PEER_TEST_R", "COMBUS_PEER_TEST_D"}))
    with pytest.raises(GeneratorError, match="TILT"):
        generate(_canon(_ch("TILT", "downlink_pt")), ctx, tmp_path)
