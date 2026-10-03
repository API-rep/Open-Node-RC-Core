#!/usr/bin/env python3
"""
ND2 diffusion tests — exercise diffuse_local_channels() against
fictitious 2-board nodes.

These tests cover the ND2 spec:
  - Mirror simple: BAT_LOW declared uplink on mainboard only ->
    soundboard receives BAT_LOW in downlink.
  - Complementary pair: uplink on mainboard + downlink on soundboard
    (both explicit) -> OK, no error.
  - Conflict uplink/uplink -> FATAL.
  - Conflict downlink/downlink -> FATAL.
  - both/both on 2 boards -> OK.
  - none/X on 2 boards -> OK.
  - Semantic conflict both/uplink -> FATAL.
  - Local conflict: 2 declarations of same id in ONE board's .cb ->
    FATAL (distinct error class).
  - REMOTE channel present on one board only -> passed through to
    both boards WITHOUT mirroring.
  - Legacy mode: diffusion.py is never called (verified by the fact
    that the orchestrator only calls it in node mode).

Ownership convention (ND2):
  A declaration is "owned" by a board iff its `requires` list is
  satisfied by that board's BuildContext. A declaration with no
  `requires` is active for ALL boards (and would trigger an intra-board
  conflict if duplicated). This matches the spec:
    "Résoudre, pour chaque board du node, quelles déclarations brutes
     de cet `id` sont actives pour son `BuildContext` (même logique de
     filtre `requires` que `resolve_requires`)."

Usage:
    python scripts/combus_builder/tests/test_nd2_diffusion.py

Exit codes:
    0  success
    1  validation failure
"""
from __future__ import annotations

import contextlib
import io
import sys
import tempfile
from pathlib import Path
from typing import Any

# Ensure the combus_builder package is importable when running this
# file directly from the repo root.
THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.diffusion import (
    DiffusionConflictError,
    DiffusionLocalConflictError,
    diffuse_local_channels,
)
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.node_context import NodeContext


# =============================================================================
# Test fixtures
# =============================================================================

def _make_ctx(defines: set[str], project_root: Path) -> BuildContext:
    """Build a BuildContext for testing."""
    return BuildContext(
        buildroot=(project_root / "src").resolve(),
        defines=frozenset(defines),
        defines_with_value={},
        cppdefines_source="test",
        raw_cppdefines_sample="<test>",
    )


def _make_node(
    project_root: Path,
    board_defines: dict[str | None, set[str]],
) -> NodeContext:
    """Build a NodeContext with the given boards."""
    boards = {
        board_id: _make_ctx(defines, project_root)
        for board_id, defines in board_defines.items()
    }
    # Determine mother_env: the None key if present, else the first board.
    mother_env = None
    if None in boards:
        non_none = [k for k in boards if k is not None]
        if non_none:
            mother_env = non_none[0]
        else:
            mother_env = "test_env"
    else:
        mother_env = "test_env"
    return NodeContext(
        mother_env=mother_env,
        boards=boards,
        is_node=True,
    )


def _make_parsed(
    cb_files: dict[str, str],
    src_dir: Path,
) -> list[tuple[Path, str, Any]]:
    """Create .cb files and return parsed data.

    cb_files: dict mapping filename -> YAML content.
    """
    import yaml
    parsed: list[tuple[Path, str, Any]] = []
    for name, content in cb_files.items():
        path = src_dir / name
        path.write_text(content, encoding="utf-8")
        data = yaml.safe_load(content)
        parsed.append((path, "channel_def", data))
    return parsed


# =============================================================================
# Test 1: Single uplink producer (ND2 generalised rule 5)
# =============================================================================

