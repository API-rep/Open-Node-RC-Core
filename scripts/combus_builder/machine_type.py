#!/usr/bin/env python3
"""
combus_builder/machine_type.py — Detect the active machine-type from a BuildContext.

Scope (correctif 1/3, ND8):

  A .cb file under `core/config/machines/<type>/` belongs to one machine
  type (e.g. dumper_truck, excavator, loader). The build context, in a
  well-formed PlatformIO env, defines exactly ONE `MACHINE_TYPE_*` flag
  (e.g. `-D MACHINE_TYPE_DUMPER_TRUCK`). `.cb` files under
  `core/config/machines/<other_type>/` must be EXCLUDED from the build
  context that targets <type>.

  This module is the single source of truth for that detection. It is
  REUSED wherever a machine-type decision is needed (currently: the
  `.cb` discovery filter, see parser.filter_by_machine_type). Future
  consumers (e.g. an MD5 prelude, a build report) can import the same
  helper — no second implementation.

Design choices:

  - The detector reads a BuildContext. The flags we inspect
    (`MACHINE_TYPE_*`) are part of the build contract: the same
    BuildContext is used everywhere a machine-type decision is needed,
    so all consumers agree.
  - Detection is by EXACT name match on the flag (`MACHINE_TYPE_DUMPER_TRUCK`,
    `MACHINE_TYPE_EXCAVATOR`, ...). We do NOT pattern-match on the
    directory name: a directory under `core/config/machines/<X>/` is
    eligible to be included iff the active flag is `MACHINE_TYPE_<X>`.
  - The flag vocabulary is centralised in MACHINE_TYPE_FLAGS (a
    frozenset). Adding a new machine type = adding the flag here AND
    creating the corresponding `core/config/machines/<type>/` folder.
  - The mapping `flag -> directory_segment` is `MACHINE_TYPE_<X>` -> `<x>`.
    This is a simple lowercase transform; if a future flag needs a
    non-trivial mapping, extend MACHINE_TYPE_DIR_OVERRIDES below.

If the context has zero or more than one `MACHINE_TYPE_*` flag, this is
treated as "no machine type active" (returns None). The rationale:
non-machine builds (sound_node, remotes) do not define any
MACHINE_TYPE_*; pathological multi-flag contexts are user error and
should NOT silently pick a winner (the filter would then drop .cb
files of a legitimate type).
"""
from __future__ import annotations

from typing import Iterable


# =============================================================================
# ERRORS
# =============================================================================

class AmbiguousMachineTypeError(Exception):
    """
    Raised when a BuildContext defines MORE THAN ONE MACHINE_TYPE_* flag.

    Per the module docstring: we do not silently pick a winner. A build
    that defines two MACHINE_TYPE_* flags is a user error (typically a
    copy-paste in platformio.ini) and must be flagged loudly.
    """

    def __init__(self, flags: list[str]):
        self.flags = sorted(flags)
        super().__init__(
            "BuildContext defines multiple MACHINE_TYPE_* flags: "
            f"{self.flags!r}. Exactly one is expected. "
            "Check the [env:...] build_flags in platformio.ini."
        )


# =============================================================================
# VOCABULARY
# =============================================================================

# All recognised MACHINE_TYPE_* flags. Each entry maps 1:1 to a directory
# under core/config/machines/<type>/. The directory segment is the
# lowercase form of the flag suffix (see _flag_to_dir_segment).
MACHINE_TYPE_FLAGS: frozenset[str] = frozenset({
    "MACHINE_TYPE_DUMPER_TRUCK",
    "MACHINE_TYPE_EXCAVATOR",
    "MACHINE_TYPE_LOADER",
    # Fictitious types of the tests of the links between nodes (link rework
    # 3.1): listed so that a real machine excludes them.
    "MACHINE_TYPE_TEST_TRACTOR",
    "MACHINE_TYPE_TEST_TRAILER",
})


def _flag_to_dir_segment(flag: str) -> str:
    """
    Convert a MACHINE_TYPE_<X> flag to the corresponding directory segment.

    Convention: directory segments are the lowercase form of the
    MACHINE_TYPE suffix (e.g. MACHINE_TYPE_DUMPER_TRUCK -> dumper_truck).

    This is intentionally a simple deterministic transform: it keeps the
    flag vocabulary and the directory layout coupled by construction, and
    makes "the directory <X> corresponds to the flag MACHINE_TYPE_<X>"
    a structural invariant of the codebase.
    """
    prefix = "MACHINE_TYPE_"
    if not flag.startswith(prefix):
        raise ValueError(
            f"flag {flag!r} does not start with the expected prefix {prefix!r}"
        )
    return flag[len(prefix):].lower()


def all_machine_type_dirs() -> list[str]:
    """Return all directory segments that could host machine-type .cb files.

    Useful for the .cb discovery filter: it lists every segment we
    should consider for the EXCLUSION rule, irrespective of which one is
    currently active.
    """
    return [_flag_to_dir_segment(f) for f in sorted(MACHINE_TYPE_FLAGS)]


# =============================================================================
# DETECTION
# =============================================================================

