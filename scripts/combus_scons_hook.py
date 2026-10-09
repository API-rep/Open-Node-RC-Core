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

ND1 (multiboard) — 2026-09-13:
  - When the current PlatformIO env is a sub-env "board" of a node
    (e.g. [env:volvo_A60H_bruder_mainboard] is a sub-env of the
    mother env [env:volvo_A60H_bruder]), the hook:
      1. Detects the node hierarchy via parse_env_hierarchy().
      2. Resolves the BuildContext of every sister sub-env via
         resolve_build_context_static() (no subprocess, no compile).
      3. Calls generate() once per board with a per-board
         filename_prefix, producing per-board artefacts
         (combus_<prefix>_ids.h, combus_<prefix>.h, combus_<prefix>.cpp,
          combus_local_<prefix>_ids.h, combus_remote_<prefix>_ids.h,
          combus_<prefix>_md5.h, combus_local_<prefix>_md5.h,
          combus_remote_<prefix>_md5.h).
         combus_wire_common.h is shared (not prefixed).
  - Legacy single-board mode (no sister sub-env detected) is
    STRICTLY UNCHANGED: same artefacts, same names, same behaviour.

ND1 fix (2026-09-13, post-review): the per-board loop now correctly
resolves the mother env's BuildContext. When the current build is a
sub-env (e.g. volvo_A60H_bruder_mainboard), the unprefixed artefacts
(combus.h, combus.cpp, combus_ids.h, ...) must reflect the MOTHER
env's defines, not the sub-env's. The previous code reused the live
`ctx` (which is the sub-env's) for the mother entry, causing silent
drift of the unprefixed artefacts depending on which sub-env was
built last. The fix: when board_id is None (mother entry), use the
live `ctx` only if pio_env == node.mother_env; otherwise resolve the
mother env statically via resolve_build_context_static().
"""
from __future__ import annotations

import os
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

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

# Legacy single-board artefacts (ND1: unchanged for backward compat).
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


def _missing_artefacts(out_dir: Path, expected: list[str]) -> list[str]:
    """Return the list of expected artefacts that are not present."""
    return [name for name in expected
            if not (out_dir / name).is_file()]


# =============================================================================
# ND1: platformio.ini env list + per-node expected artefacts
# =============================================================================

# Regex matching `[env:NAME]` section headers in platformio.ini.
# `[env]` (the unnamed base env) is intentionally NOT matched — it is
# not a buildable env on its own.
_RE_ENV_SECTION = re.compile(r"^\[env:([A-Za-z0-9_\-]+)\]\s*$")


def _read_all_env_names(project_dir: Path) -> list[str]:
    """Parse platformio.ini and return the list of `[env:NAME]` names.

    Used by ND1 to feed parse_env_hierarchy(). The parser is a simple
    line-by-line regex — it does NOT try to evaluate `extends=`,
    `${...}` interpolation, or any other PlatformIO feature. It only
    needs the raw section names, which are syntactically trivial.

    Returns an empty list if platformio.ini is missing or unreadable.
    """
    ini_path = Path(project_dir) / "platformio.ini"
    if not ini_path.is_file():
        return []
    envs: list[str] = []
    try:
        text = ini_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    for line in text.splitlines():
        m = _RE_ENV_SECTION.match(line.strip())
        if m:
            envs.append(m.group(1))
    return envs


def _collect_node_boards(
    mother_env: str,
    all_env_names: list[str],
) -> list[tuple[str | None, str]]:
    """Collect the list of (board_id, env_name) for a node.

    Returns a list of (board_id, env_name) tuples:
      - One entry per sub-env `<mother_env>_<board_id>` declared in
        all_env_names (board_id = the suffix after '<mother_env>_').
      - One entry with board_id = None for the mother env itself
        (if it is declared in all_env_names).

    The order is: mother first (board_id=None), then sub-envs in
    alphabetical order of board_id.
    """
    boards: list[tuple[str | None, str]] = []
    if mother_env in all_env_names:
        boards.append((None, mother_env))
    sub_envs: list[tuple[str, str]] = []
    for other in all_env_names:
        if other == mother_env:
            continue
        if not other.startswith(mother_env + "_"):
            continue
        board_id = other[len(mother_env) + 1:]
        if not board_id:
            continue
        sub_envs.append((board_id, other))
    sub_envs.sort(key=lambda x: x[0])
    boards.extend(sub_envs)
    return boards


def _expected_artifacts_for_node(
    boards: list[tuple[str | None, str]],
    is_node: bool,
) -> list[str]:
    """Return the list of expected artefacts for the current build.

    Legacy mode (is_node=False): returns EXPECTED_ARTIFACTS unchanged.

    Node mode (is_node=True): returns the legacy artefacts (the mother
    env's own artefacts, if any) PLUS the per-board prefixed artefacts
    for every sister sub-env. combus_wire_common.h is shared and is
    NOT duplicated per board.
    """
    artifacts: list[str] = list(EXPECTED_ARTIFACTS)
    if not is_node:
        return artifacts
    for board_id, _env_name in boards:
        if board_id is None:
            # Mother env: no prefix, artefacts already in EXPECTED_ARTIFACTS.
            continue
        prefix = board_id
        artifacts.extend([
            f"combus_{prefix}_ids.h",
            f"combus_{prefix}.h",
            f"combus_{prefix}.cpp",
            f"combus_local_{prefix}_ids.h",
            f"combus_remote_{prefix}_ids.h",
            f"combus_{prefix}_md5.h",
            f"combus_local_{prefix}_md5.h",
            f"combus_remote_{prefix}_md5.h",
        ])
    return artifacts


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


def _check_staging_complete(staging_dir: Path, expected: list[str]) -> None:
    """Verify that the staging directory contains the full expected
    artefacts list BEFORE any publication."""
    missing = [name for name in expected
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
# ND1: per-board generation (extracted for testability)
# =============================================================================

class _BoardGenerationError(Exception):
    """Raised by _generate_per_board() on any failure.

    The caller (_generate_combus_action) catches this, cleans up the
    staging directory, and returns a non-zero exit code to SCons.
    """


@dataclass
class _BoardResult:
    """Result of generating one board's artefacts.

    Wraps the Selection returned by generate() plus the BuildContext
    used (so the caller can log source/defines/values without having
    to thread them through separately).
    """
    sel: Any  # Selection from generator.generate()
    board_ctx: Any  # BuildContext used for this board
    prefix: str  # filename_prefix passed to generate()


def _generate_per_board(
    *,
    board_id: str | None,
    board_env_name: str,
    pio_env: str,
    node_mother_env: str,
    live_ctx: Any,
    project_dir: Path | None,
    canonical_definitions: Any,
    staging_dir: Path,
    board_ctx: Any = None,
) -> _BoardResult:
    """Generate the artefacts for one board (mother or sister sub-env).

    Extracted from _generate_combus_action() so it can be unit-tested
    without a full SCons env. The caller is responsible for cleaning
    up the staging directory on failure.

    Args:
      board_id:             None for the mother env, the board_id
                            suffix (e.g. "mainboard") for a sister
                            sub-env.
      board_env_name:       The PlatformIO env name for this board
                            (e.g. "volvo_A60H_bruder" or
                            "volvo_A60H_bruder_mainboard").
      pio_env:              The PlatformIO env currently being built.
      node_mother_env:      The detected mother env name.
      live_ctx:             The BuildContext acquired by
                            acquire_build_context(env=env) for the
                            env actually being built.
      project_dir:          The PlatformIO project root.
      canonical_definitions: The canonised channel definitions
                            (board-specific, after ND2 diffusion).
      staging_dir:          Where to write the artefacts.
      board_ctx:            Optional pre-resolved BuildContext. If
                            provided, skip the internal resolution
                            (used by ND2 to avoid resolving the same
                            board twice — once for diffusion, once
                            for generation).

    Returns:
      _BoardResult wrapping the Selection and the BuildContext used.

    Raises:
      _BoardGenerationError on any failure (static resolution,
      generation, etc.). The message is human-readable and safe to
      log to SCons stderr.
    """
    # Late imports — _generate_per_board is only called from within
    # the SCons action (or from a test that has already imported
    # scripts.combus_builder.*).
    from scripts.combus_builder.flags import BuildContextError
    from scripts.combus_builder.generator import GeneratorError, generate
    from scripts.combus_builder.node_context import (
        NodeContextError,
        resolve_build_context_static,
    )

    # --- Resolve the BuildContext for this board -----------------------
    if board_ctx is not None:
        # Pre-resolved by the caller (ND2: resolved once for diffusion,
        # reused here for generation).
        prefix = "" if board_id is None else board_id
    elif board_id is None:
        # Mother env entry. The unprefixed artefacts (combus.h,
        # combus.cpp, combus_ids.h, ...) MUST reflect the mother
        # env's defines, not the sub-env's. If the current build IS
        # the mother env, the live `ctx` is authoritative. Otherwise
        # (we are building a sub-env), we must resolve the mother
        # env statically — reusing the live `ctx` here would silently
        # write the sub-env's defines into the unprefixed artefacts,
        # causing drift depending on which sub-env was built last.
        if pio_env == node_mother_env:
            board_ctx = live_ctx
        else:
            if project_dir is None:
                raise _BoardGenerationError(
                    f"cannot resolve mother env {node_mother_env!r} "
                    f"statically: PROJECT_DIR is not available on env."
                )
            try:
                board_ctx = resolve_build_context_static(
                    project_dir, node_mother_env,
                )
            except (NodeContextError, BuildContextError) as e:
                raise _BoardGenerationError(
                    f"static resolution failed for mother env "
                    f"{node_mother_env!r}: {e}"
                ) from e
        prefix = ""
    else:
        # Sister sub-env: resolve statically (no subprocess, no
        # compile). This is the parallel, deliberately lighter path
        # introduced by ND1.
        if project_dir is None:
            raise _BoardGenerationError(
                f"cannot resolve board env {board_env_name!r} "
                f"statically: PROJECT_DIR is not available on env."
            )
        try:
            board_ctx = resolve_build_context_static(
                project_dir, board_env_name,
            )
        except (NodeContextError, BuildContextError) as e:
            raise _BoardGenerationError(
                f"static resolution failed for board env "
                f"{board_env_name!r}: {e}"
            ) from e
        prefix = board_id

    # --- Generate ------------------------------------------------------
    try:
        sel = generate(
            canonical_definitions,
            board_ctx,
            staging_dir,
            filename_prefix=prefix,
        )
    except GeneratorError as e:
        raise _BoardGenerationError(
            f"generation failed for board {board_id!r} "
            f"(env {board_env_name!r}): {e}"
        ) from e
    except Exception as e:
        raise _BoardGenerationError(
            f"unexpected error during generation for board "
            f"{board_id!r} (env {board_env_name!r}): {e}"
        ) from e

    return _BoardResult(sel=sel, board_ctx=board_ctx, prefix=prefix)


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

    ND1 (multiboard):
      - Detects whether the current env is a sub-env "board" of a node.
      - In node mode, generates per-board artefacts (one set per
        board_id, with a per-board filename_prefix).
      - In legacy mode, behaviour is strictly identical to pre-ND1.
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
        from scripts.combus_builder.machine_type import (
            AmbiguousMachineTypeError,
            detect_machine_type,
        )
        from scripts.combus_builder.node_context import (
            NodeContextError,
            parse_env_hierarchy,
            resolve_build_context_static,
        )
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

    # --- 4b. ND8: detect machine type for the .cb discovery filter ------
    # The machine type is derived from the LIVE BuildContext. In legacy
    # single-board mode this is the only context. In node mode the
    # boards of a node all share the same machine type (verified later
    # at step 6c); the live ctx is either the mother's (when
    # pio_env == node.mother_env) or a sub-env's, and in both cases
    # the MACHINE_TYPE_* flag is identical across boards of the same
    # node (ND1 invariant). Boards that resolve to a different
    # machine type are detected at step 6c below and raise a build
    # error there.
    try:
        machine_type = detect_machine_type(ctx)
    except AmbiguousMachineTypeError as e:
        sys.stderr.write(f"[combus_scons_action] FATAL: {e}\n")
        return 1
    if machine_type is not None:
        print(
            f"[combus_scons_action] ND8 machine-type filter active: "
            f"machine_type={machine_type!r} (excludes "
            f"core/config/machines/<other>/)"
        )

    # --- 5. Discover + parse (ND8: with machine_type filter) ------------
    try:
        _, _, parsed = discover_and_parse(env=env, machine_type=machine_type)
    except ParseError as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: YAML parse failed: {e}\n"        )
        return 1

    # --- 6. Canonise ----------------------------------------------------
    try:
        canon = canonize(parsed)
    except Exception as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: canonisation failed: {e}\n"
        )
        return 1

    # --- 6b. ND1: detect node mode ---------------------------------------
    # parse_env_hierarchy() is a pure function over the env name and
    # the list of env names declared in platformio.ini. It does NOT
    # touch the filesystem beyond reading platformio.ini (via
    # _read_all_env_names), and it does NOT compile anything.
    project_dir = env.get("PROJECT_DIR")
    pio_env = env.get("PIOENV") or "default"
    all_env_names = _read_all_env_names(Path(project_dir)) if project_dir else []
    try:
        node = parse_env_hierarchy(pio_env, all_env_names)
    except NodeContextError as e:
        sys.stderr.write(
            f"[combus_scons_action] FATAL: node detection failed: {e}\n"
        )
        return 1

    # Collect the list of (board_id, env_name) for this node.
    boards = _collect_node_boards(node.mother_env, all_env_names)

    if node.is_node:
        print(
            f"[combus_scons_action] ND1 node mode detected: "
            f"mother_env={node.mother_env!r}, "
            f"boards={[b[0] for b in boards]}"
        )

    # --- 6c. ND8: verify all boards of a node agree on machine type ------
    # In node mode, every board's BuildContext must resolve to the
    # same machine type as the live ctx (or to None). If any board
    # disagrees, the build fails loudly. This check is what makes the
    # discovery filter safe: a divergence detected here means two
    # boards of the node would have received different .cb pools, so
    # the cross-board diffusion (ND2) would mix incompatible
    # contracts.
    if node.is_node and machine_type is not None:
        try:
            for _bid, board_env_name in boards:
                if _bid is None and pio_env == node.mother_env:
                    # Mother env: already verified via the live ctx.
                    continue
                if not project_dir:
                    sys.stderr.write(
                        f"[combus_scons_action] FATAL: cannot resolve "
                        f"board env {board_env_name!r} for machine type "
                        f"consistency check: PROJECT_DIR is not available "
                        f"on env.\n"                    )
                    return 1
                try:
                    bctx = resolve_build_context_static(
                        Path(project_dir), board_env_name,
                    )
                except (NodeContextError, BuildContextError) as e:
                    sys.stderr.write(
                        f"[combus_scons_action] FATAL: static "
                        f"resolution failed for board env "
                        f"{board_env_name!r}: {e}\n"                    )
                    return 1
                board_mt = detect_machine_type(bctx)
                if board_mt is not None and board_mt != machine_type:
                    sys.stderr.write(
                        f"[combus_scons_action] FATAL: board env "
                        f"{board_env_name!r} resolves to machine type "
                        f"{board_mt!r}, which differs from the active "
                        f"machine type {machine_type!r} (from the live "
                        f"ctx). All boards of a node must share the "
                        f"same MACHINE_TYPE_*. Check platformio.ini.\n"                    )
                    return 1
        except AmbiguousMachineTypeError as e:
            sys.stderr.write(f"[combus_scons_action] FATAL: {e}\n")
            return 1

    # --- 7. Generate (atomic) -------------------------------------------
    staging_dir = out_dir.with_suffix(out_dir.suffix + ".new")
    if staging_dir.exists():
        shutil.rmtree(staging_dir)

    if not node.is_node:
        # --- 7a. Legacy single-board mode (ND1: strictly unchanged) -----
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
        print(
            f"[combus_scons_action] generated "
            f"combus ({sel.full.ch_count} ch, WIRE_END={sel.full.wire_end}), "
            f"combus_local ({sel.local.ch_count} ch), "
            f"combus_remote ({sel.remote.ch_count} ch) "
            f"-> {staging_dir}"
        )
    else:
        # --- 7b. ND1+ND2 node mode: one generate() call per board -------
        # ND2: LOCAL channels are diffused across boards BEFORE
        # canonize(). This requires resolving every board's
        # BuildContext first (so diffusion knows which declarations
        # are active for which board), then calling
        # diffuse_local_channels(), then calling canonize() PER
        # BOARD on the diffused data.
        #
        # The .cb pool is repo-wide and board-agnostic
        # (discover_and_parse ran once above), but the canonised
        # channel list is now PER BOARD (it depends on which LOCAL
        # channels are active for that board after diffusion).
        from scripts.combus_builder.diffusion import (
            DiffusionError,
            diffuse_local_channels,
        )
        from scripts.combus_builder.node_context import NodeContext

        # --- 7b.1. Resolve all boards' BuildContexts (needed for diffusion) ---
        node_boards_ctx: dict[str | None, Any] = {}
        for board_id, board_env_name in boards:
            if board_id is None:
                # Mother env: use live ctx if building the mother,
                # otherwise resolve statically.
                if pio_env == node.mother_env:
                    node_boards_ctx[board_id] = ctx
                else:
                    if not project_dir:
                        sys.stderr.write(
                            f"[combus_scons_action] FATAL: cannot resolve "
                            f"mother env {node.mother_env!r} statically: "
                            f"PROJECT_DIR is not available on env.\n"
                        )
                        return 1
                    try:
                        node_boards_ctx[board_id] = resolve_build_context_static(
                            Path(project_dir), node.mother_env,
                        )
                    except (NodeContextError, BuildContextError) as e:
                        sys.stderr.write(
                            f"[combus_scons_action] FATAL: static resolution "
                            f"failed for mother env {node.mother_env!r}: {e}\n"
                        )
                        return 1
            else:
                # Sister sub-env: resolve statically.
                if not project_dir:
                    sys.stderr.write(
                        f"[combus_scons_action] FATAL: cannot resolve board "
                        f"env {board_env_name!r} statically: PROJECT_DIR is "
                        f"not available on env.\n"
                    )
                    return 1
                try:
                    node_boards_ctx[board_id] = resolve_build_context_static(
                        Path(project_dir), board_env_name,
                    )
                except (NodeContextError, BuildContextError) as e:
                    sys.stderr.write(
                        f"[combus_scons_action] FATAL: static resolution "
                        f"failed for board env {board_env_name!r}: {e}\n"
                    )
                    return 1

        node_resolved = NodeContext(
            mother_env=node.mother_env,
            boards=node_boards_ctx,
            is_node=True,
        )

        # --- 7b.2. Diffuse LOCAL channels across boards ---
        try:
            diffused = diffuse_local_channels(parsed, node_resolved)
        except DiffusionError as e:
            if staging_dir.exists():
                shutil.rmtree(staging_dir)
            sys.stderr.write(
                f"[combus_scons_action] FATAL: diffusion failed: {e}\n"
            )
            return 1

        # --- 7b.3. Per-board canonize() + generate() ---
        for board_id, board_env_name in boards:
            board_parsed = diffused.get(board_id)
            if board_parsed is None:
                sys.stderr.write(
                    f"[combus_scons_action] FATAL: diffusion did not produce "
                    f"a resolved list for board {board_id!r}\n"
                )
                if staging_dir.exists():
                    shutil.rmtree(staging_dir)
                return 1
            try:
                board_canon = canonize(board_parsed)
            except Exception as e:
                if staging_dir.exists():
                    shutil.rmtree(staging_dir)
                sys.stderr.write(
                    f"[combus_scons_action] FATAL: canonisation failed for "
                    f"board {board_id!r}: {e}\n"
                )
                return 1
            try:
                result = _generate_per_board(
                    board_id=board_id,
                    board_env_name=board_env_name,
                    pio_env=pio_env,
                    node_mother_env=node.mother_env,
                    live_ctx=ctx,
                    project_dir=Path(project_dir) if project_dir else None,
                    canonical_definitions=board_canon.canonical_definitions,
                    staging_dir=staging_dir,
                    board_ctx=node_boards_ctx[board_id],
                )
            except _BoardGenerationError as e:
                if staging_dir.exists():
                    shutil.rmtree(staging_dir)
                sys.stderr.write(f"[combus_scons_action] FATAL: {e}\n")
                return 1
            sel = result.sel
            board_ctx = result.board_ctx
            print(
                f"[combus_scons_action] generated board {board_id!r} "
                f"(env {board_env_name!r}, "
                f"source={board_ctx.cppdefines_source}, "
                f"{len(board_ctx.defines)} defines, "
                f"{len(board_ctx.defines_with_value)} valued): "
                f"combus ({sel.full.ch_count} ch, WIRE_END={sel.full.wire_end}), "
                f"combus_local ({sel.local.ch_count} ch), "
                f"combus_remote ({sel.remote.ch_count} ch) "
                f"-> {staging_dir}"
            )

    # --- 8. Pre-publish completeness check ------------------------------
    expected = _expected_artifacts_for_node(boards, node.is_node)
    try:
        _check_staging_complete(staging_dir, expected)
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
        f"[combus_scons_action] published {len(expected)} artefact(s) "
        f"-> {out_dir}"
    )
    return 0


def _skip_action(target, source, env):
    """No-op action for COMBUS_BUILDER_SKIP=1 (artefacts must already be present)."""
    out_dir = _default_out_dir(env)
    # In skip mode we only check the legacy artefacts (the ones the
    # rest of the codebase includes unconditionally). Per-board
    # artefacts are not required for the build to succeed in skip mode
    # — they are only needed when the corresponding board is actually
    # being built.
    missing = _missing_artefacts(out_dir, list(EXPECTED_ARTIFACTS))
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
    # ND1: in node mode, the targets list is extended with the per-board
    # artefacts. We compute it eagerly here (at registration time) so
    # SCons knows the full set of expected outputs up front. The
    # detection itself is cheap (a regex over platformio.ini).
    all_env_names = _read_all_env_names(Path(project_dir)) if project_dir else []
    targets = [str(out_dir / name) for name in EXPECTED_ARTIFACTS]
    try:
        from scripts.combus_builder.node_context import parse_env_hierarchy
        node = parse_env_hierarchy(pio_env, all_env_names)
        if node.is_node:
            boards = _collect_node_boards(node.mother_env, all_env_names)
            for board_id, _env_name in boards:
                if board_id is None:
                    continue
                prefix = board_id
                targets.extend([
                    str(out_dir / f"combus_{prefix}_ids.h"),
                    str(out_dir / f"combus_{prefix}.h"),
                    str(out_dir / f"combus_{prefix}.cpp"),
                    str(out_dir / f"combus_local_{prefix}_ids.h"),
                    str(out_dir / f"combus_remote_{prefix}_ids.h"),
                    str(out_dir / f"combus_{prefix}_md5.h"),
                    str(out_dir / f"combus_local_{prefix}_md5.h"),
                    str(out_dir / f"combus_remote_{prefix}_md5.h"),
                ])
    except Exception:
        # If node detection fails at registration time, fall back to
        # legacy targets. The action will re-detect and fail loudly if
        # the situation is genuinely broken.
        pass

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
