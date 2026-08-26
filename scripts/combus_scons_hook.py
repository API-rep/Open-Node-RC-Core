#!/usr/bin/env python3
"""
combus_scons_hook.py — A12 implementation: PlatformIO extra_script hook.

This script is loaded by PlatformIO via `extra_scripts = pre:...` in
platformio.ini. It runs ONCE, BEFORE SCons resolves `build_src_filter`.

Pipeline (A16 refactor — deferred SCons task):
  1. Eagerly add the `combus_generated/` directory to CPPPATH and
     `combus.cpp` to PIOBUILDFILES so SCons picks them up when the
     source list is resolved.
  2. Register a SINGLE SCons `env.Command()` that produces all the
     `combus_generated/*` artefacts LAZILY — at build time, not at
     hook-registration time. This is the key change versus the
     previous A12/A15 approach which called the generator eagerly.

The SCons task is invalidated when:
  - any of the `*.cb` / `*.cbch` source files change, AND/OR
  - any of the configuration values listed in `varlist` (CPPDEFINES,
    CPPPATH, LIBDEPS, buildroot) change.

This solves the two failure modes the previous eager-hook approach
suffered from:
  - `pre:`  → CPPDEFINES is empty (env not yet built)
  - `post:` → PIOBUILDFILES is frozen (sources already resolved)

By deferring the actual generation work to a SCons task, the env is
fully built by the time the action runs, and the artefacts are
written before any source is compiled (SCons topological sort).

A single Command is registered on `env` (idempotency guard prevents a
second registration on `projenv`).

Usage in platformio.ini:

    extra_scripts =
        pre:scripts/combus_scons_hook.py

Failure policy (revision 7, 2026-08-25):
  - Any error in the action is fatal (return non-zero from the action).
  - Anti-stale: artefacts are written into a sibling staging directory;
    only after a completeness check on the staging does the hook publish
    them into the final location.
  - COMBUS_BUILDER_SKIP=1: the Command is registered but its action is
    replaced by a no-op (and the build only succeeds if the artefacts
    are already present).
  - If env["CPPDEFINES"] is empty or required structural flags are
    missing at action time, the build fails loudly (no silent
    generation with an incomplete context).
"""
from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

# SCons / PlatformIO inject `env` and (sometimes) `projenv` at top level.
Import("env")  # noqa: F821 — SCons inject
try:
    Import("projenv")  # noqa: F821 — SCons inject, may not exist
except Exception:  # pragma: no cover — some PIO versions omit it
    projenv = None  # type: ignore[assignment]

# Make `scripts.combus_builder.*` importable from the repo root.
_REPO_ROOT = Path(env["PROJECT_DIR"]).resolve()
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))


# =============================================================================
# Expected artefacts (the contract that this hook guarantees)
# =============================================================================

EXPECTED_ARTIFACTS = (
    # combus (full view) — still needs .h/.cpp because it's the runtime
    # bus instance.
    "combus.h",
    "combus.cpp",
    "combus_ids.h",
    # combus_local / combus_remote: ids only (Phase 2).
    "combus_local_ids.h",
    "combus_remote_ids.h",
    # MD5 artefacts (A7). All 3 views are hashed.
    "combus_local_md5.h",
    "combus_remote_md5.h",
    "combus_md5.h",
    "combus_wire_common.h",
)


def _missing_artefacts(out_dir: Path) -> list[str]:
    """Return the list of expected artefacts that are not present."""
    return [name for name in EXPECTED_ARTIFACTS
            if not (out_dir / name).is_file()]


# =============================================================================
# Failure helpers
# =============================================================================

def _fatal(msg: str) -> None:
    """Print a fatal error to stderr and exit with code 1.

    Used by the HOOK itself (registration time). Inside the SCons
    action we prefer returning a non-zero exit code (so SCons records
    the failure on the target).
    """
    sys.stderr.write(f"[combus_scons_hook] FATAL: {msg}\n")
    sys.stderr.flush()
    sys.exit(1)