def test_mirror_simple() -> bool:
    """BAT_LOW declared uplink on mainboard only -> soundboard (absent) gets downlink.

    Ownership via requires:
      - mainboard.cb: BAT_LOW with requires=[IS_MAINBOARD] -> active for mainboard only
      - soundboard.cb: SPK_VOL with requires=[IS_EXT_BOARD] -> active for soundboard only

    Per the generalised ND2 rule:
      - BAT_LOW: rule 5 (n_up==1, n_down==0) -> soundboard (absent) becomes downlink.
      - SPK_VOL: rule 6 (n_up==0, n_down==1) -> WARNING 'canal mort', mainboard (absent) becomes none.
        (The old 'mirror' behavior is gone in the N-board version: if there's
         no producer uplink anywhere, we don't invent one.)
    """
    print("\n=== Test 1: Single uplink producer (rule 5) ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: SPK_VOL
    infoName: "Speaker volume"
    type: analog
    scope: LOCAL
    theme: core
    direction: downlink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        # Capture stderr: SPK_VOL should emit a rule-6 warning.
        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            result = diffuse_local_channels(parsed, node)
        stderr_output = captured.getvalue()
        assert "SPK_VOL" in stderr_output and "canal mort" in stderr_output, (
            f"expected a rule-6 warning for SPK_VOL, got: {stderr_output!r}"
        )

        # mainboard: BAT_LOW=own uplink. SPK_VOL: rule 6, absent board gets `none`.
        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        assert mainboard_decls["BAT_LOW"] == "uplink", (
            f"mainboard BAT_LOW should be uplink, got {mainboard_decls['BAT_LOW']}"
        )
        assert mainboard_decls.get("SPK_VOL") == "none", (
            f"mainboard SPK_VOL (absent) should be `none` (rule 6, no producer), "
            f"got {mainboard_decls.get('SPK_VOL')!r}"
        )
        print(f"  OK   mainboard: {mainboard_decls}")

        # soundboard: SPK_VOL=own downlink. BAT_LOW: rule 5, absent board gets `downlink`.
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert soundboard_decls["SPK_VOL"] == "downlink"
        assert soundboard_decls.get("BAT_LOW") == "downlink", (
            f"soundboard BAT_LOW (absent) should be downlink (rule 5, single uplink), "
            f"got {soundboard_decls.get('BAT_LOW')!r}"
        )
        print(f"  OK   soundboard: {soundboard_decls}")

    return True


# =============================================================================
# Test 2: Complementary pair (explicit uplink + explicit downlink)
# =============================================================================

def test_complementary_pair() -> bool:
    """uplink on mainboard + explicit downlink on soundboard -> OK."""
    print("\n=== Test 2: Complementary pair ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        # Should NOT raise.
        result = diffuse_local_channels(parsed, node)

        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert mainboard_decls["BAT_LOW"] == "uplink"
        assert soundboard_decls["BAT_LOW"] == "downlink"
        print(f"  OK   mainboard BAT_LOW= uplink, soundboard BAT_LOW= downlink")

    return True


# =============================================================================
# Test 3: Conflict uplink/uplink
# =============================================================================

def test_conflict_uplink_uplink() -> bool:
    """Both boards declare BAT_LOW as uplink -> FATAL."""
    print("\n=== Test 3: Conflict uplink/uplink ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        try:
            diffuse_local_channels(parsed, node)
            print("  FAIL expected DiffusionConflictError, got none")
            return False
        except DiffusionConflictError as e:
            assert e.channel_id == "BAT_LOW"
            assert not e.semantic
            # The N-board error has an `involved` list; both boards
            # should be there with direction "uplink".
            assert len(e.involved) == 2
            directions = sorted(d for _b, d, _p in e.involved)
            assert directions == ["uplink", "uplink"], (
                f"expected both directions 'uplink', got {directions}"
            )
            print(f"  OK   raised DiffusionConflictError: {e}")
            return True


# =============================================================================
# Test 4: downlink/downlink (CHANGED: warning + none, not fatal)
# =============================================================================

def test_downlink_downlink_now_warning() -> bool:
    """Both boards declare BAT_LOW as downlink -> WARNING (canal mort), not fatal.

    CHANGED BEHAVIOUR (ND2 generalised to N boards): in the
    2-board version this was a fatal DiffusionConflictError. The
    generalised rule (point 6) says: n_up == 0, n_down >= 1 ->
    emit a warning and let absent boards receive `none`. Build
    continues.

    To verify the warning is emitted we capture stderr.
    """
    import io
    import contextlib
    print("\n=== Test 4: downlink/downlink (warning, not fatal) ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        # Should NOT raise. Capture stderr to verify the warning.
        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            result = diffuse_local_channels(parsed, node)

        stderr_output = captured.getvalue()
        assert "WARNING" in stderr_output, (
            f"expected a WARNING on stderr, got: {stderr_output!r}"
        )
        assert "canal mort" in stderr_output, (
            f"expected 'canal mort' in warning, got: {stderr_output!r}"
        )
        assert "BAT_LOW" in stderr_output, (
            f"expected channel id in warning, got: {stderr_output!r}"
        )
        print(f"  OK   warning emitted: {stderr_output.strip()}")

        # Both boards should keep their explicit downlink declarations.
        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert mainboard_decls["BAT_LOW"] == "downlink"
        assert soundboard_decls["BAT_LOW"] == "downlink"
        print(f"  OK   both boards retain BAT_LOW=downlink (no fatal error)")

    return True


# =============================================================================
# Test 5: both/both on 2 boards -> OK
# =============================================================================

def test_both_both_ok() -> bool:
    """Both boards declare BAT_LOW as both -> OK (BOTH_OR aggregation)."""
    print("\n=== Test 5: both/both -> OK ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: both
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: both
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        # Should NOT raise.
        result = diffuse_local_channels(parsed, node)
        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert mainboard_decls["BAT_LOW"] == "both"
        assert soundboard_decls["BAT_LOW"] == "both"
        print(f"  OK   both boards have BAT_LOW=both")

    return True


# =============================================================================
# Test 6: none/X -> OK
# =============================================================================

def test_none_x_ok() -> bool:
    """none on mainboard + uplink on soundboard -> OK."""
    print("\n=== Test 6: none/X -> OK ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: none
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        # Should NOT raise.
        result = diffuse_local_channels(parsed, node)
        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert mainboard_decls["BAT_LOW"] == "none"
        assert soundboard_decls["BAT_LOW"] == "uplink"
        print(f"  OK   mainboard BAT_LOW=none, soundboard BAT_LOW=uplink")

    return True


# =============================================================================
# Test 7: Semantic conflict both/uplink
# =============================================================================

def test_semantic_conflict_both_uplink() -> bool:
    """both on mainboard + uplink on soundboard -> FATAL (semantic)."""
    print("\n=== Test 7: Semantic conflict both/uplink ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: both
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        try:
            diffuse_local_channels(parsed, node)
            print("  FAIL expected DiffusionConflictError, got none")
            return False
        except DiffusionConflictError as e:
            assert e.channel_id == "BAT_LOW"
            assert e.semantic, "should be a semantic conflict"
            print(f"  OK   raised DiffusionConflictError (semantic): {e}")
            return True


# =============================================================================
# Test 8: Local conflict (intra-board)
# =============================================================================

def test_local_conflict() -> bool:
    """Two declarations of same id active for the SAME board -> FATAL (local).

    Both declarations have requires=[IS_MAINBOARD], so both are active
    for mainboard -> intra-board conflict.
    """
    print("\n=== Test 8: Local conflict (intra-board) ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard_a.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            "mainboard_b.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low (dup)"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: SPK_VOL
    infoName: "Speaker volume"
    type: analog
    scope: LOCAL
    theme: core
    direction: downlink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        try:
            diffuse_local_channels(parsed, node)
            print("  FAIL expected DiffusionLocalConflictError, got none")
            return False
        except DiffusionLocalConflictError as e:
            assert e.channel_id == "BAT_LOW"
            assert e.board_id == "mainboard"
            print(f"  OK   raised DiffusionLocalConflictError: {e}")
            return True


# =============================================================================
# Test 9: REMOTE channel pass-through (no mirroring)
# =============================================================================

def test_remote_passthrough() -> bool:
    """REMOTE channel present on one board only -> passed through to both, no mirroring.

    REMOTE channels are NOT subject to diffusion (ND5 is out of scope).
    They are passed through to every board unchanged.
    """
    print("\n=== Test 9: REMOTE pass-through ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        # STEER_CMD is REMOTE, declared only in mainboard.cb.
        # It should appear in BOTH boards with the SAME direction (no mirroring).
        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: STEER_CMD
    infoName: "Steering command"
    type: analog
    scope: REMOTE
    theme: core
    direction: downlink
""",
            "soundboard.cb": """\
channels:
  - id: SPK_VOL
    infoName: "Speaker volume"
    type: analog
    scope: LOCAL
    theme: core
    direction: downlink
    requires:
      - IS_EXT_BOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},
        })

        result = diffuse_local_channels(parsed, node)

        # Both boards should have STEER_CMD with direction=downlink (unchanged).
        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert "STEER_CMD" in mainboard_decls, f"mainboard missing STEER_CMD: {mainboard_decls}"
        assert "STEER_CMD" in soundboard_decls, f"soundboard missing STEER_CMD: {soundboard_decls}"
        assert mainboard_decls["STEER_CMD"] == "downlink", (
            f"mainboard STEER_CMD should be downlink (unchanged), got {mainboard_decls['STEER_CMD']}"
        )
        assert soundboard_decls["STEER_CMD"] == "downlink", (
            f"soundboard STEER_CMD should be downlink (NOT mirrored to uplink), "
            f"got {soundboard_decls['STEER_CMD']}"
        )
        print(f"  OK   STEER_CMD passed through to both boards with direction=downlink (no mirroring)")

    return True


# =============================================================================
# Test 10: Legacy mode (diffusion.py not called)
# =============================================================================

def test_legacy_mode_not_called() -> bool:
    """Verify that diffusion.py is NOT called in legacy mode.

    This is a documentation test: the orchestrator (combus_scons_hook.py)
    only calls diffuse_local_channels() in node mode. We verify this by
    checking that the function exists and is importable, and by
    documenting the contract.
    """
    print("\n=== Test 10: Legacy mode (diffusion.py not called) ===")
    hook_path = Path(REPO_ROOT) / "scripts" / "combus_scons_hook.py"
    hook_source = hook_path.read_text(encoding="utf-8")
    assert "diffuse_local_channels" in hook_source, (
        "diffuse_local_channels should be referenced in combus_scons_hook.py"
    )
    lines = hook_source.split("\n")
    in_node_branch = False
    found_call = False
    for line in lines:
        if "if node.is_node" in line or "if node.is_node:" in line:
            in_node_branch = True
        if "diffuse_local_channels" in line and "def " not in line:
            found_call = True
            assert in_node_branch, (
                f"diffuse_local_channels() called outside node.is_node branch: {line!r}"
            )
    assert found_call, "diffuse_local_channels() not found in combus_scons_hook.py"
    print(f"  OK   diffuse_local_channels() is only called inside node.is_node branch")
    return True


# =============================================================================
# Helpers
# =============================================================================

def _extract_channel_ids_and_dirs(
    parsed: list[tuple[Path, str, Any]],
) -> dict[str, str]:
    """Extract {channel_id: direction} from a parsed list."""
    out: dict[str, str] = {}
    for _path, _type, raw in parsed:
        if not isinstance(raw, dict):
            continue
        section = raw.get("channels")
        if not isinstance(section, list):
            continue
        for decl in section:
            if isinstance(decl, dict):
                cid = decl.get("id")
                direction = decl.get("direction", "none")
                if isinstance(cid, str):
                    out[cid] = direction
    return out


# =============================================================================
# Main
# =============================================================================

# =============================================================================
# 3-BOARD TESTS (ND2 generalised to N >= 2)
# =============================================================================

def test_3b_uplink_2downlink_explicit() -> bool:
    """1 uplink + 2 downlink explicites sur 3 boards -> pas d'erreur, rien à diffuser."""
    print("\n=== Test 11: 3b — 1 uplink + 2 downlink explicites ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_SOUNDBOARD
""",
            "lightboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_LIGHTBOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_SOUNDBOARD"},
            "lightboard": {"IS_LIGHTBOARD"},
        })

        # Should NOT raise.
        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            result = diffuse_local_channels(parsed, node)
        assert "WARNING" not in captured.getvalue(), (
            f"no warning expected (1 uplink + 2 downlink is compatible), "
            f"got: {captured.getvalue()!r}"
        )

        main = _extract_channel_ids_and_dirs(result["mainboard"])
        snd = _extract_channel_ids_and_dirs(result["soundboard"])
        lgt = _extract_channel_ids_and_dirs(result["lightboard"])
        assert main["BAT_LOW"] == "uplink"
        assert snd["BAT_LOW"] == "downlink"
        assert lgt["BAT_LOW"] == "downlink"
        print(f"  OK   main=uplink, sound=downlink, light=downlink (no synthesis)")

    return True


def test_3b_uplink_downlink_absent() -> bool:
    """1 uplink + 1 downlink explicite + 1 board absente -> absente devient downlink."""
    print("\n=== Test 12: 3b — 1 uplink + 1 downlink + 1 absente ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_SOUNDBOARD
""",
            # lightboard.cb is absent — no declaration for BAT_LOW there.
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_SOUNDBOARD"},
            "lightboard": {"IS_LIGHTBOARD"},
        })

        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            result = diffuse_local_channels(parsed, node)
        assert "WARNING" not in captured.getvalue(), (
            f"no warning expected, got: {captured.getvalue()!r}"
        )

        main = _extract_channel_ids_and_dirs(result["mainboard"])
        snd = _extract_channel_ids_and_dirs(result["soundboard"])
        lgt = _extract_channel_ids_and_dirs(result["lightboard"])
        assert main["BAT_LOW"] == "uplink"
        assert snd["BAT_LOW"] == "downlink"
        assert lgt["BAT_LOW"] == "downlink", (
            f"lightboard (absent) should receive synthetic downlink, got {lgt.get('BAT_LOW')!r}"
        )
        print(f"  OK   main=uplink, sound=downlink, light=synthetic downlink")

    return True


def test_3b_2uplink_conflict() -> bool:
    """2 uplink sur 2 boards différentes, 3e absente -> conflit fatal, message nomme les 2 boards en uplink."""
    print("\n=== Test 13: 3b — 2 uplink (conflit fatal) ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_SOUNDBOARD
""",
            # lightboard absent.
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_SOUNDBOARD"},
            "lightboard": {"IS_LIGHTBOARD"},
        })

        try:
            diffuse_local_channels(parsed, node)
            print("  FAIL expected DiffusionConflictError, got none")
            return False
        except DiffusionConflictError as e:
            assert e.channel_id == "BAT_LOW"
            assert not e.semantic
            # The message must name BOTH uplink boards.
            msg = str(e)
            assert "mainboard" in msg, f"mainboard should be named in message: {msg}"
            assert "soundboard" in msg, f"soundboard should be named in message: {msg}"
            # The conflict involves the 2 explicit uplink boards; the
            # `involved` list contains them.
            involved_boards = [b for b, _d, _p in e.involved]
            assert "mainboard" in involved_boards
            assert "soundboard" in involved_boards
            print(f"  OK   raised DiffusionConflictError naming both uplink boards")
            return True


def test_3b_3downlink_warning() -> bool:
    """3 downlink explicites, aucun uplink -> warning 'canal mort', pas d'erreur fatale."""
    print("\n=== Test 14: 3b — 3 downlink (warning, pas fatal) ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_SOUNDBOARD
""",
            "lightboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: downlink
    requires:
      - IS_LIGHTBOARD
""",
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_SOUNDBOARD"},
            "lightboard": {"IS_LIGHTBOARD"},
        })

        # Should NOT raise. Capture stderr to verify the warning.
        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            result = diffuse_local_channels(parsed, node)

        stderr_output = captured.getvalue()
        assert "WARNING" in stderr_output, (
            f"expected WARNING on stderr, got: {stderr_output!r}"
        )
        assert "canal mort" in stderr_output, (
            f"expected 'canal mort' in warning, got: {stderr_output!r}"
        )
        # All 3 boards should be named in the warning.
        assert "mainboard" in stderr_output
        assert "soundboard" in stderr_output
        assert "lightboard" in stderr_output
        print(f"  OK   warning emitted naming all 3 downlink boards")

        # All boards keep their explicit downlink.
        for board_id in ("mainboard", "soundboard", "lightboard"):
            decls = _extract_channel_ids_and_dirs(result[board_id])
            assert decls["BAT_LOW"] == "downlink"
        print(f"  OK   all 3 boards retain BAT_LOW=downlink (no fatal error)")

    return True


def test_3b_1both_2absent() -> bool:
    """1 both + 2 boards absentes -> les 2 absentes deviennent both (rule 3)."""
    print("\n=== Test 15: 3b — 1 both + 2 absentes ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: both
    requires:
      - IS_MAINBOARD
""",
            # soundboard.cb and lightboard.cb absent.
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_SOUNDBOARD"},
            "lightboard": {"IS_LIGHTBOARD"},
        })

        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            result = diffuse_local_channels(parsed, node)
        assert "WARNING" not in captured.getvalue(), (
            f"no warning expected, got: {captured.getvalue()!r}"
        )

        main = _extract_channel_ids_and_dirs(result["mainboard"])
        snd = _extract_channel_ids_and_dirs(result["soundboard"])
        lgt = _extract_channel_ids_and_dirs(result["lightboard"])
        assert main["BAT_LOW"] == "both"
        assert snd["BAT_LOW"] == "both", (
            f"soundboard (absent) should receive synthetic both, got {snd.get('BAT_LOW')!r}"
        )
        assert lgt["BAT_LOW"] == "both", (
            f"lightboard (absent) should receive synthetic both, got {lgt.get('BAT_LOW')!r}"
        )
        print(f"  OK   main=both, sound=synthetic both, light=synthetic both")

    return True


def test_3b_1both_1uplink_1absent_semantic() -> bool:
    """1 both + 1 uplink (2 autres absentes) -> conflit sémantique fatal."""
    print("\n=== Test 16: 3b — 1 both + 1 uplink (conflit sémantique) ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: both
    requires:
      - IS_MAINBOARD
""",
            "soundboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_SOUNDBOARD
""",
            # lightboard absent.
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_SOUNDBOARD"},
            "lightboard": {"IS_LIGHTBOARD"},
        })

        try:
            diffuse_local_channels(parsed, node)
            print("  FAIL expected DiffusionConflictError (semantic), got none")
            return False
        except DiffusionConflictError as e:
            assert e.channel_id == "BAT_LOW"
            assert e.semantic, "should be a semantic conflict"
            print(f"  OK   raised DiffusionConflictError (semantic): {e}")
            return True


# =============================================================================
# Test 17: PIPELINE COMPLET — requires stale fix
# =============================================================================

def test_pipeline_full_requires_stale_fix() -> bool:
    """End-to-end pipeline test: diffuse + canonize + generate.

    Scenario: BAT_LOW declared with `requires=[IS_MAINBOARD]` on
    mainboard only, direction uplink. The synthetic declaration
    diffused to soundboard must NOT carry the stale `requires`
    field — otherwise resolve_requires() in the downstream
    pipeline would raise RequiresResolutionError on soundboard
    (which does NOT have IS_MAINBOARD).

    This test exercises the FULL pipeline (not just
    diffuse_local_channels() in isolation) and would FAIL on the
    pre-fix code with RequiresResolutionError.
    """
    print("\n=== Test 17: pipeline complet — requires stale fix ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "mainboard.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
    requires:
      - IS_MAINBOARD
""",
            # soundboard.cb absent — no declaration for BAT_LOW.
        }, src_dir)

        node = _make_node(project_root, {
            "mainboard": {"IS_MAINBOARD"},
            "soundboard": {"IS_EXT_BOARD"},  # does NOT have IS_MAINBOARD
        })

        # Step 1: diffuse.
        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            diffused = diffuse_local_channels(parsed, node)

        # Step 2: canonize the soundboard list.
        from scripts.combus_builder.canon import canonize
        soundboard_canon = canonize(diffused["soundboard"])

        # Step 3: generate() with the REAL soundboard BuildContext
        # (which does NOT have IS_MAINBOARD). This is where the
        # pre-fix code would raise RequiresResolutionError.
        from scripts.combus_builder.generator import (
            RequiresResolutionError,
            generate,
        )
        soundboard_ctx = node.boards["soundboard"]
        out_dir = tmp_path / "out_soundboard"
        out_dir.mkdir()
        try:
            sel = generate(
                soundboard_canon.canonical_definitions,
                soundboard_ctx,
                out_dir,
                filename_prefix="soundboard",
            )
        except RequiresResolutionError as e:
            print(f"  FAIL RequiresResolutionError raised: {e}")
            return False

        # Verify BAT_LOW is present, active, in downlink.
        # The full view contains all LOCAL channels.
        all_ids = [vc.ch.id for vc in sel.full.channels]
        assert "BAT_LOW" in all_ids, (
            f"BAT_LOW should be in soundboard's full view, got {all_ids}"
        )
        bat_low_vc = next(vc for vc in sel.full.channels if vc.ch.id == "BAT_LOW")
        # direction_bits: 0=none, 1=uplink, 2=downlink, 3=both
        assert bat_low_vc.direction_bits == 2, (
            f"BAT_LOW direction_bits should be 2 (downlink, mirrored from mainboard's uplink), "
            f"got {bat_low_vc.direction_bits}"
        )
        print(f"  OK   BAT_LOW present in soundboard, direction=downlink (no RequiresResolutionError)")

    return True


# =============================================================================
# Main
# =============================================================================

def main() -> int:
    import io
    import contextlib

    results = []
    # 2-board tests (existing + adapted).
    results.append(("mirror_simple", test_mirror_simple()))
    results.append(("complementary_pair", test_complementary_pair()))
    results.append(("conflict_uplink_uplink", test_conflict_uplink_uplink()))
    results.append(("downlink_downlink_now_warning", test_downlink_downlink_now_warning()))
    results.append(("both_both_ok", test_both_both_ok()))
    results.append(("none_x_ok", test_none_x_ok()))
    results.append(("semantic_conflict_both_uplink", test_semantic_conflict_both_uplink()))
    results.append(("local_conflict", test_local_conflict()))
    results.append(("remote_passthrough", test_remote_passthrough()))
    results.append(("legacy_mode_not_called", test_legacy_mode_not_called()))
    # 3-board tests (new, ND2 generalised).
    results.append(("3b_uplink_2downlink_explicit", test_3b_uplink_2downlink_explicit()))
    results.append(("3b_uplink_downlink_absent", test_3b_uplink_downlink_absent()))
    results.append(("3b_2uplink_conflict", test_3b_2uplink_conflict()))
    results.append(("3b_3downlink_warning", test_3b_3downlink_warning()))
    results.append(("3b_1both_2absent", test_3b_1both_2absent()))
    results.append(("3b_1both_1uplink_1absent_semantic", test_3b_1both_1uplink_1absent_semantic()))
    # Pipeline-complet test (requires stale fix).
    results.append(("pipeline_full_requires_stale_fix", test_pipeline_full_requires_stale_fix()))

    print("\n=== Summary ===")
    all_ok = True
    for name, ok in results:
        status = "OK  " if ok else "FAIL"
        print(f"  [{status}] {name}")
        if not ok:
            all_ok = False

    if all_ok:
        print("\nAll ND2 diffusion tests passed.")
        return 0
    else:
        print("\nSome ND2 diffusion tests FAILED.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
