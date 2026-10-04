#!/usr/bin/env python3
"""
ND6 tests — `both_or` field (anti-oscillation flag for LOCAL channels
in direction `both`).

Scope (ND6 — Python only):
  - canon/channels.py: add `both_or` to ALLOWED_FIELDS, validate the
    constraints (scope=LOCAL, direction=both, type=digital), expose
    the flag on the ChannelDefinition dataclass.
  - diffusion.py: preserve `both_or` in synthetic declarations (auto,
    since the source dict is copied in full). Emit WARNINGs (not
    errors) for divergent and single-writer configurations.
  - generator.py: emit `.bothOr = <bool>` initialiser on the
    generated `DigitalComBus` struct.
  - combus_defs.h: add the `bool bothOr = false` field to
    `DigitalComBus` (runtime wiring deferred to a separate chantier).

Out of scope here:
  - The C++ runtime wiring in combus_frame_apply() / combus_set_digital().
  - merge_channels() in canon/channels.py (untouched — both_or is
    carried through per-declaration, and the existing uniqueness
    constraint on `id` is what matters for merge_channels).
"""
from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import (
    ALLOWED_FIELDS,
    ChannelValidationError,
    validate_channel,
)
from scripts.combus_builder.canon.channels import ChannelDefinition
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.diffusion import (
    DiffusionConflictError,
    diffuse_local_channels,
)
from scripts.combus_builder.generator import (
    generate,
    _render_source,
)
from scripts.combus_builder.node_context import NodeContext


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
    both_or: bool | None = None,
) -> dict:
    """
    Build a raw channel dict for tests. The `both_or` field is added
    only when not None (None = the field is absent, which is the
    historical default behaviour).
    """
    raw: dict = {
        "id": cid,
        "infoName": f"Info {cid}",
        "type": ctype,
        "scope": scope,
        "theme": theme,
        "direction": direction,
    }
    if both_or is not None:
        raw["both_or"] = both_or
    return raw


# =============================================================================
# TESTS — CANON LAYER (validate_channel)
# =============================================================================

def test_canon_both_or_accepted_for_local_both_digital():
    """both_or=true with direction=both, scope=LOCAL, type=digital → accepted."""
    raw = _make_channel(both_or=True)
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    assert isinstance(ch, ChannelDefinition)
    assert ch.both_or is True
    assert ch.scope == "LOCAL"
    assert ch.direction == frozenset({"uplink", "downlink"})
    assert ch.type == "digital"


def test_canon_both_or_default_false_when_absent():
    """both_or field absent → defaults to False (legacy behaviour)."""
    raw = _make_channel()  # no both_or
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    assert ch.both_or is False


def test_canon_both_or_explicit_false_accepted():
    """both_or=false is accepted (= no flag, no behaviour change)."""
    raw = _make_channel(both_or=False)
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    assert ch.both_or is False


def test_canon_both_or_rejected_for_uplink():
    """both_or=true with direction=uplink → explicit rejection."""
    raw = _make_channel(direction="uplink", both_or=True)
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "direction" in msg
        assert "uplink" in msg
        return
    raise AssertionError("expected ChannelValidationError for both_or + uplink")


def test_canon_both_or_rejected_for_downlink():
    """both_or=true with direction=downlink → explicit rejection."""
    raw = _make_channel(direction="downlink", both_or=True)
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "downlink" in msg
        return
    raise AssertionError("expected ChannelValidationError for both_or + downlink")


def test_canon_both_or_rejected_for_none():
    """both_or=true with direction=none → explicit rejection."""
    raw = _make_channel(direction="none", both_or=True)
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "none" in msg
        return
    raise AssertionError("expected ChannelValidationError for both_or + none")


def test_canon_both_or_rejected_for_remote_scope():
    """both_or=true with scope=REMOTE → explicit rejection (no use case)."""
    raw = _make_channel(scope="REMOTE", both_or=True)
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
    """both_or=true with scope=SYSTEM → explicit rejection."""
    # SYSTEM accepts only direction=none, so we need both_or=true +
    # direction=none to test the scope rejection specifically.
    raw = _make_channel(scope="SYSTEM", direction="none", both_or=True)
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        # The scope check fires first; the direction check is not reached.
        return
    raise AssertionError("expected ChannelValidationError for both_or + SYSTEM")


def test_canon_both_or_rejected_for_analog_type():
    """both_or=true with type=analog → explicit rejection (OR is boolean)."""
    raw = _make_channel(ctype="analog", both_or=True)
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "analog" in msg
        return
    raise AssertionError("expected ChannelValidationError for both_or + analog")


