#!/usr/bin/env python3
"""
Tests for flags.py — A4.

Two layers:

  1. Unit tests (FakeEnv, no PlatformIO):
     - build_flags extraction
     - env CPPDEFINES extraction (dict / list-strings / list-tuples)
     - acquire_build_context (override / env / fallback / errors)
     - BuildContext helpers
     - _extract_from_env_cppdefines: STRICT mode (no silent skip)

  2. Integration tests (real PlatformIO):
     - skip unless pio is on PATH AND a primary env exists
     - cross-check ctx vs `pio run -t idedata` for a real env
     - assert no uncontrolled divergence

The integration tests are NOT run by default — they require pio and
working network/toolchains. They are guarded by env var
COMBUS_A4_INTEGRATION=1 so that CI can opt in.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import inspect
import tempfile
import traceback
from pathlib import Path

# Ensure the combus_builder package is importable when running this
# file directly from the repo root.
THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.flags import (
    BuildContext,
    BuildContextError,
    acquire_build_context,
    _extract_from_build_flags_string,
    _extract_from_env_cppdefines,
    _idedata_to_defines,
    validate_against_idedata,
)


# =============================================================================
# Fake SCons env builder
# =============================================================================

class FakeEnv:
    """
    Minimal SCons-env facade for the bits flags.py reads.

    - .get(key, default) / .get(key) semantics
    - .GetProjectOption(name, default) for the build_flags fallback
    - dict-like iteration via __getitem__/__setitem__ (mirrors SCons)
    """

    def __init__(self, *, project_src_dir=None, project_dir=None,
                 cppdefines=None, build_flags=""):
        self._data = {}
        if project_src_dir is not None:
            self._data["PROJECT_SRC_DIR"] = project_src_dir
        if project_dir is not None:
            self._data["PROJECT_DIR"] = project_dir
        if cppdefines is not None:
            self._data["CPPDEFINES"] = cppdefines
        self._build_flags = build_flags

    def get(self, key, default=None):
        return self._data.get(key, default)

    def __getitem__(self, key):
        return self._data[key]

    def __setitem__(self, key, value):
        self._data[key] = value

    def __contains__(self, key):
        return key in self._data

    def GetProjectOption(self, name, default=None):
        return self._build_flags


# =============================================================================
# Helpers
# =============================================================================

def _tmp_src_dir(tmp_path: Path) -> Path:
    """Create a real directory tree so resolve_buildroot passes."""
    p = tmp_path / "proj" / "src"
    p.mkdir(parents=True)
    return p


# =============================================================================
# Tests for build_flags string extraction
# =============================================================================

def test_extract_from_build_flags_simple():
    defines, values = _extract_from_build_flags_string("-DFOO -DBAR")
    assert defines == {"FOO", "BAR"}
    assert values == {}


def test_extract_from_build_flags_with_value():
    defines, values = _extract_from_build_flags_string('-DFOO=1 -DBAR="hello world"')
    assert defines == set()
    assert values == {"FOO": "1", "BAR": "hello world"}


def test_extract_from_build_flags_mixed():
    text = "some/random -DFOO -I include -DBAR=42 -DDEBUG path/to/src"
    defines, values = _extract_from_build_flags_string(text)
    assert defines == {"FOO", "DEBUG"}
    assert values == {"BAR": "42"}


def test_extract_from_build_flags_no_flags():
    defines, values = _extract_from_build_flags_string("-I include -std=gnu++17")
    assert defines == set()
    assert values == {}


def test_extract_from_build_flags_value_with_equals():
    defines, values = _extract_from_build_flags_string("-DBAZ=key=value")
    assert values == {"BAZ": "key=value"}


# =============================================================================
# Tests for env["CPPDEFINES"] extraction
# =============================================================================

def test_extract_from_env_cppdefines_dict():
    env = FakeEnv(cppdefines={"FOO": 1, "BAR": None, "BAZ": "hello"})
    defines, values = _extract_from_env_cppdefines(env)
    assert defines == {"BAR"}
    assert values == {"FOO": "1", "BAZ": "hello"}


def test_extract_from_env_cppdefines_list_of_strings():
    env = FakeEnv(cppdefines=["FOO", "BAR=42", "QUX"])
    defines, values = _extract_from_env_cppdefines(env)
    assert defines == {"FOO", "QUX"}
    assert values == {"BAR": "42"}


def test_extract_from_env_cppdefines_list_of_tuples():
    env = FakeEnv(cppdefines=[("FOO", 1), ("BAR",), ("BAZ", "hi")])
    defines, values = _extract_from_env_cppdefines(env)
    assert defines == {"BAR"}
    assert values == {"FOO": "1", "BAZ": "hi"}


def test_extract_from_env_cppdefines_none():
    env = FakeEnv(cppdefines=None)
    defines, values = _extract_from_env_cppdefines(env)
    assert defines == set()
    assert values == {}


def test_extract_from_env_cppdefines_unsupported_entry_raises():
    """
    A4 must NOT silently skip entries it doesn't recognise. Per the
    A4 prompt: "ne pas masquer le problème par une implémentation fragile".
    SCons can host objects like SCons.Node.Python.Value that don't fit
    the documented shapes — a silent skip would mask a real source-of-truth
    mismatch.
    """
    class WeirdValue:
        """Placeholder for the kind of object SCons can host."""
        def __repr__(self):
            return "<WeirdValue>"

    env = FakeEnv(cppdefines=[("FOO", 1), WeirdValue()])
    try:
        _extract_from_env_cppdefines(env)
    except BuildContextError as e:
        assert "unsupported" in str(e)
        assert "WeirdValue" in str(e)
    else:
        raise AssertionError("expected BuildContextError on unsupported entry shape")


def test_extract_from_env_cppdefines_unsupported_container_raises():
    """A single int at the top level (not a dict / list / tuple) must raise."""
    env = FakeEnv(cppdefines=42)  # type: ignore[arg-type]
    try:
        _extract_from_env_cppdefines(env)
    except BuildContextError as e:
        assert "unsupported container type" in str(e)
    else:
        raise AssertionError("expected BuildContextError on unsupported container type")


# =============================================================================
# Tests for full acquire_build_context (need tmp_path)
# =============================================================================

def test_acquire_with_override(tmp_path):
    src = _tmp_src_dir(tmp_path)
    ctx = acquire_build_context(
        project_root=tmp_path / "proj",
        override_cppdefines=["-DALPHA", "-DBETA=1"],
    )
    assert ctx.buildroot == src
    assert ctx.cppdefines_source == "override"
    assert "ALPHA" in ctx.defines
    assert ctx.defines_with_value.get("BETA") == "1"
    assert ctx.has("ALPHA")
    assert ctx.has("BETA")
    assert ctx.value_of("BETA") == "1"
    assert ctx.value_of("ALPHA") is None


def test_acquire_with_env_cppdefines(tmp_path):
    """
    When CPPDEFINES is a dict, only None-valued entries go to `defines`;
    valued entries go to `defines_with_value`. This matches the
    actual SCons semantics (a value-less -D is just absent-value).
    """
    src = _tmp_src_dir(tmp_path)
    env = FakeEnv(
        project_src_dir=str(src),
        cppdefines={"IS_MACHINE": 1, "MACHINE_VOLVO_A60_H_BRUDER": 1, "FOO": None},
    )
    ctx = acquire_build_context(env=env)
    assert ctx.buildroot == src
    assert ctx.cppdefines_source == "env"
    assert "FOO" in ctx.defines
    assert ctx.has("IS_MACHINE")
    assert ctx.has("MACHINE_VOLVO_A60_H_BRUDER")
    assert ctx.value_of("IS_MACHINE") == "1"
    assert ctx.value_of("MACHINE_VOLVO_A60_H_BRUDER") == "1"


def test_acquire_fallback_to_build_flags(tmp_path):
    src = _tmp_src_dir(tmp_path)
    env = FakeEnv(
        project_src_dir=str(src),
        cppdefines={},
        build_flags="-DDEBUG_SYSTEM -DIS_MACHINE -I include",
    )
    ctx = acquire_build_context(env=env)
    assert ctx.buildroot == src
    assert ctx.cppdefines_source == "build_flags"
    assert "DEBUG_SYSTEM" in ctx.defines
    assert "IS_MACHINE" in ctx.defines


def test_acquire_no_defines_raises(tmp_path):
    src = _tmp_src_dir(tmp_path)
    env = FakeEnv(
        project_src_dir=str(src),
        cppdefines={},
        build_flags="",
    )
    try:
        acquire_build_context(env=env)
    except BuildContextError as e:
        assert "no CPPDEFINES" in str(e)
    else:
        raise AssertionError("expected BuildContextError")


def test_acquire_no_defines_allowed_when_not_required(tmp_path):
    src = _tmp_src_dir(tmp_path)
    env = FakeEnv(
        project_src_dir=str(src),
        cppdefines={},
        build_flags="",
    )
    ctx = acquire_build_context(env=env, require_non_empty=False)
    assert ctx.buildroot == src
    assert ctx.defines == frozenset()
    assert ctx.defines_with_value == {}


def test_acquire_bad_buildroot_raises(tmp_path):
    fake_root = tmp_path / "proj"
    fake_root.mkdir()
    try:
        acquire_build_context(
            project_root=fake_root,
            override_cppdefines=["-DFOO"],
        )
    except BuildContextError as e:
        assert "buildroot" in str(e).lower()
    else:
        raise AssertionError("expected BuildContextError for missing buildroot")


def test_acquire_standalone_no_env_no_override_raises(tmp_path):
    cwd_before = Path.cwd()
    try:
        os.chdir(tmp_path)
        acquire_build_context()
    except BuildContextError:
        pass
    else:
        raise AssertionError("expected BuildContextError in standalone no-env mode")
    finally:
        os.chdir(cwd_before)


# =============================================================================
# Tests for BuildContext helpers (no tmp_path needed)
# =============================================================================

def test_buildcontext_all_names_sorted():
    ctx = BuildContext(
        buildroot=Path("/tmp/x"),
        defines=frozenset({"C", "A"}),
        defines_with_value={"B": "1", "D": "2"},
    )
    assert ctx.all_names() == ["A", "B", "C", "D"]


def test_buildcontext_has_and_value_of():
    ctx = BuildContext(
        buildroot=Path("/tmp/x"),
        defines=frozenset({"AAA"}),
        defines_with_value={"BBB": "hello"},
    )
    assert ctx.has("AAA")
    assert ctx.has("BBB")
    assert not ctx.has("CCC")
    assert ctx.value_of("AAA") is None
    assert ctx.value_of("BBB") == "hello"
    assert ctx.value_of("CCC") is None


def test_buildcontext_to_dict_sorts_defines():
    ctx = BuildContext(
        buildroot=Path("/tmp/x"),
        defines=frozenset({"Z", "A", "M"}),
    )
    d = ctx.to_dict()
    assert d["defines"] == ["A", "M", "Z"]
    assert d["buildroot"] == str(Path("/tmp/x"))


def test_buildcontext_is_frozen():
    ctx = BuildContext(buildroot=Path("/tmp/x"), defines=frozenset())
    try:
        ctx.defines = frozenset({"NEW"})  # type: ignore[misc]
    except Exception:
        pass
    else:
        raise AssertionError("expected frozen dataclass to reject mutation")


# =============================================================================
# Tests for idedata cross-validation helpers (no PlatformIO needed)
# =============================================================================

def test_idedata_to_defines_basic():
    """The idedata parser should handle the canonical `defines` list."""
    payload = {
        "defines": ["FOO", "BAR=42", "QUX"],
        "other_junk": "ignored",
    }
    defines, values = _idedata_to_defines(payload)
    assert defines == {"FOO", "QUX"}
    assert values == {"BAR": "42"}


def test_idedata_to_defines_empty():
    defines, values = _idedata_to_defines({})
    assert defines == set()
    assert values == {}


def test_idedata_to_defines_skips_non_strings():
    """Idedata can contain non-string entries (defensive)."""
    payload = {"defines": ["FOO", 42, None, "BAR=1"]}
    defines, values = _idedata_to_defines(payload)
    assert defines == {"FOO"}
    assert values == {"BAR": "1"}


# =============================================================================
# Integration tests (real PlatformIO, opt-in)
# =============================================================================

def _pio_exists() -> bool:
    try:
        subprocess.run(
            ["pio", "--version"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=10,
        )
        return True
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False


def _read_primary_envs(repo_root: Path) -> list[str]:
    """Heuristic: list `[env:*]` sections from platformio.ini."""
    ini = repo_root / "platformio.ini"
    if not ini.is_file():
        return []
    envs = []
    for line in ini.read_text(encoding="utf-8", errors="replace").splitlines():
        s = line.strip()
        if s.startswith("[env:") and s.endswith("]"):
            envs.append(s[len("[env:"):-1])
    return envs


def test_integration_against_idedata_machine_env(tmp_path):
    """
    Run A4's idedata cross-validation against the real project on a
    PlatformIO env that targets a machine. Skips if pio / env / network
    are unavailable.
    """
    if os.environ.get("COMBUS_A4_INTEGRATION") != "1":
        print("  SKIP (set COMBUS_A4_INTEGRATION=1 to run)")
        return

    if not _pio_exists():
        print("  SKIP (pio not on PATH)")
        return

    envs = _read_primary_envs(REPO_ROOT)
    if not envs:
        print("  SKIP (no [env:*] in platformio.ini)")
        return

    # Pick the first env that looks like a machine (or any first one).
    target = next((e for e in envs if "machine" in e.lower() or "main" in e.lower()), envs[0])
    print(f"  using env '{target}' from {len(envs)} candidate(s)")

    ctx, idedata = validate_against_idedata(
        project_dir=REPO_ROOT,
        env_name=target,
        strict=True,
    )
    assert ctx.buildroot.is_dir()
    print(f"  ctx has {len(ctx.defines)} defines, {len(ctx.defines_with_value)} values")
    print(f"  idedata envs: {idedata.get('envs', ids_in_idedata := '<see defines>')}")


def test_integration_against_idedata_remote_env(tmp_path):
    """
    Same as above but on a remote-style env. Skips if no such env exists.
    """
    if os.environ.get("COMBUS_A4_INTEGRATION") != "1":
        print("  SKIP (set COMBUS_A4_INTEGRATION=1 to run)")
        return

    if not _pio_exists():
        print("  SKIP (pio not on PATH)")
        return

    envs = _read_primary_envs(REPO_ROOT)
    remote = next((e for e in envs if "remote" in e.lower() or "ext" in e.lower()), None)
    if remote is None:
        print("  SKIP (no remote-style env in platformio.ini)")
        return

    print(f"  using env '{remote}'")
    ctx, idedata = validate_against_idedata(
        project_dir=REPO_ROOT,
        env_name=remote,
        strict=True,
    )
    assert ctx.buildroot.is_dir()
    print(f"  ctx has {len(ctx.defines)} defines, {len(ctx.defines_with_value)} values")


# =============================================================================
# Test runner
# =============================================================================

def _run_all():
    """Discover all test_* functions and run them. Tests that take a
    tmp_path receive a real tempdir; tests that take no args are called bare."""
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
