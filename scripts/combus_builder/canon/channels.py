#!/usr/bin/env python3
"""
combus_builder/canon/channels.py — A5.1: contrat et canonisation des `channels:`.

Ce module regroupe TOUT le vocabulaire et les règles propres au format
`channels:`. Il est volontairement monolithique (par format, pas par
constante) pour rester lisible et éditable.

Vocabulaires centralisés (modifiables ici, sans toucher au reste) :
  - ALLOWED_FIELDS       : clés autorisées par channel.
  - VALID_TYPES          : valeurs autorisées pour `type`.
  - VALID_SCOPES         : valeurs autorisées pour `scope`.
  - VALID_THEMES         : valeurs autorisées pour `theme`.
  - SCOPE_ORDER          : ordre canonique des scopes.
  - DIRECTION_TOKENS     : tokens de surface autorisés.
  - DIRECTION_BY_SCOPE   : tokens autorisés par scope (incl. `both`/`none`).

Règles de validation :
  - clés inconnues → erreur explicite ;
  - `id` requis, non vide, unique globalement ;
  - `infoName` REQUIS, non vide ;
  - `type` ∈ VALID_TYPES ;
  - `scope` ∈ VALID_SCOPES ;
  - `theme` ∈ VALID_THEMES ;
  - `direction` : contrat par scope (voir _validate_direction) ;
  - `requires` : optionnel, AND, doublons interdits.

Représentation interne de `direction` :
  - `both`  → frozenset({"uplink", "downlink"})
  - `none`  → frozenset()
  - `uplink` / `downlink` → frozenset({"uplink"}) / frozenset({"downlink"})
  - Combinaisons incohérentes (ex. `both` + `uplink`) → erreur.

Sortie :
  - ChannelDefinition (frozen dataclass) avec `direction: frozenset[str]`
    normalisé (uniquement `uplink` et/ou `downlink`, jamais `both`/`none`).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


# =============================================================================
# VOCABULAIRES (un seul endroit à modifier pour étendre)
# =============================================================================

# Champs autorisés par channel. Toute autre clé est une erreur.
ALLOWED_FIELDS: frozenset[str] = frozenset({
    "id",
    "infoName",
    "type",
    "scope",
    "theme",
    "direction",
    "requires",
})

# Types autorisés.
VALID_TYPES: frozenset[str] = frozenset({"analog", "digital"})

# Scopes autorisés.
VALID_SCOPES: frozenset[str] = frozenset({"LOCAL", "REMOTE", "SYSTEM"})

# Thèmes autorisés. À étendre ici quand un nouveau thème apparaît.
VALID_THEMES: frozenset[str] = frozenset({
    "core",        # A9.1: STEERING_BUS (dumper_truck/combus/steering_bus.cb)
    "failsafe",
    "vbat",
    # Ajouter ici les nouveaux thèmes légitimes.
})

# Ordre canonique des scopes (utilisé par le tri).
SCOPE_ORDER: dict[str, int] = {
    "LOCAL": 0,
    "REMOTE": 1,
    "SYSTEM": 2,
}

# Tokens de surface autorisés dans `direction`.
DIRECTION_TOKENS: frozenset[str] = frozenset({"uplink", "downlink", "both", "none"})

# Tokens autorisés par scope (en surface).
DIRECTION_BY_SCOPE: dict[str, frozenset[str]] = {
    "LOCAL": frozenset({"uplink", "downlink", "both", "none"}),
    "REMOTE": frozenset({"uplink", "downlink", "both", "none"}),
    "SYSTEM": frozenset({"none"}),  # seule valeur de surface acceptée
}


# =============================================================================
# ERREURS
# =============================================================================

class ChannelError(Exception):
    """Base class pour les erreurs A5.1 channels."""


class ChannelValidationError(ChannelError):
    """Une définition de channel est invalide (structure ou sémantique)."""

    def __init__(self, path: Path, channel_id: str | None, message: str):
        self.path = path
        self.channel_id = channel_id
        loc = f" (channel id={channel_id!r})" if channel_id else ""
        super().__init__(f"{path}: {message}{loc}")


class ChannelConflictError(ChannelError):
    """Deux définitions partagent le même id."""

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
            f"id is unique globally (all types, scopes, themes confounded). "
            f"Resolve the conflict before continuing."
        )


# =============================================================================
# DATA STRUCTURE
# =============================================================================

@dataclass(frozen=True)
class ChannelDefinition:
    """
    Définition de channel validée et normalisée.

    `direction` est TOUJOURS un frozenset de `{"uplink", "downlink"}`
    (ou vide pour `none`/SYSTEM). Les tokens de surface `both` et
    `none` sont normalisés à la validation et n'apparaissent jamais
    dans cette représentation.
    """

    id: str
    info_name: str
    type: str
    scope: str
    theme: str
    direction: frozenset[str]
    requires: frozenset[str]
    source_path: Path
    raw: dict[str, Any] = field(default_factory=dict)

    def sort_key(self) -> tuple[int, str, str, str]:
        """Clé de tri canonique : (scope, type, theme, id)."""
        return (SCOPE_ORDER.get(self.scope, len(SCOPE_ORDER)),
                self.type, self.theme, self.id)


# =============================================================================
# VALIDATION : direction
# =============================================================================

def _normalize_direction(
    path: Path,
    channel_id: str | None,
    scope: str,
    raw_dir: Any,
) -> frozenset[str]:
    """
    Valide et normalise `direction` selon le scope.

    Règles :
      - Doit être une liste (ou absente pour SYSTEM).
      - SYSTEM : champ absent OU [none] → frozenset().
                Toute autre valeur → erreur.
      - LOCAL/REMOTE : liste non vide, tokens autorisés par scope,
                       pas de doublons, pas de combinaisons incohérentes.
      - `both` est un raccourci pour {uplink, downlink}.
      - `none` est un raccourci pour {} (uniquement LOCAL).
      - Combinaisons incohérentes (both+uplink, none+uplink, etc.) → erreur.

    Retourne un frozenset normalisé (uniquement uplink/downlink).
    """
    # Cas SYSTEM : champ absent → frozenset()
    if raw_dir is None:
        if scope == "SYSTEM":
            return frozenset()
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` is required for scope={scope!r} "
            "(SYSTEM may omit it and defaults to 'none')",
        )

    # Doit être une liste.
    if not isinstance(raw_dir, list):
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` must be a list, got {type(raw_dir).__name__}",
        )

    # Chaque entrée doit être une string (sinon set()/dict() échoue).
    # On valide le type AVANT toute opération nécessitant des valeurs hashables.
    bad_type = [d for d in raw_dir if not isinstance(d, str)]
    if bad_type:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` entries must be strings, got {bad_type!r}",
        )

    # Doublons interdits (vérifié en premier pour donner un message clair).
    if len(raw_dir) != len(set(raw_dir)):
        seen: set[str] = set()
        dups: list[str] = []
        for d in raw_dir:
            if d in seen:
                dups.append(d)
            seen.add(d)
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` contains duplicates: {dups!r}",
        )

    # SYSTEM : seule [none] est acceptée.
    if scope == "SYSTEM":
        if raw_dir == ["none"]:
            return frozenset()
        raise ChannelValidationError(
            path, channel_id,
            f"scope=SYSTEM does not have a wire direction; "
            f"only `direction: [none]` (or absent) is accepted, "
            f"got {raw_dir!r}",
        )

    # LOCAL/REMOTE : liste non vide.
    if not raw_dir:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` must be a non-empty list for scope={scope!r}, "
            f"got empty list",
        )

    # Tokens autorisés par scope.
    allowed = DIRECTION_BY_SCOPE[scope]
    bad = [d for d in raw_dir if d not in DIRECTION_TOKENS]
    if bad:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` entries must be in {sorted(DIRECTION_TOKENS)}, "
            f"got invalid entries {bad!r}",
        )
    bad_scope = [d for d in raw_dir if d not in allowed]
    if bad_scope:
        raise ChannelValidationError(
            path, channel_id,
            f"scope={scope!r} does not accept direction tokens {bad_scope!r}; "
            f"allowed for this scope: {sorted(allowed)}",
        )

    # Normalisation : `both` → {uplink, downlink}, `none` → {}.
    # Combinaisons incohérentes (both+uplink, none+uplink, etc.) → erreur.
    #
    # On détecte les combinaisons incohérentes en deux passes :
    # 1. Compter les tokens de surface (both, none, uplink, downlink).
    # 2. Vérifier que la combinaison est cohérente.
    has_both = "both" in raw_dir
    has_none = "none" in raw_dir
    has_wire = any(d in ("uplink", "downlink") for d in raw_dir)

    if has_both and has_wire:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction: [both]` cannot be combined with "
            f"`uplink` or `downlink`, got {raw_dir!r}",
        )
    if has_none and has_wire:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction: [none]` cannot be combined with "
            f"`uplink` or `downlink`, got {raw_dir!r}",
        )
    if has_none and has_both:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction: [none]` cannot be combined with "
            f"`both`, got {raw_dir!r}",
        )

    # Normalisation effective.
    normalized: set[str] = set()
    for d in raw_dir:
        if d == "both":
            normalized.add("uplink")
            normalized.add("downlink")
        elif d == "none":
            pass  # none = {} (déjà vérifié ci-dessus)
        else:
            normalized.add(d)

    return frozenset(normalized)


# =============================================================================
# VALIDATION : requires
# =============================================================================

def _validate_requires(
    path: Path,
    channel_id: str | None,
    raw_req: Any,
) -> frozenset[str]:
    """
    Valide `requires`.

    Règles :
      - Optionnel. Absent ou [] → frozenset() (actif par défaut).
      - Si présent : liste de strings non vides.
      - Doublons interdits.
      - Sémantique : AND (tous les flags doivent être présents).
        La résolution effective est du ressort de A8.
    """
    if raw_req is None:
        return frozenset()
    if not isinstance(raw_req, list):
        raise ChannelValidationError(
            path, channel_id,
            f"`requires` must be a list, got {type(raw_req).__name__}",
        )
    bad = [r for r in raw_req if not isinstance(r, str) or not r]
    if bad:
        raise ChannelValidationError(
            path, channel_id,
            f"`requires` entries must be non-empty strings, "
            f"got invalid entries {bad!r}",
        )
    if len(raw_req) != len(set(raw_req)):
        seen: set[str] = set()
        dups: list[str] = []
        for r in raw_req:
            if r in seen:
                dups.append(r)
            seen.add(r)
        raise ChannelValidationError(
            path, channel_id,
            f"`requires` contains duplicates: {dups!r}",
        )
    return frozenset(raw_req)


# =============================================================================
# VALIDATION : un channel complet
# =============================================================================

def validate_channel(
    path: Path,
    raw: Any,
    index: int,
) -> ChannelDefinition:
    """
    Valide un channel brut et retourne un ChannelDefinition normalisé.

    Lève ChannelValidationError sur tout problème (structure, sémantique,
    clé inconnue, doublon, combinaison incohérente).
    """
    if not isinstance(raw, dict):
        raise ChannelValidationError(
            path, None,
            f"channel #{index} must be a mapping, got {type(raw).__name__}",
        )

    # --- clés inconnues (rejet explicite) ---
    unknown = set(raw.keys()) - ALLOWED_FIELDS
    if unknown:
        raise ChannelValidationError(
            path, None,
            f"channel #{index} has unknown field(s) {sorted(unknown)!r}; "
            f"allowed fields are {sorted(ALLOWED_FIELDS)}",
        )

    # --- id (requis, non vide) ---
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

    # --- infoName (REQUIS, non vide) ---
    info_name = raw.get("infoName")
    if info_name is None:
        raise ChannelValidationError(
            path, cid, "missing required field `infoName`",
        )
    if not isinstance(info_name, str) or not info_name:
        raise ChannelValidationError(
            path, cid,
            f"`infoName` must be a non-empty string, got {info_name!r}",
        )

    # --- type ---
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

    # --- scope ---
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

    # --- theme (cadré) ---
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
    if ctheme not in VALID_THEMES:
        raise ChannelValidationError(
            path, cid,
            f"`theme` must be one of {sorted(VALID_THEMES)}, got {ctheme!r}; "
            f"add the new theme to VALID_THEMES in canon/channels.py if "
            f"this is intentional",
        )

    # --- direction (contrat par scope) ---
    direction = _normalize_direction(path, cid, cscope, raw.get("direction"))

    # --- requires ---
    requires = _validate_requires(path, cid, raw.get("requires"))

    return ChannelDefinition(
        id=cid,
        info_name=info_name,
        type=ctype,
        scope=cscope,
        theme=ctheme,
        direction=direction,
        requires=requires,
        source_path=path,
        raw=dict(raw),
    )


# =============================================================================
# EXTRACTION DEPUIS A3
# =============================================================================

def extract_channels_sections(
    parsed: list[tuple[Path, str, Any]],
) -> list[tuple[Path, list[Any]]]:
    """
    Extrait la section `channels:` de chaque document parsé par A3.

    A5.1 sélectionne par CLÉ, pas par extension. Un `.cbch` avec une
    section `channels:` est traité comme un `.cb` avec une section
    `channels:`.

    Distinction précise :
      - clé `channels:` absente  → ignorée silencieusement
      - `channels: null`          → erreur de validation explicite
      - `channels: []`            → section valide vide
      - `channels: [...]`         → section valide non vide

    Les documents sans `channels:` peuvent contenir d'autres sections
    (comme `chains:`) qui ne sont pas du ressort de A5.1.
    """
    out: list[tuple[Path, list[Any]]] = []
    for path, _type_label, raw in parsed:
        if not isinstance(raw, dict):
            continue
        if "channels" not in raw:
            # Clé absente : le fichier ne déclare aucun channel.
            continue
        section = raw["channels"]
        if section is None:
            # Clé présente mais valeur null : erreur explicite.
            raise ChannelValidationError(
                path, None,
                "`channels:` is explicitly null; expected a list of channels "
                "(use `channels: []` for an empty section, or omit the key "
                "to declare no channels)",
            )
        if not isinstance(section, list):
            raise ChannelValidationError(
                path, None,
                f"`channels:` must be a list, got {type(section).__name__}",
            )
        out.append((path, section))
    return out


# =============================================================================
# FUSION + CONFLITS
# =============================================================================

def merge_channels(
    sections: list[tuple[Path, list[Any]]],
) -> list[ChannelDefinition]:
    """
    Valide chaque channel, détecte les conflits d'ID, retourne la liste
    fusionnée (non triée).

    Lève ChannelValidationError ou ChannelConflictError.
    """
    by_id: dict[str, ChannelDefinition] = {}
    for path, section in sections:
        for idx, raw in enumerate(section):
            ch = validate_channel(path, raw, idx)
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
# TRI CANONIQUE
# =============================================================================

def canonize_channels(channels: list[ChannelDefinition]) -> list[ChannelDefinition]:
    """Tri canonique : (scope, type, theme, id)."""
    return sorted(channels, key=lambda ch: ch.sort_key())
