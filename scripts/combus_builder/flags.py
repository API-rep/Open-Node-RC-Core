#!/usr/bin/env python3
"""
combus_builder/flags.py — A4: Build context acquisition.

Scope (A4 only):
  - Acquire the buildroot of the current PlatformIO node.
  - Acquire the CPPDEFINES actually transmitted to the compiler for the
    current PlatformIO environment.
  - Reify this as a `combus_ctx.json` artifact during a real build so
    that an integration test can compare it against the ground truth
    (`pio run -t idedata`).

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
        This is the ONLY path that reflects what the compiler actually sees.
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

Real-build ground-truth validation (post: hook):

  The integration test fixture is a `post:` hook that drops
  `.pio/build/<env>/combus_ctx.json` from the REAL env["CPPDEFINES"]
  (no fiddling, no override). The corresponding integration test then:

    1. triggers a real `pio run -e <env>` (which fires the hook and
       produces the JSON),
    2. triggers `pio run -t idedata -e <env>` for the ground truth,
    3. compares the two.

  This is the only structurally sound way to assert that what A4 sees
  in a real build matches what PlatformIO actually hands to the compiler.
  No text round-trip, no derivation from idedata itself.

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
    - all_names() -> list[str]           (sorted, for diagnostics)
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from collections import OrderedDict, deque
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


class ContextDivergenceError(Exception):
    """Raised when a real-build ctx diverges from the idedata ground truth."""


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

    Strict semantics: any entry whose shape does NOT match one of the
    documented forms aborts extraction with a BuildContextError. We
    do NOT silently skip entries — SCons can host objects that almost
    look like strings (e.g. SCons.Node.Python.Value) and a silent
    "skip" would mask a real source-of-truth mismatch.
    """
    defines: set[str] = set()
    values: dict[str, str] = {}

    raw = env.get("CPPDEFINES") if hasattr(env, "get") else env["CPPDEFINES"]
    if raw is None:
        return defines, values

    # Normalise to a list of (name, value_or_none).
    #
    # Accepted container types:
    #   - dict / OrderedDict          (key -> value mapping)
    #   - list / tuple / deque        (ordered sequence of entries)
    #
    # PlatformIO 6.x injects CPPDEFINES as a `collections.deque`
    # (to preserve order across `extends` resolution); older PIO
    # versions used plain `list`. Both must work.
    items: list[tuple[str, str | None]]
    if isinstance(raw, (dict, OrderedDict)):
        items = [(str(k), v if v is not None else None) for k, v in raw.items()]
    elif isinstance(raw, (list, tuple, deque)):
        items = []
        for idx, entry in enumerate(raw):
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
                # Unknown shape — fail loudly. Per the prompt:
                # "ne pas masquer le problème par une implémentation fragile".
                raise BuildContextError(
                    "env['CPPDEFINES'] contains an entry of unsupported "
                    f"shape at index {idx}: {entry!r} (type={type(entry).__name__}). "
                    "A4 cannot reliably extract a complete build context "
                    "from this value. Pass override_cppdefines explicitly, "
                    "or extend the extractor to handle this shape."
                )
    else:
        raise BuildContextError(
            "env['CPPDEFINES'] has an unsupported container type: "
            f"{type(raw).__name__} (value={raw!r}). A4 cannot reliably "
            "extract a complete build context from this value."
        )

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
                             If non-None, used as the primary source (label: 'override').
                             If None, env["CPPDEFINES"] is consulted.
      require_non_empty:     if True (default), raise when zero defines are found.

    Returns:
      BuildContext frozen dataclass.

    Raises:
      BuildContextError on:
        - buildroot not findable
        - PlatformIO env absent and no override / project_root given
        - CPPDEFINES is empty when require_non_empty=True
        - CPPDEFINES contains an entry of unknown shape (no silent skip)
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
        env_defines, env_values = _extract_from_env_cppdefines(env)

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
            text = (
                " ".join(str(x) for x in raw) if isinstance(raw, (list, tuple))
                else str(raw)
            )
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
# Real-build ground-truth validation
# =============================================================================
#
# The architecture is intentionally split into two pieces to avoid the
# "round-trip text" trap identified in the review:
#
#   1. dump_ctx_to_json(env, out_path)
#        Runs INSIDE a real PlatformIO build (post: hook). It calls
#        acquire_build_context(env) WITHOUT override and writes
#        ctx.to_dict() to a JSON file. NO idedata, NO override, NO
#        text round-trip on the same data.
#
#   2. compare_context_to_idedata(ctx, idedata, env_name)
#        Pure, deterministic comparison between a real-build ctx
#        (whatever the source) and a parsed idedata payload.
#
#   3. test_integration_… reads the dumped JSON and the idedata
#      parsed from a separate `pio run -t idedata` invocation, then
#      calls compare_context_to_idedata. This is the only sequence
#      that closes the "is the prod path the same as the idedata path"
#      question, because the two data sources are produced by two
#      separate pio invocations, not one re-derived from the other.
# =============================================================================

def dump_ctx_to_json(env: Any, out_path: Path) -> BuildContext:
    """
    Acquire the build context from a real SCons env and serialise it
    to a JSON file. NO override is allowed in this path — it's the
    artifact that integration tests will compare against idedata.

    Designed to be called from a `post:` PlatformIO extra_script.
    """
    if not isinstance(out_path, Path):
        out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    ctx = acquire_build_context(env)  # must use real env, no override
    out_path.write_text(json.dumps(ctx.to_dict(), indent=2), encoding="utf-8")
    return ctx