def test_canon_both_or_must_be_boolean():
    """both_or with non-boolean value (e.g. string, int) → rejection."""
    raw = _make_channel(both_or="true")  # type: ignore[arg-type]
    try:
        validate_channel(path=Path("test.cb"), raw=raw, index=0)
    except ChannelValidationError as e:
        msg = str(e)
        assert "both_or" in msg
        assert "boolean" in msg or "bool" in msg
        return
    raise AssertionError("expected ChannelValidationError for non-boolean both_or")


def test_canon_both_or_in_allowed_fields():
    """The `both_or` key is registered in ALLOWED_FIELDS (no unknown-key rejection)."""
    assert "both_or" in ALLOWED_FIELDS


# =============================================================================
# TESTS — DIFFUSION LAYER (diffuse_local_channels)
# =============================================================================

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


def test_diffusion_preserves_both_or_in_synthetic():
    """
    both_or=true on the board that declares the channel is PRESERVED
    in the synthetic copy on the sister board (auto-preservation via
    full-dict copy in step 4b). No divergence possible in this case
    (only one board declares it).
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        # Create a .cb file with both_or=true on a single channel.
        cb_path = root / "test.cb"
        cb_path.write_text(yaml.safe_dump({
            "channels": [_make_channel("SHARED_FLAG", both_or=True)]
        }), encoding="utf-8")

        # Two boards: mainboard declares, soundboard is absent.
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

        # Diffusion should produce synthetic both_or=true on soundboard.
        out = diffuse_local_channels(parsed, node)
        # Both boards should have a declaration for SHARED_FLAG with both_or=true.
        for board_id in ("mainboard", "soundboard"):
            entries = out[board_id]
            found = False
            for _p, _label, doc in entries:
                for decl in doc.get("channels", []):
                    if decl.get("id") == "SHARED_FLAG":
                        assert decl.get("both_or") is True, (
                            f"board {board_id}: both_or not preserved in "
                            f"synthetic copy (got {decl.get('both_or')!r})"
                        )
                        found = True
            assert found, f"SHARED_FLAG missing in board {board_id}"


def test_diffusion_both_or_divergence_warns(capsys):
    """
    both_or=true on mainboard AND both_or=false on soundboard for the
    same id in direction=both → WARNING emitted, build continues, no
    fatal error.

    Setup: each board declares the same id with its own `requires`
    filter, so each declaration is active only on its own board.
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        # main.cb: SHARED_FLAG with both_or=true, requires=[IS_MAINBOARD]
        decl_main = _make_channel("SHARED_FLAG", both_or=True)
        decl_main["requires"] = ["IS_MAINBOARD"]
        cb_path_main = root / "main.cb"
        cb_path_main.write_text(yaml.safe_dump({
            "channels": [decl_main],
        }), encoding="utf-8")
        # sound.cb: SHARED_FLAG with both_or=false, requires=[IS_SOUNDBOARD]
        decl_sound = _make_channel("SHARED_FLAG", both_or=False)
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
        # Parse both files.
        parsed = []
        for p in (cb_path_main, cb_path_sound):
            parsed += _parse_cb(p, p.read_text(encoding="utf-8"))

        # Diffusion should not raise, but should warn about divergence.
        out = diffuse_local_channels(parsed, node)
        captured = capsys.readouterr()
        assert "both_or divergent" in captured.err, (
            f"expected both_or divergence warning, got stderr: {captured.err!r}"
        )
        # Build continues: out is populated.
        assert "mainboard" in out
        assert "soundboard" in out


