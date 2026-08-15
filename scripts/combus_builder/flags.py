#!/usr/bin/env python3
"""
combus_builder/flags.py — A4: Build context acquisition.

Scope (A4 only):
  - Acquire the buildroot of the current PlatformIO node.
  - Acquire the CPPDEFINES actually transmitted to the compiler for the
    current PlatformIO environment.
  - Represent those facts in a small, well-typed structure consumable by
    the next pipeline steps (A5 fusion, A6 generation, etc.).

A4 does NOT:
  - discover .cb / .cbch files                 (A3 — parser.py)
  - merge / canonise definitions                (A5 — canon.py)
  - generate C++ headers                        (A6 — generator.py)
  - compute MD5                                 (A7 — md5.py)
  - validate processors                         (Phase C)

Acquisition strategy (real PlatformIO environment):

  1. Buildroot: env["PROJECT_SRC_DIR"] (PIO 6.x canonical) then
     env["PROJECT_DIR"] + "/src" fallback. Both come from the SCons
     environment PlatformIO injects into extra_scripts (via `Import("env")`).

  2. CPPDEFINES: Three observable sources, in order of reliability:
     a) env["CPPDEFINES"]  — the SCons dict mapping flag -> value as it
        will be passed to the compiler. Most reliable when the script
        runs after `extends` resolution (i.e. `post:` hook in PIO 6.x).
     b) env.GetProjectOption("build_flags")  — returns the raw "build_flags"
        string for the current env. May MISS inherited flags from `extends`
        if PIO 6.x hasn't expanded them yet (i.e. `pre:` hook). Falls back
        to regex extraction of "-DFOO" / "-DFOO=bar" tokens.
     c) User-provided override (env argument or explicit extra flag list),
        for CLI / standalone tests.

  A4 tries (a) first, then (b) if (a) is empty, and raises an explicit
  BuildContextError if both are inaccessible / empty. This avoids the
  "fragile implementation that hides the problem" trap.

  We do NOT merge `extends` chains by hand (the doc marks this as a
  transient hack in combus_md5.py). If we observe `extends` failures in
  `pre:` we document them and rely on `post:` instead.

Representation:

  BuildContext is a frozen dataclass with:
    - buildroot: Path                    (resolved source dir)
    - defines: frozenset[str]            (flags without an explicit value)
    - defines_with_value: dict[str, str] (flags with explicit value)
    - cppdefines_source: str             (one of: 'env', 'build_flags', 'override', 'none')
    - raw_cppdefines_sample: str | None  (debug: first ~200 chars of the raw source)

  Helpers:
    - has(flag) -> bool
    - value_of(flag) -> str | None
    - require(*flags)                    (membership helper, raises on miss)
    - all_names() -> list[str]           (sorted, for diagnostics)
"""

from __future__ import annotations

import os
import re
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Iterable

# Reuse A3's buildroot resolution to avoid parallel logic.
from .parser import resolve_buildroot


# =============================================================================
# ERRORS
# =============================================================================

class BuildContextError(Exception):
    """Raised when the build context cannot be determined."""


# =============================================================================
# DATA STRUCTURE
# =============================================================================

@dataclass(frozen=True)
class BuildContext:
    """
    Snapshot of the build environment for one PlatformIO invocation.

    buildroot            : resolved source dir of the current node.
    defines              : set of active defines that have no value.
    defines_with_value   : dict of active defines that carry an explicit value.
    cppdefines_source    : which acquisition path was used.
    raw_cppdefines_sample: short sample of the raw defs (debug aid, may be None).
    """

    buildroot: Path
    defines: frozenset[str]
    defines_with_value: dict[str, str] = field(default_factory=dict)
    cppdefines_source: str = "none"
    raw_cppdefines_sample: str | None = None

    # -- convenience helpers --

    def has(self, flag: str) -> bool:
        """True if `flag` is defined (with or without value)."""
        return flag in self.defines or flag in self.defines_with_value

    def value_of(self, flag: str) -> str | None:
        """Return the value of `flag` if defined with one, else None."""
        return self.defines_with_value.get(flag)

    def all_names(self) -> list[str]:
        """Return all defined flag names, sorted (for diagnostics)."""
        return sorted(self.defines | set(self.defines_with_value.keys()))

    def to_dict(self) -> dict[str, Any]:
        """Plain dict representation (paths as str, sets as sorted lists)."""
        d = asdict(self)
        d["buildroot"] = str(self.buildroot)
        d["defines"] = sorted(self.defines)
        return d