def load_ctx_from_json(path: Path) -> BuildContext:
    """Inverse of dump_ctx_to_json; used by integration tests."""
    raw = json.loads(Path(path).read_text(encoding="utf-8"))
    return BuildContext(
        buildroot=Path(raw["buildroot"]),
        defines=frozenset(raw["defines"]),
        defines_with_value=dict(raw["defines_with_value"]),
        cppdefines_source=raw["cppdefines_source"],
        raw_cppdefines_sample=raw.get("raw_cppdefines_sample"),
    )


def read_idedata(project_dir: Path, env_name: str, *, timeout: int = 120) -> dict:
    """
    Run `pio run -t idedata -e <env_name>` and return the parsed JSON.

    `pio run -t idedata` is the documented PlatformIO mechanism that emits
    the resolved build context (including the fully-expanded CPPDEFINES
    that the compiler will see) as JSON. We use it as ground truth.

    Raises:
      BuildContextError if `pio` is not available, the subprocess fails,
      or the JSON cannot be parsed.
    """
    try:
        proc = subprocess.run(
            ["pio", "run", "-t", "idedata", "-e", env_name],
            cwd=str(project_dir),
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except FileNotFoundError as e:
        raise BuildContextError(
            "`pio` not found on PATH. Install PlatformIO CLI or activate "
            "the project's venv."
        ) from e
    except subprocess.TimeoutExpired as e:
        raise BuildContextError(
            f"`pio run -t idedata -e {env_name}` timed out after {timeout}s"
        ) from e

    if proc.returncode != 0:
        raise BuildContextError(
            f"`pio run -t idedata -e {env_name}` failed (exit {proc.returncode}): "
            f"{(proc.stderr or proc.stdout).strip()[:500]}"
        )

    out = proc.stdout.strip()
    if not out:
        raise BuildContextError(
            f"`pio run -t idedata -e {env_name}` produced no JSON output"
        )
    try:
        return json.loads(out)
    except json.JSONDecodeError as e:
        raise BuildContextError(
            f"`pio run -t idedata -e {env_name}` output is not valid JSON: {e}"
        ) from e


def idedata_to_defines(idedata: dict) -> tuple[set[str], dict[str, str]]:
    """
    Convert `pio run -t idedata` JSON to (defines, values) for cross-check.

    The idedata payload has shape:
        "defines": ["FOO", "BAR=value", ...]
    """
    defines: set[str] = set()
    values: dict[str, str] = {}

    raw_defines = idedata.get("defines") or []
    for entry in raw_defines:
        if not isinstance(entry, str):
            continue
        if "=" in entry:
            k, v = entry.split("=", 1)
            values[k] = v
        else:
            defines.add(entry)

    return defines, values


def compare_context_to_idedata(
    ctx: BuildContext,
    idedata: dict,
    env_name: str,
    *,
    strict: bool = True,
) -> dict:
    """
    Pure comparison between a real-build ctx and a parsed idedata payload.

    Returns a report dict (also acceptable as a non-throwing diagnostic).
    Raises ContextDivergenceError on strict=True AND any divergence.

    `ctx` MUST come from a real production path (a post: hook dropping
    combus_ctx.json) — NOT from an override derived from idedata. The
    two sources have to be independent for the comparison to be
    meaningful.
    """
    true_defines, true_values = idedata_to_defines(idedata)

    missing_from_ctx = (
        (true_defines - ctx.defines)
        | (set(true_values) - set(ctx.defines_with_value))
    )
    extra_in_ctx = (
        (ctx.defines - true_defines)
        | (set(ctx.defines_with_value) - set(true_values))
    )
    value_mismatches: dict[str, dict[str, str]] = {}
    for k, v in ctx.defines_with_value.items():
        if k in true_values and true_values[k] != v:
            value_mismatches[k] = {"ctx": v, "idedata": true_values[k]}

    report = {
        "env_name": env_name,
        "ctx_source": ctx.cppdefines_source,
        "ctx_defines_count": len(ctx.defines),
        "ctx_values_count": len(ctx.defines_with_value),
        "idedata_defines_count": len(true_defines),
        "idedata_values_count": len(true_values),
        "missing_from_ctx": sorted(missing_from_ctx),
        "extra_in_ctx": sorted(extra_in_ctx),
        "value_mismatches": value_mismatches,
        "diverges": bool(missing_from_ctx or extra_in_ctx or value_mismatches),
    }

    if strict and report["diverges"]:
        lines = [
            f"BuildContext (real build, source={ctx.cppdefines_source}) "
            f"diverges from pio idedata for env '{env_name}':"
        ]
        if missing_from_ctx:
            lines.append(f"  present in idedata, absent in ctx: {sorted(missing_from_ctx)}")
        if extra_in_ctx:
            lines.append(f"  present in ctx, absent in idedata: {sorted(extra_in_ctx)}")
        if value_mismatches:
            for k, d in sorted(value_mismatches.items()):
                lines.append(f"  value mismatch for {k}: ctx={d['ctx']!r} idedata={d['idedata']!r}")
        raise ContextDivergenceError("\n".join(lines))

    return report


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
