#!/usr/bin/env python3
"""
ND6 tests — `both_or` is now a DIRECTION TOKEN (correctif 2/3).

Scope (correctif 2/3 — Python only):
  - canon/channels.py: `both_or` is no longer a YAML field. It is now
    a 5th token of `direction` (alongside uplink/downlink/both/none).
    `direction: both_or` replaces the old `direction: both` + `both_or: true`.
    The bool `both_or` is still exposed on ChannelDefinition but is
    derived internally from the raw token (True iff the token was
    'both_or').
  - diffusion.py: `both_or` mirrors to `both_or` (no inversion, like
    `both`). Mixing `both_or` with `uplink`/`downlink` is a fatal
    semantic conflict (same family as `both` + simple). Mixing
    `both_or` with `both` is a non-fatal WARNING (same divergence
    class as the old both_or divergence warning).

Out of scope here (deferred to a separate chantier):
  - The C++ runtime wiring in combus_frame_apply() / combus_set_digital()
    (reading/writing the `bothOr` field on `DigitalComBus`).

This file replaces the previous test_nd6_both_or.py which tested the
old `both_or: true` field-based schema. The old tests are removed
because they no longer make sense: `both_or` IS a direction token now,
not a combination of two fields.
"""
from __future__ import annotations

import sys
import tempfile
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import (
    ALLOWED_FIELDS,
    DIRECTION_TOKENS,
    ChannelValidationError,
    validate_channel,
)
from scripts.combus_builder.canon.channels import ChannelDefinition
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.diffusion import (
    DiffusionConflictError,
    diffuse_local_channels,
)
from scripts.combus_builder.node_context import NodeContext
from scripts.combus_builder.generator import select_views


# =============================================================================
# HELPERS
# =============================================================================

def _make_channel(
    cid: str = "TEST_CH",
    *,
    scope: str = "LOCAL",
    ctype: str = "digital",
    direction: str | None = "both",
    theme: str = "core",
) -> dict:
    """
    Build a raw channel dict for tests. The `direction` field is the
    ONLY way to express both_or now (no separate `both_or` field).
    """
    raw: dict = {
        "id": cid,
        "infoName": f"Info {cid}",
        "type": ctype,
        "scope": scope,
        "theme": theme,
        "direction": direction,
    }
    return raw


def _build_node(
    boards: dict[str, BuildContext],
) -> NodeContext:
    """Build a NodeContext for tests, with boards populated."""
    return NodeContext(mother_env="", boards=boards)


def _parse_cb(
    cb_path: Path,
    cb_text: str,
) -> list[tuple[Path, str, dict]]:
    """Build a parsed entry from raw YAML text (one .cb file)."""
    import yaml
    doc = yaml.safe_load(cb_text) or {}
    return [(cb_path, "channel_def", doc)]


# =============================================================================
# TESTS — CANON LAYER (validate_channel)
# =============================================================================

def test_canon_both_or_accepted_for_local_digital():
    """direction=both_or with scope=LOCAL + type=digital → accepted."""
    raw = _make_channel(direction="both_or")
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    assert isinstance(ch, ChannelDefinition)
    assert ch.both_or is True
    assert ch.scope == "LOCAL"
    assert ch.direction == frozenset({"uplink", "downlink"})
    assert ch.type == "digital"


def test_canon_both_or_default_false_when_token_is_both():
    """direction=both (not both_or) → both_or=False (legacy behaviour)."""
    raw = _make_channel(direction="both")
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    assert ch.both_or is False
    assert ch.direction == frozenset({"uplink", "downlink"})


def test_canon_both_or_default_false_when_token_is_uplink():
    """direction=uplink → both_or=False."""
    raw = _make_channel(direction="uplink")
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    assert ch.both_or is False
    assert ch.direction == frozenset({"uplink"})


def test_canon_both_or_rejected_for_remote_scope():
    """direction=both_or with scope=REMOTE → explicit rejection."""
    raw = _make_channel(scope="REMOTE", direction="both_or")
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "scope" in msg
        assert "REMOTE" in msg
        return
    raise AssertionError("expected ChannelValidationError for both_or + REMOTE")


