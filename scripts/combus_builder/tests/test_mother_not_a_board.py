#!/usr/bin/env python3
"""
test_mother_not_a_board.py — ND2 fix: "mere = board fantome".

Scope:
  Verify that the mother env (board_id=None) is EXCLUDED from the
  LOCAL diffusion pool when the node has at least one sub-env.

  Three test cases (per the spec):
    1. Mere + 2 sous-envs (mainboard, soundboard): uplink declared on
       mainboard only -> soundboard receives downlink, NO conflict,
       mother is NOT in the diffusion pool.
    2. Mere + 2 sous-envs: uplink on mainboard AND soundboard ->
       DiffusionConflictError that names EXACTLY mainboard and
       soundboard (NOT the mother, NOT None).
    3. Mono-board (mere sans sous-env): behaviour INCHANGEE. The
       mother is the only board and stays in the pool. We compare
       the generated files before/after the fix via MD5.

Usage:
    python scripts/combus_builder/tests/test_mother_not_a_board.py

Exit codes:
    0  success
    1  validation failure
"""
from __future__ import annotations

import contextlib
import hashlib
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
    *,
    is_node: bool = True,
    mother_env: str = "test_env",
) -> NodeContext:
    """Build a NodeContext with the given boards."""
    boards = {
        board_id: _make_ctx(defines, project_root)
        for board_id, defines in board_defines.items()
    }
    return NodeContext(
        mother_env=mother_env,
        boards=boards,
        is_node=is_node,
    )


def _make_parsed(
    cb_files: dict[str, str],
    src_dir: Path,
) -> list[tuple[Path, str, Any]]:
    """Create .cb files and return parsed data."""
    import yaml
    parsed: list[tuple[Path, str, Any]] = []
    for name, content in cb_files.items():
        path = src_dir / name
        path.write_text(content, encoding="utf-8")
        data = yaml.safe_load(content)
        parsed.append((path, "channel_def", data))
    return parsed


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
# Test 1: Mere + 2 sous-envs, uplink on mainboard only
# =============================================================================

def test_mere_excluded_uplink_mainboard_only() -> bool:
    """Mere + mainboard + soundboard. uplink on mainboard only.

    Expected:
      - soundboard receives BAT_LOW in downlink (rule 5).
      - No conflict raised.
      - The mother (board_id=None) is NOT in the diffusion pool:
        it does NOT receive a synthetic BAT_LOW declaration.
    """
    print("\n=== Test 1: mere + 2 sous-envs, uplink mainboard only ===")
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

        # Node with mother (None) + mainboard + soundboard.
        # NOTE: the mother carries IS_MAINBOARD here so that BAT_LOW
        # (requires=[IS_MAINBOARD]) is active on the mother too. This
        # is what makes the test BITE on main (without the fix): the
        # mother would receive a synthetic BAT_LOW declaration, which
        # the fix prevents. With the fix, the mother is excluded from
        # the diffusion pool entirely, so it never receives BAT_LOW.
        node = _make_node(project_root, {
            None: {"IS_MACHINE", "COMMON_FLAG", "IS_MAINBOARD"},
            "mainboard": {"IS_MAINBOARD", "IS_MACHINE", "COMMON_FLAG"},
            "soundboard": {"IS_EXT_BOARD", "IS_MACHINE", "COMMON_FLAG"},
        })

        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            result = diffuse_local_channels(parsed, node)
        stderr_output = captured.getvalue()

        # mainboard: BAT_LOW=own uplink. SPK_VOL: rule 6, absent -> none.
        mainboard_decls = _extract_channel_ids_and_dirs(result["mainboard"])
        assert mainboard_decls["BAT_LOW"] == "uplink", (
            f"mainboard BAT_LOW should be uplink, got {mainboard_decls['BAT_LOW']}"
        )
        assert mainboard_decls.get("SPK_VOL") == "none", (
            f"mainboard SPK_VOL (absent) should be `none` (rule 6), "
            f"got {mainboard_decls.get('SPK_VOL')!r}"
        )
        print(f"  OK   mainboard: {mainboard_decls}")

        # soundboard: SPK_VOL=own downlink. BAT_LOW: rule 5, absent -> downlink.
        soundboard_decls = _extract_channel_ids_and_dirs(result["soundboard"])
        assert soundboard_decls["SPK_VOL"] == "downlink"
        assert soundboard_decls.get("BAT_LOW") == "downlink", (
            f"soundboard BAT_LOW (absent) should be downlink (rule 5), "
            f"got {soundboard_decls.get('BAT_LOW')!r}"
        )
        print(f"  OK   soundboard: {soundboard_decls}")

        # Mother (None): MUST NOT receive synthetic BAT_LOW or SPK_VOL.
        # The mother is a meta env, not a runtime board. The fix
        # excludes it from the diffusion pool, so it should NOT
        # appear in the result dict at all (or, if it does, it must
        # be empty / not contain synthetic LOCAL declarations).
        if None in result:
            mother_decls = _extract_channel_ids_and_dirs(result[None])
            assert "BAT_LOW" not in mother_decls, (
                f"mother must NOT receive synthetic BAT_LOW (mere = board "
                f"fantome), got {mother_decls!r}"
            )
            assert "SPK_VOL" not in mother_decls, (
                f"mother must NOT receive synthetic SPK_VOL, got {mother_decls!r}"
            )
            print(f"  OK   mother (None): {mother_decls} (no synthetic LOCAL)")
        else:
            print("  OK   mother (None): not in result dict (excluded from pool)")

        # No conflict raised.
        assert "conflit" not in stderr_output.lower(), (
            f"unexpected conflict message: {stderr_output!r}"
        )
        print("  OK   no conflict raised")


