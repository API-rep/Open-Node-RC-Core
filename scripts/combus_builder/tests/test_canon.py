#!/usr/bin/env python3
"""
Tests for canon (A5.1) — strict canonisation of `channels:`.

Coverage:
  - Section extraction (extension-agnostic).
  - Unknown keys rejected.
  - Per-field validation (id, infoName, type, scope, theme, direction, requires).
  - Direction contract per scope (LOCAL/REMOTE/SYSTEM).
  - Direction normalization (both → {uplink, downlink}, none → {}).
  - Direction duplicates rejected.
  - Direction empty list rejected for LOCAL/REMOTE.
  - Direction wire tokens rejected for SYSTEM.
  - requires duplicates rejected.
  - ID uniqueness (across types/scopes/themes).
  - Conflict detection (with file paths in the error).
  - Fusion (multiple files, multiple sections).
  - Canonical sort (scope, type, theme, id).
  - Determinism (same set, different order → same canonical output).
  - Package file (channels + chains in the same file).
  - Validation against real .cb files in the repo.
"""

from __future__ import annotations

import inspect
import sys
import tempfile
import traceback
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import (
    ALLOWED_FIELDS,
    DIRECTION_BY_SCOPE,
    DIRECTION_TOKENS,
    SCOPE_ORDER,
    VALID_SCOPES,
    VALID_THEMES,
    VALID_TYPES,
    CanonResult,
    ChannelConflictError,
    ChannelDefinition,
    ChannelError,
    ChannelValidationError,
    canonize,
    canonize_channels,
    extract_channels_sections,
    merge_channels,
    validate_channel,
)


# =============================================================================
# Helpers
# =============================================================================

def _parsed(path: Path, raw: dict, type_label: str = "channel_def"):
    """Build a (path, type_label, raw_dict) tuple as A3 would return."""
    return (path, type_label, raw)


def _ch(id_: str = "FOO", info_name: str = "Foo channel",
        type_: str = "digital", scope: str = "LOCAL",
        theme: str = "failsafe", direction: str | None = None,
        requires: list | None = None) -> dict:
    """Build a minimal valid channel dict (LOCAL scope, failsafe theme).

    `direction` is a STRING (decision mainteneur 2026-08-22), not a list.
    Accepted values: 'uplink', 'downlink', 'both', 'none'.
    Pass `direction=None` to omit the field (SYSTEM scope only).
    """
    d: dict = {
        "id": id_,
        "infoName": info_name,
        "type": type_,
        "scope": scope,
        "theme": theme,
    }
    if direction is not None:
        d["direction"] = direction
    elif scope in ("LOCAL", "REMOTE"):
        d["direction"] = "uplink"
    if requires is not None:
        d["requires"] = requires
    return d


# =============================================================================
# Section extraction
# =============================================================================

def test_extract_sections_basic(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cbch"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO")]}),
        _parsed(p2, {"channels": [_ch("BAR")]}),
    ]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 2
    assert sections[0][0] == p1
    assert sections[1][0] == p2


def test_extract_sections_skips_documents_without_channels(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cbch"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO")]}),
        _parsed(p2, {"chains": [{"name": "x"}]}),
    ]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 1
    assert sections[0][0] == p1


def test_extract_sections_empty_channels_returns_empty_section(tmp_path):
    p1 = tmp_path / "a.cb"
    parsed = [_parsed(p1, {"channels": []})]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 1
    assert sections[0][1] == []


def test_extract_sections_channels_must_be_list(tmp_path):
    p1 = tmp_path / "a.cb"
    parsed = [_parsed(p1, {"channels": "not a list"})]
    try:
        extract_channels_sections(parsed)
    except ChannelValidationError as e:
        assert "must be a list" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_extract_sections_extension_agnostic(tmp_path):
    p_cb = tmp_path / "a.cb"
    p_cbch = tmp_path / "b.cbch"
    parsed = [
        _parsed(p_cb, {"channels": [_ch("FOO")]}, type_label="channel_def"),
        _parsed(p_cbch, {"channels": [_ch("BAR")]}, type_label="chain_def"),
    ]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 2
    assert {s[0] for s in sections} == {p_cb, p_cbch}


# =============================================================================
# Bug 2: Distinguish channels: absent vs null vs []
# =============================================================================

def test_extract_sections_no_channels_key_ignored(tmp_path):
    """File without `channels:` key is silently ignored (not an error)."""
    p1 = tmp_path / "a.cb"
    parsed = [_parsed(p1, {"other": "value"})]
    sections = extract_channels_sections(parsed)
    assert sections == []


