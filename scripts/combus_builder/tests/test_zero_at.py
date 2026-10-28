"""The `zeroAt` field of an analog channel (link rework, step S3-4).

Rules validated by the owner (2026-10-28):
  - `zeroAt: neutral | start` places the LOGICAL zero of an analog channel in
    its raw range: at `CbusNeutral` (neutral) or at 0 (start, the default,
    today's encoding).  It changes neither the storage type (raw uint16_t)
    nor the frame;
  - analog only: `zeroAt` on a digital channel, or another value, breaks the
    build;
  - the canonical form is the boolean `zero_at_neutral` (C++: `zeroAtNeutral`).

Point 1 of S3-4: the field.  The MD5, the generated flag and the accessors
are the next points.
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
from scripts.combus_builder.canon.channels import ChannelValidationError  # noqa: E402
from scripts.combus_builder.parser import discover_and_parse  # noqa: E402


def _canon(tmp_path, zero_at=None, type_="analog", value="CbusNeutral"):
    folder = tmp_path / "src" / "core"
    folder.mkdir(parents=True)
    extra = f"  zeroAt: {zero_at}\n" if zero_at is not None else ""
    (folder / "x.cb").write_text(
        f"channels:\n- id: X_BUS\n  infoName: x\n  type: {type_}\n  scope: SYSTEM\n"
        f"  theme: core\n  direction: uplink\n  value: {value}\n{extra}", encoding="utf-8")
    _, _, parsed = discover_and_parse(project_root=tmp_path)
    return canonize(parsed).canonical_definitions[0]


def test_row1_neutral_marks_the_channel(tmp_path):
    assert _canon(tmp_path, "neutral").zero_at_neutral is True


@pytest.mark.parametrize("zero_at", [None, "start"])
def test_row4_absent_or_start_is_today_encoding(tmp_path, zero_at):
    assert _canon(tmp_path, zero_at).zero_at_neutral is False


def test_row2_zero_at_on_a_digital_breaks_the_build(tmp_path):
    with pytest.raises(ChannelValidationError, match="`zeroAt` is"):
        _canon(tmp_path, "neutral", type_="digital", value="low")


@pytest.mark.parametrize("zero_at", ["middle", "Neutral", "true", "0"])
def test_row3_unknown_value_breaks_the_build(tmp_path, zero_at):
    with pytest.raises(ChannelValidationError, match="`zeroAt` is"):
        _canon(tmp_path, zero_at)


def test_no_real_channel_uses_it_yet():
    """S3-4 builds the mechanism only: GEAR_BUS / SUBGEAR_BUS move with the simulation WIP."""
    _, _, parsed = discover_and_parse(project_root=REPO_ROOT)
    assert not any(ch.zero_at_neutral for ch in canonize(parsed).canonical_definitions)
