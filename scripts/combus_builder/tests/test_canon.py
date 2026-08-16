#!/usr/bin/env python3
"""
Tests for canon.py — A5.

Coverage:
  - Section extraction (extension-agnostic).
  - Per-channel validation (each field, each error path).
  - Direction defaults (SYSTEM → none, LOCAL/REMOTE → required).
  - ID uniqueness (across types).
  - Conflict detection (with file paths in the error).
  - Fusion (multiple files, multiple sections).
  - Canonical sort (scope, type, theme, id).
  - Determinism (same set, different order → same canonical output).
  - Package file (channels + chains in the same file).
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
    CanonError,
    ChannelConflictError,
    ChannelDefinition,
    ChannelValidationError,
    VALID_DIRECTIONS,
    VALID_SCOPES,
    VALID_TYPES,
    canonize,
    extract_channels_sections,
)


# =============================================================================
# Helpers
# =============================================================================

def _parsed(path: Path, raw: dict, type_label: str = "channel_def"):
    """Build a (path, type_label, raw_dict) tuple as A3 would return."""
    return (path, type_label, raw)


def _ch(id_: str, type_: str = "digital", scope: str = "LOCAL",
        theme: str = "test", direction: list | None = None,
        requires: list | None = None, info_name: str | None = None) -> dict:
    """Build a minimal valid channel dict.

    For LOCAL/REMOTE scopes, direction defaults to ["uplink"] if not
    explicitly provided (since direction is required for those scopes).
    For SYSTEM, direction defaults to None (which means "none").
    """
    d: dict = {"id": id_, "type": type_, "scope": scope, "theme": theme}
    if direction is not None:
        d["direction"] = direction
    elif scope in ("LOCAL", "REMOTE"):
        d["direction"] = ["uplink"]
    if requires is not None:
        d["requires"] = requires
    if info_name is not None:
        d["infoName"] = info_name
    return d


# =============================================================================
# Section extraction
# =============================================================================

def test_extract_sections_basic(tmp_path):
    p1 = tmp_path / "a.cb"
    p2 = tmp_path / "b.cbch"  # extension-agnostic
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
        _parsed(p2, {"chains": [{"name": "x"}]}),  # no channels
    ]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 1
    assert sections[0][0] == p1


def test_extract_sections_empty_channels_returns_empty_section(tmp_path):
    """An empty channels list is a valid (empty) section, not a skip."""
    p1 = tmp_path / "a.cb"
    parsed = [_parsed(p1, {"channels": []})]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 1
    assert sections[0][0] == p1
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
    """
    A `.cbch` file with a `channels:` section is treated identically
    to a `.cb` file with a `channels:` section. This is the explicit
    "extension does not determine content" rule.
    """
    p_cb = tmp_path / "a.cb"
    p_cbch = tmp_path / "b.cbch"
    parsed = [
        _parsed(p_cb, {"channels": [_ch("FOO")]}, type_label="channel_def"),
        _parsed(p_cbch, {"channels": [_ch("BAR")]}, type_label="chain_def"),
    ]
    sections = extract_channels_sections(parsed)
    assert len(sections) == 2
    # type_label is ignored — both are picked up.
    assert {s[0] for s in sections} == {p_cb, p_cbch}


# =============================================================================
# Per-channel validation
# =============================================================================

def test_validate_minimal_channel(tmp_path):
    p = tmp_path / "a.cb"
    # Use SYSTEM scope so direction defaults to "none" (empty frozenset).
    parsed = [_parsed(p, {"channels": [_ch("FOO", scope="SYSTEM")]})]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 1
    ch = result.canonical_definitions[0]
    assert ch.id == "FOO"
    assert ch.type == "digital"
    assert ch.scope == "SYSTEM"
    assert ch.theme == "test"
    assert ch.direction == frozenset()
    assert ch.requires == frozenset()
    assert ch.info_name is None


def test_validate_missing_id(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [{"type": "digital", "scope": "LOCAL",
                                          "theme": "t", "direction": ["uplink"]}]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "id" in str(e).lower()
    else:
        raise AssertionError("expected ChannelValidationError")


def test_validate_empty_id(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "non-empty" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_validate_invalid_type(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", type_="bool")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "type" in str(e)
        assert "bool" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_validate_invalid_scope(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", scope="GLOBAL")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "scope" in str(e)
        assert "GLOBAL" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_validate_empty_theme(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", theme="")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "theme" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_validate_channel_must_be_mapping(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": ["not a mapping"]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "mapping" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# Direction defaults
# =============================================================================

def test_direction_required_for_local(tmp_path):
    p = tmp_path / "a.cb"
    # Build dict manually to bypass the helper's auto-direction.
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO", "type": "digital", "scope": "LOCAL", "theme": "t",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "LOCAL" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError for LOCAL without direction")


def test_direction_required_for_remote(tmp_path):
    p = tmp_path / "a.cb"
    # Build dict manually to bypass the helper's auto-direction.
    parsed = [_parsed(p, {"channels": [{
        "id": "FOO", "type": "digital", "scope": "REMOTE", "theme": "t",
    }]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "REMOTE" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError for REMOTE without direction")


def test_direction_optional_for_system_defaults_to_none(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", scope="SYSTEM")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset()


def test_direction_explicit_uplink(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", direction=["uplink"])]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink"})


def test_direction_explicit_downlink(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", direction=["downlink"])]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"downlink"})


def test_direction_bidirectional(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", direction=["uplink", "downlink"])]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink", "downlink"})


def test_direction_invalid_entry(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", direction=["sideways"])]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "sideways" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_direction_must_be_list(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", direction="uplink")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "direction" in str(e)
        assert "list" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_direction_dedup(tmp_path):
    """Duplicates in direction are silently deduped (not an error)."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", direction=["uplink", "uplink"])]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].direction == frozenset({"uplink"})