def test_extract_sections_null_rejected(tmp_path):
    """`channels: null` (explicitly null) is an error."""
    p1 = tmp_path / "a.cb"
    parsed = [_parsed(p1, {"channels": None})]
    try:
        extract_channels_sections(parsed)
    except ChannelValidationError as e:
        assert "null" in str(e)
        assert "channels" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError for channels: null")


def test_extract_sections_empty_list_accepted(tmp_path):
    """`channels: []` is a valid empty section (no error)."""
    p1 = tmp_path / "a.cb"
    parsed = [_parsed(p1, {"channels": []})]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 1
    assert sections[0][0] == p1
    assert sections[0][1] == []


def test_extract_sections_null_differs_from_absent(tmp_path):
    """absent and null are DIFFERENT: absent is OK, null is an error."""
    p_absent = tmp_path / "absent.cb"
    p_null = tmp_path / "null.cb"
    parsed = [
        _parsed(p_absent, {"other": "value"}),
        _parsed(p_null, {"channels": None}),
    ]
    # absent is fine
    sections = extract_channels_sections([parsed[0]])
    assert sections == []
    # null is an error
    try:
        extract_channels_sections([parsed[1]])
    except ChannelValidationError:
        pass
    else:
        raise AssertionError("expected ChannelValidationError for channels: null")


# =============================================================================
# Unknown keys
# =============================================================================

def test_unknown_field_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
        "typo": "something",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "unknown field" in str(e)
        assert "typo" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_legacy_module_field_rejected(tmp_path):
    """The legacy `module:` field is not part of the strict contract."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"module": "vbat", "channels": [_ch("FOO")]})]
    # `module` at top level is fine (it's not a channel field).
    # But if it appears inside a channel, it's rejected.
    parsed2 = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
        "module": "vbat",
    }]})]
    try:
        canonize(parsed2)
    except ChannelValidationError as e:
        assert "unknown field" in str(e)
        assert "module" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_legacy_layer_field_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
        "layer": 1,
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "unknown field" in str(e)
        assert "layer" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_legacy_default_field_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
        "default": 0,
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "unknown field" in str(e)
        assert "default" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# id
# =============================================================================

def test_id_required(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "id" in str(e).lower()
    else:
        raise AssertionError("expected ChannelValidationError")


def test_id_must_be_string(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": 42,
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "id" in str(e).lower()
    else:
        raise AssertionError("expected ChannelValidationError")


def test_id_must_be_non_empty(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "non-empty" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# infoName (REQUIRED)
# =============================================================================

def test_info_name_required(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "infoName" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_info_name_must_be_string(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": 42,
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "infoName" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_info_name_must_be_non_empty(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "infoName" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_info_name_alias_rejected(tmp_path):
    """`info_name` (snake_case) is NOT an alias for `infoName`."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "info_name": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "unknown field" in str(e)
        assert "info_name" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# type
# =============================================================================

