#!/usr/bin/env python3
"""
ND1 multiboard validation script (CLI, no real PlatformIO build).

This script validates the ND1 implementation by simulating a node with
2 fictitious sub-envs and verifying that:

  1. parse_env_hierarchy() correctly detects the node structure.
  2. NodeContext.boards contains 2 distinct BuildContexts with the
     correct defines (one per board).
  3. generate() is called 2 times with 2 different filename_prefixes.
  4. The generated files have the expected per-board names.

It also validates the legacy single-board mode (no node detected) by
verifying that the behaviour is strictly identical to pre-ND1.

Usage:
    python scripts/combus_builder/tests/test_nd1_multiboard.py

Exit codes:
    0  success
    1  validation failure
"""
from __future__ import annotations

import sys
import tempfile
from pathlib import Path

# Ensure the combus_builder package is importable when running this
# file directly from the repo root.
THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.generator import generate
from scripts.combus_builder.node_context import (
    NodeContext,
    NodeContextError,
    parse_env_hierarchy,
)
from scripts.combus_builder.parser import discover_and_parse


# =============================================================================
# Test fixtures
# =============================================================================

# Minimal .cb content for a single channel (used to populate the
# fictitious src/ tree so discover_and_parse() has something to find).
SAMPLE_CB_CONTENT = """\
channels:
  - id: TEST_CHANNEL
    infoName: "Test channel"
    type: analog
    scope: REMOTE
    theme: core
    direction: uplink
"""

# Fictitious platformio.ini with a node of 2 sub-envs.
FICTITIOUS_INI_NODE = """\
[env]
platform = espressif32
build_flags =
    -D COMMON_FLAG

[env:volvo_A60H_bruder]
build_flags =
    ${env.build_flags}
    -D IS_MACHINE
    -D MACHINE_VOLVO_A60_H_BRUDER

[env:volvo_A60H_bruder_mainboard]
build_flags =
    ${env:volvo_A60H_bruder.build_flags}
    -D IS_MAINBOARD
    -D BOARD_ESP32_8M_6S

[env:volvo_A60H_bruder_soundboard]
build_flags =
    ${env:volvo_A60H_bruder.build_flags}
    -D IS_EXT_BOARD
    -D BOARD=SOUND_BOARD_ESP32
"""

# Fictitious platformio.ini with a single env (legacy mode).
FICTITIOUS_INI_LEGACY = """\
[env]
platform = espressif32
build_flags =
    -D COMMON_FLAG

[env:volvo_A60H_bruder]
build_flags =
    ${env.build_flags}
    -D IS_MACHINE
    -D MACHINE_VOLVO_A60_H_BRUDER
"""


def _make_fake_project(
    tmp_path: Path,
    ini_content: str,
) -> tuple[Path, Path]:
    """Create a fake PlatformIO project under tmp_path.

    Returns (project_root, src_dir).
    """
    project_root = tmp_path / "proj"
    src_dir = project_root / "src"
    src_dir.mkdir(parents=True)
    (project_root / "platformio.ini").write_text(ini_content, encoding="utf-8")
    # Drop a minimal .cb file so discover_and_parse() finds something.
    (src_dir / "test.cb").write_text(SAMPLE_CB_CONTENT, encoding="utf-8")
    return project_root, src_dir


def _make_fake_build_context(
    project_root: Path,
    defines: set[str],
    values: dict[str, str] | None = None,
) -> BuildContext:
    """Build a BuildContext without going through acquire_build_context().

    Used to simulate the result of resolve_build_context_static() in
    tests where PlatformIO is not installed.
    """
    return BuildContext(
        buildroot=(project_root / "src").resolve(),
        defines=frozenset(defines),
        defines_with_value=dict(values or {}),
        cppdefines_source="static",
        raw_cppdefines_sample="<fake>",
    )


# =============================================================================
# Test 1: legacy single-board mode (ND1: strictly unchanged)
# =============================================================================

