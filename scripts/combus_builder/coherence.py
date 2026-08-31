#!/usr/bin/env python3
"""
combus_builder/coherence.py - A8: CPPDEFINES / PlatformIO coherence check.

Scope (A8 only):
  - Verify, for a PlatformIO environment, that the BuildContext acquired
    by A4 (acquire_build_context) is consistent with the ground truth
    produced by `pio run -t idedata -e <env>` (read_idedata +
    idedata_to_defines).
  - A8 REUSES A4: it does NOT re-implement CPPDEFINES acquisition.
    If A4 ever changes its representation, A8 follows it automatically
    via the existing helpers.
  - A8 VERIFIES: it does NOT modify or correct the context.
  - A8 is parameterisable by PlatformIO environment: it can be called
    once per env, with independent results per call. No env is
    hard-coded.

Out of scope (A8):
  - discovery .cb / .cbch (A3)
  - canonisation / merge (A5)
  - generation C++ (A6)
  - MD5 (A7)
  - .cbch / processors (Phase C)

Comparison rules (derived from A4 representation):
  - flags without value : in ctx.defines (frozenset)
  - flags with value    : in ctx.defines_with_value (dict[str,str])
  - idedata emits       : list of strings ("FOO" or "FOO=value")
  - per-flag equality   : name in both + value match (when present)

Public API:
  check_coherence(env_name, *, project_dir, override_cppdefines=None,
                  env=None, strict=True, timeout=120) -> CoherenceReport
  check_coherence_multi(env_names, *, project_dir, ...) -> list[CoherenceReport]
  list_known_envs(project_dir) -> list[str]

Errors:
  CoherenceError             -- one or more envs diverge (strict mode only)
  UnknownEnvironmentError   -- env_name not declared in platformio.ini
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .flags import (
    BuildContextError,
    ContextDivergenceError,
    acquire_build_context,
    compare_context_to_idedata,
    read_idedata,
)


# =============================================================================
# ERRORS
# =============================================================================

class CoherenceError(Exception):
    """Raised by A8 when one or more envs fail the coherence check.

    The first divergent env's full diagnostic is included.
    """


class UnknownEnvironmentError(Exception):
    """Raised when an env_name is not declared in platformio.ini."""


# =============================================================================
# DATA STRUCTURE
# =============================================================================

@dataclass(frozen=True)
class CoherenceReport:
    """
    Result of one A8 coherence check.

    env_name                : PlatformIO env name.
    coherent                : True if the ctx agrees with idedata.
    ctx_source              : how A4 sourced the ctx (env/build_flags/override).
    ctx_defines_count       : number of value-less defines in ctx.
    ctx_values_count        : number of value defines in ctx.
    idedata_defines_count   : number of value-less defines in idedata.
    idedata_values_count    : number of value defines in idedata.
    missing_from_ctx        : flags present in idedata, absent in ctx.
    extra_in_ctx            : flags present in ctx, absent in idedata.
    value_mismatches        : {flag: {ctx: ..., idedata: ...}}.
    skipped                 : True if the check was skipped (e.g. pio unavailable).
    skip_reason             : human-readable reason when skipped.
    """

    env_name: str
    coherent: bool
    ctx_source: str = "unknown"
    ctx_defines_count: int = 0
    ctx_values_count: int = 0
    idedata_defines_count: int = 0
    idedata_values_count: int = 0
    missing_from_ctx: list[str] = field(default_factory=list)
    extra_in_ctx: list[str] = field(default_factory=list)
    value_mismatches: dict[str, dict[str, str]] = field(default_factory=dict)
    skipped: bool = False
    skip_reason: str = ""

    def summary(self) -> str:
        if self.skipped:
            return f"[{self.env_name}] SKIPPED: {self.skip_reason}"
        if self.coherent:
            return (
                f"[{self.env_name}] COHERENT "
                f"(ctx={self.ctx_defines_count}+{self.ctx_values_count} "
                f"idedata={self.idedata_defines_count}+{self.idedata_values_count}, "
                f"source={self.ctx_source})"
            )
        lines = [f"[{self.env_name}] DIVERGENT (source={self.ctx_source}):"]
        if self.missing_from_ctx:
            lines.append(f"  missing from ctx: {self.missing_from_ctx}")
        if self.extra_in_ctx:
            lines.append(f"  extra in ctx    : {self.extra_in_ctx}")
        if self.value_mismatches:
            for k, d in sorted(self.value_mismatches.items()):
                lines.append(f"  value mismatch  : {k} ctx={d['ctx']!r} idedata={d['idedata']!r}")
        return "\n".join(lines)


# =============================================================================
# platformio.ini parsing
# =============================================================================

# Match [env:NAME] or [NAME] in a platformio.ini file. PlatformIO only
# treats [env:NAME] as a build environment; [NAME] is reserved for
# SCons internal variables (e.g. [platformio], [env]).
_RE_ENV_SECTION = re.compile(
    r"^\[(?P<name>[A-Za-z_][A-Za-z0-9_]*:[A-Za-z0-9_]*)\]\s*$",
    re.MULTILINE,
)

# Recognised env-name prefix per PlatformIO docs.
_ENV_PREFIX = "env:"


def _parse_env_sections(text: str) -> list[str]:
    """Return the list of [env:NAME] sections found in a platformio.ini text."""
    names: list[str] = []
    for m in _RE_ENV_SECTION.finditer(text):
        name = m.group("name")
        if name.startswith(_ENV_PREFIX):
            names.append(name[len(_ENV_PREFIX):])
    return names


def list_known_envs(project_dir: Path | str) -> list[str]:
    """
    Return the list of PlatformIO env names declared in platformio.ini.

    Sorted alphabetically. Does NOT validate that `pio` accepts them
    (an env declared in the .ini can still fail at build time).
    """
    ini = Path(project_dir) / "platformio.ini"
    if not ini.is_file():
        raise FileNotFoundError(
            f"platformio.ini not found at {ini}. Pass a valid project_dir."
        )
    return sorted(_parse_env_sections(ini.read_text(encoding="utf-8")))


def is_known_env(env_name: str, project_dir: Path | str) -> bool:
    """True if `env_name` is declared as a [env:NAME] in platformio.ini."""
    return env_name in list_known_envs(project_dir)


# =============================================================================
# Single-env coherence check
# =============================================================================

def check_coherence(
    env_name: str,
    *,
    project_dir: Path | str,
    override_cppdefines: list[str] | None = None,
    env: Any | None = None,
    strict: bool = True,
    timeout: int = 120,
) -> CoherenceReport:
    """
    Verify the CPPDEFINES coherence for a single PlatformIO environment.

    Strategy (per A8 brief + A4 architecture):
      1. Verify the env is declared in platformio.ini (explicit error
         if not, instead of waiting for pio to fail opaquely).
      2. Acquire the BuildContext through A4's `acquire_build_context`.
         Reuses A4's representation exactly. Honours `override_cppdefines`
         for test-only paths (label='override').
      3. Run `pio run -t idedata -e env_name` to obtain ground truth
         (reuses A4's `read_idedata`).
      4. Compare via A4's `compare_context_to_idedata` (strict=True).
      5. Return a CoherenceReport. If strict=True AND the report
         diverges, also raise CoherenceError (after building the report,
         so the caller has both).

    The check is *independent per call*: there is no shared mutable
    state between two consecutive check_coherence() calls. Multiple
    envs can therefore be tested independently.

    Args:
      env_name            : PlatformIO env to test.
      project_dir         : path to the project containing platformio.ini.
      override_cppdefines : optional explicit list of "-D" tokens used by A4
                            to construct the ctx in 'override' mode.
                            Useful for tests. None = use the real acquisition
                            path (env -> build_flags -> raise).
      env                 : optional SCons env (real PlatformIO env passed
                            by a post: hook). When provided, A4 uses it
                            before any override. None = standalone / CLI.
      strict              : if True, raise CoherenceError on any divergence.
      timeout             : idedata subprocess timeout (seconds).

    Returns:
      CoherenceReport (always built, even when strict raises).

    Raises:
      UnknownEnvironmentError if env_name is not declared.
      CoherenceError          if strict=True AND the check diverges.
      BuildContextError       if A4 fails to acquire the ctx (e.g. empty
                              CPPDEFINES with require_non_empty=True). This
                              is a real A8 finding (the env cannot be
                              resolved) and is re-raised as-is.
    """
    project_dir = Path(project_dir)
    if not is_known_env(env_name, project_dir):
        known = list_known_envs(project_dir)
        raise UnknownEnvironmentError(
            f"PlatformIO env {env_name!r} is not declared in "
            f"{project_dir / 'platformio.ini'}. "
            f"Known envs: {known}"
        )

    # Step 2: acquire the ctx through A4. We pass `env` first (real path),
    # `override_cppdefines` second (test path). A4 itself prefers env when
    # both are present? No - actually A4 prefers override_cppdefines over env.
    # So if the caller wants to test the real env, they pass env=None,
    # override_cppdefines=None. If they want to test an override scenario,
    # they pass override_cppdefines=[...]. Both are honoured by A4.
    try:
        ctx = acquire_build_context(
            env=env,
            project_root=project_dir,
            override_cppdefines=override_cppdefines,
            require_non_empty=True,
        )
    except BuildContextError as e:
        # A4 says the ctx cannot be acquired. Surface as a SKIPPED report
        # so callers can iterate multiple envs without one failure killing
        # the rest. The exception is preserved in the message.
        return CoherenceReport(
            env_name=env_name,
            coherent=False,
            ctx_source="acquire_failed",
            skipped=True,
            skip_reason=f"A4 could not acquire the ctx: {e}",
        )

    # Step 3: idedata.
    try:
        idedata = read_idedata(project_dir, env_name, timeout=timeout)
    except BuildContextError as e:
        return CoherenceReport(
            env_name=env_name,
            coherent=False,
            ctx_source=ctx.cppdefines_source,
            ctx_defines_count=len(ctx.defines),
            ctx_values_count=len(ctx.defines_with_value),
            skipped=True,
            skip_reason=f"`pio run -t idedata` failed: {e}",
        )

    # Step 4: compare via A4.
    try:
        raw_report = compare_context_to_idedata(
            ctx, idedata, env_name, strict=True,
        )
        coherent = not raw_report["diverges"]
        report = CoherenceReport(
            env_name=env_name,
            coherent=coherent,
            ctx_source=ctx.cppdefines_source,
            ctx_defines_count=raw_report["ctx_defines_count"],
            ctx_values_count=raw_report["ctx_values_count"],
            idedata_defines_count=raw_report["idedata_defines_count"],
            idedata_values_count=raw_report["idedata_values_count"],
            missing_from_ctx=raw_report["missing_from_ctx"],
            extra_in_ctx=raw_report["extra_in_ctx"],
            value_mismatches=raw_report["value_mismatches"],
        )
    except ContextDivergenceError as e:
        # compare_context_to_idedata raised - build a divergent report
        # from the same diff lines.
        report = CoherenceReport(
            env_name=env_name,
            coherent=False,
            ctx_source=ctx.cppdefines_source,
            skipped=False,
        )
        # Try to extract structured fields from the message for diagnostics.
        # We re-run compare_context_to_idedata with strict=False to obtain
        # the structured report without re-raising.
        try:
            raw_report = compare_context_to_idedata(
                ctx, idedata, env_name, strict=False,
            )
            report = CoherenceReport(
                env_name=env_name,
                coherent=False,
                ctx_source=ctx.cppdefines_source,
                ctx_defines_count=raw_report["ctx_defines_count"],
                ctx_values_count=raw_report["ctx_values_count"],
                idedata_defines_count=raw_report["idedata_defines_count"],
                idedata_values_count=raw_report["idedata_values_count"],
                missing_from_ctx=raw_report["missing_from_ctx"],
                extra_in_ctx=raw_report["extra_in_ctx"],
                value_mismatches=raw_report["value_mismatches"],
            )
        except BuildContextError:
            pass

    if strict and not report.coherent and not report.skipped:
        raise CoherenceError(report.summary())

    return report


# =============================================================================
# Multi-env coherence check
# =============================================================================

def check_coherence_multi(
    env_names: list[str],
    *,
    project_dir: Path | str,
    override_cppdefines_per_env: dict[str, list[str]] | None = None,
    env_per_env: dict[str, Any] | None = None,
    strict: bool = True,
    timeout: int = 120,
) -> list[CoherenceReport]:
    """
    Run check_coherence over a list of env names.

    Each env is tested independently (no shared mutable state).
    The first divergent env (when strict=True) aborts the loop and
    raises CoherenceError after the corresponding report is built -
    callers can catch and inspect the partial result.

    Args:
      env_names                       : ordered list of env names to test.
      project_dir                     : project root.
      override_cppdefines_per_env     : optional {env_name: [...]}. If set,
                                        the matching env is tested with the
                                        given override (A4 label='override').
                                        Env_names without an override use the
                                        real acquisition path.
      env_per_env                     : optional {env_name: SCons env}. If set,
                                        the matching env is tested with the
                                        real SCons env injected by PlatformIO.
                                        Most useful inside a post: hook that
                                        loops over multiple envs.
      strict                          : if True, raise on first divergence.
      timeout                         : per-env idedata timeout.

    Returns:
      list[CoherenceReport], one per env_name in input order.

    Raises:
      CoherenceError if strict=True and at least one env diverges
      (the first divergent env's diagnostic is included).
    """
    override_per_env = override_cppdefines_per_env or {}
    env_per_env = env_per_env or {}
    reports: list[CoherenceReport] = []
    first_divergence: CoherenceReport | None = None

    for name in env_names:
        report = check_coherence(
            name,
            project_dir=project_dir,
            override_cppdefines=override_per_env.get(name),
            env=env_per_env.get(name),
            strict=False,            # never raise inside the loop
            timeout=timeout,
        )
        reports.append(report)
        if not report.coherent and not report.skipped and first_divergence is None:
            first_divergence = report

    if strict and first_divergence is not None:
        raise CoherenceError(first_divergence.summary())

    return reports


# =============================================================================
# CLI entry point
# =============================================================================

def main(argv: list[str] | None = None) -> int:
    """
    CLI entry point: `python -m scripts.combus_builder.coherence <env>...`.

    Examples:
      python -m scripts.combus_builder.coherence volvo_A60H_bruder
      python -m scripts.combus_builder.coherence volvo_A60H_bruder remotes
    """
    import argparse
    import sys

    parser = argparse.ArgumentParser(
        prog="combus_builder.coherence",
        description="A8 CPPDEFINES / PlatformIO coherence check.",
    )
    parser.add_argument(
        "envs", nargs="*",
        help="PlatformIO env name(s) to check. If empty, all envs in platformio.ini.",
    )
    parser.add_argument(
        "--project-dir", default=".",
        help="Path to the project root (containing platformio.ini).",
    )
    parser.add_argument(
        "--no-strict", action="store_true",
        help="Do not raise on divergence; only report.",
    )

    args = parser.parse_args(argv)
    project_dir = Path(args.project_dir)

    envs = args.envs if args.envs else list_known_envs(project_dir)
    if not envs:
        print("[combus_builder] no envs to check", file=sys.stderr)
        return 1

    try:
        reports = check_coherence_multi(
            envs,
            project_dir=project_dir,
            strict=not args.no_strict,
        )
    except CoherenceError as e:
        print(str(e), file=sys.stderr)
        return 2

    import sys as _sys
    for r in reports:
        print(r.summary())
    return 0 if all(r.coherent or r.skipped for r in reports) else 2


if __name__ == "__main__":
    import sys
    sys.exit(main())