def test_canon_both_or_rejected_for_system_scope():
    """direction=both_or with scope=SYSTEM → explicit rejection."""
    raw = _make_channel(scope="SYSTEM", direction="both_or")
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        # The scope check fires first; the direction check is not reached.
        return
    raise AssertionError("expected ChannelValidationError for both_or + SYSTEM")


def test_canon_both_or_rejected_for_analog_type():
    """direction=both_or with type=analog → explicit rejection (OR is boolean)."""
    raw = _make_channel(ctype="analog", direction="both_or")
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "analog" in msg
        return
    raise AssertionError("expected ChannelValidationError for both_or + analog")


def test_canon_both_or_field_no_longer_accepted():
    """The old `both_or: true` field is now an unknown key → rejection."""
    raw = _make_channel(direction="both")
    raw["both_or"] = True  # old schema, must be rejected now
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "unknown" in msg
        return
    raise AssertionError("expected ChannelValidationError for old both_or field")


def test_canon_both_or_in_direction_tokens():
    """`both_or` is registered in DIRECTION_TOKENS."""
    assert "both_or" in DIRECTION_TOKENS


def test_canon_both_or_not_in_allowed_fields():
    """`both_or` is NO LONGER in ALLOWED_FIELDS (it's a direction token now)."""
    assert "both_or" not in ALLOWED_FIELDS


# =============================================================================
# TESTS — DIFFUSION LAYER (diffuse_local_channels)
# =============================================================================

