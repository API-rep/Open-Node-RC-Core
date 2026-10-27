"""The `fault` value of the failsafe digital channels (link rework, step S3a).

Rules validated by the owner (2026-10-27):
  - the safe value of a channel is neutral (analog CbusNeutral, digital
    false), except the digital channels of the failsafe group ("true =
    fault"), safe at true;
  - that exception is DECLARED in the channel, not deduced from its theme:
    `value: fault` (renders true, like `high`);
  - strict check on the real .cb files: a digital channel of the theme
    `failsafe` must declare `value: fault`; `value: fault` outside the theme
    `failsafe` is an error; `value: high` is refused (a true safe value only
    makes sense for an alarm, and it is written `fault`);
  - the theme `com` (link-health alarms) is merged into `failsafe`.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import VALID_THEMES, canonize  # noqa: E402
from scripts.combus_builder.generator import _render_value  # noqa: E402
from scripts.combus_builder.parser import ParseError, discover_and_parse  # noqa: E402

SRC = REPO_ROOT / "src"
TYPES = sorted(p.name for p in (SRC / "core" / "config" / "machines").iterdir()
               if p.is_dir() and p.name != "common")


def _tree(tmp_path, theme, value, type_="digital"):
    folder = tmp_path / "src" / "core"
    folder.mkdir(parents=True)
    (folder / "x.cb").write_text(
        f"channels:\n- id: X_FAIL\n  infoName: x\n  type: {type_}\n  scope: SYSTEM\n"
        f"  theme: {theme}\n  direction: uplink\n  value: {value}\n", encoding="utf-8")
    return tmp_path


def test_fault_on_a_failsafe_digital_passes(tmp_path):
    _, _, parsed = discover_and_parse(project_root=_tree(tmp_path, "failsafe", "fault"))
    ch = canonize(parsed).canonical_definitions[0]
    assert ch.value == "fault"
    assert _render_value(ch, ch.type) == "true"          # same literal as `high`: same wire, same MD5


@pytest.mark.parametrize("theme,value", [
    ("failsafe", "low"),     # an alarm must declare its true safe value
    ("failsafe", "high"),    # ... and write it `fault`
    ("core", "fault"),       # `fault` belongs to the failsafe group only
    ("core", "high"),        # a true safe value outside an alarm: refused
])
def test_strict_check_of_the_failsafe_group(tmp_path, theme, value):
    with pytest.raises(ParseError, match="X_FAIL"):
        discover_and_parse(project_root=_tree(tmp_path, theme, value))


def test_theme_com_is_merged_into_failsafe():
    assert "com" not in VALID_THEMES


@pytest.mark.parametrize("machine_type", [None] + TYPES)
def test_every_failsafe_digital_of_the_project_declares_fault(machine_type):
    _, _, parsed = discover_and_parse(project_root=REPO_ROOT, machine_type=machine_type)
    channels = canonize(parsed).canonical_definitions
    wrong = sorted(ch.id for ch in channels
                   if ch.type == "digital" and (ch.theme == "failsafe") != (ch.value == "fault"))
    assert wrong == []
