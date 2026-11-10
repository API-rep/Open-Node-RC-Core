#!/usr/bin/env python3
"""
combus_builder/canon — orchestration of the canonisation.

This package holds one canoniser per section format.  Each format
(`channels:`, `chains:`...) has its own module, with its contract and its
rules.

Today:
  - canon.channels  : contract and canonisation of `channels:`.

TODO(ComBus processors):
  - canon.chains    : wiring of the chains (CbProcChain).

The orchestration (entry point canonize()) lives here.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .channels import (
    ALLOWED_FIELDS,
    ANALOG_VALUE_MAX,
    ANALOG_VALUE_MIN,
    DIRECTION_BY_SCOPE,
    DIRECTION_TOKENS,
    SCOPE_ORDER,
    VALID_ANALOG_VALUES,
    VALID_DIGITAL_VALUES,
    VALID_SCOPES,
    VALID_THEMES,
    VALID_TYPES,
    ChannelConflictError,
    ChannelDefinition,
    ChannelError,
    ChannelValidationError,
    ChannelValueConflictError,
    canonize_channels,
    extract_channels_sections,
    merge_channels,
    validate_channel,
)


__all__ = [
    # Vocabularies
    "ALLOWED_FIELDS",
    "VALID_TYPES",
    "VALID_SCOPES",
    "VALID_THEMES",
    "VALID_ANALOG_VALUES",
    "VALID_DIGITAL_VALUES",
    "ANALOG_VALUE_MIN",
    "ANALOG_VALUE_MAX",
    "SCOPE_ORDER",
    "DIRECTION_TOKENS",
    "DIRECTION_BY_SCOPE",
    # Errors
    "ChannelError",
    "ChannelValidationError",
    "ChannelConflictError",
    "ChannelValueConflictError",
    # Data
    "ChannelDefinition",
    # API
    "canonize",
    "CanonResult",
    "extract_channels_sections",
    "validate_channel",
    "merge_channels",
    "canonize_channels",
]


@dataclass(frozen=True)
class CanonResult:
    """
    Output of the canonisation.

    active_definitions    : validated, without conflict, in discovery order
                            (for diagnostics).
    canonical_definitions : same content, sorted by (scope, type, theme, id).
                            What the hook, the CLI and links.py consume.
    """

    active_definitions: list[ChannelDefinition]
    canonical_definitions: list[ChannelDefinition]


def canonize(
    parsed: list[tuple[Path, str, Any]],
) -> CanonResult:
    """
    Entry point of the canonisation.

    Takes the output of parser.py (list[tuple[Path, type_label, raw_dict]])
    and returns a CanonResult.

    Only `channels:` is canonised today; `chains:` will be added without
    changing this signature.
    """
    sections = extract_channels_sections(parsed)
    active = merge_channels(sections)
    canonical = canonize_channels(active)
    return CanonResult(
        active_definitions=active,
        canonical_definitions=canonical,
    )


# =============================================================================
# CLI / diagnostic
# =============================================================================

def main(parsed: list[tuple[Path, str, Any]]) -> int:
    """CLI entry point: print the canonical order and return 0/1."""
    import sys
    try:
        result = canonize(parsed)
    except ChannelError as e:
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