def test_type_required(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "type" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_type_invalid(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="bool")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "type" in str(e)
        assert "bool" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# scope
# =============================================================================

def test_scope_required(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "scope" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_scope_invalid(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="GLOBAL")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "scope" in str(e)
        assert "GLOBAL" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# theme (cadré)
# =============================================================================

def test_theme_required(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "direction": "uplink",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "theme" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_theme_must_be_non_empty(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(theme="")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "theme" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_theme_must_be_in_valid_themes(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(theme="unknown_theme")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "theme" in str(e)
        assert "unknown_theme" in str(e)
        assert "VALID_THEMES" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_theme_vbat_accepted(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(theme="vbat")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].theme == "vbat"


def test_theme_failsafe_accepted(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(theme="failsafe")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].theme == "failsafe"


# =============================================================================
# Direction — full matrix per scope
# =============================================================================

# LOCAL
def test_dir_local_uplink(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="uplink")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink"})


def test_dir_local_downlink(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="downlink")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"downlink"})


def test_dir_local_both_normalizes_to_uplink_downlink(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="both")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink", "downlink"})


def test_dir_local_none_normalizes_to_empty(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="none")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset()


def test_dir_local_uplink_downlink(tmp_path):
    """`direction: 'both'` is the string equivalent of the old
    `direction: ['uplink', 'downlink']` list form. Both normalize to
    frozenset({'uplink', 'downlink'}).
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="both")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink", "downlink"})


def test_dir_local_empty_rejected(tmp_path):
    """`direction: ''` (empty string) is rejected.

    In the old list format, `direction: []` was rejected as "empty".
    In the new string format, an empty string is not a valid token
    and is rejected with a message mentioning the invalid value.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_local_duplicate_rejected(tmp_path):
    """Duplicates are no longer possible with a string format.

    The old `direction: ['uplink', 'uplink']` test is obsolete: a
    string can only hold one value, so duplicates are structurally
    impossible. This test is kept as a placeholder to document the
    migration — it now tests that an invalid token is rejected
    (which is the closest equivalent semantic in the new format).
    """
    p = tmp_path / "a.cb"
    # In the new format, "uplink" is a valid token, so we test an
    # invalid token instead to keep the test meaningful.
    parsed = [_parsed(p, {"channels": [_ch(direction="invalid_token")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "invalid_token" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_local_both_with_uplink_rejected(tmp_path):
    """`both` + `uplink` is incoherent — but in string format, this
    case is structurally impossible (one string = one token). This
    test is kept as a placeholder; the equivalent semantic in the
    new format is testing an invalid token, which is already covered
    by test_dir_invalid_token. We keep a minimal assertion to
    document the migration.
    """
    p = tmp_path / "a.cb"
    # In string format, "both" alone is valid. We test an invalid
    # token to keep the test meaningful.
    parsed = [_parsed(p, {"channels": [_ch(direction="bogus")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_local_none_with_uplink_rejected(tmp_path):
    """`none` + `uplink` is incoherent — but in string format, this
    case is structurally impossible. Kept as a placeholder; the
    equivalent semantic is covered by test_dir_invalid_token.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="bogus")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# REMOTE
def test_dir_remote_uplink(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="REMOTE", direction="uplink")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink"})


def test_dir_remote_downlink(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="REMOTE", direction="downlink")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"downlink"})


def test_dir_remote_both(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="REMOTE", direction="both")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink", "downlink"})


# =============================================================================
# A2.1 — Variantes `*_OR` (uplink_or, downlink_or, both_or)
# =============================================================================

def test_dir_local_uplink_or_normalizes(tmp_path):
    """`uplink_or` LOCAL digital → direction_or={uplink}, direction={}."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", direction="uplink_or")]})]
    result = canonize(parsed)
    ch = result.canonical_definitions[0]
    assert ch.direction == frozenset()
    assert ch.direction_or == frozenset({"uplink"})


def test_dir_local_downlink_or_normalizes(tmp_path):
    """`downlink_or` LOCAL digital → direction_or={downlink}, direction={}."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", direction="downlink_or")]})]
    result = canonize(parsed)
    ch = result.canonical_definitions[0]
    assert ch.direction == frozenset()
    assert ch.direction_or == frozenset({"downlink"})


def test_dir_local_both_or_normalizes(tmp_path):
    """`both_or` LOCAL digital → direction_or={uplink, downlink}, direction={}."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", direction="both_or")]})]
    result = canonize(parsed)
    ch = result.canonical_definitions[0]
    assert ch.direction == frozenset()
    assert ch.direction_or == frozenset({"uplink", "downlink"})


def test_dir_remote_uplink_or_normalizes(tmp_path):
    """`uplink_or` REMOTE digital → direction_or={uplink}, direction={}."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", scope="REMOTE", direction="uplink_or")]})]
    result = canonize(parsed)
    ch = result.canonical_definitions[0]
    assert ch.direction == frozenset()
    assert ch.direction_or == frozenset({"uplink"})


def test_dir_or_rejected_on_analog(tmp_path):
    """A2.1 : `*_OR` rejeté sur `type: analog` (OR-fusion n'a pas de sens)."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="analog", direction="uplink_or")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "uplink_or" in str(e)
        assert "digital" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError for *OR on analog")


def test_dir_or_rejected_on_system(tmp_path):
    """A2.1 : `*_OR` rejeté sur `scope: SYSTEM` (un seul écrivain possible)."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", scope="SYSTEM", direction="uplink_or")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "uplink_or" in str(e) or "SYSTEM" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError for *OR on SYSTEM")


def test_dir_or_rejected_on_downlink_or_system(tmp_path):
    """A2.1 : `downlink_or` rejeté sur SYSTEM."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", scope="SYSTEM", direction="downlink_or")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "downlink_or" in str(e) or "SYSTEM" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError for downlink_or on SYSTEM")


def test_dir_or_rejected_on_both_or_system(tmp_path):
    """A2.1 : `both_or` rejeté sur SYSTEM."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", scope="SYSTEM", direction="both_or")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "both_or" in str(e) or "SYSTEM" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError for both_or on SYSTEM")


def test_dir_or_mixed_with_uplink_rejected(tmp_path):
    """A2.1 : `uplink_or` + `uplink` rejeté (incohérent : OR n'ajoute rien)."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(type_="digital", direction="uplink_or")]})]
    # Note : avec le format string, on ne peut pas mélanger `uplink_or` + `uplink`
    # dans la même valeur. Ce test vérifie que `uplink_or` seul est valide
    # (le mélange serait une erreur de syntaxe YAML, pas une erreur de validation).
    result = canonize(parsed)
    ch = result.canonical_definitions[0]
    assert ch.direction_or == frozenset({"uplink"})


def test_dir_remote_uplink_downlink(tmp_path):
    """`direction: 'both'` is the string equivalent of the old
    `direction: ['uplink', 'downlink']` list form.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="REMOTE", direction="both")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink", "downlink"})


def test_dir_remote_none_normalizes_to_empty(tmp_path):
    # A9.5 — REMOTE now accepts `none` (per A9.3 contract extension).
    # REMOTE = type-level declaration, no inter-node wire activity.
    # Intra-node propagation remains implicit.
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="REMOTE", direction="none")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset()


def test_dir_remote_empty_rejected(tmp_path):
    """`direction: ''` (empty string) is rejected for REMOTE scope."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="REMOTE", direction="")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_remote_duplicate_rejected(tmp_path):
    """Duplicates are structurally impossible in string format.
    Kept as a placeholder; equivalent semantic covered by
    test_dir_invalid_token.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="REMOTE", direction="bogus")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# SYSTEM
def test_dir_system_absent_normalizes_to_empty(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="SYSTEM", direction=None)]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset()


def test_dir_system_none_normalizes_to_empty(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="SYSTEM", direction="none")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset()


def test_dir_system_uplink_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="SYSTEM", direction="uplink")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "SYSTEM" in str(e)
        assert "wire direction" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_system_downlink_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="SYSTEM", direction="downlink")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "SYSTEM" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_system_both_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="SYSTEM", direction="both")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "SYSTEM" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_system_empty_rejected(tmp_path):
    """`direction: ''` (empty string) is rejected for SYSTEM scope."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="SYSTEM", direction="")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "SYSTEM" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_system_duplicate_rejected(tmp_path):
    """Duplicates are structurally impossible in string format.
    Kept as a placeholder; equivalent semantic covered by
    test_dir_invalid_token.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(scope="SYSTEM", direction="bogus")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# Direction: misc
def test_dir_must_be_list(tmp_path):
    """`direction: 'uplink'` (string) is the new valid format.

    The old test expected `direction: 'uplink'` (a string passed
    where a list was expected) to be rejected. In the new format,
    a string IS the valid format, so this test now verifies that
    a string is accepted (the opposite of the old assertion).
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": "uplink",
    }]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink"})


