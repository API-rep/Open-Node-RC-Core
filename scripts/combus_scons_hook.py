#!/usr/bin/env python3
"""
combus_scons_hook.py — A12 implementation: PlatformIO extra_script hook.

This script is loaded by PlatformIO via `extra_scripts = post:...` in
platformio.ini. It runs ONCE after `extends` resolution and CPPDEFINES
construction, but BEFORE SCons starts compiling the `.cpp` files.

Pipeline:
  1. Acquire BuildContext from the live SCons env (`env["CPPDEFINES"]`).
  2. Discover and parse the `.cb` files in the current buildroot.
  3. Canonise and resolve `requires` against the BuildContext.
  4. Emit the 3 view triplets (combus, combus_local, combus_remote) +
     MD5 artefacts (A7) into a generated directory.
  5. Add the generated directory to env['CPPPATH'] so the .cpp files
     pick them up via `#include "combus.h"` etc.

This script is the production integration of A12. It does NOT use
`env.AddPostAction()` — the generation happens BEFORE compilation, not
after. See doc/combus_v2 - A12 hook mechanism.md for the rationale.

Usage in platformio.ini:

    extra_scripts =
        post:scripts/combus_scons_hook.py

Failure policy (revision 6, 2026-08-22):
  - ANY error in the pipeline (import, BuildContext, parse, canonise,
    generate, write) is FATAL. The hook calls sys.exit(1) which SCons
    interprets as a build failure. The build cannot continue with stale
    artefacts.
  - Anti-stale: artefacts are written into a sibling staging
    directory; only after a completeness check on the staging does the
    hook publish them into the final location. If generation fails,
    the staging is cleaned up and the previous out_dir is left
    untouched. The build then aborts because _fatal() exits with code
    1, before any .cpp is compiled.
  - COMBUS_BUILDER_SKIP=1: explicit opt-out. The hook verifies that
    the FULL EXPECTED_ARTIFACTS list is already present in the
    expected directory; if not, it fails loudly. This prevents the
    silent "skip + missing headers" trap.

The "atomic" claim of the publish step is about renaming only; the
real safety against stale-artefact builds is the FATAL-on-any-error
policy, which makes it impossible for a compile to start against
artefacts that don't match the current configuration.

See doc/combus_v2 - A12 hook mechanism.md section 15 for the full
contract.
"""
from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

# SCons / PlatformIO injects `env` at top level.
Import("env")  # noqa: F821 — SCons inject

# Make `scripts.combus_builder.*` importable from the repo root.
_REPO_ROOT = Path(__file__).resolve().parent.parent
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))


# =============================================================================
# Expected artefacts (the contract that this hook guarantees)
# =============================================================================

EXPECTED_ARTIFACTS = (
    # 3 views x 3 files (A6.1):
    "combus.h",
    "combus.cpp",
    "combus_ids.h",
    "combus_local.h",
    "combus_local.cpp",
    "combus_local_ids.h",
    "combus_remote.h",
    "combus_remote.cpp",
    "combus_remote_ids.h",
    # MD5 artefacts (A7). All 3 views are hashed.
    "combus_local_md5.h",
    "combus_remote_md5.h",
    "combus_md5.h",
    "combus_wire_common.h",
)


def _missing_artefacts(out_dir: Path) -> list[str]:
    """Return the list of EXPECTED_ARTEFACTS that are not present."""
    return [name for name in EXPECTED_ARTIFACTS
            if not (out_dir / name).is_file()]


# =============================================================================
# Failure helpers
# =============================================================================

def _fatal(msg: str) -> None:
    """
    Print a fatal error to stderr and exit with code 1.

    SCons / PlatformIO interpret a non-zero exit from an extra_script
    as a build failure. This is the ONLY way the hook reports failure.
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


def _verify_skip_artifacts(out_dir: Path) -> None:
    """
    When COMBUS_BUILDER_SKIP=1 is set, the hook does NOT regenerate.
    It MUST verify that EVERY expected artefact is already present in
    out_dir (the full EXPECTED_ARTIFACTS list, not just a sentinel).
    If anything is missing, the build cannot succeed (the .cpp files
    include several of these headers) and we must fail loudly rather
    than let the compiler emit a confusing "file not found" error.
    """
    missing = _missing_artefacts(out_dir)
    if missing:
        _fatal(
            f"COMBUS_BUILDER_SKIP=1 is set but the artefacts in {out_dir} "
            f"are incomplete. Missing files: {missing}. Either unset "
            "COMBUS_BUILDER_SKIP to regenerate, or run a build without "
            "the skip first to produce the full artefact set."
        )
    print(
        f"[combus_scons_hook] COMBUS_BUILDER_SKIP=1 — using pre-existing "
        f"artefacts in {out_dir} ({len(EXPECTED_ARTIFACTS)} files verified)."
    )


# =============================================================================
# Output directory resolution
# =============================================================================

def _default_out_dir(env) -> Path:
    """
    Where to write the generated headers.

    Prefer `PROJECT_BUILD_DIR` (PlatformIO 6.x), fall back to
    `.pio/build/<PIOENV>` derived from PROJECT_DIR + PIOENV. Refuse to
    run if neither is available (the caller has to set up at least one).
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
        "cannot determine where to write generated headers."
    )


# =============================================================================
# Anti-stale: atomic publish
# =============================================================================

