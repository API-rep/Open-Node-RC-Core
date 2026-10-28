"""The safe value of an analog channel is CbusNeutral (link rework, step S3).

Rules validated by the owner (2026-10-27):
  - the safe value of a channel is neutral: analog `CbusNeutral` (digital
    `low`, failsafe alarms `fault`: test_fault_value.py), on every scope;
  - an analog channel declaring something else is reported by the builder,
    one warning per channel, WITHOUT breaking the build: the chains of
    several real channels are not safe at neutral yet (brake, gear, sub-gear,
    engine rpm, runlevel: `doc/WIP - ComBus processors.md` §2.5-2.6);
  - the build log is the living list of what remains; when it is empty, the
    warning becomes an error (`NEUTRAL_VALUE_IS_ERROR`);
  - the test channels (test_tractor, test_trailer) are already at neutral.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder import parser as parser_mod  # noqa: E402
from scripts.combus_builder.parser import ParseError, discover_and_parse  # noqa: E402

SRC = REPO_ROOT / "src"
TYPES = sorted(p.name for p in (SRC / "core" / "config" / "machines").iterdir()
               if p.is_dir() and p.name != "common")

  # The living list: real analog channels whose chain is not safe at neutral yet.
NOT_YET_NEUTRAL = {"BRAKE_BUS", "GEAR_BUS", "SUBGEAR_BUS", "ESC_RPM_BUS", "RUNLEVEL"}


@pytest.fixture(autouse=True)
def _fresh_warnings(monkeypatch):
    """Each test starts with no channel already reported."""
    monkeypatch.setattr(parser_mod, "_neutral_warned", set())


def _tree(tmp_path, value, type_="analog"):
    folder = tmp_path / "src" / "core"
    folder.mkdir(parents=True)
    (folder / "x.cb").write_text(
        f"channels:\n- id: X_BUS\n  infoName: x\n  type: {type_}\n  scope: SYSTEM\n"
        f"  theme: core\n  direction: uplink\n  value: {value}\n", encoding="utf-8")
    return tmp_path


def test_neutral_analog_passes_silently(tmp_path, capsys):
    discover_and_parse(project_root=_tree(tmp_path, "CbusNeutral"))
    assert "X_BUS" not in capsys.readouterr().err


@pytest.mark.parametrize("value", ["0", "1", "CbusMinVal", "CbusMaxVal"])
def test_other_analog_value_is_reported_without_breaking(tmp_path, capsys, value):
    discover_and_parse(project_root=_tree(tmp_path, value))          # no raise
    err = capsys.readouterr().err
    assert "[combus_builder] WARNING:" in err
    assert "X_BUS" in err and "CbusNeutral" in err and f"`value: {value}`" in err


def test_digital_channels_are_not_concerned(tmp_path, capsys):
    discover_and_parse(project_root=_tree(tmp_path, "low", type_="digital"))
    assert "X_BUS" not in capsys.readouterr().err


def test_a_channel_is_reported_once(tmp_path, capsys):
    root = _tree(tmp_path, "0")
    discover_and_parse(project_root=root)
    discover_and_parse(project_root=root)                            # parsed twice in a build (links.py)
    assert capsys.readouterr().err.count("X_BUS") == 1


def test_the_warning_becomes_an_error_when_switched(tmp_path, monkeypatch):
    monkeypatch.setattr(parser_mod, "NEUTRAL_VALUE_IS_ERROR", True)
    with pytest.raises(ParseError, match="X_BUS"):
        discover_and_parse(project_root=_tree(tmp_path, "0"))


def _reported(machine_type, capsys):
    """Channels reported by the builder for the real project, core + `machine_type`."""
    discover_and_parse(project_root=REPO_ROOT, machine_type=machine_type)
    err = capsys.readouterr().err
    return {line.split("channel ")[1].split(":")[0].strip("'")
            for line in err.splitlines() if "[combus_builder] WARNING:" in line and "CbusNeutral" in line}


@pytest.mark.parametrize("machine_type", [None] + TYPES)
def test_no_channel_outside_the_living_list(machine_type, capsys):
    reported = _reported(machine_type, capsys)
    assert reported <= NOT_YET_NEUTRAL
    assert not any(cid.startswith("TEST_") for cid in reported)      # test channels already at neutral


def test_living_list_of_the_project(capsys, monkeypatch):
    """Over every type, the real channels not at neutral yet are exactly the known ones (update when one is fixed)."""
    reported = set()
    for machine_type in [None] + TYPES:
        monkeypatch.setattr(parser_mod, "_neutral_warned", set())    # each type is a separate build
        reported |= _reported(machine_type, capsys)
    assert reported == NOT_YET_NEUTRAL
