#!/usr/bin/env python3
"""
combus_builder/canon.py — A5: Canonisation (fusion + tri global).

Scope (A5 only):
  - Take the raw documents collected by A3 (parser.py).
  - Select the `channels:` section from each document (regardless of
    the file's extension — A3 does not impose a 1:1 mapping between
    extension and section).
  - Validate each channel definition semantically (id, type, scope,
    theme, direction, requires).
  - Detect ID collisions globally (across all types).
  - Merge into a single active_definitions list.
  - Apply the canonical sort key (scope, type, theme, id).
  - Return canonical_definitions.

A5 does NOT:
  - Discover files (A3 — parser.py).
  - Acquire build context / CPPDEFINES (A4 — flags.py).
  - Generate C++ headers (A6 — generator.py).
  - Compute MD5 (A7 — md5.py).
  - Validate processors (Phase C).
  - Resolve `requires` against CPPDEFINES (A8 — coherence.py).

Contract with A3:
  A3 returns a list of (path, type_label, raw_dict) tuples. A5 reads
  the `channels:` key from each raw_dict. A5 does NOT depend on
  type_label (the extension-derived label) for routing — a `.cbch`
  file with a `channels:` section is treated the same as a `.cb` file
  with a `channels:` section. This is the explicit "extension does
  not determine content" rule.

Contract with A4:
  A5 does NOT consume BuildContext. The `requires` field is preserved
  as-is in the canonical output; resolution against CPPDEFINES is
  A8's job. A5 only validates the SHAPE of `requires` (list of
  strings), not its semantic activation.

Validation rules implemented (per doc §3, §4, §10, §18):
  - id: required, non-empty string, unique globally.
  - type: required, in {analog, digital}.
  - scope: required, in {REMOTE, LOCAL, SYSTEM}.
  - theme: required, non-empty string (no closed enum).
  - direction: required for LOCAL/REMOTE, optional for SYSTEM
    (defaults to "none" when scope=SYSTEM and direction is absent).
  - requires: optional, list of non-empty strings.
  - infoName: optional, string.
  - module: optional, string (top-level metadata, not per-channel).

Validation rules NOT implemented (ambiguities flagged in doc §10):
  - scope × type × theme combinations beyond the per-axis enums.
  - Any "FAILSAFE must be REMOTE" rule (the doc says REMOTE but the
    actual .cb files use LOCAL — flagged for resolution).

Canonical sort key:
  (scope, type, theme, id)
  in this exact order, ascending, with the following tie-breakers:
  - scope: SYSTEM < LOCAL < REMOTE (alphabetical, matches the doc's
    enumeration order).
  - type: analog < digital (alphabetical).
  - theme: alphabetical.
  - id: alphabetical.

  The sort is stable: two equal keys preserve their input order, but
  since the key is fully specified, equal keys are by definition
  duplicates (which A5 rejects before sorting).

Determinism:
  The output of canonize() depends ONLY on the set of (path, raw_dict)
  inputs, not on their order. Two runs with the same set of inputs
  produce byte-identical output.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


# =============================================================================
# ERRORS
# =============================================================================

class CanonError(Exception):
    """Base class for A5 errors."""


class ChannelValidationError(CanonError):
    """A channel definition is structurally or semantically invalid."""

    def __init__(self, path: Path, channel_id: str | None, message: str):
        self.path = path
        self.channel_id = channel_id
        loc = f" (channel id={channel_id!r})" if channel_id else ""
        super().__init__(f"{path}: {message}{loc}")


class ChannelConflictError(CanonError):
    """Two channel definitions share the same id."""

    def __init__(self, channel_id: str, first_path: Path, second_path: Path,
                 first_def: dict, second_def: dict):
        self.channel_id = channel_id
        self.first_path = first_path
        self.second_path = second_path
        self.first_def = first_def
        self.second_def = second_def
        super().__init__(
            f"channel id {channel_id!r} is defined in two files:\n"
            f"  - {first_path}\n"
            f"  - {second_path}\n"
            f"id is unique globally (all types confounded). "
            f"Resolve the conflict before continuing."
        )


# =============================================================================
# DATA STRUCTURE
# =============================================================================

@dataclass(frozen=True)
class ChannelDefinition:
    """
    A validated, semantically-coherent channel definition.

    This is the unit of work for A5's output. It carries enough
    information for A6 (generator) to produce C++ headers and for
    A7 (md5) to compute a stable hash.

    Fields:
      id            : unique global identifier.
      type          : 'analog' or 'digital'.
      scope         : 'REMOTE', 'LOCAL', or 'SYSTEM'.
      theme         : free-form string (no closed enum).
      direction     : frozenset of {'uplink', 'downlink'} (always
                      present; 'none' is represented as an empty set).
      requires      : frozenset of flag names (empty if absent).
      info_name     : optional human-readable name (None if absent).
      source_path   : Path of the .cb / .cbch file that declared it.
      raw           : the original raw dict (kept for diagnostics and
                      for A6 to read fields A5 does not interpret).
    """

    id: str
    type: str
    scope: str
    theme: str
    direction: frozenset[str]
    requires: frozenset[str]
    info_name: str | None
    source_path: Path
    raw: dict[str, Any] = field(default_factory=dict)

    def sort_key(self) -> tuple[str, str, str, str]:
        """Canonical sort key: (scope, type, theme, id)."""
        return (self.scope, self.type, self.theme, self.id)


# =============================================================================
# VALIDATION CONSTANTS
# =============================================================================

VALID_TYPES: frozenset[str] = frozenset({"analog", "digital"})
VALID_SCOPES: frozenset[str] = frozenset({"REMOTE", "LOCAL", "SYSTEM"})
VALID_DIRECTIONS: frozenset[str] = frozenset({"uplink", "downlink"})

# Canonical scope ordering for the sort key. Alphabetical matches the
# doc's enumeration order (LOCAL < REMOTE < SYSTEM alphabetically,
# but we want SYSTEM < LOCAL < REMOTE per the doc's intent — see
# §9 "Canonisation" and §10 "scope, type, theme, id").
#
# The doc does not explicitly mandate a scope ordering. We use the
# alphabetical order as a deterministic, well-defined default. If a
# different ordering is required (e.g. SYSTEM < LOCAL < REMOTE to
# match the doc's narrative), it can be changed in one place here.
_SCOPE_ORDER: dict[str, int] = {s: i for i, s in enumerate(sorted(VALID_SCOPES))}


# =============================================================================
# SECTION EXTRACTION
# =============================================================================

def extract_channels_sections(
    parsed: list[tuple[Path, str, Any]],
) -> list[tuple[Path, list[Any]]]:
    """
    Pull the `channels:` section out of each parsed document.

    A5 selects sections by KEY, not by file extension. A `.cbch` file
    with a `channels:` section is treated identically to a `.cb` file
    with a `channels:` section.

    Returns a list of (path, channels_list) for documents that have a
    `channels:` section. Documents without a `channels:` section are
    silently skipped (they may contain other sections like `chains:`
    that are not A5's concern).

    A document with `channels:` set to a non-list value is a
    ChannelValidationError.
    """
    out: list[tuple[Path, list[Any]]] = []
    for path, _type_label, raw in parsed:
        if not isinstance(raw, dict):
            # A3 already enforces top-level mapping; defensive only.
            continue
        if "channels" not in raw:
            continue
        section = raw["channels"]
        if section is None:
            continue
        if not isinstance(section, list):
            raise ChannelValidationError(
                path, None,
                f"`channels:` must be a list, got {type(section).__name__}",
            )
        out.append((path, section))
    return out


# =============================================================================
# PER-CHANNEL VALIDATION
# =============================================================================

def _validate_channel_dict(
    path: Path,
    raw: Any,
    index: int,
) -> ChannelDefinition:
    """
    Validate a single channel dict and return a ChannelDefinition.

    Raises ChannelValidationError on any structural or semantic issue.
    """
    if not isinstance(raw, dict):
        raise ChannelValidationError(
            path, None,
            f"channel #{index} must be a mapping, got {type(raw).__name__}",
        )

    # --- id (required, non-empty string) ---
    cid = raw.get("id")
    if cid is None:
        raise ChannelValidationError(
            path, None, f"channel #{index} is missing required field `id`",
        )
    if not isinstance(cid, str) or not cid:
        raise ChannelValidationError(
            path, None,
            f"channel #{index} `id` must be a non-empty string, got {cid!r}",
        )

    # --- type (required, in VALID_TYPES) ---
    ctype = raw.get("type")
    if ctype is None:
        raise ChannelValidationError(
            path, cid, "missing required field `type`",
        )
    if ctype not in VALID_TYPES:
        raise ChannelValidationError(
            path, cid,
            f"`type` must be one of {sorted(VALID_TYPES)}, got {ctype!r}",
        )

    # --- scope (required, in VALID_SCOPES) ---
    cscope = raw.get("scope")
    if cscope is None:
        raise ChannelValidationError(
            path, cid, "missing required field `scope`",
        )
    if cscope not in VALID_SCOPES:
        raise ChannelValidationError(
            path, cid,
            f"`scope` must be one of {sorted(VALID_SCOPES)}, got {cscope!r}",
        )

    # --- theme (required, non-empty string) ---
    ctheme = raw.get("theme")
    if ctheme is None:
        raise ChannelValidationError(
            path, cid, "missing required field `theme`",
        )
    if not isinstance(ctheme, str) or not ctheme:
        raise ChannelValidationError(
            path, cid,
            f"`theme` must be a non-empty string, got {ctheme!r}",
        )

    # --- direction (required for LOCAL/REMOTE, optional for SYSTEM) ---
    raw_dir = raw.get("direction")
    if raw_dir is None:
        if cscope == "SYSTEM":
            direction: frozenset[str] = frozenset()
        else:
            raise ChannelValidationError(
                path, cid,
                f"`direction` is required for scope={cscope!r} "
                "(SYSTEM may omit it and defaults to 'none')",
            )
    else:
        if not isinstance(raw_dir, list):
            raise ChannelValidationError(
                path, cid,
                f"`direction` must be a list, got {type(raw_dir).__name__}",
            )
        bad = [d for d in raw_dir if d not in VALID_DIRECTIONS]
        if bad:
            raise ChannelValidationError(
                path, cid,
                f"`direction` entries must be in {sorted(VALID_DIRECTIONS)}, "
                f"got invalid entries {bad!r}",
            )
        # Deduplicate while preserving order.
        seen: set[str] = set()
        ordered: list[str] = []
        for d in raw_dir:
            if d not in seen:
                seen.add(d)
                ordered.append(d)
        direction = frozenset(ordered)

    # --- requires (optional, list of non-empty strings) ---
    raw_req = raw.get("requires")
    if raw_req is None:
        requires: frozenset[str] = frozenset()
    else:
        if not isinstance(raw_req, list):
            raise ChannelValidationError(
                path, cid,
                f"`requires` must be a list, got {type(raw_req).__name__}",
            )
        bad_req = [r for r in raw_req if not isinstance(r, str) or not r]
        if bad_req:
            raise ChannelValidationError(
                path, cid,
                f"`requires` entries must be non-empty strings, "
                f"got invalid entries {bad_req!r}",
            )
        requires = frozenset(raw_req)

    # --- infoName (optional, string) ---
    raw_info = raw.get("infoName")
    if raw_info is None:
        info_name: str | None = None
    else:
        if not isinstance(raw_info, str):
            raise ChannelValidationError(
                path, cid,
                f"`infoName` must be a string, got {type(raw_info).__name__}",
            )
        info_name = raw_info

    return ChannelDefinition(
        id=cid,
        type=ctype,
        scope=cscope,
        theme=ctheme,
        direction=direction,
        requires=requires,
        info_name=info_name,
        source_path=path,
        raw=dict(raw),
    )


# =============================================================================
# FUSION + CONFLICT DETECTION
# =============================================================================

def _merge_channels(
    sections: list[tuple[Path, list[Any]]],
) -> list[ChannelDefinition]:
    """
    Validate every channel in every section, detect ID conflicts,
    return the merged list of ChannelDefinition (unsorted).

    Raises:
      ChannelValidationError on any structural / semantic issue.
      ChannelConflictError on duplicate id (across all types).
    """
    by_id: dict[str, ChannelDefinition] = {}
    for path, section in sections:
        for idx, raw in enumerate(section):
            ch = _validate_channel_dict(path, raw, idx)
            if ch.id in by_id:
                existing = by_id[ch.id]
                raise ChannelConflictError(
                    channel_id=ch.id,
                    first_path=existing.source_path,
                    second_path=ch.source_path,
                    first_def=existing.raw,
                    second_def=ch.raw,
                )
            by_id[ch.id] = ch
    return list(by_id.values())


# =============================================================================
# CANONICAL SORT
# =============================================================================

def _canonical_sort_key(ch: ChannelDefinition) -> tuple[int, str, str, str]:
    """
    Sort key for the canonical order (scope, type, theme, id).

    The scope component uses _SCOPE_ORDER (currently alphabetical) so
    that the sort is fully deterministic and the ordering is defined
    in one place. If a different scope ordering is required, change
    _SCOPE_ORDER — no other code needs to be touched.
    """
    return (
        _SCOPE_ORDER.get(ch.scope, len(_SCOPE_ORDER)),
        ch.type,
        ch.theme,
        ch.id,
    )


def _canonize(channels: list[ChannelDefinition]) -> list[ChannelDefinition]:
    """Apply the canonical sort. Pure function."""
    return sorted(channels, key=_canonical_sort_key)


# =============================================================================
# TOP-LEVEL ENTRY POINT
# =============================================================================

@dataclass(frozen=True)
class CanonResult:
    """
    Output of A5.

    active_definitions    : validated, conflict-free, unsorted list.
                            Preserved for diagnostics (what was found
                            and accepted, in discovery order).
    canonical_definitions : same content, sorted by (scope, type, theme, id).
                           This is what A6 / A7 / A8 consume.
    """

    active_definitions: list[ChannelDefinition]
    canonical_definitions: list[ChannelDefinition]


def canonize(
    parsed: list[tuple[Path, str, Any]],
) -> CanonResult:
    """
    A5 top-level entry point.

    Args:
      parsed: list of (path, type_label, raw_dict) as returned by
              A3's discover_and_parse() / parse_all().

    Returns:
      CanonResult with active_definitions and canonical_definitions.

    Raises:
      ChannelValidationError, ChannelConflictError, CanonError.
    """
    sections = extract_channels_sections(parsed)
    active = _merge_channels(sections)
    canonical = _canonize(active)
    return CanonResult(
        active_definitions=active,
        canonical_definitions=canonical,
    )


# =============================================================================
# CLI / diagnostic entry point
# =============================================================================

def main(parsed: list[tuple[Path, str, Any]]) -> int:
    """CLI entry point: print the canonical order and return 0/1."""
    try:
        result = canonize(parsed)
    except CanonError as e:
        import sys
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1

    print(f"[combus_builder] active_definitions: {len(result.active_definitions)}")
    print(f"[combus_builder] canonical_definitions: {len(result.canonical_definitions)}")
    print("[combus_builder] canonical order:")
    for ch in result.canonical_definitions:
        dirs = sorted(ch.direction) or ["none"]
        print(
            f"  {ch.scope:<6} {ch.type:<7} {ch.theme:<10} "
            f"{ch.id:<20} dir={','.join(dirs)} "
            f"requires={sorted(ch.requires)} "
            f"({ch.source_path.name})"
        )
    return 0
