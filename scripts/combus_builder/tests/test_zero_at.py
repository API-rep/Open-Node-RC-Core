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
from scripts.combus_builder.generator import select_views  # noqa: E402
from scripts.combus_builder.links import LinkError, compute_link_set, link_md5  # noqa: E402
from scripts.combus_builder.md5 import canonical_bytes, compute_view_hash  # noqa: E402
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


# =============================================================================
# Point 2: the MD5 (view MD5 and link MD5)
# =============================================================================

def _raw(zero_at=None, scope="BASE", direction="downlink_pt"):
    raw = {"id": "X_BUS", "infoName": "x", "type": "analog", "scope": scope, "theme": "core",
           "direction": direction, "value": "CbusNeutral"}
    if zero_at is not None:
        raw["zeroAt"] = zero_at
    return raw


def _defs(*raws):
    parsed = [(Path(f"ch{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


def _view_bytes(zero_at, scope="BASE"):
    sel = select_views(_defs(_raw(zero_at, scope=scope, direction="uplink")))
    return canonical_bytes(sel.base if scope == "BASE" else sel.system)


@pytest.mark.parametrize("scope", ["BASE", "SYSTEM"])
def test_row5_neutral_and_start_give_two_view_md5(scope):
    assert compute_view_hash(_view_bytes_view("neutral", scope)) != compute_view_hash(_view_bytes_view("start", scope))


@pytest.mark.parametrize("scope", ["BASE", "SYSTEM"])
def test_row4_start_hashes_like_absent_md5_unchanged(scope):
    assert _view_bytes(None, scope) == _view_bytes("start", scope)   # existing MD5 do not move
    assert b"zeroAt" not in _view_bytes("start", scope)


def _view_bytes_view(zero_at, scope):
    sel = select_views(_defs(_raw(zero_at, scope=scope, direction="uplink")))
    return sel.base if scope == "BASE" else sel.system


def test_row5_link_md5_follows_zero_at():
    def md5(zero_at):
        t, r = _defs(_raw(zero_at)), _defs(_raw(zero_at))
        return link_md5(compute_link_set("t", t, "r", r, "IS_VEHICLE"))
    assert md5("neutral") != md5("start")
    assert md5(None) == md5("start")                                  # existing link MD5 do not move


def test_link_refuses_two_types_disagreeing_on_zero_at():
    t, r = _defs(_raw("neutral")), _defs(_raw("start"))
    with pytest.raises(LinkError, match="zeroAt"):
        compute_link_set("t", t, "r", r, "IS_VEHICLE")


def test_no_real_channel_uses_it_yet():
    """S3-4 builds the mechanism only: GEAR_BUS / SUBGEAR_BUS move with the simulation WIP."""
    _, _, parsed = discover_and_parse(project_root=REPO_ROOT)
    assert not any(ch.zero_at_neutral for ch in canonize(parsed).canonical_definitions)