def _publish(src_dir: Path, dst_dir: Path) -> None:
    """
    Publish the staging directory into the final location.

    IMPORTANT: This function is NOT crash-safe by itself. There is a
    window between the `shutil.rmtree(dst_dir)` and the
    `os.replace(src_dir, dst_dir)` in which `dst_dir` does not exist
    on disk. If the process is killed inside that window, the build
    is in an inconsistent state. We accept this risk because:

    1. The only thing that runs after the publish is the SCons
       compilation, which the hook cannot reach on fatal errors
       (`_fatal` -> `sys.exit(1)` propagates immediately).
    2. The pre-publish completeness check (`_check_staging_complete`)
       prevents publishing a partial staging in the first place.

    The real safety net is therefore the FATAL-on-any-error policy
    upstream of this function, not the publish itself.
    """
    if dst_dir.exists():
        shutil.rmtree(dst_dir)
    os.replace(src_dir, dst_dir)


def _check_staging_complete(staging_dir: Path) -> None:
    """
    Verify that the staging directory contains the full
    EXPECTED_ARTIFACTS list BEFORE any publication. If anything is
    missing, fail loudly and clean the staging.

    Rationale: even if `generate()` reported success, we want a
    belt-and-braces check here. A partial staging could replace a
    previously valid out_dir with an incomplete one, breaking the
    compile with confusing errors. Better to fail the build than to
    publish a partial set.
    """
    missing = [name for name in EXPECTED_ARTIFACTS
               if not (staging_dir / name).is_file()]
    if missing:
        # Clean the staging so we don't leave junk around.
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        _fatal(
            f"staging directory {staging_dir} is incomplete after "
            f"generation. Missing files: {missing}. The previous "
            "out_dir is left untouched; the build is aborted."
        )


# =============================================================================
# Main pipeline
# =============================================================================

def main(env) -> int:
    """
    Run the A12 hook pipeline.

    Returns 0 on success. On ANY error, calls _fatal() which calls
    sys.exit(1) — this function never returns a non-zero code, because
    SCons must treat any failure as a build failure.
    """
    out_dir = _default_out_dir(env)

    # --- Skip path ---------------------------------------------------------
    if _skip_requested():
        _verify_skip_artifacts(out_dir)
        # Add to CPPPATH so the .cpp files can find the pre-existing
        # headers. We do NOT regenerate.
        out_dir_str = str(out_dir)
        cpppath = env.get("CPPPATH", [])
        if isinstance(cpppath, str):
            cpppath = [cpppath]
        if out_dir_str not in cpppath:
            cpppath.append(out_dir_str)
            env["CPPPATH"] = cpppath
        print(f"[combus_scons_hook] CPPPATH += {out_dir_str} (from skip)")
        return 0

    # --- Late imports ------------------------------------------------------
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
        _fatal(f"cannot import combus_builder: {e}. "
               "Make sure the repo root is on sys.path (it should be).")

    # --- 1. Build context --------------------------------------------------
    try:
        ctx = acquire_build_context(env=env)
    except BuildContextError as e:
        _fatal(f"BuildContext acquisition failed: {e}")

    print(
        f"[combus_scons_hook] BuildContext acquired "
        f"(source={ctx.cppdefines_source}, "
        f"{len(ctx.defines)} defines, "
        f"{len(ctx.defines_with_value)} valued)"
    )

    # --- 2. Discover + parse ----------------------------------------------
    try:
        _, _, parsed = discover_and_parse(env=env)
    except ParseError as e:
        _fatal(f"YAML parse failed: {e}")

    # --- 3. Canonise ------------------------------------------------------
    try:
        canon = canonize(parsed)
    except Exception as e:  # ChannelError family; keep broad for visibility
        _fatal(f"canonisation failed: {e}")

    # --- 4. Generate (atomic) ---------------------------------------------
    # Write into a sibling temp dir, then atomically publish. If anything
    # fails between here and the publish, the previous artefacts (if any)
    # are gone and the build will fail with a missing-header error.
    staging_dir = out_dir.with_suffix(out_dir.suffix + ".new")
    # Clean any leftover staging from a previous failed attempt.
    if staging_dir.exists():
        shutil.rmtree(staging_dir)

    try:
        sel = generate(canon.canonical_definitions, ctx, staging_dir)
    except GeneratorError as e:
        # Clean staging; do NOT touch out_dir (it may still hold valid
        # artefacts from a previous successful build, but the build
        # will fail anyway because we exit 1).
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        _fatal(f"generation failed: {e}")
    except Exception as e:
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        _fatal(f"unexpected error during generation: {e}")

    # --- 4b. Pre-publish completeness check -------------------------------
    # Belt-and-braces: even if generate() returned without error, do
    # not publish a staging that is missing one of the expected files.
    _check_staging_complete(staging_dir)

    # --- 5. Publish -------------------------------------------------------
    try:
        _publish(staging_dir, out_dir)
    except OSError as e:
        _fatal(f"failed to publish generated artefacts: {e}")

    print(
        f"[combus_scons_hook] generated "
        f"combus ({sel.full.ch_count} ch, WIRE_END={sel.full.wire_end}), "
        f"combus_local ({sel.local.ch_count} ch), "
        f"combus_remote ({sel.remote.ch_count} ch) "
        f"-> {out_dir}"
    )

    # --- 6. CPPPATH -------------------------------------------------------
    out_dir_str = str(out_dir)
    cpppath = env.get("CPPPATH", [])
    if isinstance(cpppath, str):
        cpppath = [cpppath]
    if out_dir_str not in cpppath:
        cpppath.append(out_dir_str)
        env["CPPPATH"] = cpppath

    print(f"[combus_scons_hook] CPPPATH += {out_dir_str}")
    return 0


# =============================================================================
# Module-level execution
# =============================================================================
# SCons imports this script and runs it. main() either returns 0 (success)
# or calls sys.exit(1) (fatal). We do NOT catch the SystemExit here:
# letting it propagate is what makes SCons treat the build as failed.
main(env)