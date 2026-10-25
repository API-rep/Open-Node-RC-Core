"""Direction of a channel inside the node, and `downlink_pt` (link rework, step 2.4).

Rules validated by the owner (2026-10-25):
  - the direction of a BASE `.cb` is the direction of the NODE (between
    nodes); inside the node, the source of a BASE channel is the board that
    establishes the connection with another node (a flag of
    INTER_NODE_FLAGS, today INPUT_PS4_DS4_BT);
  - `downlink_pt` (BASE only): the connected board redistributes it in the
    node (node direction uplink), the other boards receive it (downlink);
  - `downlink` without `_pt`: the connected board uses it alone (node
    direction none everywhere); `uplink`, `both`: none in the node;
    `both_or`: unchanged (the merge of step 1.1b relays it);
  - SYSTEM / BOARD: the node direction is the direction (SYSTEM diffusion);
  - `both_pt` is refused (several sources by nature), `_pt` on SYSTEM too
    (SYSTEM is already diffused);
  - `_pt` does not change the MD5 (same .cb on every board; the relay
    between nodes comes in 3.2);
  - two boards of a node connected to other nodes: build error until 3.1.
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
from scripts.combus_builder.canon.channels import ChannelValidationError
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.generator import (
    GeneratorError,
    check_single_connected_board,
    generate,
    node_direction,
    select_views,
)
from scripts.combus_builder.md5 import compute_view_hash


def _ch(id_="THROTTLE", scope="BASE", type_="analog", direction="downlink_pt"):
    return {"id": id_, "infoName": id_.lower(), "type": type_, "scope": scope,
            "theme": "core", "direction": direction}


def _canon(*raws):
    parsed = [(Path(f"ch{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


def _ctx(*flags):
    return BuildContext(buildroot=Path("."), defines=frozenset(flags), defines_with_value={},
                        cppdefines_source="override")


UP, DOWN, BOTH, NONE = frozenset({"uplink"}), frozenset({"downlink"}), frozenset({"uplink", "downlink"}), frozenset()


# =============================================================================
# Declaration
# =============================================================================

def test_downlink_pt_is_a_downlink_that_is_redistributed():
    (ch,) = _canon(_ch())
    assert ch.direction == DOWN
    assert ch.passthrough is True


def test_plain_downlink_is_not_redistributed():
    (ch,) = _canon(_ch(direction="downlink"))
    assert ch.passthrough is False


def test_both_pt_is_refused():
    with pytest.raises(ChannelValidationError, match="both_pt"):
        _canon(_ch(direction="both_pt"))


def test_pt_on_a_system_channel_is_refused():
    with pytest.raises(ChannelValidationError, match="_pt"):
        _canon(_ch(scope="SYSTEM"))


# =============================================================================
# Direction inside the node
# =============================================================================

@pytest.mark.parametrize("scope,type_,token,connected,expected", [
    ("BASE",   "analog",  "downlink_pt", True,  UP),     # the connected board redistributes
    ("BASE",   "analog",  "downlink_pt", False, DOWN),   # the other boards receive
    ("BASE",   "analog",  "downlink",    True,  NONE),   # the connected board uses it alone
    ("BASE",   "analog",  "downlink",    False, NONE),
    ("BASE",   "analog",  "uplink",      True,  NONE),   # produced by the connected board, sent out
    ("BASE",   "analog",  "uplink",      False, NONE),
    ("BASE",   "analog",  "both",        True,  NONE),
    ("BASE",   "digital", "both_or",     False, BOTH),   # the 1.1b merge relays it
    ("BASE",   "analog",  "none",        False, NONE),
    ("SYSTEM", "analog",  "uplink",      False, UP),     # SYSTEM: the diffused direction
    ("SYSTEM", "analog",  "downlink",    True,  DOWN),
    ("SYSTEM", "analog",  "none",        False, NONE),
])
def test_node_direction(scope, type_, token, connected, expected):
    (ch,) = _canon(_ch(scope=scope, type_=type_, direction=token))
    assert node_direction(ch, connected) == expected


def test_generated_array_carries_the_node_direction(tmp_path):
    chs = _canon(_ch("THROTTLE"), _ch("LIGHTS", type_="digital", direction="downlink"))
    generate(chs, _ctx(), tmp_path)
    text = (tmp_path / "combus.cpp").read_text(encoding="utf-8")
    assert ".flow = { .betweenNodes = Direction::Downlink, .betweenBoards = Direction::Downlink } }, // THROTTLE" in text
    assert ".betweenBoards = Direction::None }, .bothOr = { .enabled = false } }, // LIGHTS" in text


def test_connected_board_redistributes(tmp_path):
    generate(_canon(_ch("THROTTLE")), _ctx("INPUT_PS4_DS4_BT"), tmp_path)
    text = (tmp_path / "combus.cpp").read_text(encoding="utf-8")
    assert ".flow = { .betweenNodes = Direction::Downlink, .betweenBoards = Direction::Uplink } }, // THROTTLE" in text


def test_pt_does_not_change_the_md5():
    plain = select_views(_canon(_ch(direction="downlink")))
    pt = select_views(_canon(_ch(direction="downlink_pt")))
    for name in ("base", "system", "full"):
        assert compute_view_hash(getattr(plain, name)).md5_hex == compute_view_hash(getattr(pt, name)).md5_hex


# =============================================================================
# One connected board per node
# =============================================================================

def test_one_connected_board_is_accepted():
    check_single_connected_board({None: _ctx("INPUT_PS4_DS4_BT"),          # the mother env is not a board
                                  "mainboard": _ctx("INPUT_PS4_DS4_BT"),
                                  "extboard": _ctx()})


def test_two_connected_boards_are_refused():
    with pytest.raises(GeneratorError, match="mainboard.*extboard|extboard.*mainboard"):
        check_single_connected_board({"mainboard": _ctx("INPUT_PS4_DS4_BT"),
                                      "extboard": _ctx("INPUT_PS4_DS4_BT")})