def test_diffusion_both_or_mirrors_to_both_or():
    """
    direction=both_or on one board, absent on the other → absent board
    receives a synthetic declaration with direction=both_or (NOT both,
    NOT uplink/downlink). The both_or flag is preserved in the
    synthetic copy.
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        cb_path = root / "test.cb"
        cb_path.write_text(yaml.safe_dump({
            "channels": [_make_channel("SHARED_BOTH_OR", direction="both_or")]
        }), encoding="utf-8")

        bctx_main = BuildContext(
            defines=frozenset({"IS_MAINBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        bctx_sound = BuildContext(
            defines=frozenset({"IS_SOUNDBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        node = _build_node({
            "mainboard": bctx_main,
            "soundboard": bctx_sound,
        })
        parsed = _parse_cb(cb_path, cb_path.read_text(encoding="utf-8"))

        out = diffuse_local_channels(parsed, node)
        # Both boards should have a declaration for SHARED_BOTH_OR with
        # direction=both_or (preserved through diffusion).
        for board_id in ("mainboard", "soundboard"):
            entries = out[board_id]
            found = False
            for _p, _label, doc in entries:
                for decl in doc.get("channels", []):
                    if decl.get("id") == "SHARED_BOTH_OR":
                        assert decl.get("direction") == "both_or", (
                            f"board {board_id}: direction not preserved "
                            f"(got {decl.get('direction')!r})"
                        )
                        found = True
            assert found, f"SHARED_BOTH_OR missing in board {board_id}"


def test_diffusion_both_or_mixed_with_uplink_is_fatal():
    """
    direction=both_or on one board + direction=uplink on the other →
    FATAL semantic conflict (same family as both + simple).
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        # main.cb: SHARED with direction=both_or, requires=[IS_MAINBOARD]
        decl_main = _make_channel("SHARED", direction="both_or")
        decl_main["requires"] = ["IS_MAINBOARD"]
        cb_path_main = root / "main.cb"
        cb_path_main.write_text(yaml.safe_dump({
            "channels": [decl_main],
        }), encoding="utf-8")
        # sound.cb: SHARED with direction=uplink, requires=[IS_SOUNDBOARD]
        decl_sound = _make_channel("SHARED", direction="uplink")
        decl_sound["requires"] = ["IS_SOUNDBOARD"]
        cb_path_sound = root / "sound.cb"
        cb_path_sound.write_text(yaml.safe_dump({
            "channels": [decl_sound],
        }), encoding="utf-8")

        bctx_main = BuildContext(
            defines=frozenset({"IS_MAINBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        bctx_sound = BuildContext(
            defines=frozenset({"IS_SOUNDBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        node = _build_node({
            "mainboard": bctx_main,
            "soundboard": bctx_sound,
        })
        parsed = []
        for p in (cb_path_main, cb_path_sound):
            parsed += _parse_cb(p, p.read_text(encoding="utf-8"))

        try:
            diffuse_local_channels(parsed, node)
        except DiffusionConflictError as e:
            assert e.semantic is True, (
                f"expected semantic=True (both_or + uplink), got {e.semantic}"
            )
            assert "SHARED" in str(e)
            return
        raise AssertionError("expected DiffusionConflictError for both_or + uplink")


def test_diffusion_both_or_mixed_with_both_warns_not_fatal(capsys):
    """
    direction=both_or on one board + direction=both on the other →
    NOT a fatal conflict. Emit a non-fatal WARNING (same divergence
    class as the old both_or divergence warning). Build continues.
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        # main.cb: SHARED with direction=both_or, requires=[IS_MAINBOARD]
        decl_main = _make_channel("SHARED", direction="both_or")
        decl_main["requires"] = ["IS_MAINBOARD"]
        cb_path_main = root / "main.cb"
        cb_path_main.write_text(yaml.safe_dump({
            "channels": [decl_main],
        }), encoding="utf-8")
        # sound.cb: SHARED with direction=both, requires=[IS_SOUNDBOARD]
        decl_sound = _make_channel("SHARED", direction="both")
        decl_sound["requires"] = ["IS_SOUNDBOARD"]
        cb_path_sound = root / "sound.cb"
        cb_path_sound.write_text(yaml.safe_dump({
            "channels": [decl_sound],
        }), encoding="utf-8")

        bctx_main = BuildContext(
            defines=frozenset({"IS_MAINBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        bctx_sound = BuildContext(
            defines=frozenset({"IS_SOUNDBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        node = _build_node({
            "mainboard": bctx_main,
            "soundboard": bctx_sound,
        })
        parsed = []
        for p in (cb_path_main, cb_path_sound):
            parsed += _parse_cb(p, p.read_text(encoding="utf-8"))

        # Should NOT raise. Should emit a warning.
        out = diffuse_local_channels(parsed, node)
        captured = capsys.readouterr()
        # Le warning émis par diffusion.py pour le cas both vs both_or
        # est routé via _warn() avec le préfixe "[combus_diffusion]
        # WARNING:" et le message commence par
        # "channel {channel_id!r}: both vs both_or divergence across
        # boards". On assert sur le préfixe, l'id "SHARED", et on
        # vérifie que "uplink" n'apparaît pas comme cause (sinon ce
        # serait un autre warning).
        assert "[combus_diffusion] WARNING:" in captured.err, (
            f"expected '[combus_diffusion] WARNING:' prefix in stderr, "
            f"got: {captured.err!r}"
        )
        assert "SHARED" in captured.err, (
            f"expected channel id 'SHARED' in warning, got: {captured.err!r}"
        )
        assert "both vs both_or divergence" in captured.err, (
            f"expected 'both vs both_or divergence' in warning, "
            f"got: {captured.err!r}"
        )
        assert "uplink" not in captured.err, (
            f"unexpected 'uplink' in warning (would indicate wrong "
            f"warning class), got: {captured.err!r}"
        )
        # Build continues: out is populated.
        assert "mainboard" in out
        assert "soundboard" in out


def test_diffusion_both_or_repeated_on_two_boards_ok(capsys):
    """
    direction=both_or on TWO boards (no divergence) → OK, no warning,
    no error. Same as both repeated on 2 boards.
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        # main.cb: SHARED with direction=both_or, requires=[IS_MAINBOARD]
        decl_main = _make_channel("SHARED", direction="both_or")
        decl_main["requires"] = ["IS_MAINBOARD"]
        cb_path_main = root / "main.cb"
        cb_path_main.write_text(yaml.safe_dump({
            "channels": [decl_main],
        }), encoding="utf-8")
        # sound.cb: SHARED with direction=both_or, requires=[IS_SOUNDBOARD]
        decl_sound = _make_channel("SHARED", direction="both_or")
        decl_sound["requires"] = ["IS_SOUNDBOARD"]
        cb_path_sound = root / "sound.cb"
        cb_path_sound.write_text(yaml.safe_dump({
            "channels": [decl_sound],
        }), encoding="utf-8")

        bctx_main = BuildContext(
            defines=frozenset({"IS_MAINBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        bctx_sound = BuildContext(
            defines=frozenset({"IS_SOUNDBOARD"}),
            defines_with_value={},
            buildroot=root,
        )
        node = _build_node({
            "mainboard": bctx_main,
            "soundboard": bctx_sound,
        })
        parsed = []
        for p in (cb_path_main, cb_path_sound):
            parsed += _parse_cb(p, p.read_text(encoding="utf-8"))

        out = diffuse_local_channels(parsed, node)
        captured = capsys.readouterr()
        # No divergence warning (both boards agree).
        assert "divergent" not in captured.err
        # Both boards have the channel.
        for board_id in ("mainboard", "soundboard"):
            found = False
            for _p, _label, doc in out[board_id]:
                for decl in doc.get("channels", []):
                    if decl.get("id") == "SHARED":
                        assert decl.get("direction") == "both_or"
                        found = True
            assert found, f"SHARED missing on board {board_id}"


# =============================================================================
# TESTS — GENERATOR LAYER (generator.py)
# =============================================================================
#
# Correctif 3/3: la sortie C++ ne change pas de forme — seule la
# source de la donnée change (dérivée du token de direction, plus d'un
# champ YAML séparé). On vérifie que :
#   - `direction: both_or` (canon ch.both_or == True) → .bothOr = true
#   - `direction: both`    (canon ch.both_or == False) → .bothOr = false
#   - `direction: uplink`  (canon ch.both_or == False) → .bothOr = false
#   - `direction: none`    (canon ch.both_or == False) → .bothOr = false
#   - analog channels n'émettent JAMAIS .bothOr (validation le rejette)

def _canon_from_yamls(*yamls):
    """
    Build a list of ChannelDefinitions from a list of channel dicts
    (test helper — same signature as in test_md5.py / test_generator.py).
    """
    from scripts.combus_builder.canon import canonize
    parsed = [
        (Path(f"ch{i}.cb"), "channel_def", {"channels": [y]})
        for i, y in enumerate(yamls)
    ]
    return canonize(parsed).canonical_definitions


def _ch(cid="FOO", *, info_name=None, type_="digital", scope="LOCAL",
        theme="core", direction="both_or"):
    if info_name is None:
        info_name = f"Info {cid}"
    return {
        "id": cid,
        "infoName": info_name,
        "type": type_,
        "scope": scope,
        "theme": theme,
        "direction": direction,
    }


def _ctx(flags=()):
    return BuildContext(
        defines=frozenset(flags),
        defines_with_value={},
        buildroot=Path("."),
    )


def test_generator_emits_bothOr_true_for_both_or_token():
    """direction=both_or → .bothOr = true dans le C++ généré."""
    from scripts.combus_builder.generator import (
        generate, _render_source, select_views,
    )
    chs = _canon_from_yamls(_ch("D1", direction="both_or"))
    sel = select_views(chs)
    out = _render_source(sel.full, _ctx())
    assert ".bothOr = true" in out, (
        f"expected '.bothOr = true' in generated .cpp for direction=both_or, "
        f"got:\n{out}"
    )


def test_generator_emits_bothOr_false_for_both_token():
    """direction=both (sans _or) → .bothOr = false dans le C++ généré."""
    from scripts.combus_builder.generator import (
        _render_source, select_views,
    )
    chs = _canon_from_yamls(_ch("D1", direction="both"))
    sel = select_views(chs)
    out = _render_source(sel.full, _ctx())
    assert ".bothOr = false" in out, (
        f"expected '.bothOr = false' in generated .cpp for direction=both, "
        f"got:\n{out}"
    )


def test_generator_emits_bothOr_false_for_uplink():
    """direction=uplink → .bothOr = false (non-bidirectionnel)."""
    from scripts.combus_builder.generator import (
        _render_source, select_views,
    )
    chs = _canon_from_yamls(_ch("D1", direction="uplink"))
    sel = select_views(chs)
    out = _render_source(sel.full, _ctx())
    assert ".bothOr = false" in out


def test_generator_emits_bothOr_false_for_downlink():
    """direction=downlink → .bothOr = false (non-bidirectionnel)."""
    from scripts.combus_builder.generator import (
        _render_source, select_views,
    )
    chs = _canon_from_yamls(_ch("D1", direction="downlink"))
    sel = select_views(chs)
    out = _render_source(sel.full, _ctx())
    assert ".bothOr = false" in out


def test_generator_does_not_emit_bothOr_for_analog():
    """Les channels analog n'émettent JAMAIS .bothOr (le champ n'existe pas
    dans AnalogComBus et la validation rejette both_or sur analog)."""
    from scripts.combus_builder.generator import (
        _render_source, select_views,
    )
    chs = _canon_from_yamls(_ch("A1", type_="analog", direction="uplink"))
    sel = select_views(chs)
    out = _render_source(sel.full, _ctx())
    # Le bloc analog ne doit pas contenir .bothOr
    assert ".bothOr" not in out, (
        f"analog block should NOT contain .bothOr, got:\n{out}"
    )


def test_generator_both_or_field_omitted_raises_validation():
    """L'ancien schéma `direction: both` + `both_or: true` est rejeté par
    la validation (both_or n'est plus un champ YAML)."""
    raw = _ch("D1", direction="both")
    raw["both_or"] = True
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        assert "both_or" in str(e)
        assert "unknown" in str(e)
        return
    raise AssertionError("expected ChannelValidationError for old both_or field")


def test_generator_full_pipeline_both_or_token_emits_bothOr_true(tmp_path):
    """Test end-to-end : canon → select_views → generate → le fichier
    combus.cpp contient bien `.bothOr = true` pour un channel direction=both_or.
    """
    from scripts.combus_builder.generator import generate
    chs = _canon_from_yamls(_ch("D1", direction="both_or"))
    generate(chs, _ctx(), tmp_path)
    cpp = (tmp_path / "combus.cpp").read_text(encoding="utf-8")
    assert ".bothOr = true" in cpp, (
        f"expected '.bothOr = true' in combus.cpp, got:\n{cpp}"
    )


# =============================================================================
# TESTS — MD5 LAYER (md5.py)
# =============================================================================
#
# Politique de hash (correctif 3/3 révisé) : `both` et `both_or` produisent
# des hash DIFFÉRENTS. Justification :
#   - Les deux tokens normalisent vers frozenset({"uplink", "downlink"}),
#     donc le frozenset seul ne les distingue pas.
#   - Mais diffusion.py traite leur divergence entre boards comme une
#     vraie anomalie de config (warning), pas comme un cas don't-care
#     (contrairement à none mélangé à autre chose).
#   - Le hash LOCAL existe pour détecter une dérive de config entre
#     boards buildées séparément — laisser ignorer cette divergence
#     recréerait le risque que tout ce chantier visait à éliminer, une
#     fois le wiring runtime de bothOr câblé (prochain chantier).
#   - Solution : ajouter `both_or` (booléen) au payload canonique, au
#     même niveau que `direction`. ONE_WAY n'est pas affecté (il ne
#     neutralise que uplink/downlink, jamais both_or).

def test_md5_both_and_both_or_produce_different_hash():
    """`direction: both` et `direction: both_or` produisent des hash
    DIFFÉRENTS sur les vues où le channel apparaît (combus_local, combus).

    Politique de hash : les deux tokens partagent le même frozenset
    direction mais sont sémantiquement distincts (diffusion.py émet un
    warning si une board déclare `both` et l'autre `both_or`). Le hash
    doit donc les distinguer pour détecter une dérive de config entre
    boards buildées séparément. Voir le bloc de commentaire ci-dessus.

    Note : la vue combus_remote ne contient QUE les channels REMOTE.
    Notre channel de test est LOCAL (scope=LOCAL par défaut), donc il
    n'apparaît pas dans combus_remote — le hash remote est identique
    (liste vide) pour les deux cas. C'est attendu et correct.
    """
    from scripts.combus_builder.md5 import compute_view_hash
    chs_both = _canon_from_yamls(_ch("D1", direction="both"))
    chs_both_or = _canon_from_yamls(_ch("D1", direction="both_or"))
    sel_both = select_views(chs_both)
    sel_both_or = select_views(chs_both_or)
    # Le channel LOCAL apparaît dans combus_local et combus (full).
    for attr in ("local", "full"):
        h_both = compute_view_hash(getattr(sel_both, attr))
        h_both_or = compute_view_hash(getattr(sel_both_or, attr))
        assert h_both.md5_hex != h_both_or.md5_hex, (
            f"MD5 {attr}: both={h_both.md5_hex} vs both_or={h_both_or.md5_hex} "
            f"— both and both_or must produce DIFFERENT hashes "
            f"(see comment block above)"
        )


def test_md5_canonical_bytes_differ_for_both_and_both_or():
    """Les bytes canoniques (avant MD5) sont DIFFÉRENTS pour `both` et
    `both_or` sur les vues où le channel apparaît — confirmation au
    niveau JSON, pas seulement au niveau hash.
    """
    from scripts.combus_builder.md5 import canonical_bytes
    chs_both = _canon_from_yamls(_ch("D1", direction="both"))
    chs_both_or = _canon_from_yamls(_ch("D1", direction="both_or"))
    sel_both = select_views(chs_both)
    sel_both_or = select_views(chs_both_or)
    for attr in ("local", "full"):
        b_both = canonical_bytes(getattr(sel_both, attr))
        b_both_or = canonical_bytes(getattr(sel_both_or, attr))
        assert b_both != b_both_or, (
            f"canonical_bytes {attr}: both and both_or must differ "
            f"(both_or is part of the canonical payload)"
        )


def test_md5_both_or_true_on_both_boards_produces_same_hash():
    """Deux channels identiques avec `both_or=true` sur les deux →
    MÊME hash (confirmer que la distinction ne casse pas le cas
    cohérent).

    C'est le cas symétrique au test précédent : si les deux boards
    déclarent la même chose (both_or=true), le hash doit être identique
    pour ne pas signaler une fausse divergence.
    """
    from scripts.combus_builder.md5 import compute_view_hash
    chs_a = _canon_from_yamls(_ch("D1", direction="both_or"))
    chs_b = _canon_from_yamls(_ch("D1", direction="both_or"))
    sel_a = select_views(chs_a)
    sel_b = select_views(chs_b)
    for attr in ("local", "full"):
        h_a = compute_view_hash(getattr(sel_a, attr))
        h_b = compute_view_hash(getattr(sel_b, attr))
        assert h_a.md5_hex == h_b.md5_hex, (
            f"MD5 {attr}: two identical both_or=true channels must produce "
            f"the same hash (got {h_a.md5_hex} vs {h_b.md5_hex})"
        )


def test_md5_both_or_false_on_both_boards_produces_same_hash():
    """Deux channels identiques avec `both_or=false` (direction=both) sur
    les deux → MÊME hash (cas cohérent baseline).
    """
    from scripts.combus_builder.md5 import compute_view_hash
    chs_a = _canon_from_yamls(_ch("D1", direction="both"))
    chs_b = _canon_from_yamls(_ch("D1", direction="both"))
    sel_a = select_views(chs_a)
    sel_b = select_views(chs_b)
    for attr in ("local", "full"):
        h_a = compute_view_hash(getattr(sel_a, attr))
        h_b = compute_view_hash(getattr(sel_b, attr))
        assert h_a.md5_hex == h_b.md5_hex, (
            f"MD5 {attr}: two identical both=false channels must produce "
            f"the same hash (got {h_a.md5_hex} vs {h_b.md5_hex})"
        )


def test_md5_both_vs_none_produce_different_hash():
    """`direction: both` et `direction: none` produisent des hash
    DIFFÉRENTS sur les vues où le channel apparaît (combus_local, combus).

    `both` et `none` sont des signaux de contrat distincts (canal
    bidirectionnel vs canal absent du wire). Le hash doit les distinguer.
    """
    from scripts.combus_builder.md5 import compute_view_hash
    chs_both = _canon_from_yamls(_ch("D1", direction="both"))
    chs_none = _canon_from_yamls(_ch("D1", direction="none"))
    sel_both = select_views(chs_both)
    sel_none = select_views(chs_none)
    for attr in ("local", "full"):
        h_both = compute_view_hash(getattr(sel_both, attr))
        h_none = compute_view_hash(getattr(sel_none, attr))
        assert h_both.md5_hex != h_none.md5_hex, (
            f"MD5 {attr}: both={h_both.md5_hex} vs none={h_none.md5_hex} "
            f"— both and none must produce DIFFERENT hashes"
        )


def test_md5_both_or_vs_none_produce_different_hash():
    """`direction: both_or` et `direction: none` produisent des hash
    DIFFÉRENTS sur les vues où le channel apparaît (combus_local, combus).

    `both_or` et `none` sont des signaux de contrat distincts (canal
    bidirectionnel avec OR anti-oscillation vs canal absent du wire).
    Le hash doit les distinguer.
    """
    from scripts.combus_builder.md5 import compute_view_hash
    chs_both_or = _canon_from_yamls(_ch("D1", direction="both_or"))
    chs_none = _canon_from_yamls(_ch("D1", direction="none"))
    sel_both_or = select_views(chs_both_or)
    sel_none = select_views(chs_none)
    for attr in ("local", "full"):
        h_both_or = compute_view_hash(getattr(sel_both_or, attr))
        h_none = compute_view_hash(getattr(sel_none, attr))
        assert h_both_or.md5_hex != h_none.md5_hex, (
            f"MD5 {attr}: both_or={h_both_or.md5_hex} vs none={h_none.md5_hex} "
            f"— both_or and none must produce DIFFERENT hashes"
        )


def test_md5_uplink_vs_downlink_same_hash_on_local_different_on_remote():
    """`direction: uplink` et `direction: downlink` produisent le MÊME
    hash sur combus_local (ONE_WAY canonisation) mais des hash
    DIFFÉRENTS sur combus_remote (pas de canonisation).

    C'est le comportement ND7 ONE_WAY : sur la vue LOCAL, le mirror
    de rôle entre boards (uplink sur l'une, downlink sur l'autre) est
    légitime et ne doit pas signaler une divergence. Sur la vue
    REMOTE, le signal de contrat est préservé tel quel.

    Note : pour tester combus_remote, il faut un channel REMOTE (les
    channels LOCAL n'apparaissent pas dans combus_remote). On utilise
    donc scope=REMOTE ici.
    """
    from scripts.combus_builder.md5 import compute_view_hash
    # Channel REMOTE pour qu'il apparaisse dans combus_remote.
    chs_up = _canon_from_yamls(_ch("D1", scope="REMOTE", direction="uplink"))
    chs_down = _canon_from_yamls(_ch("D1", scope="REMOTE", direction="downlink"))
    sel_up = select_views(chs_up)
    sel_down = select_views(chs_down)
    # combus_local : ONE_WAY → même hash
    h_up_local = compute_view_hash(sel_up.local)
    h_down_local = compute_view_hash(sel_down.local)
    assert h_up_local.md5_hex == h_down_local.md5_hex, (
        f"MD5 combus_local: uplink={h_up_local.md5_hex} vs "
        f"downlink={h_down_local.md5_hex} — must be SAME (ONE_WAY)"
    )
    # combus_remote : pas de canonisation → hash différents
    h_up_remote = compute_view_hash(sel_up.remote)
    h_down_remote = compute_view_hash(sel_down.remote)
    assert h_up_remote.md5_hex != h_down_remote.md5_hex, (
        f"MD5 combus_remote: uplink={h_up_remote.md5_hex} vs "
        f"downlink={h_down_remote.md5_hex} — must be DIFFERENT (no ONE_WAY)"
    )


# =============================================================================
# ENTRY POINT
# =============================================================================

if __name__ == "__main__":
    # Manual run: invoke pytest programmatically (the test runner used
    # by the repo). Each test_* function will be picked up.
    import pytest
    sys.exit(pytest.main([__file__, "-v"]))