def test_legacy_mode() -> None:
    """Verify that legacy single-board mode is strictly unchanged.

    In legacy mode:
      - parse_env_hierarchy() returns is_node=False.
      - generate() is called once with filename_prefix="".
      - The generated files have the legacy names (no prefix).
    """
    print("\n=== Test 1: legacy single-board mode ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root, _ = _make_fake_project(tmp_path, FICTITIOUS_INI_LEGACY)

        # --- 1. parse_env_hierarchy() ---
        all_env_names = ["volvo_A60H_bruder"]
        node = parse_env_hierarchy("volvo_A60H_bruder", all_env_names)
        assert not node.is_node, (
            f"legacy mode: expected is_node=False, got {node.is_node}"
        )
        assert node.mother_env == "volvo_A60H_bruder", (
            f"legacy mode: expected mother_env='volvo_A60H_bruder', "
            f"got {node.mother_env!r}"
        )
        print(f"  OK   parse_env_hierarchy: is_node={node.is_node}, "
              f"mother_env={node.mother_env!r}")

        # --- 2. generate() with no prefix ---
        out_dir = tmp_path / "out_legacy"
        out_dir.mkdir()
        _, _, parsed = discover_and_parse(project_root=project_root)
        canon = canonize(parsed)
        ctx = _make_fake_build_context(
            project_root,
            defines={"IS_MACHINE", "MACHINE_VOLVO_A60_H_BRUDER"},
        )
        sel = generate(canon.canonical_definitions, ctx, out_dir)

        # --- 3. Verify legacy file names ---
        expected_files = [
            "combus.h", "combus.cpp", "combus_ids.h",
            "combus_local_ids.h", "combus_remote_ids.h",
            "combus_md5.h", "combus_local_md5.h", "combus_remote_md5.h",
            "combus_wire_common.h",
        ]
        for name in expected_files:
            assert (out_dir / name).is_file(), (
                f"legacy mode: expected file {name} not found in {out_dir}"
            )
        # Verify NO prefixed files exist.
        for p in out_dir.iterdir():
            assert "_mainboard" not in p.name and "_soundboard" not in p.name, (
                f"legacy mode: unexpected prefixed file {p.name}"
            )
        print(f"  OK   generated {len(expected_files)} legacy files (no prefix)")
        print(f"  OK   sel.full.ch_count={sel.full.ch_count}, "
              f"sel.full.wire_end={sel.full.wire_end}")


# =============================================================================
# Test 2: node mode with 2 fictitious sub-envs
# =============================================================================

def test_node_mode_2_boards() -> None:
    """Verify that node mode with 2 sub-envs works correctly.

    In node mode:
      - parse_env_hierarchy() returns is_node=True.
      - NodeContext.boards contains 2 entries (mainboard, soundboard).
      - generate() is called 2 times with 2 different filename_prefixes.
      - The generated files have the expected per-board names.
    """
    print("\n=== Test 2: node mode with 2 sub-envs ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root, _ = _make_fake_project(tmp_path, FICTITIOUS_INI_NODE)

        # --- 1. parse_env_hierarchy() ---
        all_env_names = [
            "volvo_A60H_bruder",
            "volvo_A60H_bruder_mainboard",
            "volvo_A60H_bruder_soundboard",
        ]
        # Simulate building the mainboard sub-env.
        node = parse_env_hierarchy("volvo_A60H_bruder_mainboard", all_env_names)
        assert node.is_node, (
            f"node mode: expected is_node=True, got {node.is_node}"
        )
        assert node.mother_env == "volvo_A60H_bruder", (
            f"node mode: expected mother_env='volvo_A60H_bruder', "
            f"got {node.mother_env!r}"
        )
        print(f"  OK   parse_env_hierarchy: is_node={node.is_node}, "
              f"mother_env={node.mother_env!r}")

        # --- 2. Simulate resolve_build_context_static() for each board ---
        # PlatformIO is not installed in this test environment, so we
        # build the BuildContexts manually. The defines below mirror
        # what platformio.project.config.ProjectConfig would resolve
        # from FICTITIOUS_INI_NODE (extends= + ${...} interpolation).
        boards_ctx: dict[str | None, BuildContext] = {}
        boards_ctx["mainboard"] = _make_fake_build_context(
            project_root,
            defines={"IS_MACHINE", "MACHINE_VOLVO_A60_H_BRUDER",
                     "IS_MAINBOARD", "BOARD_ESP32_8M_6S"},
        )
        boards_ctx["soundboard"] = _make_fake_build_context(
            project_root,
            defines={"IS_MACHINE", "MACHINE_VOLVO_A60_H_BRUDER",
                     "IS_EXT_BOARD"},
            values={"BOARD": "SOUND_BOARD_ESP32"},
        )
        # Build a NodeContext with the resolved boards.
        node_with_boards = NodeContext(
            mother_env=node.mother_env,
            boards=boards_ctx,
            is_node=node.is_node,
        )
        assert len(node_with_boards.boards) == 2, (
            f"node mode: expected 2 boards, got {len(node_with_boards.boards)}"
        )
        assert "mainboard" in node_with_boards.boards
        assert "soundboard" in node_with_boards.boards
        print(f"  OK   NodeContext.boards has 2 entries: "
              f"{list(node_with_boards.boards.keys())}")

        # Verify the defines are distinct per board.
        mainboard_defines = node_with_boards.boards["mainboard"].defines
        soundboard_defines = node_with_boards.boards["soundboard"].defines
        assert "IS_MAINBOARD" in mainboard_defines
        assert "IS_MAINBOARD" not in soundboard_defines
        assert "IS_EXT_BOARD" in soundboard_defines
        assert "IS_EXT_BOARD" not in mainboard_defines
        print(f"  OK   mainboard defines: {sorted(mainboard_defines)}")
        print(f"  OK   soundboard defines: {sorted(soundboard_defines)}")

        # --- 3. generate() once per board ---
        out_dir = tmp_path / "out_node"
        out_dir.mkdir()
        _, _, parsed = discover_and_parse(project_root=project_root)
        canon = canonize(parsed)

        generate_calls: list[tuple[str, str]] = []  # (board_id, prefix)
        for board_id, board_ctx in node_with_boards.boards.items():
            prefix = board_id or ""
            sel = generate(
                canon.canonical_definitions,
                board_ctx,
                out_dir,
                filename_prefix=prefix,
            )
            generate_calls.append((board_id, prefix))
            print(f"  OK   generate() called for board {board_id!r} "
                  f"with prefix={prefix!r}: "
                  f"combus ({sel.full.ch_count} ch, "
                  f"WIRE_END={sel.full.wire_end})")

        assert len(generate_calls) == 2, (
            f"node mode: expected 2 generate() calls, got {len(generate_calls)}"
        )
        prefixes = [p for _, p in generate_calls]
        assert "mainboard" in prefixes
        assert "soundboard" in prefixes
        print(f"  OK   generate() called 2 times with prefixes: {prefixes}")

        # --- 4. Verify per-board file names ---
        expected_per_board = {
            "mainboard": [
                "combus_mainboard_ids.h",
                "combus_mainboard.h",
                "combus_mainboard.cpp",
                "combus_local_mainboard_ids.h",
                "combus_remote_mainboard_ids.h",
                "combus_mainboard_md5.h",
                "combus_local_mainboard_md5.h",
                "combus_remote_mainboard_md5.h",
            ],
            "soundboard": [
                "combus_soundboard_ids.h",
                "combus_soundboard.h",
                "combus_soundboard.cpp",
                "combus_local_soundboard_ids.h",
                "combus_remote_soundboard_ids.h",
                "combus_soundboard_md5.h",
                "combus_local_soundboard_md5.h",
                "combus_remote_soundboard_md5.h",
            ],
        }
        for board_id, files in expected_per_board.items():
            for name in files:
                assert (out_dir / name).is_file(), (
                    f"node mode: expected file {name} for board "
                    f"{board_id!r} not found in {out_dir}"
                )
            print(f"  OK   board {board_id!r}: {len(files)} files generated")

        # combus_wire_common.h is shared (not prefixed).
        assert (out_dir / "combus_wire_common.h").is_file(), (
            "node mode: expected shared combus_wire_common.h"
        )
        print(f"  OK   shared combus_wire_common.h present")

        # Verify NO unprefixed combus files exist (except combus_wire_common.h).
        for p in out_dir.iterdir():
            if p.name == "combus_wire_common.h":
                continue
            if p.name.startswith("combus") and "_" not in p.name.split(".")[0]:
                # combus.h, combus.cpp, combus_ids.h, combus_md5.h
                # should NOT exist in node mode (they are replaced by
                # the prefixed versions).
                assert False, (
                    f"node mode: unexpected unprefixed file {p.name}"
                )
        print(f"  OK   no unprefixed combus files (except combus_wire_common.h)")

        # --- 5. Verify #include directives in generated files ---
        # combus_mainboard.h should #include "combus_mainboard_ids.h"
        # (not "combus_ids.h").
        combus_h = (out_dir / "combus_mainboard.h").read_text(encoding="utf-8")
        assert '#include "combus_mainboard_ids.h"' in combus_h, (
            f"node mode: combus_mainboard.h should #include "
            f'"combus_mainboard_ids.h", got:\n{combus_h[:500]}'
        )
        assert '#include "combus_ids.h"' not in combus_h, (
            f"node mode: combus_mainboard.h should NOT #include "
            f'"combus_ids.h"'
        )
        print(f"  OK   combus_mainboard.h #include is correctly prefixed")

        combus_cpp = (out_dir / "combus_mainboard.cpp").read_text(encoding="utf-8")
        assert '#include "combus_mainboard.h"' in combus_cpp, (
            f"node mode: combus_mainboard.cpp should #include "
            f'"combus_mainboard.h"'
        )
        print(f"  OK   combus_mainboard.cpp #include is correctly prefixed")


# =============================================================================
# Test 3: parse_env_hierarchy edge cases
# =============================================================================

def test_parse_env_hierarchy_edge_cases() -> None:
    """Verify edge cases of parse_env_hierarchy()."""
    print("\n=== Test 3: parse_env_hierarchy edge cases ===")

    # Case 1: empty env_name raises NodeContextError.
    try:
        parse_env_hierarchy("", ["foo"])
    except NodeContextError:
        print("  OK   empty env_name raises NodeContextError")
    else:
        raise AssertionError("empty env_name should raise NodeContextError")

    # Case 2: env_name not in all_env_names → legacy mode.
    node = parse_env_hierarchy("standalone_env", ["other_env"])
    assert not node.is_node
    assert node.mother_env == "standalone_env"
    print(f"  OK   standalone env: is_node={node.is_node}")

    # Case 3: nested prefixes → longest prefix wins.
    all_envs = [
        "foo",
        "foo_bar",
        "foo_bar_baz",
        "foo_bar_baz_qux",
    ]
    node = parse_env_hierarchy("foo_bar_baz_qux", all_envs)
    assert node.is_node
    assert node.mother_env == "foo_bar_baz", (
        f"nested prefixes: expected mother_env='foo_bar_baz', "
        f"got {node.mother_env!r}"
    )
    print(f"  OK   nested prefixes: mother_env={node.mother_env!r}")

    # Case 4: building the mother env directly (with sister sub-envs).
    all_envs_with_sisters = [
        "volvo_A60H_bruder",
        "volvo_A60H_bruder_mainboard",
        "volvo_A60H_bruder_soundboard",
    ]
    node = parse_env_hierarchy("volvo_A60H_bruder", all_envs_with_sisters)
    assert node.is_node, (
        f"mother env with sisters: expected is_node=True, got {node.is_node}"
    )
    assert node.mother_env == "volvo_A60H_bruder"
    print(f"  OK   building mother env directly: is_node={node.is_node}")



# =============================================================================
# Main
# =============================================================================

if __name__ == "__main__":
    import pytest
    sys.exit(pytest.main([__file__, "-q"]))