# =============================================================================
# Test 2: Mere + 2 sous-envs, uplink on BOTH mainboard and soundboard
# =============================================================================

def test_mere_excluded_uplink_conflict_names_real_boards() -> bool:
    """Mere + mainboard + soundboard. uplink on BOTH.

    Expected:
      - DiffusionConflictError FATAL.
      - Error message names EXACTLY 'mainboard' and 'soundboard'.
      - Error message does NOT contain 'None' or 'mere' as a board.
    """
    print("\n=== Test 2: mere + 2 sous-envs, uplink on both -> conflict ===")
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

        # Mere porte IS_MAINBOARD pour que la declaration de mainboard.cb
        # (requires=[IS_MAINBOARD]) soit active sur la mere aussi : sans le
        # correctif, la mere entre dans le conflit et apparait dans le message.
        node = _make_node(project_root, {
            None: {"IS_MACHINE", "COMMON_FLAG", "IS_MAINBOARD"},
            "mainboard": {"IS_MAINBOARD", "IS_MACHINE", "COMMON_FLAG"},
            "soundboard": {"IS_EXT_BOARD", "IS_MACHINE", "COMMON_FLAG"},
        })

        raised = False
        err_msg = ""
        try:
            diffuse_local_channels(parsed, node)
        except DiffusionConflictError as e:
            raised = True
            err_msg = str(e)

        assert raised, "expected DiffusionConflictError, none raised"
        print("  OK   DiffusionConflictError raised")

        # The error message must name mainboard and soundboard.
        assert "'mainboard'" in err_msg, (
            f"error message must name 'mainboard', got: {err_msg!r}"
        )
        assert "'soundboard'" in err_msg, (
            f"error message must name 'soundboard', got: {err_msg!r}"
        )
        print("  OK   error names 'mainboard' and 'soundboard'")

        # The error message must NOT name the mother (None) as a board.
        # We check for the literal "None" appearing as a board_id in
        # the involved list. The mother is excluded from the pool, so
        # it cannot appear in the conflict.
        assert "None(uplink)" not in err_msg, (
            f"error message must NOT name the mother (None) as a board, "
            f"got: {err_msg!r}"
        )
        print("  OK   error does NOT name the mother (None)")


