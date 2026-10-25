"""Candidate channels, stable / next MD5 and the DEV version field (link rework, step 2.2a).

Rules validated by the owner (2026-10-23):
  - a BASE channel may be a candidate (`candidate: true`); the .cb gives its
    final name and the builder adds `_NEXT` (removing `candidate` promotes it
    under that name); `_NEXT` written by hand is refused; the name without the
    suffix must not be the one of another channel; SYSTEM / BOARD channels
    cannot be candidates (for now);
  - candidates are numbered after the stable BASE channels of their type and
    before the SYSTEM channels: the stable BASE ids never move;
  - the BASE set has two MD5: stable (without the candidates, unchanged by
    adding one) and next (stable then candidates); equal without candidates;
  - the SYSTEM set contains the candidates: two boards of a node with
    different candidates have different SYSTEM MD5 (their link is refused);
  - project_version.h has a third field, PROJECT_VERSION_DEV, written in
    combus_wire_common.h (logged only by the handshake, step 2.2b).
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
from scripts.combus_builder.generator import generate, select_views
from scripts.combus_builder.md5 import (
    ProjectVersionError,
    compute_view_hash,
    read_project_version,
    stable_view,
)


# =============================================================================
# Helpers
# =============================================================================

def _ch(id_, scope="BASE", type_="digital", theme="input", direction="uplink", candidate=None):
    raw = {"id": id_, "infoName": id_.lower(), "type": type_, "scope": scope,
           "theme": theme, "direction": direction}
    if candidate is not None:
        raw["candidate"] = candidate
    return raw


def _canon(*raws):
    parsed = [(Path(f"ch{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


def _ids(view):
    return {vc.ch.id: vc.numeric_id for vc in view.channels}


def _ctx():
    return BuildContext(buildroot=Path("."), defines=frozenset(), defines_with_value={},
                        cppdefines_source="override")


STABLE = [_ch("BRAKE"), _ch("HORN"), _ch("STEER", type_="analog"), _ch("LOCAL", scope="SYSTEM")]
TILT = _ch("TILT", candidate=True)


# =============================================================================
# Declaration (canon)
# =============================================================================

def test_base_candidate_is_accepted_and_named_with_the_suffix():
    (ch,) = _canon(TILT)
    assert ch.candidate is True
    assert ch.id == "TILT_NEXT"


def test_channel_without_the_field_is_not_a_candidate():
    (ch,) = _canon(_ch("BRAKE"))
    assert ch.candidate is False


@pytest.mark.parametrize("scope,direction", [("SYSTEM", "uplink"), ("BOARD", "none")])
def test_candidate_outside_base_is_refused(scope, direction):
    with pytest.raises(ChannelValidationError, match="candidate"):
        _canon(_ch("TILT", scope=scope, direction=direction, candidate=True))


@pytest.mark.parametrize("candidate", [None, False, True])
def test_next_suffix_written_by_hand_is_refused(candidate):
    with pytest.raises(ChannelValidationError, match="_NEXT"):
        _canon(_ch("TILT_NEXT", candidate=candidate))


def test_candidate_with_the_name_of_another_channel_is_refused():
    """Promoting it would create a duplicate: refused at once."""
    with pytest.raises(ChannelValidationError, match="TILT"):
        _canon(_ch("TILT", type_="analog"), TILT)


def test_candidate_must_be_a_boolean():
    with pytest.raises(ChannelValidationError, match="candidate"):
        _canon(_ch("TILT", candidate="yes"))


# =============================================================================
# Numbering (row 10)
# =============================================================================

def test_candidate_comes_after_the_stable_base_channels_of_its_type():
    sel = select_views(_canon(*STABLE, TILT))
    digital = [vc.ch.id for vc in sel.full.channels if vc.ch.type == "digital"]
    assert digital == ["BRAKE", "HORN", "TILT_NEXT", "LOCAL"]


def test_adding_a_candidate_keeps_the_stable_base_ids():
    before = select_views(_canon(*STABLE))
    after = select_views(_canon(*STABLE, TILT))
    for cid, nid in _ids(before.base).items():
        assert _ids(after.full)[cid] == nid, cid


def test_a_candidate_whose_name_sorts_first_still_comes_after_the_stable_ones():
    sel = select_views(_canon(*STABLE, _ch("AAA", candidate=True)))
    digital = [vc.ch.id for vc in sel.base.channels if vc.ch.type == "digital"]
    assert digital == ["BRAKE", "HORN", "AAA_NEXT"]


# =============================================================================
# Stable / next MD5 (rows 1, 10, 11) and the SYSTEM set (rule b)
# =============================================================================

def test_without_candidate_stable_and_next_md5_are_equal():
    sel = select_views(_canon(*STABLE))
    assert compute_view_hash(stable_view(sel.base)).md5_hex == compute_view_hash(sel.base).md5_hex


def test_adding_a_candidate_keeps_the_stable_md5():
    before = select_views(_canon(*STABLE))
    after = select_views(_canon(*STABLE, TILT))
    assert compute_view_hash(stable_view(after.base)).md5_hex == compute_view_hash(before.base).md5_hex


def test_adding_a_candidate_changes_the_next_md5():
    before = select_views(_canon(*STABLE))
    after = select_views(_canon(*STABLE, TILT))
    assert compute_view_hash(after.base).md5_hex != compute_view_hash(before.base).md5_hex


def test_adding_a_candidate_changes_the_system_md5():
    """Two boards of a node with different candidates must not agree on SYSTEM."""
    before = select_views(_canon(*STABLE))
    after = select_views(_canon(*STABLE, TILT))
    assert compute_view_hash(after.system).md5_hex != compute_view_hash(before.system).md5_hex


def test_promotion_changes_the_stable_md5():
    """Row 11: at the next major, `candidate` is removed: the channel takes its final name."""
    candidate = select_views(_canon(*STABLE, TILT))
    promoted = select_views(_canon(*STABLE, _ch("TILT")))
    assert compute_view_hash(stable_view(promoted.base)).md5_hex != \
           compute_view_hash(stable_view(candidate.base)).md5_hex


# =============================================================================
# Generated files and build log
# =============================================================================

def test_base_md5_header_carries_stable_and_next(tmp_path):
    sel = generate(_canon(*STABLE, TILT), _ctx(), tmp_path)
    text = (tmp_path / "combus_base_md5.h").read_text(encoding="utf-8")
    stable_hex = compute_view_hash(stable_view(sel.base)).md5_hex
    next_hex = compute_view_hash(sel.base).md5_hex
    assert f'kCombusBaseComBusMd5Hex =\n    "{stable_hex}";' in text
    assert f'kCombusBaseComBusMd5NextHex =\n    "{next_hex}";' in text
    assert "kCombusBaseComBusMd5Next[16]" in text


def test_build_log_shows_the_next_set(tmp_path, capsys):
    sel = generate(_canon(*STABLE, TILT), _ctx(), tmp_path)
    out = capsys.readouterr().out
    stable_hex = compute_view_hash(stable_view(sel.base)).md5_hex
    next_hex = compute_view_hash(sel.base).md5_hex
    assert f"[combus_builder] set BASE    md5={stable_hex}  A1+D2" in out
    assert f"[combus_builder] set NEXT    md5={next_hex}  A1+D3  (1 candidate)" in out


# =============================================================================
# DEV version field
# =============================================================================

HEADER = """
namespace project {
static constexpr uint8_t PROJECT_VERSION_MAJOR = 2u;
static constexpr uint8_t PROJECT_VERSION_MINOR = 5u;
static constexpr uint8_t PROJECT_VERSION_DEV = 7u;
}
"""


def test_dev_is_read(tmp_path):
    (tmp_path / "project_version.h").write_text(HEADER, encoding="utf-8")
    assert read_project_version(tmp_path) == (2, 5, 7)


def test_missing_dev_is_an_error(tmp_path):
    text = HEADER.replace("static constexpr uint8_t PROJECT_VERSION_DEV = 7u;\n", "")
    (tmp_path / "project_version.h").write_text(text, encoding="utf-8")
    with pytest.raises(ProjectVersionError, match="PROJECT_VERSION_DEV"):
        read_project_version(tmp_path)


def test_real_project_header_has_dev():
    major, minor, dev = read_project_version(REPO_ROOT)
    assert 0 <= dev <= 255


def test_dev_is_written_in_the_common_header(tmp_path):
    generate(_canon(*STABLE), _ctx(), tmp_path, project_version=(2, 5, 7))
    text = (tmp_path / "combus_wire_common.h").read_text(encoding="utf-8")
    assert "kProjectVersionDev = 7u;" in text


# =============================================================================
# Step 2.2b — what the firmware needs from the builder
# =============================================================================

def test_base_md5_header_carries_the_stable_channel_counts(tmp_path):
    """In stable mode a BASE link sends and applies the stable channels only."""
    generate(_canon(*STABLE, TILT, _ch("ROLL", type_="analog", candidate=True)), _ctx(), tmp_path)
    text = (tmp_path / "combus_base_md5.h").read_text(encoding="utf-8")
    assert "kCombusBaseStableAnalogCount = 1u;" in text
    assert "kCombusBaseStableDigitalCount = 2u;" in text


def test_handshake_payload_carries_dev(tmp_path):
    generate(_canon(*STABLE), _ctx(), tmp_path, project_version=(2, 5, 7))
    text = (tmp_path / "combus_wire_common.h").read_text(encoding="utf-8")
    assert "kCombusHandshakeWirePayloadLen = 19u;" in text