def detect_machine_type(ctx) -> str | None:
    """
    Detect the active machine type from a BuildContext.

    Returns:
        - the directory segment (e.g. "dumper_truck") if exactly one
          MACHINE_TYPE_* flag is present in ctx (with or without value).
        - None if no MACHINE_TYPE_* flag is present (non-machine build,
          e.g. sound_node, remote).

    Raises:
        AmbiguousMachineTypeError if more than one MACHINE_TYPE_* flag
        is present. A build that resolves to two machine types is
        user error — the filter MUST refuse to silently pick one.
    """
    # Iterate over both value-less and valued defines: MACHINE_TYPE_X
    # may be declared as `-D MACHINE_TYPE_X` (value-less, in ctx.defines)
    # or as `-D MACHINE_TYPE_X=1` (valued, in ctx.defines_with_value).
    # BuildContext has() abstracts over both — use it.
    active: list[str] = []
    for flag in MACHINE_TYPE_FLAGS:
        if ctx.has(flag):
            active.append(flag)

    if len(active) == 0:
        return None
    if len(active) > 1:
        raise AmbiguousMachineTypeError(active)
    return _flag_to_dir_segment(active[0])


def machine_type_dir_for(ctx) -> str | None:
    """
    Convenience wrapper around detect_machine_type().

    Identical behaviour, but the function name mirrors the directory
    semantics. Kept separate so the call sites read clearly:
    `machine_type_dir_for(ctx)` is more intention-revealing than
    `detect_machine_type(ctx)` in filter code.
    """
    return detect_machine_type(ctx)


# =============================================================================
# FILE FILTER (pure helper, no I/O)
# =============================================================================

# Marker: a path belongs to a machine-type-specific directory iff it
# contains a segment of the form `core/config/machines/<X>/` for some
# recognised X. The marker is built as a tuple for tuple-in-path
# matching; we use the path parts (not the raw string) to avoid false
# positives on names like `core/config/something_with_dumper_truck_in_it/`.
_MACHINE_TYPE_DIR_PREFIX = ("core", "config", "machines")


def is_machine_type_path(path, machine_type_dirs: Iterable[str]) -> bool:
    """
    Return True iff `path` is under one of the given machine-type dirs.

    `path` may be a pathlib.Path or any object with `.parts` (a tuple of
    path components). The function is deliberately agnostic of the path
    type so it can be unit-tested without filesystem I/O.

    A path is considered "under" <X> iff its parts contain the
    contiguous sequence ("core", "config", "machines", X). The check is
    case-sensitive on path segments (the codebase uses lowercase
    directory names uniformly — see `MACHINE_TYPE_FLAGS`).
    """
    try:
        parts = tuple(path.parts)
    except AttributeError:
        # Defensive: allow plain tuples/strings as a fallback (handy
        # for tests that build paths from scratch without a real
        # filesystem). A string is treated as a single "part".
        if isinstance(path, str):
            parts = (path,)
        else:
            parts = tuple(path)

    machine_type_set = set(machine_type_dirs)
    # Look for the contiguous (core, config, machines, X) marker.
    for i in range(len(parts) - 3):
        if (
            parts[i] == "core"
            and parts[i + 1] == "config"
            and parts[i + 2] == "machines"
            and parts[i + 3] in machine_type_set
        ):
            return True
    return False


def filter_files_by_machine_type(files, machine_type: str | None) -> list:
    """
    Filter a list of file paths to keep only those compatible with the
    given active machine type.

    Behaviour:
      - machine_type is None (non-machine build, e.g. a remote): KEEP the
        generic files, EXCLUDE every machine type profile.  Link rework
        3.1: the profiles of node types that are peers share channel names
        on purpose (BRAKE_BUS in a tractor and in its trailer); loaded
        together they would clash.  A node without type reads the machine
        types as peers only (links.load_type_base_channels), never in its
        own ComBus.  Before 2026-10-25 every file was kept.
      - machine_type is a known directory segment (e.g. "dumper_truck"):
        KEEP every file EXCEPT those under
        core/config/machines/<other_type>/. Generic files
        (e.g. core/system/failsafe/failsafe.cb) are always kept.

    The returned list preserves the input order.

    Args:
      files: iterable of path-like objects.
      machine_type: the active type (None, or a directory segment like
                    "dumper_truck").

    Returns:
      A new list (filtered copy).
    """
    files = list(files)
    if machine_type is None:
        # No machine type active: no machine type profile at all (link
        # rework 3.1, see the docstring).
        all_dirs = [_flag_to_dir_segment(f) for f in MACHINE_TYPE_FLAGS]
        return [p for p in files if not is_machine_type_path(p, all_dirs)]

    # Active machine type known: exclude paths under
    # core/config/machines/<X>/ for X != machine_type.
    machine_type_set = set(MACHINE_TYPE_FLAGS)
    # We need the directory segments corresponding to all known
    # MACHINE_TYPE_* flags, so we can build the "other types" set.
    all_dirs = [_flag_to_dir_segment(f) for f in machine_type_set]
    other_dirs = [d for d in all_dirs if d != machine_type]

    kept: list = []
    for p in files:
        if is_machine_type_path(p, other_dirs):
            # Path is under a NON-active machine-type dir: exclude.
            continue
        kept.append(p)
    return kept