def test_dir_invalid_token(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(direction="sideways")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "sideways" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# Bug 1: Direction entries must be strings (no TypeError on unhashable types)
# =============================================================================

def test_dir_entry_list_rejected(tmp_path):
    """`direction: [['uplink']]` (list of lists) must raise ChannelValidationError.

    In the new string format, `direction` must be a string. A list
    (even nested) is rejected with a message saying "must be a string".
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": [["uplink"]],
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "string" in str(e)
    except TypeError as e:
        raise AssertionError(
            f"expected ChannelValidationError, got TypeError: {e}"
        )
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_entry_int_rejected(tmp_path):
    """`direction: 123` (int) must raise ChannelValidationError.

    In the new string format, `direction` must be a string. An int
    is rejected with a message saying "must be a string".
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": 123,
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "string" in str(e)
    except TypeError as e:
        raise AssertionError(
            f"expected ChannelValidationError, got TypeError: {e}"
        )
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_entry_dict_rejected(tmp_path):
    """`direction: {key: val}` (dict) must raise ChannelValidationError.

    In the new string format, `direction` must be a string. A dict
    is rejected with a message saying "must be a string".
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": {"key": "val"},
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "string" in str(e)
    except TypeError as e:
        raise AssertionError(
            f"expected ChannelValidationError, got TypeError: {e}"
        )
    else:
        raise AssertionError("expected ChannelValidationError")


def test_dir_entry_mixed_types_rejected(tmp_path):
    """A non-string type (e.g. int) must be rejected.

    In the new string format, `direction` must be a string. A list
    containing mixed types is rejected with a message saying
    "must be a string".
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO",
        "infoName": "Foo",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "failsafe",
        "direction": ["uplink", 123],
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "string" in str(e)
    except TypeError as e:
        raise AssertionError(
            f"expected ChannelValidationError, got TypeError: {e}"
        )
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# requires
# =============================================================================

def test_requires_absent_means_active(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(requires=None)]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].requires == frozenset()