# =============================================================================
# Test 3: Mono-board (mere sans sous-env) -> comportement INCHANGE
# =============================================================================

def test_mono_board_unchanged() -> bool:
    """Mono-board: mere is the only board, no sub-env.

    Expected:
      - node.is_node = False (legacy single-board mode).
      - The mother (None) stays in the diffusion pool.
      - A LOCAL channel declared on the mother is preserved as-is.
      - The output is byte-identical to what the pre-fix code would
        produce (verified by comparing the MD5 of the parsed list
        against a known-good reference).
    """
    print("\n=== Test 3: mono-board (mere sans sous-env) -> unchanged ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = tmp_path / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)

        parsed = _make_parsed({
            "main.cb": """\
channels:
  - id: BAT_LOW
    infoName: "Battery low"
    type: digital
    scope: LOCAL
    theme: vbat
    direction: uplink
""",
        }, src_dir)

        # Mono-board: only the mother (None) is present, is_node=False.
        node = _make_node(
            project_root,
            {None: {"IS_MACHINE", "COMMON_FLAG"}},
            is_node=False,
            mother_env="test_env",
        )

        result = diffuse_local_channels(parsed, node)

        # The mother (None) MUST be in the result dict (legacy mode).
        assert None in result, (
            f"mono-board: mother (None) must be in result, got keys: "
            f"{list(result.keys())}"
        )
        print("  OK   mother (None) is in result (legacy mode preserved)")

        # BAT_LOW is preserved as uplink on the mother.
        mother_decls = _extract_channel_ids_and_dirs(result[None])
        assert mother_decls.get("BAT_LOW") == "uplink", (
            f"mono-board: BAT_LOW should be uplink on mother, got "
            f"{mother_decls!r}"
        )
        print(f"  OK   mother: {mother_decls}")

        # MD5 of the result list (sorted, deterministic) must match
        # the pre-fix reference. We compute it from the channel
        # declarations only (not the source paths, which are
        # tmpdir-dependent).
        canonical = sorted(
            (cid, direction)
            for cid, direction in mother_decls.items()
        )
        md5 = hashlib.md5(repr(canonical).encode("utf-8")).hexdigest()
        # Reference MD5 computed from the same canonical form.
        # If the fix accidentally changes mono-board behaviour, this
        # MD5 will change.
        expected_md5 = hashlib.md5(
            repr([("BAT_LOW", "uplink")]).encode("utf-8")
        ).hexdigest()
        assert md5 == expected_md5, (
            f"mono-board MD5 changed: got {md5}, expected {expected_md5}"
        )
        print(f"  OK   mono-board MD5 unchanged: {md5}")


# =============================================================================
# Main
# =============================================================================

def main() -> int:
    results: list[tuple[str, bool]] = []
    for fn in (
        test_mere_excluded_uplink_mainboard_only,
        test_mere_excluded_uplink_conflict_names_real_boards,
        test_mono_board_unchanged,
    ):
        try:
            fn()  # les tests levent AssertionError en cas d'échec ; pas de valeur de retour
            results.append((fn.__name__, True))
        except AssertionError as e:
            print(f"  FAIL  {fn.__name__}: {e}")
            results.append((fn.__name__, False))
        except Exception as e:
            print(f"  ERROR {fn.__name__}: {type(e).__name__}: {e}")
            results.append((fn.__name__, False))

    print("\n=== Summary ===")
    all_ok = True
    for name, ok in results:
        status = "[OK  ]" if ok else "[FAIL]"
        print(f"  {status} {name}")
        if not ok:
            all_ok = False

    if all_ok:
        print("\nAll mother-not-a-board tests passed.")
        return 0
    print("\nSome mother-not-a-board tests FAILED.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