# =============================================================================
# requires
# =============================================================================

def test_requires_optional(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].requires == frozenset()


def test_requires_explicit(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", requires=["HAS_X", "HAS_Y"])]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].requires == frozenset({"HAS_X", "HAS_Y"})


def test_requires_must_be_list(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", requires="HAS_X")]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "requires" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


def test_requires_empty_string_rejected(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", requires=[""])]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "requires" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# infoName
# =============================================================================

def test_info_name_optional(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].info_name is None


def test_info_name_explicit(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", info_name="My channel")]})]
    result = canonize(parsed)
    assert result.canonical_definitions[0].info_name == "My channel"


def test_info_name_must_be_string(tmp_path):
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [_ch("FOO", info_name=42)]})]
    try:
        canonize(parsed)
    except ChannelValidationError as e:
        assert "infoName" in str(e)
    else:
        raise AssertionError("expected ChannelValidationError")


# =============================================================================
# ID uniqueness (across types)
# =============================================================================

def test_id_unique_across_types_raises(tmp_path):
    """
    Same id with different types is a CONFLICT, not a silent merge.
    Per doc §10: id is unique globally, all types confounded.
    """
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
        assert "FOO" in str(e)
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
    parsed = [_parsed(p, {"channels": [_ch("FOO"), _ch("BAR")], "module": "x"})]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 2


def test_fusion_order_independent(tmp_path):
    """
    Same set of channels, different file order → same canonical output.
    """
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
    """
    Same channels in different YAML order → same canonical output.
    """
    p = tmp_path / "a.cb"
    parsed_a = [_parsed(p, {"channels": [_ch("FOO"), _ch("BAR"), _ch("BAZ")], "module": "x"})]
    parsed_b = [_parsed(p, {"channels": [_ch("BAZ"), _ch("FOO"), _ch("BAR")], "module": "x"})]
    result_a = canonize(parsed_a)
    result_b = canonize(parsed_b)
    assert [ch.id for ch in result_a.canonical_definitions] == \
           [ch.id for ch in result_b.canonical_definitions]


# =============================================================================
# Canonical sort
# =============================================================================

def test_canonical_sort_scope_type_theme_id(tmp_path):
    """
    Verify the canonical order is (scope, type, theme, id) and not
    just id-sorted.
    """
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [
        _ch("Z_LOCAL_DIGITAL", scope="LOCAL", type_="digital", theme="z"),
        _ch("A_LOCAL_DIGITAL", scope="LOCAL", type_="digital", theme="a"),
        _ch("A_LOCAL_ANALOG", scope="LOCAL", type_="analog", theme="a"),
        _ch("A_SYSTEM_DIGITAL", scope="SYSTEM", type_="digital", theme="a"),
        _ch("A_REMOTE_DIGITAL", scope="REMOTE", type_="digital", theme="a"),
    ]})]
    result = canonize(parsed)
    ids = [ch.id for ch in result.canonical_definitions]
    # Alphabetical scope order: LOCAL < REMOTE < SYSTEM
    # Within LOCAL: analog < digital, then theme, then id
    assert ids == [
        "A_LOCAL_ANALOG",     # LOCAL, analog, a, A
        "A_LOCAL_DIGITAL",    # LOCAL, digital, a, A
        "Z_LOCAL_DIGITAL",    # LOCAL, digital, z, Z
        "A_REMOTE_DIGITAL",   # REMOTE, digital, a, A
        "A_SYSTEM_DIGITAL",   # SYSTEM, digital, a, A
    ]