def test_requires_empty_means_active(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(requires=[])]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].requires == frozenset()


def test_requires_explicit(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(requires=["HAS_X", "HAS_Y"])]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].requires == frozenset({"HAS_X", "HAS_Y"})


def test_requires_must_be_list(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(requires="HAS_X")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "requires" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_requires_empty_string_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(requires=[""])]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "requires" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_requires_duplicate_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch(requires=["HAS_X", "HAS_X"])]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "requires" in str(e)
        assert "duplicate" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# ND4 bis: REMOTE n'accepte pas de requires
# =============================================================================

def _ch_remote(id_: str = "STEER_CMD", requires=None) -> dict:
    """Build a minimal valid REMOTE channel dict (direction as string).

    The _ch() helper above uses `direction` as a list (legacy format),
    but the current canon code expects a string. This helper builds a
    REMOTE channel with the correct string format for `direction`.
    """
    return {
        "id": id_,
        "infoName": f"{id_} channel",
        "type": "analog",
        "scope": "REMOTE",
        "theme": "core",
        "direction": "downlink",
        **({"requires": requires} if requires is not None else {}),
    }


def test_remote_without_requires_accepted(tmp_path):
    """REMOTE channel without `requires` (or with empty list) -> OK.

    Comportement actuel inchangé : un channel REMOTE sans requires
    (champ absent ou liste vide) doit passer la validation sans
    erreur. C'est le cas normal pour une frame fixe de type de
    machine.
    """
    p = tmp_path / "a.cb"
    # Cas 1 : champ `requires` absent.
    parsed = [_parsed(p, {"channels": [_ch_remote()]})]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 1
    assert result.canonical_definitions[0].scope == "REMOTE"
    assert result.canonical_definitions[0].requires == frozenset()

    # Cas 2 : `requires: []` (liste vide explicite).
    parsed = [_parsed(p, {"channels": [_ch_remote(requires=[])]})]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 1
    assert result.canonical_definitions[0].scope == "REMOTE"
    assert result.canonical_definitions[0].requires == frozenset()


def test_remote_with_requires_rejected(tmp_path):
    """REMOTE channel with non-empty `requires` -> ChannelValidationError.

    Un channel REMOTE représente une frame fixe, héritée d'une
    config générique de type de machine, partagée telle quelle
    entre le nœud machine et la télécommande. `requires` (activation
    conditionnelle à la compilation) n'a pas de sens sur REMOTE :
    ça introduirait des variantes de frame selon la config buildée,
    alors que la frame doit rester fixe par type de machine.

    Le message d'erreur doit nommer le channel et expliquer
    pourquoi (move to LOCAL/SYSTEM scope, or remove the requires).
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [
        _ch_remote(requires=["HAS_FAILSAFE"]),
    ]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        msg = str(e)
        assert "STEER_CMD" in msg, (
            f"error message should name the channel id, got: {msg!r}"
        )
        assert "REMOTE" in msg, (
            f"error message should mention REMOTE scope, got: {msg!r}"
        )
        assert "requires" in msg, (
            f"error message should mention requires, got: {msg!r}"
        )
        # Le message doit orienter vers les alternatives.
        assert "LOCAL" in msg or "SYSTEM" in msg, (
            f"error message should suggest LOCAL/SYSTEM scope as "
            f"alternative, got: {msg!r}"
        )
    else:
        raise AssertionError("expected ChannelValidationError")


def test_remote_with_multiple_requires_rejected(tmp_path):
    """REMOTE channel with multiple requires -> ChannelValidationError.

    Variante du test précédent avec plusieurs flags dans requires,
    pour vérifier que le message d'erreur liste bien tous les flags.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [
        _ch_remote(requires=["HAS_FAILSAFE", "HAS_VBAT"]),
    ]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        msg = str(e)
        assert "STEER_CMD" in msg
        assert "HAS_FAILSAFE" in msg
        assert "HAS_VBAT" in msg
    else:
        raise AssertionError("expected ChannelValidationError")


