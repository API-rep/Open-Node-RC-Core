"""The dumper_truck channel set after the recadrage of 2026-10-25 (link rework, before step 2.4).

Rules validated by the owner:
  - BASE holds the standard command buses only, the ones a remote sends:
    no BASE channel with direction `none` remains;
  - the remote sends the intent (the button), the machine computes the state:
    KEY_BTN, DIRECT_DRIVE_BTN, CRUISE_TOGGLE_BTN are BASE downlink;
    KEY_ACTIVE, DIRECT_DRIVE, CRUISE_ACTIVE are SYSTEM uplink;
  - the values computed by the processor chains are SYSTEM uplink:
    ESC_SPEED_BUS, DRIVE_STATE_BUS, ESC_RPM_BUS;
  - GEAR_SHIFTING is removed (no reader);
  - a .cb lives next to what is responsible for it: the SYSTEM channels
    computed by the chains of a machine type live under
    core/config/machines/<type>/processors/, not in the generic core.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize  # noqa: E402
from scripts.combus_builder.parser import discover_and_parse  # noqa: E402


@pytest.fixture(scope="module")
def channels():
    _, _, parsed = discover_and_parse(project_root=REPO_ROOT)
    return {ch.id: ch for ch in canonize(parsed).canonical_definitions}


def _dir(ch):
    """Surface direction of a canonical channel: uplink, downlink, both, none (OR variants with _or)."""
    d, d_or = ch.direction, ch.direction_or
    base = d | d_or
    name = {frozenset({"uplink"}): "uplink", frozenset({"downlink"}): "downlink",
            frozenset({"uplink", "downlink"}): "both"}.get(frozenset(base), "none")
    return name + ("_or" if d_or else "")


def test_no_base_channel_without_direction(channels):
    left = sorted(cid for cid, ch in channels.items() if ch.scope == "BASE" and _dir(ch) == "none")
    assert left == [], f"BASE channels with direction none: {left}"


@pytest.mark.parametrize("cid", ["KEY_BTN", "DIRECT_DRIVE_BTN", "CRUISE_TOGGLE_BTN"])
def test_buttons_are_base_downlink(channels, cid):
    assert (channels[cid].scope, _dir(channels[cid])) == ("BASE", "downlink")


@pytest.mark.parametrize("cid", ["KEY_ACTIVE", "DIRECT_DRIVE", "CRUISE_ACTIVE",
                                 "ESC_SPEED_BUS", "DRIVE_STATE_BUS", "ESC_RPM_BUS"])
def test_computed_states_are_system_uplink(channels, cid):
    assert (channels[cid].scope, _dir(channels[cid])) == ("SYSTEM", "uplink")


def test_gear_shifting_is_removed(channels):
    assert "GEAR_SHIFTING" not in channels


@pytest.mark.parametrize("cid", ["KEY_ACTIVE", "DIRECT_DRIVE", "CRUISE_ACTIVE",
                                 "ESC_SPEED_BUS", "DRIVE_STATE_BUS", "ESC_RPM_BUS"])
def test_computed_states_live_in_the_type_profile(channels, cid):
    parts = channels[cid].source_path.parts
    assert ("core", "config", "machines", "dumper_truck", "processors") == \
        tuple(parts[parts.index("core"):parts.index("core") + 5]), channels[cid].source_path


def test_generic_core_holds_no_dumper_channel():
    folder = REPO_ROOT / "src" / "core" / "system" / "combus" / "processors"
    assert sorted(p.name for p in folder.glob("*.cb")) == []


@pytest.mark.parametrize("cid,requires,theme", [
    ("KEY_ACTIVE", {"HAS_INPUT_PROC", "HAS_IGNIT_KEY"}, "input"),
    ("DIRECT_DRIVE", {"HAS_INPUT_PROC"}, "input"),
    ("CRUISE_ACTIVE", {"HAS_INPUT_PROC"}, "input"),
    ("ESC_SPEED_BUS", {"HAS_SIM_PROC"}, "sim"),
    ("DRIVE_STATE_BUS", {"HAS_SIM_PROC"}, "sim"),
    ("ESC_RPM_BUS", {"HAS_SIM_PROC"}, "sim"),
])
def test_computed_states_are_produced_by_their_processing_board(channels, cid, requires, theme):
    """Only the board that runs the chain declares the channel uplink: the others get it by diffusion."""
    assert set(channels[cid].requires) == requires
    assert channels[cid].theme == theme
