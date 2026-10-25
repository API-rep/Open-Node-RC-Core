"""Safe value of every channel (link rework, step 3.2a).

Rules validated by the owner (2026-10-25):
  - the default value of a .cb IS its safe value: sent until the writer has
    written, and taken back when the source of the channel is lost;
  - the implicit default of the builder (CbusNeutral, false) is not always
    safe (a brake pedal at mid-travel brakes): without `value:`, the build
    breaks, for BASE, SYSTEM and BOARD channels alike;
  - safe values chosen: BRAKE_BUS 0 (a pedal without action does not brake),
    HAZARDS low, sticks CbusNeutral, buttons and states low.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize  # noqa: E402
from scripts.combus_builder.parser import ParseError, discover_and_parse  # noqa: E402

SRC = REPO_ROOT / "src"
TYPES = sorted(p.name for p in (SRC / "core" / "config" / "machines").iterdir()
               if p.is_dir() and p.name != "common")


def _tree(tmp_path, body):
    folder = tmp_path / "src" / "core"
    folder.mkdir(parents=True)
    (folder / "x.cb").write_text(body, encoding="utf-8")
    return tmp_path


@pytest.mark.parametrize("scope", ["BASE", "SYSTEM", "BOARD"])
def test_a_channel_without_value_breaks_the_build(tmp_path, scope):
    root = _tree(tmp_path, f"channels:\n- id: NO_VALUE\n  infoName: x\n  type: analog\n"
                           f"  scope: {scope}\n  theme: core\n  direction: none\n")
    with pytest.raises(ParseError, match="NO_VALUE"):
        discover_and_parse(project_root=root)


def test_a_channel_with_a_value_passes(tmp_path):
    root = _tree(tmp_path, "channels:\n- id: WITH_VALUE\n  infoName: x\n  type: digital\n"
                           "  scope: SYSTEM\n  theme: core\n  direction: none\n  value: low\n")
    _, _, parsed = discover_and_parse(project_root=root)
    assert len(parsed) == 1


@pytest.mark.parametrize("machine_type", [None] + TYPES)
def test_every_channel_of_the_project_has_a_value(machine_type):
    """The whole tree, for each machine type profile: no channel is left without a safe value."""
    discover_and_parse(project_root=REPO_ROOT, machine_type=machine_type)


@pytest.mark.parametrize("cid,value", [
    ("BRAKE_BUS", 0),
    ("HAZARDS", "low"),
    ("THROTTLE_STICK", "CbusNeutral"),
    ("KEY_ACTIVE", "low"),
])
def test_chosen_safe_values_of_the_dumper(cid, value):
    _, _, parsed = discover_and_parse(project_root=REPO_ROOT, machine_type="dumper_truck")
    channels = {ch.id: ch for ch in canonize(parsed).canonical_definitions}
    assert channels[cid].value == value