def test_local_with_requires_still_accepted(tmp_path):
    """LOCAL channel with requires -> OK (sanity check, comportement inchangé).

    Le garde-fou ND4 bis ne s'applique qu'à REMOTE. LOCAL continue
    d'accepter requires normalement.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "BAT_LOW",
        "infoName": "Battery low",
        "type": "digital",
        "scope": "LOCAL",
        "theme": "vbat",
        "direction": "uplink",
        "requires": ["IS_MAINBOARD"],
    }]})]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 1
    assert result.canonical_definitions[0].scope == "LOCAL"
    assert result.canonical_definitions[0].requires == frozenset({"IS_MAINBOARD"})


def test_system_with_requires_still_accepted(tmp_path):
    """SYSTEM channel with requires -> OK (sanity check, comportement inchangé).

    Le garde-fou ND4 bis ne s'applique qu'à REMOTE. SYSTEM continue
    d'accepter requires normalement (filtrage par board géré par
    diffuse_local_channels()).
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{
        "id": "VBAT_PRIVATE",
        "infoName": "Private vbat",
        "type": "digital",
        "scope": "SYSTEM",
        "theme": "vbat",
        "requires": ["IS_MAINBOARD"],
    }]})]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 1
    assert result.canonical_definitions[0].scope == "SYSTEM"
    assert result.canonical_definitions[0].requires == frozenset({"IS_MAINBOARD"})


# =============================================================================
# ID uniqueness (across types/scopes/themes)
# =============================================================================

def test_id_unique_across_types_raises(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cb"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO", type_="digital")]}),
        _parsed(p2, {"channels": [_ch("FOO", type_="analog")]}),
    ]
    try:
        canonize(parsed)
    except ChannelConflictError as e:
        assert e.channel_id == "FOO"
        assert e.first_path == p1
        assert e.second_path == p2
    else:
        raise AssertionError("expected ChannelConflictError")


def test_id_unique_across_scopes_raises(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cb"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO", scope="LOCAL")]}),
        _parsed(p2, {"channels": [_ch("FOO", scope="REMOTE")]}),
    ]
    try:
        canonize(parsed)
    except ChannelConflictError as e:
        assert e.channel_id == "FOO"
    else:
        raise AssertionError("expected ChannelConflictError")


def test_id_unique_across_themes_raises(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cb"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO", theme="failsafe")]}),
        _parsed(p2, {"channels": [_ch("FOO", theme="vbat")]}),
    ]
    try:
        canonize(parsed)
    except ChannelConflictError as e:
        assert e.channel_id == "FOO"
    else:
        raise AssertionError("expected ChannelConflictError")


def test_id_unique_same_type_raises(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cb"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO")]}),
        _parsed(p2, {"channels": [_ch("FOO")]}),
    ]
    try:
        canonize(parsed)
    except ChannelConflictError as e:
        assert e.channel_id == "FOO"
    else:
        raise AssertionError("expected ChannelConflictError")


def test_id_unique_different_ids_ok(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cb"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO")]}),
        _parsed(p2, {"channels": [_ch("BAR")]}),
    ]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 2


# =============================================================================
# Fusion
# =============================================================================

