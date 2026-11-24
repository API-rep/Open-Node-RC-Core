"""The channels of the PS4 source (src/core/system/input/ps4/ps4.cb).

Rules (doc/WIP - Local remote.md §7, R3):
  - BOARD, direction none, theme `source`, requires [HAS_REMOTE_PS4]: they
    exist on the board of the module only and never travel;
  - safe value: neutral for an axis (a trigger at rest is the neutral), low for
    a button;
  - they are not inputs of the type: no *_STICK / *_BTN name, never on the
    remote link (links.is_remote_input()).
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import VALID_THEMES, canonize  # noqa: E402
from scripts.combus_builder.links import is_remote_input  # noqa: E402
from scripts.combus_builder.parser import discover_and_parse  # noqa: E402

ANALOG = ["PS4_LX", "PS4_LY", "PS4_RX", "PS4_RY", "PS4_L2", "PS4_R2"]
DIGITAL = ["PS4_SQUARE", "PS4_CROSS", "PS4_CIRCLE", "PS4_TRIANGLE", "PS4_L1", "PS4_R1",
           "PS4_L2_PRESSED", "PS4_R2_PRESSED", "PS4_UP", "PS4_RIGHT", "PS4_DOWN", "PS4_LEFT",
           "PS4_SHARE", "PS4_OPTIONS", "PS4_PS", "PS4_TOUCHPAD", "PS4_L3", "PS4_R3"]


@pytest.fixture(scope="module")
def ps4():
    _, _, parsed = discover_and_parse(project_root=REPO_ROOT, machine_type="dumper_truck")
    defs = canonize(parsed).canonical_definitions
    return {ch.id: ch for ch in defs if ch.id.startswith("PS4_")}


def test_the_theme_source_exists():
    assert "source" in VALID_THEMES


def test_the_channels_of_the_source_are_all_there(ps4):
    assert sorted(ps4) == sorted(ANALOG + DIGITAL)


@pytest.mark.parametrize("cid", ANALOG + DIGITAL)
def test_a_channel_of_the_source_is_board_and_never_travels(ps4, cid):
    ch = ps4[cid]
    assert (ch.scope, ch.theme, set(ch.requires), ch.direction | ch.direction_or) == \
        ("BOARD", "source", {"HAS_REMOTE_PS4"}, frozenset())


@pytest.mark.parametrize("cid", ANALOG)
def test_an_axis_is_safe_at_neutral(ps4, cid):
    assert (ps4[cid].type, ps4[cid].raw["value"]) == ("analog", "CbusNeutral")


@pytest.mark.parametrize("cid", DIGITAL)
def test_a_button_is_safe_released(ps4, cid):
    assert (ps4[cid].type, ps4[cid].raw["value"]) == ("digital", "low")


def test_no_channel_of_the_source_is_an_input_of_the_type(ps4):
    named = sorted(cid for cid in ps4 if cid.endswith(("_STICK", "_BTN")))
    remote = sorted(cid for cid, ch in ps4.items() if is_remote_input(ch))
    assert (named, remote) == ([], [])
