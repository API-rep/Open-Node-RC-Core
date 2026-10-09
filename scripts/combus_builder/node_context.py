#!/usr/bin/env python3
"""
combus_builder/node_context.py — ND1: formalise the "node" dimension.

Scope (ND1 only):
  - Detect whether the current PlatformIO env is a sub-env "board" of a
    node (e.g. [env:volvo_A60H_bruder_mainboard] is a sub-env of the
    mother env [env:volvo_A60H_bruder]).
  - Resolve the BuildContext of every sister sub-env WITHOUT compiling
    them, using a static path (no `pio` subprocess, no SCons).
  - Provide a NodeContext dataclass that the orchestrator (combus_scons_hook)
    consumes to drive per-board generation.

Out of scope (ND1):
  - Channel diffusion between boards (ND2).
  - Direction conflict detection (ND3).
  - MD5 ONE_WAY canonisation (ND7).
  - merge_channels() in canon/channels.py is intentionally untouched:
    a duplicated id between two .cb files of different boards still
    raises ChannelConflictError. This is a transient behaviour, not a
    bug of ND1.

Detection rule (ND1):
  - An env_name is a sub-env "board" of a node iff there exists another
    env in `all_env_names` whose name is a strict prefix of env_name
    followed by '_' (i.e. `<mother>_<board_id>`).
  - The mother env is the longest such prefix (defensive: in case of
    nested prefixes, the closest ancestor wins).
  - If env_name is itself a mother env (i.e. it has at least one
    sub-env `<env_name>_<board_id>` in `all_env_names`), it is also
    treated as a node (the user is building the mother directly).
  - If neither condition holds, env_name is treated as a "base" env
    (legacy single-board mode, board_id = None).

BuildContext resolution (ND1):
  - resolve_build_context_static() uses
    platformio.project.config.ProjectConfig to read the resolved
    build_flags of env_name (extends= and ${...} interpolation are
    handled by PlatformIO itself).
  - The resolved build_flags string is then parsed via the existing
    _extract_from_build_flags_string() helper in flags.py — no new
    parsing logic.
  - acquire_build_context() in flags.py is NOT touched: it remains the
    single source of truth for the env actually being built (the
    mono-board / legacy case). resolve_build_context_static is a
    parallel, deliberately lighter path used only for sister boards
    that we do NOT compile in this process.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .flags import (
    BuildContext,
    BuildContextError,
    _extract_from_build_flags_string,
    resolve_buildroot,
)


# =============================================================================
# ERRORS
# =============================================================================

class NodeContextError(Exception):
    """Raised when the node hierarchy cannot be resolved."""


# =============================================================================
# DATA STRUCTURE
# =============================================================================

@dataclass(frozen=True)
class NodeContext:
    """
    The "node" view of the current PlatformIO build.

    mother_env : name of the mother env (e.g. "volvo_A60H_bruder").
                 Always set, even in legacy single-board mode (in which
                 case it equals env_name).
    boards     : dict[board_id -> BuildContext]. In legacy mode, contains
                 a single entry with board_id = None. In node mode,
                 contains one entry per detected sub-env (board_id is
                 the suffix after '<mother>_', e.g. "mainboard",
                 "soundboard").
    is_node    : True iff at least one sister sub-env was detected.
                 False in legacy single-board mode.
    """

    mother_env: str
    boards: dict[str | None, BuildContext] = field(default_factory=dict)
    is_node: bool = False

    def board_ids(self) -> list[str | None]:
        """Return the list of board_ids (None for legacy)."""
        return list(self.boards.keys())


# =============================================================================
# HIERARCHY DETECTION
# =============================================================================

def parse_env_hierarchy(
    env_name: str,
    all_env_names: list[str],
) -> NodeContext:
    """
    Determine whether `env_name` is a sub-env "board" of a node.

    Detection rule:
      - An env_name is a sub-env "board" iff there exists another env
        in `all_env_names` whose name is a strict prefix of env_name
        followed by '_' (i.e. `<mother>_<board_id>`).
      - The mother env is the LONGEST such prefix (closest ancestor
        wins in case of nested prefixes).
      - If env_name is itself a mother env (i.e. it has at least one
        sub-env `<env_name>_<board_id>` in `all_env_names`), it is
        also treated as a node (the user is building the mother
        directly).
      - If neither condition holds, env_name is treated as a "base"
        env (legacy single-board mode, board_id = None).

    Args:
      env_name:      the PlatformIO env currently being built.
      all_env_names: the full list of env names declared in
                     platformio.ini (or any superset thereof).

    Returns:
      NodeContext with:
        - mother_env = env_name (legacy) or the detected mother.
        - boards     = {} (caller is expected to populate it via
                       resolve_build_context_static()).
        - is_node    = True iff at least one sister sub-env was found.

    NOTE: this function does NOT resolve BuildContexts. The caller
    (orchestrator) is responsible for calling
    resolve_build_context_static() for each board it wants to generate.
    """
    if not env_name:
        raise NodeContextError("env_name must be a non-empty string")

    # Step 1: find the mother env.
    # Case A: env_name is a sub-env (there exists a longer env that is
    # a strict prefix of env_name followed by '_').
    candidates: list[str] = []
    for other in all_env_names:
        if other == env_name:
            continue
        prefix = other + "_"
        if env_name.startswith(prefix):
            candidates.append(other)

    if candidates:
        # env_name is a sub-env: longest prefix wins.
        mother = max(candidates, key=len)
    else:
        # Case B: env_name might be the mother env itself. Check if it
        # has at least one sub-env in all_env_names.
        has_sub_envs = any(
            other != env_name and other.startswith(env_name + "_")
            for other in all_env_names
        )
        if not has_sub_envs:
            # Legacy single-board mode.
            return NodeContext(
                mother_env=env_name,
                boards={},
                is_node=False,
            )
        # env_name is the mother env itself.
        mother = env_name

    # Step 2: collect sister sub-envs (every env whose name starts with
    # `<mother>_` AND is not the mother itself).
    sister_boards: list[tuple[str, str]] = []
    for other in all_env_names:
        if other == mother:
            continue
        if not other.startswith(mother + "_"):
            continue
        board_id = other[len(mother) + 1:]
        if not board_id:
            # Defensive: an env named exactly "<mother>_" (trailing
            # underscore, empty board_id) is malformed; skip it.
            continue
        sister_boards.append((board_id, other))

    return NodeContext(
        mother_env=mother,
        boards={},  # populated by the orchestrator
        is_node=True,
    )


# =============================================================================
# STATIC BUILD-CONTEXT RESOLUTION
# =============================================================================

def _load_project_config(project_root: Path):
    """
    Lazy import of platformio.project.config.ProjectConfig.

    PlatformIO is an optional dependency for combus_builder: the
    standalone CLI mode (combus_builder.py) does not require it, and
    the unit tests in tests/ do not require it. We only need it for
    the static resolution path used by ND1.

    Raises NodeContextError if PlatformIO is not importable.
    """
    try:
        from platformio.project.config import ProjectConfig  # type: ignore
    except ImportError as e:
        raise NodeContextError(
            "resolve_build_context_static() requires the `platformio` "
            "Python package (it uses platformio.project.config.ProjectConfig "
            "to resolve build_flags with extends= and ${...} interpolation). "
            "Install PlatformIO CLI / its Python package, or run this code "
            "path from within a PlatformIO build context where the package "
            "is already on PYTHONPATH."
        ) from e
    return ProjectConfig


def _items_to_dict(items_raw: Any) -> dict[str, Any]:
    """Normalise the output of ProjectConfig.items(env=...) to a dict.

    Why this function exists (ND1 fix, 2026-09-13):
      The original implementation called `config.options(env=env_name)`,
      which in PlatformIO 6.x returns a list of bare key names (e.g.
      `['build_flags', 'platform']`) — NOT a dict and NOT a list of
      (key, value) tuples. Calling `.get('build_flags', '')` on that
      list raised `AttributeError: 'list' object has no attribute 'get'`.

      The correct API for retrieving resolved (key, value) pairs is
      `config.items(env=env_name)`, which returns a list of
      `(key, value)` tuples where `value` may itself be a list (for
      multi-line options like `build_flags`). This function normalises
      that list-of-tuples to a plain dict so the rest of the code can
      use a uniform `.get(key, default)` lookup.

      We also accept a dict directly (older PlatformIO versions, or
      future API changes) and raise NodeContextError on any other
      shape so the failure mode is explicit rather than silent.
    """
    if isinstance(items_raw, dict):
        return items_raw
    if isinstance(items_raw, (list, tuple)):
        options: dict[str, Any] = {}
        for item in items_raw:
            if isinstance(item, (list, tuple)) and len(item) >= 2:
                options[item[0]] = item[1]
            elif isinstance(item, str):
                # Bare key with no value.
                options[item] = ""
        return options
    raise NodeContextError(
        f"unexpected items type from ProjectConfig.items(): "
        f"{type(items_raw).__name__}"
    )


def resolve_build_context_static(
    project_root: Path,
    env_name: str,
) -> BuildContext:
    """
    Resolve the BuildContext of `env_name` WITHOUT invoking a subprocess
    or compiling anything.

    Uses platformio.project.config.ProjectConfig to read the resolved
    build_flags of env_name (extends= and ${...} interpolation are
    handled by PlatformIO itself). The resolved build_flags string is
    then parsed via the existing _extract_from_build_flags_string()
    helper in flags.py — no new parsing logic.

    This is a deliberately lighter path than acquire_build_context():
      - It does NOT consult env["CPPDEFINES"] (no SCons env available
        outside a real build).
      - It does NOT call `pio` as a subprocess.
      - It does NOT compile anything.

    It is used only for sister boards that we do NOT compile in this
    process (ND1). The env actually being built still goes through
    acquire_build_context() in flags.py — that path is untouched.

    Args:
      project_root: the PlatformIO project root (where platformio.ini lives).
      env_name:     the env to resolve (e.g. "volvo_A60H_bruder_mainboard").

    Returns:
      BuildContext with:
        - buildroot = <project_root>/src (same convention as flags.py).
        - defines / defines_with_value parsed from the resolved build_flags.
        - cppdefines_source = "static" (new label, distinct from
          "env" / "build_flags" / "override" / "none").

    Raises:
      NodeContextError if PlatformIO is not importable, the project
      config cannot be loaded, or env_name is not declared.
      BuildContextError if the resolved build_flags string yields zero
      defines (we refuse to generate with an incomplete context, same
      policy as acquire_build_context()).
    """
    project_root = Path(project_root).resolve()
    ini_path = project_root / "platformio.ini"
    if not ini_path.is_file():
        raise NodeContextError(
            f"platformio.ini not found at {ini_path}; cannot resolve "
            f"build context statically for env {env_name!r}."
        )

    ProjectConfig = _load_project_config(project_root)

    try:
        config = ProjectConfig(str(ini_path))
    except Exception as e:
        raise NodeContextError(
            f"failed to load ProjectConfig from {ini_path}: {e}"
        ) from e

    # ProjectConfig.items(env=...) returns the resolved (key, value)
    # pairs for the given env, with extends= and ${...} interpolation
    # already applied by PlatformIO itself.
    #
    # NOTE: in PlatformIO 6.x, items() returns a list of (key, value)
    # tuples (not a dict). We normalise it to a dict for the lookup
    # via _items_to_dict() — see that function's docstring for why
    # options() was the wrong API.
    try:
        items_raw = config.items(env=env_name)
    except Exception as e:
        raise NodeContextError(
            f"failed to resolve items for env {env_name!r}: {e}"
        ) from e

    options = _items_to_dict(items_raw)

    raw_build_flags = options.get("build_flags", "")
    if isinstance(raw_build_flags, (list, tuple)):
        text = " ".join(str(x) for x in raw_build_flags)
    else:
        text = str(raw_build_flags or "")

    defines, values = _extract_from_build_flags_string(text)

    if not defines and not values:
        raise BuildContextError(
            f"resolve_build_context_static: env {env_name!r} resolved "
            f"to an empty build_flags string; refusing to build a "
            f"BuildContext with no defines (would generate an empty "
            f"combus view)."
        )

    # buildroot follows the same convention as flags.py: <project_root>/src.
    # We do NOT honour PROJECT_SRC_DIR here because we are outside a
    # SCons env — the canonical PlatformIO convention is that src_dir
    # is <project_root>/src unless explicitly overridden in
    # platformio.ini (which we don't try to introspect here; the
    # default is sufficient for ND1's purpose).
    buildroot = resolve_buildroot(env=None, project_root=project_root)

    return BuildContext(
        buildroot=buildroot,
        defines=frozenset(defines),
        defines_with_value=dict(values),
        cppdefines_source="static",
        raw_cppdefines_sample=(text[:200] + "...") if len(text) > 200 else text,
    )


# =============================================================================
# CLI / diagnostic
# =============================================================================

def main(env_name: str, all_env_names: list[str]) -> int:
    """CLI entry point: print the detected hierarchy and return 0/1."""
    try:
        node = parse_env_hierarchy(env_name, all_env_names)
    except NodeContextError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1

    print(f"[combus_builder] env_name      = {env_name}")
    print(f"[combus_builder] mother_env    = {node.mother_env}")
    print(f"[combus_builder] is_node       = {node.is_node}")
    print(f"[combus_builder] boards (raw)  = {node.board_ids()}")
    return 0


if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser(description="Detect node hierarchy for a PlatformIO env.")
    p.add_argument("env_name", help="The PlatformIO env to inspect.")
    p.add_argument(
        "--all-envs",
        action="append",
        default=[],
        help="All env names declared in platformio.ini. May be repeated.",
    )
    args = p.parse_args()
    sys.exit(main(args.env_name, args.all_envs))