# =============================================================================
# Skip policy
# =============================================================================

def _skip_requested() -> bool:
    """Honour COMBUS_BUILDER_SKIP=1 to bypass generation entirely."""
    return os.environ.get("COMBUS_BUILDER_SKIP", "").strip().lower() in (
        "1",
        "true",
        "yes",
        "on",
    )


# =============================================================================
# Output directory resolution
# =============================================================================

def _default_out_dir(env) -> Path:
    """Where to write the generated headers.

    Prefer `PROJECT_BUILD_DIR` (PlatformIO 6.x), fall back to
    `.pio/build/<PIOENV>` derived from PROJECT_DIR + PIOENV.
    """
    build_dir = env.get("PROJECT_BUILD_DIR")
    if build_dir:
        return Path(build_dir) / "combus_generated"

    project_dir = env.get("PROJECT_DIR")
    pio_env = env.get("PIOENV") or "default"
    if project_dir:
        return Path(project_dir) / ".pio" / "build" / pio_env / "combus_generated"

    raise RuntimeError(
        "Neither PROJECT_BUILD_DIR nor PROJECT_DIR is available on env; "
        "cannot determine where to write generated artefacts."
    )


# =============================================================================
# Anti-stale: atomic publish
# =============================================================================

def _publish(src_dir: Path, dst_dir: Path) -> None:
    """Atomically publish the staging directory into the final location.

    Crash-safety window: between `rmtree(dst_dir)` and `os.replace`.
    Accepted because `_fatal` propagates immediately on any error.
    """
    if dst_dir.exists():
        shutil.rmtree(dst_dir)
    os.replace(src_dir, dst_dir)


def _check_staging_complete(staging_dir: Path) -> None:
    """Verify that the staging directory contains the full
    EXPECTED_ARTIFACTS list BEFORE any publication."""
    missing = [name for name in EXPECTED_ARTIFACTS
               if not (staging_dir / name).is_file()]
    if missing:
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        raise RuntimeError(
            f"staging directory {staging_dir} is incomplete after "
            f"generation. Missing files: {missing}. The previous "
            "out_dir is left untouched; the build is aborted."
        )


# =============================================================================
# Source discovery (used to compute the SCons source list + for the action)
# =============================================================================

def _discover_cb_sources(buildroot: Path) -> list[str]:
    """Return the list of `*.cb` / `*.cbch` source files (strings).

    Used both:
    - as the `source` list of the SCons Command (for incremental build),
    - and as the input of the action (for invalidation awareness).
    """
    sources: list[str] = []
    for ext in ("*.cb", "*.cbch"):
        for p in buildroot.rglob(ext):
            sources.append(str(p))
    return sorted(sources)


# =============================================================================
# The SCons action — runs LAZILY at build time, with the env fully built
# =============================================================================