def test_diffusion_both_or_single_writer_warns(capsys):
    """
    both_or=true on a channel active on a SINGLE board only
    post-diffusion → WARNING emitted ("both_or sans effet, écrivain
    unique"). The channel must be active on exactly one board.
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        # Channel with requires=[IS_MAINBOARD] — active only on
        # mainboard.
        cb_path = root / "main.cb"
        decl = _make_channel("SOLO_FLAG", both_or=True)
        decl["requires"] = ["IS_MAINBOARD"]
        cb_path.write_text(yaml.safe_dump({
            "channels": [decl],
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
        captured = capsys.readouterr()
        assert "single board" in captured.err or "écrivain unique" in captured.err, (
            f"expected single-writer warning, got stderr: {captured.err!r}"
        )
        # Build continues: mainboard has the channel.
        assert "mainboard" in out


def test_diffusion_both_or_does_not_break_normal_diffusion(capsys):
    """
    Sanity check: a normal both_or=true channel that diffuses cleanly
    (rule 3, both only) does NOT emit any of the two new warnings.
    """
    import yaml

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        cb_path = root / "test.cb"
        cb_path.write_text(yaml.safe_dump({
            "channels": [_make_channel("CLEAN_BOTH_OR", both_or=True)],
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
        captured = capsys.readouterr()
        # No both_or-specific warnings in this clean case.
        assert "both_or divergent" not in captured.err
        assert "single board" not in captured.err
        # Both boards have the channel.
        for board_id in ("mainboard", "soundboard"):
            found = False
            for _p, _label, doc in out[board_id]:
                for decl in doc.get("channels", []):
                    if decl.get("id") == "CLEAN_BOTH_OR":
                        found = True
            assert found, f"CLEAN_BOTH_OR missing on board {board_id}"


# =============================================================================
# TESTS — GENERATOR LAYER
# =============================================================================

def test_generator_emits_both_or_true_for_digital():
    """A digital channel with both_or=true → `.bothOr = true` in generated .cpp."""
    raw = _make_channel("OR_FLAG", both_or=True)
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    ctx = BuildContext(
        defines=frozenset(),
        defines_with_value={},
        buildroot=Path(tempfile.gettempdir()),
    )
    sel = generate([ch], ctx, out_dir=Path(tempfile.gettempdir()) / "combus_nd6_test")
    # Re-render the source from the in-memory view to inspect the output.
    src = _render_source(sel.full, ctx)
    # The digital initializer must include `.bothOr = true`.
    assert re.search(r"\.bothOr\s*=\s*true\b", src), (
        f"expected `.bothOr = true` in generated source, got:\n{src}"
    )


def test_generator_emits_both_or_false_for_digital():
    """A digital channel with both_or=false → `.bothOr = false` in generated .cpp."""
    raw = _make_channel("NO_OR_FLAG", both_or=False)
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    ctx = BuildContext(
        defines=frozenset(),
        defines_with_value={},
        buildroot=Path(tempfile.gettempdir()),
    )
    sel = generate([ch], ctx, out_dir=Path(tempfile.gettempdir()) / "combus_nd6_test")
    src = _render_source(sel.full, ctx)
    assert re.search(r"\.bothOr\s*=\s*false\b", src), (
        f"expected `.bothOr = false` in generated source, got:\n{src}"
    )


def test_generator_omits_both_or_for_analog():
    """
    Analog channels do NOT get `.bothOr =` in the initializer (the
    field is digital-only). Validation already rejects both_or=true
    on analog at the canon layer, so this is a pure structural check.
    """
    # Build a valid analog channel (no both_or).
    raw = _make_channel("ANALOG_CH", ctype="analog", both_or=None)
    ch = validate_channel(path=Path("test.cb"), raw=raw, index=0)
    assert ch.both_or is False
    ctx = BuildContext(
        defines=frozenset(),
        defines_with_value={},
        buildroot=Path(tempfile.gettempdir()),
    )
    sel = generate([ch], ctx, out_dir=Path(tempfile.gettempdir()) / "combus_nd6_test")
    src = _render_source(sel.full, ctx)
    # No .bothOr in the analog initializer block.
    # We extract the analog init line and assert no bothOr there.
    analog_block = re.search(
        r"AnalogComBus AnalogComBusArray\[.*?\};",
        src,
        re.DOTALL,
    )
    assert analog_block, "expected AnalogComBusArray block in generated source"
    assert ".bothOr" not in analog_block.group(0), (
        f"analog block unexpectedly contains `.bothOr`:\n{analog_block.group(0)}"
    )


# =============================================================================
# TESTS — C++ HEADER (combus_defs.h)
# =============================================================================

def test_cpp_header_has_bothOr_field_on_DigitalComBus():
    """The runtime DigitalComBus struct carries `bool bothOr = false`."""
    header_path = (
        REPO_ROOT / "src" / "core" / "system" / "combus" / "combus_defs.h"
    )
    assert header_path.is_file(), f"combus_defs.h not found at {header_path}"
    text = header_path.read_text(encoding="utf-8")
    # Look for the field declaration in the DigitalComBus struct.
    # Match a "bool bothOr = false" line anywhere in the file.
    assert re.search(r"bool\s+bothOr\s*=\s*false\b", text), (
        f"`bool bothOr = false` not found in combus_defs.h"
    )
    # Verify it lives in the DigitalComBus struct, not AnalogComBus.
    # We check by extracting the DigitalComBus block and looking inside.
    m = re.search(
        r"typedef\s+struct\s*\{[^}]*?bool\s+bothOr[^}]*?\}\s*DigitalComBus\s*;",
        text,
        re.DOTALL,
    )
    assert m, (
        f"`bothOr` field not found inside `DigitalComBus` struct in combus_defs.h"
    )


# =============================================================================
# ENTRY POINT
# =============================================================================

if __name__ == "__main__":
    # Manual run: invoke pytest programmatically (the test runner used
    # by the repo). Each test_* function will be picked up.
    import pytest
    sys.exit(pytest.main([__file__, "-v"]))
