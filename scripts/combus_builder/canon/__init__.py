#!/usr/bin/env python3
"""
combus_builder/canon — A5: orchestration de la canonisation.

Ce package regroupe les canoniseurs par format de section. Chaque
format (`channels:`, `chains:`, etc.) a son propre module qui
expose son contrat et ses règles.

Aujourd'hui :
  - canon.channels  : contrat et canonisation des `channels:`.

Demain (Phase C) :
  - canon.chains    : câblage des chaînes (CbProcChain).

L'orchestration générale (point d'entrée canonize()) vit ici.
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
    # Vocabulaires
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
    # Erreurs
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
    Sortie de la canonisation.

    active_definitions    : validé, sans conflit, ordre de découverte
                            (pour diagnostic).
    canonical_definitions : même contenu, trié par (scope, type, theme, id).
                            C'est ce que A6 / A7 / A8 consomment.
    """

    active_definitions: list[ChannelDefinition]
    canonical_definitions: list[ChannelDefinition]


def canonize(
    parsed: list[tuple[Path, str, Any]],
) -> CanonResult:
    """
    Point d'entrée A5.

    Prend la sortie de A3 (list[tuple[Path, type_label, raw_dict]]) et
    retourne un CanonResult.

    Aujourd'hui : ne canonise que `channels:`. Demain : ajoutera
    `chains:` (Phase C) sans changer cette signature.
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