def _generate_combus_action(target, source, env):
    """
    SCons action that produces all `combus_generated/*` artefacts.

    Invoked by SCons at build time, after `env` is fully constructed
    (so `env["CPPDEFINES"]` is populated and `PioBuildFiles` is
    resolved). Writes into a staging directory, then atomically
    publishes into the final out_dir.

    Hard failures (returns non-zero) on:
      - env["CPPDEFINES"] empty
      - missing structural flags (e.g. IS_MACHINE / IS_REMOTE)
      - generator / parser / canon errors
      - atomic-publish failure
    """
    print("[combus_scons_action] running combus builder…")

    out_dir = _default_out_dir(env)

    # --- 1. Hard fail on empty CPPDEFINES --------------------------------
    # By the time the action runs, the env is fully built and CPPDEFINES
    # is populated. If it isn't, the context is genuinely incomplete and
    # the previous A12/A15 fallback (GetProjectOption("build_flags"))
    # would be hiding a real misconfiguration.
    cppdefines = env.get("CPPDEFINES", [])
    if not cppdefines:
        sys.stderr.write(
            "[combus_scons_action] FATAL: env['CPPDEFINES'] is empty at "
            "action time. Refusing to generate with an incomplete context.\n"
        )
        return 1

    # --- 2. Hard fail if NO structural flag is set ------------------------
    # At least one of these should be set on every legitimate env.
    # NOTE: IS_MAINBOARD and IS_EXT_BOARD are DEPRECATED (the "board role"
    # axis no longer exists — every board is an implicit BOARD, owner of
    # the combus of the module it hosts, cf. §1.2 of board_architecture.md).
    # They MUST NOT be used as a validation criterion here.
    structural_flags = {"IS_MACHINE", "IS_REMOTE"}
    # CPPDEFINES is a list of (name, value) tuples when value-less,
    # or just `name` for the value-less form — normalise to set of names.
    define_names: set[str] = set()
    for d in cppdefines:
        if isinstance(d, tuple):
            define_names.add(d[0])
        else:
            define_names.add(d)
    if not (define_names & structural_flags):
        sys.stderr.write(
            f"[combus_scons_action] FATAL: no structural flag set "
            f"(expected one of {sorted(structural_flags)}). "
            f"Refusing to generate.\n"
        )
        return 1

    # --- 3. Late imports (so the action only imports when it runs) -------
    # SCons runs actions in a subprocess (or at least a fresh import
    # context), so sys.path is NOT inherited from the hook. We rebuild
    # it from the COMBUS_REPO_ROOT env var that the hook set at
    # registration time.
    repo_root = env.get("COMBUS_REPO_ROOT")
    if repo_root and repo_root not in sys.path:
        sys.path.insert(0, repo_root)
    try:
        from scripts.combus_builder.canon import canonize
        from scripts.combus_builder.flags import (
            BuildContextError,
            acquire_build_context,
        )
        from scripts.combus_builder.generator import GeneratorError, generate
        from scripts.combus_builder.parser import (
            ParseError,
            discover_and_parse,
        )
    except ImportError as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: cannot import combus_builder: {e}\n"
        )
        return 1

    # --- 4. Build context ------------------------------------------------
    try:
        ctx = acquire_build_context(env=env)
    except BuildContextError as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: BuildContext failed: {e}\n"
        )
        return 1
    print(
        f"[combus_scons_action] BuildContext acquired "
        f"(source={ctx.cppdefines_source}, "
        f"{len(ctx.defines)} defines, "
        f"{len(ctx.defines_with_value)} valued)"
    )

    # --- 5. Discover + parse ---------------------------------------------
    try:
        _, _, parsed = discover_and_parse(env=env)
    except ParseError as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: YAML parse failed: {e}\n"
        )
        return 1

    # --- 6. Canonise ----------------------------------------------------
    try:
        canon = canonize(parsed)
    except Exception as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: canonisation failed: {e}\n"
        )
        return 1

    # --- 7. Generate (atomic) -------------------------------------------
    staging_dir = out_dir.with_suffix(out_dir.suffix + ".new")
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    try:
        sel = generate(canon.canonical_definitions, ctx, staging_dir)
    except GeneratorError as e:
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        sys.stderr.write(
            f"[combus_scons_action] FATAL: generation failed: {e}\n"
        )
        return 1
    except Exception as e:
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        sys.stderr.write(
            f"[combus_scons_action] FATAL: unexpected error during generation: {e}\n"
        )
        return 1

    # --- 8. Pre-publish completeness check ------------------------------
    try:
        _check_staging_complete(staging_dir)
    except RuntimeError as e:
        sys.stderr.write(f"[combus_scons_action] FATAL: {e}\n")
        return 1

    # --- 9. Publish ------------------------------------------------------
    try:
        _publish(staging_dir, out_dir)
    except OSError as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: failed to publish artefacts: {e}\n"
        )
        return 1

    print(
        f"[combus_scons_action] generated "
        f"combus ({sel.full.ch_count} ch, WIRE_END={sel.full.wire_end}), "
        f"combus_local ({sel.local.ch_count} ch), "
        f"combus_remote ({sel.remote.ch_count} ch) "
        f"-> {out_dir}"
    )
    return 0


