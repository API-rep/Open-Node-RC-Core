#!/usr/bin/env python3
"""
ND1 integration tests — exercise the per-board generation loop and
resolve_build_context_static() against a real (fictitious) project.

These tests cover the two gaps identified in the post-review of ND1:

  1. test_generate_per_board_mother_uses_static_resolution
     Exercises _generate_per_board() (the function extracted from
     _generate_combus_action() for testability) with pio_env set to a
     SUB-env (volvo_A60H_bruder_mainboard). Verifies that the
     unprefixed artefacts (combus.h, combus.cpp, combus_ids.h, ...)
     reflect the MOTHER env's defines, not the sub-env's. This is
     the bug that was missed by test_node_mode_2_boards (which
     bypassed the per-board loop entirely).

  2. test_generate_per_board_mother_uses_live_ctx
     Same setup, but pio_env is the MOTHER env itself. Verifies that
     the unprefixed artefacts reflect the mother env's defines via
     the live `ctx` (no static resolution needed in this case).

  3. test_resolve_build_context_static_real_project_config
     Smoke test for resolve_build_context_static() against a real
     platformio.project.config.ProjectConfig. Skipped with an
     explicit message if the `platformio` Python package is not
     installed in the test environment.

Usage:
    python scripts/combus_builder/tests/test_nd1_integration.py

Exit codes:
    0  success
    1  validation failure

NOTE on mocking (Option A — chosen for simplicity):
  The extracted _generate_per_board() function is exec'd in a clean
  namespace. To make unittest.mock.patch() on
  `scripts.combus_builder.node_context.resolve_build_context_static`
  actually take effect, we MUST NOT strip the late imports from the
  function source. The `from scripts.combus_builder.node_context
  import resolve_build_context_static` line is therefore kept inside
  the function body — it re-resolves the name on every call, so the
  patch (which replaces the attribute on the module) is honoured.
"""
from __future__ import annotations

import sys
import tempfile
import types
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from unittest.mock import patch