def test_canonical_sort_deterministic(tmp_path):
    """Two runs with the same input → byte-identical output."""
    p = tmp_path / "a.cb"
    parsed = [_parsed(p, {"channels": [
        _ch("FOO", scope="LOCAL", type_="digital", theme="t"),
        _ch("BAR", scope="REMOTE", type_="analog", theme="t"),
        _ch("BAZ", scope="SYSTEM", type_="digital", theme="t"),
    ]})]
    r1 = canonize(parsed)
    r2 = canonize(parsed)
    assert [ch.id for ch in r1.canonical_definitions] == \
           [ch.id for ch in r2.canonical_definitions]


def test_canonical_sort_independent_of_input_order(tmp_path):
    """Permuting the input order does not change the canonical output."""
    p = tmp_path / "a.cb"
    base = [
        _ch("FOO", scope="LOCAL", type_="digital", theme="t"),
        _ch("BAR", scope="REMOTE", type_="analog", theme="t"),
        _ch("BAZ", scope="SYSTEM", type_="digital", theme="t"),
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
    """
    A file with both `channels:` and `chains:` sections is accepted.
    A5 only consumes `channels:`; `chains:` is left for Phase C.
    """
    p = tmp_path / "package.cb"
    parsed = [_parsed(p, {
        "module": "vbat",
        "channels": [_ch("FOO"), _ch("BAR")],
        "chains": [{"name": "vbat_alert", "processors": ["vbat_low_check"]}],
    })]
    result = canonize(parsed)
    assert len(result.canonical_definitions) == 2
    assert {ch.id for ch in result.canonical_definitions} == {"FOO", "BAR"}


def test_package_file_only_chains_ignored(tmp_path):
    """A file with only `chains:` (no `channels:`) is silently ignored by A5."""
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
    """
    active_definitions is the unsorted, validated list.
    canonical_definitions is the same content, sorted.
    """
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
    assert canonical_ids == ["A", "M", "Z"]  # sorted


# =============================================================================
# Structural test: A5 does NOT depend on extension for routing
# =============================================================================

def test_a5_does_not_branch_on_extension():
    """
    Pin the architectural rule: A5 selects sections by KEY, not by
    extension. We verify by introspecting the source of canon.py:
    extract_channels_sections must not reference type_label.
    """
    from scripts.combus_builder import canon as canon_mod
    src = inspect.getsource(canon_mod.extract_channels_sections)
    # type_label is the second element of the tuple; we should not
    # branch on it. The function signature uses _type_label (underscore
    # prefix) to signal "intentionally unused".
    assert "_type_label" in src, (
        "extract_channels_sections should bind type_label as _type_label "
        "to signal that it is intentionally unused."
    )
    # And it must not appear in any conditional.
    for line in src.splitlines():
        stripped = line.strip()
        if stripped.startswith("#"):
            continue
        # type_label (without underscore) must not appear in a branch.
        assert "type_label" not in stripped or "_type_label" in stripped, (
            f"extract_channels_sections must not branch on type_label: {line!r}"
        )


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
