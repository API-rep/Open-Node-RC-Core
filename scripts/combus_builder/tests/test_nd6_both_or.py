#!/usr/bin/env python3
"""
ND6 tests — A2.1 representation: `direction` (frozenset) + `direction_or`
(frozenset), mutually exclusive. No `ch.both_or` bool. No LOCAL-only
restriction on `both_or` (it is accepted on LOCAL and REMOTE).

This file replaces the previous test_nd6_both_or.py which tested the
old `ch.both_or` bool field. The old tests are removed because they
no longer make sense: `both_or` is a DIRECTION TOKEN (not a separate
field), and its semantics are carried by `direction_or`.

Coverage:
  - canon/channels.py: `both_or` is a direction token, accepted on
    LOCAL and REMOTE, rejected on SYSTEM and on analog.
  - diffusion.py: family-based rule (uplink-ish, downlink-ish,
    both-ish). 2 uplink-ish = fatal, 1 uplink-ish → absentes
    `downlink`, both-ish only → absentes `both`/`both_or`,
    both-ish + uplink/downlink = fatal sémantique, both + both_or
    = warning non fatal.
  - md5.py: payload includes `direction` + `direction_or`. LOCAL
    view applies ONE_WAY canonisation (uplink/downlink/uplink_or/
    downlink_or → same hash). REMOTE view: no canonisation.
  - generator.py: bitmask via `effective_direction`, `.bothOr = true`
    iff `direction_or == {uplink, downlink}`.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

# Make the combus_builder package importable.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from combus_builder.canon.channels import (
    DIRECTION_TOKENS,
    ChannelValidationError,
    validate_channel,
)
from combus_builder.diffusion import (
    DiffusionConflictError,
    _resolve_group,
)
from combus_builder.generator import Direction, View, ViewChannel, _emit_channel_init
from combus_builder.md5 import compute_view_hash


# =============================================================================
# HELPERS
# =============================================================================

def _make_channel(
    cid: str = "D1",
    *,
    scope: str = "LOCAL",
    ctype: str = "digital",
    direction: str = "both_or",
    theme: str = "core",
    requires: list[str] | None = None,
) -> dict:
    """Build a raw channel dict for tests."""
    raw = {
        "id": cid,
        "infoName": f"Test {cid}",
        "type": ctype,
        "scope": scope,
        "theme": theme,
        "direction": direction,
    }
    if requires is not None:
        raw["requires"] = requires
    return raw


def _canon_from_yamls(*raws: dict) -> list:
    """Validate a list of raw channel dicts and return ChannelDefinitions."""
    out = []
    for i, raw in enumerate(raws):
        ch = validate_channel(Path(f"test_{i}.cb"), raw, 0)
        out.append(ch)
    return out


def _view_with(channels: list, name: str = "combus_local") -> View:
    """Build a View from a list of ChannelDefinitions."""
    vcs = [
        ViewChannel(numeric_id=i, ch=ch, direction_bits=0)
        for i, ch in enumerate(channels)
    ]
    return View(
        name=name,
        channels=vcs,
        wire_end=len(vcs),
        wire_end_analog=0,
        wire_end_digital=len(vcs),
    )


# =============================================================================
# CANON: both_or accepted on LOCAL and REMOTE, rejected on SYSTEM and analog
# =============================================================================

def test_canon_both_or_accepted_for_local_digital():
    """direction=both_or with scope=LOCAL + type=digital → accepted."""
    raw = _make_channel(direction="both_or")
    chs = _canon_from_yamls(raw)
    assert chs[0].direction == frozenset()
    assert chs[0].direction_or == frozenset({"uplink", "downlink"})


def test_canon_both_or_accepted_for_remote_digital():
    """direction=both_or with scope=REMOTE + type=digital → accepted (A2.1)."""
    raw = _make_channel(scope="REMOTE", direction="both_or")
    chs = _canon_from_yamls(raw)
    assert chs[0].direction == frozenset()
    assert chs[0].direction_or == frozenset({"uplink", "downlink"})


def test_canon_both_or_default_false_when_token_is_both():
    """direction=both (not both_or) → direction_or=frozenset()."""
    raw = _make_channel(direction="both")
    chs = _canon_from_yamls(raw)
    assert chs[0].direction == frozenset({"uplink", "downlink"})
    assert chs[0].direction_or == frozenset()


def test_canon_both_or_default_false_when_token_is_uplink():
    """direction=uplink → direction_or=frozenset()."""
    raw = _make_channel(direction="uplink")
    chs = _canon_from_yamls(raw)
    assert chs[0].direction == frozenset({"uplink"})
    assert chs[0].direction_or == frozenset()


def test_canon_both_or_rejected_for_system_scope():
    """direction=both_or with scope=SYSTEM → explicit rejection."""
    raw = _make_channel(scope="SYSTEM", direction="both_or")
    with pytest.raises(ChannelValidationError) as exc_info:
        validate_channel(Path("test.cb"), raw, 0)
    assert "both_or" in str(exc_info.value) or "SYSTEM" in str(exc_info.value)


def test_canon_both_or_rejected_for_analog_type():
    """direction=both_or with type=analog → explicit rejection (OR is boolean)."""
    raw = _make_channel(ctype="analog", direction="both_or")
    with pytest.raises(ChannelValidationError) as exc_info:
        validate_channel(Path("test.cb"), raw, 0)
    assert "both_or" in str(exc_info.value) or "analog" in str(exc_info.value)


def test_canon_both_or_in_direction_tokens():
    """`both_or` is registered in DIRECTION_TOKENS."""
    assert "both_or" in DIRECTION_TOKENS


# =============================================================================
# DIFFUSION: family-based rule (A2.1)
# =============================================================================

def test_diffusion_two_uplink_ish_is_fatal():
    """2 uplink-ish (uplink + uplink_or) = FATAL."""
    explicit = [
        ("board_a", "uplink", Path("a.cb")),
        ("board_b", "uplink_or", Path("b.cb")),
    ]
    with pytest.raises(DiffusionConflictError):
        _resolve_group("CH", explicit, ["board_a", "board_b"])


def test_diffusion_one_uplink_ish_absentees_get_downlink():
    """1 uplink-ish (uplink_or) → absentes reçoivent `downlink` (token simple)."""
    explicit = [
        ("board_a", "uplink_or", Path("a.cb")),
    ]
    group = _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert group.synth_for_absent == [("board_b", "downlink")]


def test_diffusion_one_uplink_ish_uplink_token_absentees_get_downlink():
    """1 uplink-ish (uplink) → absentes reçoivent `downlink`."""
    explicit = [
        ("board_a", "uplink", Path("a.cb")),
    ]
    group = _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert group.synth_for_absent == [("board_b", "downlink")]


def test_diffusion_both_only_absentees_get_both():
    """both seul → absentes reçoivent `both`."""
    explicit = [
        ("board_a", "both", Path("a.cb")),
    ]
    group = _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert group.synth_for_absent == [("board_b", "both")]


def test_diffusion_both_or_only_absentees_get_both_or():
    """both_or seul → absentes reçoivent `both_or` (OR préservé)."""
    explicit = [
        ("board_a", "both_or", Path("a.cb")),
    ]
    group = _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert group.synth_for_absent == [("board_b", "both_or")]


def test_diffusion_both_ish_mixed_with_uplink_is_fatal_semantic():
    """both-ish + uplink-ish = FATAL sémantique."""
    explicit = [
        ("board_a", "both", Path("a.cb")),
        ("board_b", "uplink", Path("b.cb")),
    ]
    with pytest.raises(DiffusionConflictError) as exc_info:
        _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert exc_info.value.semantic is True


def test_diffusion_both_ish_mixed_with_downlink_is_fatal_semantic():
    """both_or + downlink = FATAL sémantique."""
    explicit = [
        ("board_a", "both_or", Path("a.cb")),
        ("board_b", "downlink", Path("b.cb")),
    ]
    with pytest.raises(DiffusionConflictError) as exc_info:
        _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert exc_info.value.semantic is True


def test_diffusion_both_plus_both_or_is_warning_not_fatal():
    """both + both_or = WARNING non fatal (build continues)."""
    explicit = [
        ("board_a", "both", Path("a.cb")),
        ("board_b", "both_or", Path("b.cb")),
    ]
    group = _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert group.warning is not None
    assert "both vs both_or divergence" in group.warning


def test_diffusion_downlink_only_is_warning_canal_mort():
    """0 uplink-ish + downlink-ish = WARNING "canal mort", absentes `none`."""
    explicit = [
        ("board_a", "downlink", Path("a.cb")),
    ]
    group = _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert group.warning is not None
    assert "canal mort" in group.warning
    assert group.synth_for_absent == [("board_b", "none")]


def test_diffusion_none_only_no_synthesis_no_warning():
    """none seul → pas de synthèse, pas de warning."""
    explicit = [
        ("board_a", "none", Path("a.cb")),
    ]
    group = _resolve_group("CH", explicit, ["board_a", "board_b"])
    assert group.synth_for_absent == []
    assert group.warning is None


# =============================================================================
# MD5: groups of equality (A2.1)
# =============================================================================

def test_md5_remote_distinguishes_all_7_tokens():
    """REMOTE view: 7 tokens → 7 hashs distincts (no ONE_WAY)."""
    tokens = ["uplink", "downlink", "both", "none",
              "uplink_or", "downlink_or", "both_or"]
    hashes = set()
    for tok in tokens:
        chs = _canon_from_yamls(_make_channel(direction=tok))
        v = _view_with(chs, name="combus_remote")
        hashes.add(compute_view_hash(v).md5_hex)
    assert len(hashes) == 7


def test_md5_local_one_way_groups():
    """LOCAL view: ONE_WAY canonisation.
    Groups: {uplink, downlink, uplink_or, downlink_or} | {both} | {both_or} | {none}."""
    tokens = ["uplink", "downlink", "both", "none",
              "uplink_or", "downlink_or", "both_or"]
    hash_to_tokens: dict[str, list[str]] = {}
    for tok in tokens:
        chs = _canon_from_yamls(_make_channel(direction=tok))
        v = _view_with(chs, name="combus_local")
        h = compute_view_hash(v).md5_hex
        hash_to_tokens.setdefault(h, []).append(tok)
    # Expect 4 groups.
    assert len(hash_to_tokens) == 4
    # Find the ONE_WAY group (4 tokens) and the singletons.
    one_way_group = None
    singletons: dict[str, list[str]] = {}
    for h, toks in hash_to_tokens.items():
        if len(toks) == 4:
            one_way_group = toks
        else:
            singletons[toks[0]] = toks
    assert set(one_way_group) == {"uplink", "downlink", "uplink_or", "downlink_or"}
    assert singletons["both"] == ["both"]
    assert singletons["both_or"] == ["both_or"]
    assert singletons["none"] == ["none"]


def test_md5_both_and_both_or_produce_different_hash():
    """`both` et `both_or` produisent des hash différents (même vue)."""
    chs_both = _canon_from_yamls(_make_channel(direction="both"))
    chs_both_or = _canon_from_yamls(_make_channel(direction="both_or"))
    v_both = _view_with(chs_both, name="combus_remote")
    v_both_or = _view_with(chs_both_or, name="combus_remote")
    h_both = compute_view_hash(v_both).md5_hex
    h_both_or = compute_view_hash(v_both_or).md5_hex
    assert h_both != h_both_or


def test_md5_uplink_and_uplink_or_produce_different_hash_remote():
    """REMOTE: `uplink` et `uplink_or` produisent des hash différents."""
    chs_uplink = _canon_from_yamls(_make_channel(direction="uplink"))
    chs_uplink_or = _canon_from_yamls(_make_channel(direction="uplink_or"))
    v_uplink = _view_with(chs_uplink, name="combus_remote")
    v_uplink_or = _view_with(chs_uplink_or, name="combus_remote")
    h_uplink = compute_view_hash(v_uplink).md5_hex
    h_uplink_or = compute_view_hash(v_uplink_or).md5_hex
    assert h_uplink != h_uplink_or


def test_md5_uplink_and_uplink_or_produce_same_hash_local():
    """LOCAL: `uplink` et `uplink_or` produisent le même hash (ONE_WAY)."""
    chs_uplink = _canon_from_yamls(_make_channel(direction="uplink"))
    chs_uplink_or = _canon_from_yamls(_make_channel(direction="uplink_or"))
    v_uplink = _view_with(chs_uplink, name="combus_local")
    v_uplink_or = _view_with(chs_uplink_or, name="combus_local")
    h_uplink = compute_view_hash(v_uplink).md5_hex
    h_uplink_or = compute_view_hash(v_uplink_or).md5_hex
    assert h_uplink == h_uplink_or


# =============================================================================
# GENERATOR: bitmask via effective_direction, .bothOr for each of 7 tokens
# =============================================================================

def _emit_for_token(tok: str, ctype: str = "digital") -> str:
    """Build a single channel and return the emitted C++ initializer line."""
    from combus_builder.generator import _emit_channel_init
    raw = _make_channel(direction=tok, ctype=ctype)
    ch = validate_channel(Path("test.cb"), raw, 0)
    vc = ViewChannel(numeric_id=0, ch=ch, direction_bits=Direction.from_frozenset(ch.effective_direction))
    body: list[str] = []
    _emit_channel_init(body, vc, ctype)
    return body[0]


def test_generator_bitmask_and_bothor_for_all_7_tokens():
    """Pour chacun des 7 tokens, vérifier le bitmask Direction::X et .bothOr."""
    expected = {
        "uplink":      ("Direction::Uplink",   "false"),
        "downlink":    ("Direction::Downlink", "false"),
        "both":        ("Direction::Both",     "false"),
        "none":        ("Direction::None",     "false"),
        "uplink_or":   ("Direction::Uplink",   "false"),
        "downlink_or": ("Direction::Downlink", "false"),
        "both_or":     ("Direction::Both",     "true"),
    }
    for tok, (dir_name, both_or) in expected.items():
        line = _emit_for_token(tok)
        assert f".direction = {dir_name}" in line, f"{tok}: expected {dir_name}, got {line}"
        assert f".bothOr = {both_or}" in line, f"{tok}: expected .bothOr = {both_or}, got {line}"


def test_generator_analog_never_emits_bothor():
    """Pour un channel analog, .bothOr n'est jamais émis (champ inexistant)."""
    line = _emit_for_token("both", ctype="analog")
    assert ".bothOr" not in line
    assert ".direction = Direction::Both" in line


def test_generator_uplink_or_emits_uplink_not_none():
    """Régression : uplink_or doit produire Direction::Uplink (pas None)."""
    line = _emit_for_token("uplink_or")
    assert ".direction = Direction::Uplink" in line
    assert ".direction = Direction::None" not in line


def test_generator_downlink_or_emits_downlink_not_none():
    """Régression : downlink_or doit produire Direction::Downlink (pas None)."""
    line = _emit_for_token("downlink_or")
    assert ".direction = Direction::Downlink" in line
    assert ".direction = Direction::None" not in line


def test_generator_both_or_emits_both_not_none():
    """Régression : both_or doit produire Direction::Both (pas None)."""
    line = _emit_for_token("both_or")
    assert ".direction = Direction::Both" in line
    assert ".direction = Direction::None" not in line
    assert ".bothOr = true" in line