def _skip_action(target, source, env):
    """No-op action for COMBUS_BUILDER_SKIP=1 (artefacts must already be present)."""
    out_dir = _default_out_dir(env)
    missing = _missing_artefacts(out_dir)
    if missing:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: COMBUS_BUILDER_SKIP=1 but "
            f"artefacts missing: {missing}\n"
        )
        return 1
    print(f"[combus_scons_action] SKIP — using pre-existing artefacts in {out_dir}")
    return 0


# =============================================================================
# CPPPATH / PIOBUILDFILES propagation (eager, at hook time)
# =============================================================================

def _add_to_cpppath(target_env, out_dir_str: str, label: str) -> None:
    if target_env is None:
        return
    cpppath = target_env.get("CPPPATH", [])
    if isinstance(cpppath, str):
        cpppath = [cpppath]
    if out_dir_str not in cpppath:
        cpppath.append(out_dir_str)
        target_env["CPPPATH"] = cpppath
    print(f"[combus_scons_hook] CPPPATH += {out_dir_str} ({label})")


def _add_common_defs_cpppath(target_env, project_dir, pio_env) -> None:
    """Add `.pio/libdeps/<pio_env>/common_defs/include` to CPPPATH if it
    exists. Mirrors `combus_test_add_generated_source.py`. The library
    is auto-CPPPATH'd after extends resolution; in `pre:` mode the env
    may not yet know about it, so we add it explicitly.
    """
    if not project_dir or not pio_env:
        return
    candidate = Path(project_dir) / ".pio" / "libdeps" / pio_env / "common_defs" / "include"
    if not candidate.is_dir():
        return
    for te in (target_env, projenv):
        if te is None:
            continue
        cpppath = te.get("CPPPATH", [])
        if isinstance(cpppath, str):
            cpppath = [cpppath]
        if str(candidate) not in cpppath:
            cpppath.append(str(candidate))
            te["CPPPATH"] = cpppath
    print(f"[combus_scons_hook] CPPPATH += {candidate}")


# =============================================================================
# Deferred task registration
# =============================================================================

def _register_deferred_task(env) -> None:
    """
    Register a SINGLE `env.Command()` for all combus_generated/* artefacts.

    Idempotency: we mark the env with `_combus_command_registered = True`
    to avoid re-registration if the hook is somehow invoked twice (e.g.
    if both `env` and `projenv` are presented to the hook — only `env`
    receives the Command).
    """
    if env.GetOption("no_exec"):
        # SCons is in dry-run / query mode. Don't touch anything.
        return
    if env.get("_combus_command_registered"):
        return

    out_dir = _default_out_dir(env)
    project_dir = env.get("PROJECT_DIR")
    pio_env = env.get("PIOENV") or "default"

    # Stash the repo root on the env so the SCons action (which runs in
    # a fresh Python interpreter that does NOT inherit our sys.path) can
    # rebuild it before importing scripts.combus_builder.
    if project_dir:
        env["COMBUS_REPO_ROOT"] = str(Path(project_dir).resolve())

    # --- Build the targets ----------------------------------------------
    targets = [str(out_dir / name) for name in EXPECTED_ARTIFACTS]

    # --- Build the source list (.cb / .cbch) -----------------------------
    # Best-effort: if the project tree isn't yet visible (PIO rare cases),
    # fall back to an empty source list — the artefacts will simply be
    # regenerated whenever any SCons-managed dependency changes.
    sources: list[str] = []
    if project_dir:
        sources = _discover_cb_sources(Path(project_dir))
    if not sources:
        # Use a non-existent sentinel so SCons still knows the action's
        # inputs are a list of strings (it accepts [] silently).
        sources = []

    # --- Choose action: skip or full ------------------------------------
    if _skip_requested():
        action = _skip_action
    else:
        action = _generate_combus_action

    # --- Register the Command ------------------------------------------
    #
    # varlist=[] would defeat the purpose; we list the env variables that
    # are semantically relevant to the generation:
    #   - CPPDEFINES: any -D flag (MACHINE_*, IS_*, HAS_*, DEBUG_*, ...)
    #     can change which channels are emitted (their `requires:` clause).
    #   - CPPPATH:    for completeness; in practice the `.cb` parser
    #     doesn't depend on it but a user-visible change should still
    #     force a regen (e.g. if a .cb #include'd a generated header).
    #   - LIBDEPS:    adding/removing a library could expose/hide headers.
    #
    # The native SCons `varlist=` is the supported mechanism for env-var
    # invalidation: SCons serialises those values into the build
    # signature, so a change in any of them triggers a rebuild.
    try:
        env.Command(
            target=targets,
            source=sources,
            action=action,
            varlist=["CPPDEFINES", "CPPPATH", "LIBDEPS"],
        )
    except Exception as e:
        sys.stderr.write(
            f"[combus_scons_hook] FATAL: could not register Command: {e}\n"
        )
        sys.exit(1)

    env["_combus_command_registered"] = True
    print(
        f"[combus_scons_hook] registered deferred Command "
        f"(targets={len(targets)}, sources={len(sources)}, "
        f"varlist=[CPPDEFINES, CPPPATH, LIBDEPS])"
    )


