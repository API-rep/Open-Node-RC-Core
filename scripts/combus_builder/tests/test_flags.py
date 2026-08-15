#!/usr/bin/env python3
"""
Tests for flags.py — A4.

These tests are self-contained: they build fake "SCons env" objects
that mimic the relevant PlatformIO/SCons API surface. This avoids the
need to actually launch a PlatformIO build for unit tests.

A real integration test against PlatformIO is recommended separately
(see doc/combus_v2 - YAML implementation.md section 27).
"""

from __future__ import annotations

import os
import sys
import inspect
import tempfile
import traceback
from pathlib import Path

# Ensure the combus_builder package is importable when running this
# file directly from the repo root.
# Path layout: repo_root/scripts/combus_builder/tests/test_flags.py
# -> repo_root = tests.parent.parent.parent
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
    # IS_MACHINE / MACHINE_VOLVO_A60_H_BRUDER are value-1 defines
    assert ctx.has("IS_MACHINE")
    assert ctx.has("MACHINE_VOLVO_A60_H_BRUDER")
    assert ctx.value_of("IS_MACHINE") == "1"
    assert ctx.value_of("MACHINE_VOLVO_A60_H_BRUDER") == "1"


def test_acquire_fallback_to_build_flags(tmp_path):
    src = _tmp_src_dir(tmp_path)
    # env has empty CPPDEFINES (simulates pre: hook) but build_flags exposes things
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
    # No src/ created inside tmp_path/proj
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
    # No env, no override, no project_root -> buildroot falls back to cwd+src
    # which doesn't exist in tmp_path.
    cwd_before = Path.cwd()
    try:
        os.chdir(tmp_path)
        acquire_build_context()
    except BuildContextError:
        pass  # OK — any BuildContextError is acceptable here
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
# Test runner
# =============================================================================

def _run_all():
    """Discover all test_* functions and run them. Tests that take a
    tmp_path receive a real tempdir; tests that take no args are called bare."""
    import functools

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