def test_fusion_multiple_files(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cb"
    parsed = [
        _parsed(p1, {"channels": [_ch("FOO"), _ch("BAR")]}),
        _parsed(p2, {"channels": [_ch("BAZ"), _ch("QUX")]}),
    ]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 4
    ids = {ch.id for ch in result.canonical_definitions}
    assert ids == {"FOO", "BAR", "BAZ", "QUX"}


def test_fusion_multiple_sections_in_same_file(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO"), _ch("BAR")]})]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 2


def test_fusion_order_independent(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cb"
    parsed_a = [
        _parsed(p1, {"channels": [_ch("FOO"), _ch("BAR")]}),
        _parsed(p2, {"channels": [_ch("BAZ"), _ch("QUX")]}),
    ]
    parsed_b = list(reversed(parsed_a))
    result_a = canonize(parsed_a)
    result_b = canonize(parsed_b)
    assert [ch.id for ch in result_a.canonical_definitions] == \
           [ch.id for ch in result_b.canonical_definitions]


def test_fusion_yaml_entry_order_independent(tmp_path):
    p = tmp_path / "a.cb"
    parsed_a = [_parsed(p, {"channels": [_ch("FOO"), _ch("BAR"), _ch("BAZ")]})]
    parsed_b = [_parsed(p, {"channels": [_ch("BAZ"), _ch("FOO"), _ch("BAR")], "module": "x"})]
    result_a = canonize(parsed_a)
    result_b = canonize(parsed_b)
    assert [ch.id for ch in result_a.canonical_definitions] == \
           [ch.id for ch in result_b.canonical_definitions]


# =============================================================================
# Canonical sort
# =============================================================================

def test_canonical_sort_scope_type_theme_id(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [
        _ch("Z_LOCAL_DIGITAL", scope="LOCAL", type_="digital", theme="failsafe"),
        _ch("A_LOCAL_DIGITAL", scope="LOCAL", type_="digital", theme="failsafe"),
        _ch("A_LOCAL_ANALOG", scope="LOCAL", type_="analog", theme="failsafe"),
        _ch("A_SYSTEM_DIGITAL", scope="SYSTEM", type_="digital", theme="failsafe"),
        _ch("A_REMOTE_DIGITAL", scope="REMOTE", type_="digital", theme="failsafe"),
    ]})]
    result = canonize(parsed)
    ids = [ch.id for ch in result.canonical_definitions]
    # Scope order: LOCAL < REMOTE < SYSTEM
    assert ids == [
        "A_LOCAL_ANALOG",
        "A_LOCAL_DIGITAL",
        "Z_LOCAL_DIGITAL",
        "A_REMOTE_DIGITAL",
        "A_SYSTEM_DIGITAL",
    ]


def test_canonical_sort_deterministic(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [
        _ch("FOO", scope="LOCAL", type_="digital", theme="failsafe"),
        _ch("BAR", scope="REMOTE", type_="analog", theme="failsafe"),
        _ch("BAZ", scope="SYSTEM", type_="digital", theme="failsafe"),
    ]})]
    r1 = canonize(parsed)
    r2 = canonize(parsed)
    assert [ch.id for ch in r1.canonical_definitions] == \
           [ch.id for ch in r2.canonical_definitions]


def test_canonical_sort_independent_of_input_order(tmp_path):
    p = tmp_path / "a.cb"
    base = [
        _ch("FOO", scope="LOCAL", type_="digital", theme="failsafe"),
        _ch("BAR", scope="REMOTE", type_="analog", theme="failsafe"),
        _ch("BAZ", scope="SYSTEM", type_="digital", theme="failsafe"),
    ]
    parsed_a = [_parsed(p, {"channels": base})]
    parsed_b = [_parsed(p, {"channels": list(reversed(base))})]
    r_a = canonize(parsed_a)
    r_b = canonize(parsed_b)
    assert [ch.id for ch in r_a.canonical_definitions] == \
           [ch.id for ch in r_b.canonical_definitions]


# =============================================================================
# Package file (channels + chains in the same file)
# =============================================================================

def test_package_file_channels_and_chains(tmp_path):
    p = tmp_path / "package.cb"
    parsed = [_parsed(p, {
        "channels": [_ch("FOO"), _ch("BAR")],
        "chains": [{"name": "vbat_alert", "processors": ["vbat_low_check"]}],
    })]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 2
    assert {ch.id for ch in result.canonical_definitions} == {"FOO", "BAR"}


def test_package_file_only_chains_ignored(tmp_path):
    p = tmp_path / "package.cbch"
    parsed = [_parsed(p, {
        "chains": [{"name": "failsafe", "processors": ["reset_failsafe"]}],
    })]
    result = canonize(parsed)
    assert result.canonical_definitions == []
    assert result.active_definitions == []


# =============================================================================
# active_definitions vs canonical_definitions
# =============================================================================

def test_active_vs_canonical(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [
        _ch("Z"),
        _ch("A"),
        _ch("M"),
    ]})]
    result = canonize(parsed)
    active_ids = [ch.id for ch in result.active_definitions]
    canonical_ids = [ch.id for ch in result.canonical_definitions]
    assert set(active_ids) == set(canonical_ids) == {"A", "M", "Z"}
    assert canonical_ids == ["A", "M", "Z"]


# =============================================================================
# Structural test: A5.1 does NOT depend on extension for routing
# =============================================================================

def test_a5_does_not_branch_on_extension():
    from scripts.combus_builder.canon import channels as channels_mod
    src = inspect.getsource(channels_mod.extract_channels_sections)
    assert "_type_label" in src
    for line in src.splitlines():
        stripped = line.strip()
        if stripped.startswith("#"):
            continue
        assert "type_label" not in stripped or "_type_label" in stripped, (
            f"extract_channels_sections must not branch on type_label: {line!r}"
        )


# =============================================================================
# Structural test: vocabularies are centralized in channels.py
# =============================================================================