# =============================================================================
# CPPDEFINES ACQUISITION
# =============================================================================

# Regex covers:
#   -D  FOO
#   -DFOO
#   -D  FOO=bar
#   -DFOO=bar
# It does NOT touch -D inside a string literal (we apply it to a per-line
# split so this is rare and an acceptable v1 limitation).
_RE_D_FLAG = re.compile(
    r"""-D\s*
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)
        (?:=(?P<value>"[^"]*"|'[^']*'|[^\s"'-]+))?
    """,
    re.VERBOSE,
)


def _extract_from_build_flags_string(text: str) -> tuple[set[str], dict[str, str]]:
    """
    Pull -D flags out of a PlatformIO build_flags string.

    Used as a fallback when env["CPPDEFINES"] is not available.
    Not perfectly accurate (cannot evaluate $VALUE expansions or full
    include-path inheritance) — that's why this is the fallback, not
    the primary source.
    """
    defines: set[str] = set()
    values: dict[str, str] = {}

    for m in _RE_D_FLAG.finditer(text):
        name = m.group("name")
        value = m.group("value")
        if value is None:
            defines.add(name)
        else:
            # Strip surrounding quotes if present
            if (value.startswith('"') and value.endswith('"')) or \
               (value.startswith("'") and value.endswith("'")):
                value = value[1:-1]
            values[name] = value

    return defines, values


def _extract_from_env_cppdefines(env: Any) -> tuple[set[str], dict[str, str]]:
    """
    Pull -D flags out of env["CPPDEFINES"] (SCons-native form).

    In SCons, CPPDEFINES is a dict-like object (or list of tuples / strings)
    where each entry is either:
      - ("FOO", "value")
      - ("FOO",)            # value-less
      - "FOO"               # shorthand for value-less
      - "FOO=value"         # shorthand
    """
    defines: set[str] = set()
    values: dict[str, str] = {}

    raw = env.get("CPPDEFINES") if hasattr(env, "get") else env["CPPDEFINES"]
    if raw is None:
        return defines, values

    # Normalise to a list of (name, value_or_none)
    items: list[tuple[str, str | None]]
    if isinstance(raw, dict):
        items = [(str(k), v if v is not None else None) for k, v in raw.items()]
    elif isinstance(raw, (list, tuple)):
        items = []
        for entry in raw:
            if isinstance(entry, str):
                # "FOO" or "FOO=value"
                if "=" in entry:
                    k, v = entry.split("=", 1)
                    items.append((k, v))
                else:
                    items.append((entry, None))
            elif isinstance(entry, (list, tuple)) and len(entry) >= 1:
                k = str(entry[0])
                v = entry[1] if len(entry) >= 2 and entry[1] is not None else None
                items.append((k, v))
            else:
                # Unknown shape — skip silently (recorded in source).
                continue
    else:
        # Unknown shape — skip.
        return defines, values

    for name, value in items:
        if value is None:
            defines.add(name)
        else:
            values[name] = str(value)

    return defines, values


def _raw_sample(raw: Any, limit: int = 200) -> str | None:
    """Return a short, ASCII-safe sample of a raw CPPDEFINES value."""
    if raw is None:
        return None
    try:
        s = str(raw)
    except Exception:
        return None
    if len(s) > limit:
        s = s[:limit] + "..."
    return s.encode("ascii", errors="replace").decode("ascii")


# =============================================================================
# TOP-LEVEL ENTRY POINT
# =============================================================================

# Minimum defines we'd expect from any real build. A4 doesn't require
# specific flags, but it does warn (via exception) when the env is
# clearly empty — that almost always means we ran too early.
_MIN_REASONABLE_DEFINES = 1