# Ensure the combus_builder package is importable when running this
# file directly from the repo root.
THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize
from scripts.combus_builder.flags import BuildContext, BuildContextError
from scripts.combus_builder.generator import GeneratorError, generate
from scripts.combus_builder.node_context import (
    NodeContextError,
    resolve_build_context_static,
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
    scope: BASE
    theme: core
    direction: uplink
    value: CbusNeutral
"""

# Fictitious platformio.ini with a node of 2 sub-envs.
# The mother env has IS_MACHINE + MACHINE_VOLVO_A60_H_BRUDER.
# The mainboard sub-env adds IS_MAINBOARD + BOARD_ESP32_8M_6S.
# The soundboard sub-env adds IS_EXT_BOARD + BOARD=SOUND_BOARD_ESP32.
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

# Expected defines per env (what resolve_build_context_static would
# return if PlatformIO were installed and the resolution worked).
EXPECTED_DEFINES_PER_ENV: dict[str, set[str]] = {
    "volvo_A60H_bruder": {
        "COMMON_FLAG",
        "IS_MACHINE",
        "MACHINE_VOLVO_A60_H_BRUDER",
    },
    "volvo_A60H_bruder_mainboard": {
        "COMMON_FLAG",
        "IS_MACHINE",
        "MACHINE_VOLVO_A60_H_BRUDER",
        "IS_MAINBOARD",
        "BOARD_ESP32_8M_6S",
    },
    "volvo_A60H_bruder_soundboard": {
        "COMMON_FLAG",
        "IS_MACHINE",
        "MACHINE_VOLVO_A60_H_BRUDER",
        "IS_EXT_BOARD",
    },
}


def _make_fake_project(
    tmp_path: Path,
    ini_content: str,
) -> Path:
    """Create a fake PlatformIO project under tmp_path.

    Returns the project_root.
    """
    project_root = tmp_path / "proj"
    src_dir = project_root / "src"
    src_dir.mkdir(parents=True)
    (project_root / "platformio.ini").write_text(ini_content, encoding="utf-8")
    # Drop a minimal .cb file so discover_and_parse() finds something.
    (src_dir / "test.cb").write_text(SAMPLE_CB_CONTENT, encoding="utf-8")
    return project_root


def _make_fake_build_context(
    project_root: Path,
    defines: set[str],
    values: dict[str, str] | None = None,
    source: str = "live",
) -> BuildContext:
    """Build a BuildContext without going through acquire_build_context().

    Used to simulate the live `ctx` (the BuildContext of the env
    actually being built) in tests where we don't want to mock the
    full SCons env.
    """
    return BuildContext(
        buildroot=(project_root / "src").resolve(),
        defines=frozenset(defines),
        defines_with_value=dict(values or {}),
        cppdefines_source=source,
        raw_cppdefines_sample="<fake>",
    )


def _mock_resolve_build_context_static(project_root: Path, env_name: str) -> BuildContext:
    """Mock implementation of resolve_build_context_static().

    Returns a BuildContext with the expected defines for the given
    env_name, based on EXPECTED_DEFINES_PER_ENV. This allows the
    integration tests to run without the `platformio` Python package
    installed.

    The mock is intentionally simple — it does NOT exercise the real
    ProjectConfig logic. The real ProjectConfig is exercised by
    test_resolve_build_context_static_real_project_config() (which
    is skipped if platformio is not installed).
    """
    defines = EXPECTED_DEFINES_PER_ENV.get(env_name)
    if defines is None:
        raise NodeContextError(
            f"mock_resolve_build_context_static: unknown env {env_name!r}"
        )
    return BuildContext(
        buildroot=(project_root / "src").resolve(),
        defines=frozenset(defines),
        defines_with_value={},
        cppdefines_source="static",
        raw_cppdefines_sample="<mock>",
    )


def _load_generate_per_board():
    """Extract _generate_per_board from combus_scons_hook.py.

    The hook module calls Import("env") at module level, which fails
    outside SCons. We work around this by extracting the function
    source and exec'ing it in a clean namespace with the required
    imports already provided.

    We register a fake module in sys.modules so that @dataclass works
    (it needs sys.modules[cls.__module__] to be a real module).

    IMPORTANT (Option A — see module docstring): we do NOT strip the
    late imports from the function source. The
    `from scripts.combus_builder.node_context import
    resolve_build_context_static` line is therefore re-executed on
    every call to the extracted function, which means
    `unittest.mock.patch` on
    `scripts.combus_builder.node_context.resolve_build_context_static`
    is honoured (the patch replaces the attribute on the module, and
    the re-import picks up the patched version).
    """
    hook_path = Path(REPO_ROOT) / "scripts" / "combus_scons_hook.py"
    hook_source = hook_path.read_text(encoding="utf-8")
    start_marker = "# =============================================================================\n# ND1: per-board generation"
    end_marker = "# =============================================================================\n# The SCons action"
    start_idx = hook_source.index(start_marker)
    end_idx = hook_source.index(end_marker)
    func_source = hook_source[start_idx:end_idx]

    # Use a real module name so @dataclass works.
    module_name = "combus_scons_hook_extracted_for_test"
    fake_module = types.ModuleType(module_name)
    sys.modules[module_name] = fake_module

    ns: dict = {
        "__name__": module_name,
        "__file__": str(hook_path),
        "Any": Any,
        "Path": Path,
        "dataclass": dataclass,
        "BuildContextError": BuildContextError,
        "GeneratorError": GeneratorError,
        "generate": generate,
        "NodeContextError": NodeContextError,
        "resolve_build_context_static": resolve_build_context_static,
    }

    # NOTE: we intentionally do NOT strip the late imports from
    # func_source. See module docstring (Option A) for why.

    exec(func_source, ns)
    return ns["_generate_per_board"], ns["_BoardGenerationError"]


# =============================================================================
# Test 1: per-board loop, building a SUB-env — mother must use static
# =============================================================================

def test_generate_per_board_mother_uses_static_resolution() -> None:
    """When building a sub-env, the mother entry must use static resolution.

    This is the bug that was missed by test_node_mode_2_boards: the
    previous code reused the live `ctx` (which is the sub-env's) for
    the mother entry, causing silent drift of the unprefixed
    artefacts depending on which sub-env was built last.

    Setup:
      - pio_env = "volvo_A60H_bruder_mainboard" (a SUB-env)
      - live_ctx = BuildContext with mainboard defines
        (IS_MACHINE, MACHINE_VOLVO_A60_H_BRUDER, IS_MAINBOARD, BOARD_ESP32_8M_6S)
      - node_mother_env = "volvo_A60H_bruder"

    Expected:
      - The mother entry (board_id=None) must use a BuildContext with
        ONLY the mother env's defines (IS_MACHINE, MACHINE_VOLVO_A60_H_BRUDER),
        NOT the mainboard defines (IS_MAINBOARD must be ABSENT).
      - The mainboard entry (board_id="mainboard") must use a
        BuildContext with the mainboard defines (IS_MAINBOARD present).
    """
    print("\n=== Test 1: per-board loop, building a SUB-env ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = _make_fake_project(tmp_path, FICTITIOUS_INI_NODE)

        # --- Setup: live_ctx is the mainboard sub-env's context ---
        live_ctx = _make_fake_build_context(
            project_root,
            defines={
                "IS_MACHINE",
                "MACHINE_VOLVO_A60_H_BRUDER",
                "IS_MAINBOARD",
                "BOARD_ESP32_8M_6S",
            },
            source="live",
        )

        # --- Load the function under test ---
        _generate_per_board, _BoardGenerationError = _load_generate_per_board()

        # --- Discover + canonise (board-agnostic) ---
        _, _, parsed = discover_and_parse(project_root=project_root)
        canon = canonize(parsed)

        # --- Run the per-board loop for all boards ---
        # We mock resolve_build_context_static so the test runs without
        # the `platformio` Python package installed. The mock returns
        # the expected defines per env (see EXPECTED_DEFINES_PER_ENV).
        staging_dir = tmp_path / "staging"
        staging_dir.mkdir()

        boards = [
            (None, "volvo_A60H_bruder"),
            ("mainboard", "volvo_A60H_bruder_mainboard"),
            ("soundboard", "volvo_A60H_bruder_soundboard"),
        ]

        with patch(
            "scripts.combus_builder.node_context.resolve_build_context_static",
            side_effect=_mock_resolve_build_context_static,
        ):
            results: dict[Any, tuple[Any, Any]] = {}
            for board_id, board_env_name in boards:
                try:
                    result = _generate_per_board(
                        board_id=board_id,
                        board_env_name=board_env_name,
                        pio_env="volvo_A60H_bruder_mainboard",  # SUB-env
                        node_mother_env="volvo_A60H_bruder",
                        live_ctx=live_ctx,
                        project_dir=project_root,
                        canonical_definitions=canon.canonical_definitions,
                        staging_dir=staging_dir,
                    )
                except _BoardGenerationError as e:
                    raise AssertionError(f"_generate_per_board raised: {e}")
                results[board_id] = (result.sel, result.board_ctx)

        # --- Verify: mother entry uses MOTHER defines, not mainboard ---
        mother_sel, mother_ctx = results[None]
        assert "IS_MACHINE" in mother_ctx.defines, (
            f"mother ctx should have IS_MACHINE, got {sorted(mother_ctx.defines)}"
        )
        assert "MACHINE_VOLVO_A60_H_BRUDER" in mother_ctx.defines, (
            f"mother ctx should have MACHINE_VOLVO_A60_H_BRUDER, "
            f"got {sorted(mother_ctx.defines)}"
        )
        assert "IS_MAINBOARD" not in mother_ctx.defines, (
            f"BUG: mother ctx should NOT have IS_MAINBOARD "
            f"(drift from sub-env), got {sorted(mother_ctx.defines)}"
        )
        assert "BOARD_ESP32_8M_6S" not in mother_ctx.defines, (
            f"BUG: mother ctx should NOT have BOARD_ESP32_8M_6S "
            f"(drift from sub-env), got {sorted(mother_ctx.defines)}"
        )
        # The mother ctx should be the STATICALLY resolved one
        # (source="static"), not the live one (source="live").
        assert mother_ctx.cppdefines_source == "static", (
            f"mother ctx should be statically resolved "
            f"(source='static'), got source={mother_ctx.cppdefines_source!r}"
        )
        print(f"  OK   mother ctx: defines={sorted(mother_ctx.defines)}, "
              f"source={mother_ctx.cppdefines_source!r}")

        # --- Verify: mainboard entry uses mainboard defines ---
        mainboard_sel, mainboard_ctx = results["mainboard"]
        assert "IS_MAINBOARD" in mainboard_ctx.defines, (
            f"mainboard ctx should have IS_MAINBOARD, "
            f"got {sorted(mainboard_ctx.defines)}"
        )
        assert "BOARD_ESP32_8M_6S" in mainboard_ctx.defines, (
            f"mainboard ctx should have BOARD_ESP32_8M_6S, "
            f"got {sorted(mainboard_ctx.defines)}"
        )
        assert mainboard_ctx.cppdefines_source == "static", (
            f"mainboard ctx should be statically resolved, "
            f"got source={mainboard_ctx.cppdefines_source!r}"
        )
        print(f"  OK   mainboard ctx: defines={sorted(mainboard_ctx.defines)}, "
              f"source={mainboard_ctx.cppdefines_source!r}")

        # --- Verify: soundboard entry uses soundboard defines ---
        soundboard_sel, soundboard_ctx = results["soundboard"]
        assert "IS_EXT_BOARD" in soundboard_ctx.defines, (
            f"soundboard ctx should have IS_EXT_BOARD, "
            f"got {sorted(soundboard_ctx.defines)}"
        )
        assert soundboard_ctx.cppdefines_source == "static", (
            f"soundboard ctx should be statically resolved, "
            f"got source={soundboard_ctx.cppdefines_source!r}"
        )
        print(f"  OK   soundboard ctx: defines={sorted(soundboard_ctx.defines)}, "
              f"source={soundboard_ctx.cppdefines_source!r}")

        # --- Verify: unprefixed combus.h exists and was generated ---
        # (the mother entry writes to the unprefixed names)
        assert (staging_dir / "combus.h").is_file(), (
            f"unprefixed combus.h should exist (mother entry), "
            f"files in staging: {sorted(p.name for p in staging_dir.iterdir())}"
        )
        assert (staging_dir / "combus_mainboard.h").is_file(), (
            f"prefixed combus_mainboard.h should exist"
        )
        assert (staging_dir / "combus_soundboard.h").is_file(), (
            f"prefixed combus_soundboard.h should exist"
        )
        print(f"  OK   unprefixed combus.h + prefixed combus_mainboard.h + "
              f"combus_soundboard.h all present")



# =============================================================================
# Test 2: per-board loop, building the MOTHER env — mother uses live ctx
# =============================================================================

def test_generate_per_board_mother_uses_live_ctx() -> None:
    """When building the mother env, the mother entry must use the live ctx.

    Setup:
      - pio_env = "volvo_A60H_bruder" (the MOTHER env)
      - live_ctx = BuildContext with mother defines
        (IS_MACHINE, MACHINE_VOLVO_A60_H_BRUDER)
      - node_mother_env = "volvo_A60H_bruder"

    Expected:
      - The mother entry (board_id=None) must use the live `ctx`
        directly (source="live"), NOT a statically resolved one.
      - The mainboard and soundboard entries must use statically
        resolved contexts (source="static").
    """
    print("\n=== Test 2: per-board loop, building the MOTHER env ===")
    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = _make_fake_project(tmp_path, FICTITIOUS_INI_NODE)

        # --- Setup: live_ctx is the mother env's context ---
        live_ctx = _make_fake_build_context(
            project_root,
            defines={"IS_MACHINE", "MACHINE_VOLVO_A60_H_BRUDER"},
            source="live",
        )

        # --- Load the function under test ---
        _generate_per_board, _BoardGenerationError = _load_generate_per_board()

        # --- Discover + canonise ---
        _, _, parsed = discover_and_parse(project_root=project_root)
        canon = canonize(parsed)

        # --- Run the per-board loop ---
        staging_dir = tmp_path / "staging"
        staging_dir.mkdir()

        boards = [
            (None, "volvo_A60H_bruder"),
            ("mainboard", "volvo_A60H_bruder_mainboard"),
            ("soundboard", "volvo_A60H_bruder_soundboard"),
        ]

        with patch(
            "scripts.combus_builder.node_context.resolve_build_context_static",
            side_effect=_mock_resolve_build_context_static,
        ):
            results: dict[Any, tuple[Any, Any]] = {}
            for board_id, board_env_name in boards:
                try:
                    result = _generate_per_board(
                        board_id=board_id,
                        board_env_name=board_env_name,
                        pio_env="volvo_A60H_bruder",  # MOTHER env
                        node_mother_env="volvo_A60H_bruder",
                        live_ctx=live_ctx,
                        project_dir=project_root,
                        canonical_definitions=canon.canonical_definitions,
                        staging_dir=staging_dir,
                    )
                except _BoardGenerationError as e:
                    raise AssertionError(f"_generate_per_board raised: {e}")
                results[board_id] = (result.sel, result.board_ctx)

        # --- Verify: mother entry uses the LIVE ctx ---
        mother_sel, mother_ctx = results[None]
        assert mother_ctx.cppdefines_source == "live", (
            f"mother ctx should be the LIVE ctx (source='live') when "
            f"building the mother env, got source={mother_ctx.cppdefines_source!r}"
        )
        assert "IS_MACHINE" in mother_ctx.defines
        assert "MACHINE_VOLVO_A60_H_BRUDER" in mother_ctx.defines
        print(f"  OK   mother ctx: defines={sorted(mother_ctx.defines)}, "
              f"source={mother_ctx.cppdefines_source!r} (live, as expected)")

        # --- Verify: mainboard and soundboard use static resolution ---
        for board_id in ("mainboard", "soundboard"):
            sel, ctx = results[board_id]
            assert ctx.cppdefines_source == "static", (
                f"{board_id} ctx should be statically resolved, "
                f"got source={ctx.cppdefines_source!r}"
            )
            print(f"  OK   {board_id} ctx: defines={sorted(ctx.defines)}, "
                  f"source={ctx.cppdefines_source!r}")



# =============================================================================
# Test 3: smoke test for resolve_build_context_static()
# =============================================================================

def test_resolve_build_context_static_real_project_config() -> None:
    """Smoke test for resolve_build_context_static() against a real ProjectConfig.

    Skipped with an explicit message if the `platformio` Python package
    is not installed in the test environment.

    Verifies that:
      - config.items(env=env_name) works as documented in the
        function's docstring.
      - The resolved build_flags string contains the expected defines
        after extends= and ${...} interpolation.
      - The BuildContext returned has the expected defines.
    """
    print("\n=== Test 3: resolve_build_context_static() smoke test ===")

    # --- Check if platformio is installed ---
    try:
        import platformio  # noqa: F401
        from platformio.project.config import ProjectConfig
    except ImportError:
        print("  SKIP platformio Python package not installed; "
              "cannot exercise resolve_build_context_static() against "
              "a real ProjectConfig. Install with: pip install platformio")
        return  # skip is success

    print(f"  OK   platformio module found")

    with tempfile.TemporaryDirectory() as td:
        tmp_path = Path(td)
        project_root = _make_fake_project(tmp_path, FICTITIOUS_INI_NODE)

        # --- Resolve the mainboard sub-env ---
        try:
            ctx = resolve_build_context_static(
                project_root, "volvo_A60H_bruder_mainboard",
            )
        except Exception as e:
            raise AssertionError(f"resolve_build_context_static raised: {e}")

        # --- Verify the resolved defines ---
        # After extends= + ${...} interpolation, the mainboard sub-env
        # should have:
        #   - COMMON_FLAG (from [env])
        #   - IS_MACHINE (from mother)
        #   - MACHINE_VOLVO_A60_H_BRUDER (from mother)
        #   - IS_MAINBOARD (from mainboard)
        #   - BOARD_ESP32_8M_6S (from mainboard)
        expected_defines = {
            "COMMON_FLAG",
            "IS_MACHINE",
            "MACHINE_VOLVO_A60_H_BRUDER",
            "IS_MAINBOARD",
            "BOARD_ESP32_8M_6S",
        }
        missing = expected_defines - ctx.defines
        assert not missing, (
            f"resolved BuildContext is missing defines: {missing}. "
            f"Got: {sorted(ctx.defines)}"
        )
        print(f"  OK   resolved defines: {sorted(ctx.defines)}")

        # --- Verify the source label ---
        assert ctx.cppdefines_source == "static", (
            f"cppdefines_source should be 'static', got {ctx.cppdefines_source!r}"
        )
        print(f"  OK   cppdefines_source={ctx.cppdefines_source!r}")

        # --- Verify the mother env resolves correctly too ---
        try:
            mother_ctx = resolve_build_context_static(
                project_root, "volvo_A60H_bruder",
            )
        except Exception as e:
            raise AssertionError(f"resolve_build_context_static(mother) raised: {e}")

        expected_mother_defines = {
            "COMMON_FLAG",
            "IS_MACHINE",
            "MACHINE_VOLVO_A60_H_BRUDER",
        }
        missing_mother = expected_mother_defines - mother_ctx.defines
        assert not missing_mother, (
            f"mother BuildContext is missing defines: {missing_mother}. "
            f"Got: {sorted(mother_ctx.defines)}"
        )
        # Mother should NOT have mainboard-specific defines.
        assert "IS_MAINBOARD" not in mother_ctx.defines, (
            f"mother ctx should NOT have IS_MAINBOARD, "
            f"got {sorted(mother_ctx.defines)}"
        )
        print(f"  OK   mother defines: {sorted(mother_ctx.defines)}")



# =============================================================================
# Main
# =============================================================================

if __name__ == "__main__":
    import pytest
    sys.exit(pytest.main([__file__, "-q"]))