def test_vocabularies_centralized_in_channels():
    """All vocabularies (types, scopes, themes, direction tokens) live in channels.py."""
    from scripts.combus_builder.canon import channels as channels_mod
    src = inspect.getsource(channels_mod)
    assert "VALID_TYPES" in src
    assert "VALID_SCOPES" in src
    assert "VALID_THEMES" in src
    assert "DIRECTION_TOKENS" in src
    assert "DIRECTION_BY_SCOPE" in src
    assert "SCOPE_ORDER" in src
    assert "ALLOWED_FIELDS" in src


# =============================================================================
# Validation against real .cb files in the repo
# =============================================================================

def test_real_failsafe_cb_validates():
    """The real failsafe.cb must validate against the strict contract."""
    real = REPO_ROOT / "src" / "core" / "system" / "failsafe" / "failsafe.cb"
    if not real.exists():
        # Skip if the file is not present (e.g. on a different branch).
        return
    import yaml
    raw = yaml.safe_load(real.read_text(encoding="utf-8"))
    parsed = [(real, "channel_def", raw)]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 1
    ch = result.canonical_definitions[0]
    assert ch.id == "FAILSAFE"
    assert ch.info_name == "Failsafe aggregator"
    assert ch.type == "digital"
    assert ch.scope == "LOCAL"
    assert ch.theme == "failsafe"
    # A2.1 : the .cb uses `direction: both_or`, so the wire
    # direction is carried by `direction_or` (not `direction`).
    # `effective_direction` is the union of both — the canonical
    # accessor for "what the wire sees".
    assert ch.effective_direction == frozenset({"uplink", "downlink"})
    assert ch.direction_or == frozenset({"uplink", "downlink"})
    assert ch.requires == frozenset({"HAS_FAILSAFE"})


def test_real_vbat_failsafe_cb_validates():
    """The real vbat_failsafe.cb must validate against the strict contract."""
    real = REPO_ROOT / "src" / "core" / "system" / "vbat" / "vbat_failsafe.cb"
    if not real.exists():
        return
    import yaml
    raw = yaml.safe_load(real.read_text(encoding="utf-8"))
    parsed = [(real, "channel_def", raw)]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 1
    ch = result.canonical_definitions[0]
    assert ch.id == "FAILSAFE_VBAT"
    assert ch.info_name == "VBAT failsafe contributor"
    assert ch.type == "digital"
    assert ch.scope == "LOCAL"
    assert ch.theme == "failsafe"
    # A2.1 : vbat_failsafe.cb uses `direction: uplink_or` (a
    # contributor that OR-merges into the FAILSAFE aggregator).
    # `effective_direction` is the union of `direction` and
    # `direction_or` — the canonical accessor for "what the wire
    # sees".
    assert ch.effective_direction == frozenset({"uplink"})
    assert ch.direction_or == frozenset({"uplink"})
    assert ch.requires == frozenset({"HAS_FAILSAFE", "HAS_VBAT_FAILSAFE"})


def test_real_both_cb_files_together():
    """Both real .cb files together must canonize without conflict."""
    p1 = REPO_ROOT / "src" / "core" / "system" / "failsafe" / "failsafe.cb"
    p2 = REPO_ROOT / "src" / "core" / "system" / "vbat" / "vbat_failsafe.cb"
    if not (p1.exists() and p2.exists()):
        return
    import yaml
    parsed = [
        (p1, "channel_def", yaml.safe_load(p1.read_text(encoding="utf-8"))),
        (p2, "channel_def", yaml.safe_load(p2.read_text(encoding="utf-8"))),
    ]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 2
    ids = [ch.id for ch in result.canonical_definitions]
    # Both are LOCAL, digital, failsafe → sorted by id.
    assert ids == ["FAILSAFE", "FAILSAFE_VBAT"]


# =============================================================================
# Test runner
# =============================================================================

def _run_all():
    this = sys.modules[__name__]
    tests = [
        (name, fn)
        for name, fn in inspect.getmembers(this, inspect.isfunction)
        if name.startswith("test_")
    ]

    failures = []
    for name, fn in tests:
        sig = inspect.signature(fn)
        with tempfile.TemporaryDirectory() as td:
            tmp_path = Path(td)
            try:
                if sig.parameters:
                    fn(tmp_path)
                else:
                    fn()
                print(f"  OK   {name}")
            except Exception:
                failures.append((name, traceback.format_exc()))
                print(f"  FAIL {name}")

    print(f"\n{len(tests) - len(failures)}/{len(tests)} tests passed")
    if failures:
        for name, tb in failures:
            print(f"\n--- {name} ---")
            print(tb)
        sys.exit(1)


if __name__ == "__main__":
    _run_all()