def acquire_build_context(
    env: Any | None = None,
    project_root: Path | None = None,
    override_cppdefines: list[str] | None = None,
    *,
    require_non_empty: bool = True,
) -> BuildContext:
    """
    Acquire the build context for the current PlatformIO invocation.

    Args:
      env:                   SCons env injected by PlatformIO (via Import("env")).
                             If None, falls back to standalone / CLI mode.
      project_root:          explicit project root for CLI mode.
      override_cppdefines:   explicit list of "-D" tokens for tests.
                             If set, used as the primary source (label: 'override').
      require_non_empty:     if True (default), raise when zero defines are found.

    Returns:
      BuildContext frozen dataclass.

    Raises:
      BuildContextError on:
        - buildroot not findable
        - PlatformIO env absent and no override / project_root given
        - CPPDEFINES is empty when require_non_empty=True
    """
    # --- 1. Buildroot ---
    buildroot = resolve_buildroot(env, project_root)
    if not buildroot.is_dir():
        raise BuildContextError(
            f"buildroot does not exist or is not a directory: {buildroot}"
        )

    # --- 2. CPPDEFINES ---
    defines: set[str] = set()
    values: dict[str, str] = {}
    source = "none"
    raw_sample: str | None = None

    if override_cppdefines is not None:
        # Test / CLI override.
        joined = " ".join(override_cppdefines)
        defines, values = _extract_from_build_flags_string(joined)
        source = "override"
        raw_sample = _raw_sample(joined)

    elif env is not None:
        # Try env["CPPDEFINES"] first (most reliable AFTER extends resolution).
        try:
            env_defines, env_values = _extract_from_env_cppdefines(env)
        except (KeyError, AttributeError):
            env_defines, env_values = set(), {}

        if env_defines or env_values:
            defines = env_defines
            values = env_values
            source = "env"
            try:
                raw_sample = _raw_sample(env["CPPDEFINES"])
            except (KeyError, AttributeError):
                pass

        if not defines and not values:
            # Fallback to GetProjectOption("build_flags") hack.
            try:
                raw = env.GetProjectOption("build_flags", "")
            except Exception:
                raw = ""
            text = " ".join(str(x) for x in raw) if isinstance(raw, (list, tuple)) else str(raw)
            defines, values = _extract_from_build_flags_string(text)
            if defines or values:
                source = "build_flags"
                raw_sample = _raw_sample(text)
    # else: env is None and no override — leave defines empty.

    if require_non_empty and not defines and not values:
        raise BuildContextError(
            "no CPPDEFINES could be resolved for the current build context. "
            "Either run this script as a PlatformIO extra_script "
            "(pre: or post: hook) so that env['CPPDEFINES'] is populated, "
            "or pass override_cppdefines explicitly. "
            f"buildroot={buildroot}"
        )

    return BuildContext(
        buildroot=buildroot,
        defines=frozenset(defines),
        defines_with_value=dict(values),
        cppdefines_source=source,
        raw_cppdefines_sample=raw_sample,
    )


# =============================================================================
# SCons / PlatformIO entry point
# =============================================================================

def main(env: Any) -> int:
    """A4 entry point usable from a PlatformIO extra_script."""
    try:
        ctx = acquire_build_context(env)
    except BuildContextError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1

    print(f"[combus_builder] buildroot = {ctx.buildroot}")
    print(f"[combus_builder] CPPDEFINES source = {ctx.cppdefines_source}")
    if ctx.raw_cppdefines_sample:
        print(f"[combus_builder] raw sample = {ctx.raw_cppdefines_sample}")
    print(f"[combus_builder] defines ({len(ctx.defines)}):")
    for name in sorted(ctx.defines):
        print(f"  -D{name}")
    print(f"[combus_builder] defines with value ({len(ctx.defines_with_value)}):")
    for k in sorted(ctx.defines_with_value):
        print(f"  -D{k}={ctx.defines_with_value[k]}")
    return 0