# =============================================================================
# Main pipeline (eager portion — runs at hook load time)
# =============================================================================

def main(env) -> int:
    """
    Hook entry point — runs at `pre:` time.

    1. Adds `out_dir` to CPPPATH and `combus.cpp` to PIOBUILDFILES
       (eager, so the artefacts are picked up by the source resolution
       that happens immediately after this hook).
    2. Adds `common_defs/include` to CPPPATH (mirrors the test helper).
    3. Registers a deferred SCons Command for the actual generation.
    """
    out_dir = _default_out_dir(env)
    out_dir_str = str(out_dir)

    # --- 1. CPPPATH for env + projenv -----------------------------------
    _add_to_cpppath(env, out_dir_str, "env")
    _add_to_cpppath(projenv, out_dir_str, "projenv")

    # --- 2. libdeps CPPPATH (common_defs) --------------------------------
    project_dir = env.get("PROJECT_DIR")
    pio_env = env.get("PIOENV") or "default"
    _add_common_defs_cpppath(env, project_dir, pio_env)

    # --- 3. PIOBUILDFILES (combus.cpp) -----------------------------------
    # The .cpp may not exist yet (the Command hasn't run), but PIOBUILDFILES
    # only needs the path string; SCons will see the file appear after the
    # Command runs and will compile it before the link step.
    sources = env.get("PIOBUILDFILES", [])
    if isinstance(sources, str):
        sources = [sources]
    cpp_str = str(out_dir / "combus.cpp")
    if cpp_str not in sources:
        sources.append(cpp_str)
        env.Replace(PIOBUILDFILES=sources)
    print(f"[combus_scons_hook] PIOBUILDFILES += {cpp_str}")
    # Mirror on projenv (some PIO versions read PIOBUILDFILES from projenv)
    if projenv is not None:
        psources = projenv.get("PIOBUILDFILES", [])
        if isinstance(psources, str):
            psources = [psources]
        if cpp_str not in psources:
            psources.append(cpp_str)
            projenv.Replace(PIOBUILDFILES=psources)

    # --- 4. Register the deferred Command (single, on env only) --------
    _register_deferred_task(env)

    return 0


# =============================================================================
# Module-level execution
# =============================================================================
# SCons imports this script and runs it. main() either returns 0 (success)
# or calls sys.exit(1) (fatal). We do NOT catch the SystemExit here.
main(env)
