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
# Test 1: Mirror simple
# =============================================================================

def test_mirror_simple() -> bool:
    """BAT_LOW declared uplink on mainboard only -> soundboard gets downlink.

    Ownership via requires:
      - mainboard.cb: BAT_LOW with requires=[IS_MAINBOARD] -> active for mainboard only
      - soundboard.cb: SPK_VOL with requires=[IS_EXT_BOARD] -> active for soundboard only
    """
    print("\n=== Test 1: Mirror simple ===")
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

        result = diffuse_local_channels(parsed, node)

        # mainboard should have BAT_LOW (uplink, own) + SPK_VOL (synthetic uplink, mirrored from soundboard's downlink).
        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        assert "BAT_LOW" in mainboard_decls, f"mainboard missing BAT_LOW: {mainboard_decls}"
        assert mainboard_decls["BAT_LOW"] == "uplink", (
            f"mainboard BAT_LOW should be uplink, got {mainboard_decls['BAT_LOW']}"
        )
        assert "SPK_VOL" in mainboard_decls, f"mainboard missing SPK_VOL (synthetic): {mainboard_decls}"
        assert mainboard_decls["SPK_VOL"] == "uplink", (
            f"mainboard SPK_VOL should be uplink (mirrored from soundboard's downlink), "
            f"got {mainboard_decls['SPK_VOL']}"
        )
        print(f"  OK   mainboard: {mainboard_decls}")

        # soundboard should have SPK_VOL (downlink, own) + BAT_LOW (synthetic downlink, mirrored from mainboard's uplink).
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert "SPK_VOL" in soundboard_decls, f"soundboard missing SPK_VOL: {soundboard_decls}"
        assert soundboard_decls["SPK_VOL"] == "downlink", (
            f"soundboard SPK_VOL should be downlink, got {soundboard_decls['SPK_VOL']}"
        )
        assert "BAT_LOW" in soundboard_decls, f"soundboard missing BAT_LOW (synthetic): {soundboard_decls}"
        assert soundboard_decls["BAT_LOW"] == "downlink", (
            f"soundboard BAT_LOW should be downlink (mirrored from mainboard's uplink), "
            f"got {soundboard_decls['BAT_LOW']}"
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
            assert e.direction_a == "uplink"
            assert e.direction_b == "uplink"
            assert not e.semantic
            print(f"  OK   raised DiffusionConflictError: {e}")
            return True


# =============================================================================
# Test 4: Conflict downlink/downlink
# =============================================================================

def test_conflict_downlink_downlink() -> bool:
    """Both boards declare BAT_LOW as downlink -> FATAL."""
    print("\n=== Test 4: Conflict downlink/downlink ===")
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

        try:
            diffuse_local_channels(parsed, node)
            print("  FAIL expected DiffusionConflictError, got none")
            return False
        except DiffusionConflictError as e:
            assert e.channel_id == "BAT_LOW"
            assert e.direction_a == "downlink"
            assert e.direction_b == "downlink"
            assert not e.semantic
            print(f"  OK   raised DiffusionConflictError: {e}")
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

def main() -> int:
    results = []
    results.append(("mirror_simple", test_mirror_simple()))
    results.append(("complementary_pair", test_complementary_pair()))
    results.append(("conflict_uplink_uplink", test_conflict_uplink_uplink()))
    results.append(("conflict_downlink_downlink", test_conflict_downlink_downlink()))
    results.append(("both_both_ok", test_both_both_ok()))
    results.append(("none_x_ok", test_none_x_ok()))
    results.append(("semantic_conflict_both_uplink", test_semantic_conflict_both_uplink()))
    results.append(("local_conflict", test_local_conflict()))
    results.append(("remote_passthrough", test_remote_passthrough()))
    results.append(("legacy_mode_not_called", test_legacy_mode_not_called()))

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
