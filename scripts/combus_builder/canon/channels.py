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
    "value",
})

# Tokens acceptés pour `value` sur un channel analog.
# Mapping vers littéraux C++ uint16_t (cf. src/core/system/combus/combus_res.h).
VALID_ANALOG_VALUES: frozenset[str] = frozenset({
    "CbusMinVal",
    "CbusNeutral",
    "CbusMaxVal",
})

# Tokens acceptés pour `value` sur un channel digital.
VALID_DIGITAL_VALUES: frozenset[str] = frozenset({
    "low",
    "high",
})

# Bornes pour les valeurs analog numériques brutes (entier accepté dans [0..65535]).
ANALOG_VALUE_MIN: int = 0
ANALOG_VALUE_MAX: int = 65535

# Types autorisés.
VALID_TYPES: frozenset[str] = frozenset({"analog", "digital"})

# Scopes autorisés.
VALID_SCOPES: frozenset[str] = frozenset({"LOCAL", "REMOTE", "SYSTEM"})

# Thèmes autorisés. À étendre ici quand un nouveau thème apparaît.
VALID_THEMES: frozenset[str] = frozenset({
    "core",        # A9.1: STEERING_BUS (dumper_truck/combus/steering_bus.cb)
    "failsafe",
    "vbat",
    "light",
    "input",       # chantier 12.5: REMOTE_LINK_LOST (replaces FAILSAFE_COMBUS_LINK)
    # Ajouter ici les nouveaux thèmes légitimes.
})

# Ordre canonique des scopes (utilisé par le tri).
SCOPE_ORDER: dict[str, int] = {
    "LOCAL": 0,
    "REMOTE": 1,
    "SYSTEM": 2,
}

# Tokens de surface autorisés dans `direction`.
# A2.1 (2026-08-29) : ajout des variantes `*_OR` (uplink_or, downlink_or, both_or)
# qui ajoutent une politique de fusion OR logique à la direction wire.
DIRECTION_TOKENS: frozenset[str] = frozenset({
    "uplink", "downlink", "both", "none",
    "uplink_or", "downlink_or", "both_or",  # A2.1
})

# Tokens autorisés par scope (en surface).
# A2.1 : les variantes `*_OR` ne sont acceptées que sur LOCAL/REMOTE
# (un channel SYSTEM n'a qu'un seul écrivain possible — la fusion OR
# n'a pas de sens, on est toujours en last-write-wins effectif).
DIRECTION_BY_SCOPE: dict[str, frozenset[str]] = {
    "LOCAL": frozenset({"uplink", "downlink", "both", "none",
                        "uplink_or", "downlink_or", "both_or"}),
    "REMOTE": frozenset({"uplink", "downlink", "both", "none",
                         "uplink_or", "downlink_or", "both_or"}),
    "SYSTEM": frozenset({"none"}),  # seule valeur de surface acceptée
}

# Tokens `*_OR` (A2.1) — sous-ensemble de DIRECTION_TOKENS.
DIRECTION_OR_TOKENS: frozenset[str] = frozenset({"uplink_or", "downlink_or", "both_or"})



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

    `direction_or` (A2.1) est TOUJOURS un frozenset de `{"uplink", "downlink"}`
    (ou vide si pas de sémantique `*_OR`). Les tokens de surface
    `uplink_or` / `downlink_or` / `both_or` sont normalisés à la
    validation et n'apparaissent jamais dans cette représentation.
    La sémantique `*_OR` est exclusive : un channel ne peut pas avoir
    à la fois `direction` et `direction_or` non vides (la fusion OR
    n'a de sens que si plusieurs sources écrivent sur le même sens).

    `value` est `None` si le champ est absent du YAML ; sinon :
      - analog : l'un des tokens `CbusMinVal` / `CbusNeutral` / `CbusMaxVal`
                 OU un entier `[ANALOG_VALUE_MIN..ANALOG_VALUE_MAX]`.
      - digital : l'un des tokens `low` / `high`.
    La forme canonique est conservée telle quelle (pas de conversion
    implicite) afin que le générateur C++ puisse décider du littéral
    exact à émettre.
    """

    id: str
    info_name: str
    type: str
    scope: str
    theme: str
    direction: frozenset[str]
    direction_or: frozenset[str]  # A2.1 — sémantique de fusion OR (vide si absent)
    requires: frozenset[str]
    source_path: Path
    raw: dict[str, Any] = field(default_factory=dict)
    value: str | int | None = None

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
    type_: str,
    raw_dir: Any,
) -> tuple[frozenset[str], frozenset[str]]:
    """
    Valide et normalise `direction` selon le scope et le type.

    Règles (decision mainteneur 2026-08-22 : direction = string, pas liste) :
      - Champ absent pour SYSTEM → (frozenset(), frozenset()) (= none implicite).
      - Doit être une STRING unique ∈ {uplink, downlink, both, none,
                                       uplink_or, downlink_or, both_or}.
      - SYSTEM : seule 'none' (ou champ absent) est acceptée.
      - LOCAL/REMOTE : 'uplink', 'downlink', 'both', 'none' tous OK
                       selon la politique par scope.
      - `both` est un raccourci pour {uplink, downlink}.
      - `none` est un raccourci pour {}.

    Règles A2.1 (variantes `*_OR`) :
      - `*_OR` uniquement sur `type: digital` (rejet sur analog).
      - `*_OR` uniquement sur `scope: LOCAL/REMOTE` (rejet sur SYSTEM).
      - `*_OR` est mutuellement exclusif avec `direction` non-vide :
        un channel ne peut pas avoir à la fois une direction wire
        classique ET une sémantique de fusion OR.
      - Normalisation :
          * `uplink_or`   → direction_or = {uplink},   direction = {}
          * `downlink_or` → direction_or = {downlink}, direction = {}
          * `both_or`     → direction_or = {uplink, downlink}, direction = {}

    Pourquoi une string (et non une liste) : les valeurs possibles sont
    mutuellement exclusives. Une liste n'apporte rien et complique la
    syntaxe YAML.

    Retourne un tuple (direction, direction_or) de frozensets normalisés
    (uniquement uplink/downlink). Un seul des deux est non-vide à la fois.
    """
    # Cas SYSTEM : champ absent → (frozenset(), frozenset())
    if raw_dir is None:
        if scope == "SYSTEM":
            return (frozenset(), frozenset())
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` is required for scope={scope!r} "
            "(SYSTEM may omit it and defaults to 'none')",
        )

    # Doit être une string.
    if not isinstance(raw_dir, str):
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` must be a string, got {type(raw_dir).__name__} "
            f"({raw_dir!r}); accepted values: 'uplink', 'downlink', 'both', "
            f"'none', 'uplink_or', 'downlink_or', 'both_or'",
        )

    # SYSTEM : seule 'none' est acceptée (les `*_OR` sont rejetés ici).
    if scope == "SYSTEM":
        if raw_dir == "none":
            return (frozenset(), frozenset())
        raise ChannelValidationError(
            path, channel_id,
            f"scope=SYSTEM does not have a wire direction; "
            f"only `direction: none` (or absent) is accepted, "
            f"got {raw_dir!r}",
        )

    # LOCAL/REMOTE : la valeur doit être un token autorisé globalement.
    if raw_dir not in DIRECTION_TOKENS:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` must be one of {sorted(DIRECTION_TOKENS)}, "
            f"got {raw_dir!r}",
        )

    # Tokens autorisés par scope.
    allowed = DIRECTION_BY_SCOPE[scope]
    if raw_dir not in allowed:
        raise ChannelValidationError(
            path, channel_id,
            f"scope={scope!r} does not accept direction token {raw_dir!r}; "
            f"allowed for this scope: {sorted(allowed)}",
        )

    # A2.1 : `*_OR` uniquement sur `type: digital`.
    if raw_dir in DIRECTION_OR_TOKENS and type_ != "digital":
        raise ChannelValidationError(
            path, channel_id,
            f"`direction: {raw_dir!r}` (A2.1 `*_OR` variant) is only valid "
            f"on `type: digital` channels (OR-fusion has no meaning on "
            f"analog magnitudes); got `type: {type_!r}`",
        )

    # Normalisation : `both` → {uplink, downlink}, `none` → {},
    # `uplink`/`downlink` → {uplink}/{downlink}.
    # A2.1 : `*_OR` → direction_or rempli, direction vide.
    if raw_dir == "both":
        return (frozenset({"uplink", "downlink"}), frozenset())
    if raw_dir == "none":
        return (frozenset(), frozenset())
    if raw_dir == "uplink_or":
        return (frozenset(), frozenset({"uplink"}))
    if raw_dir == "downlink_or":
        return (frozenset(), frozenset({"downlink"}))
    if raw_dir == "both_or":
        return (frozenset(), frozenset({"uplink", "downlink"}))
    return (frozenset({raw_dir}), frozenset())


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
# VALIDATION : value
# =============================================================================

def _validate_value(
    path: Path,
    channel_id: str | None,
    type_: str,
    raw_val: Any,
) -> str | int | None:
    """
    Valide et normalise `value` selon le `type` du channel.

    Règles :
      - Champ absent (`raw_val is None`) → None (= défaut générateur).
      - `value: null` explicite → erreur (canonique : null ≠ valeur absente).
      - analog :
          * Token ∈ {"CbusMinVal", "CbusNeutral", "CbusMaxVal"} → str tel quel.
          * Entier dans [ANALOG_VALUE_MIN..ANALOG_VALUE_MAX] → int tel quel.
          * Tout autre type ou valeur hors bornes → erreur.
      - digital :
          * Token ∈ {"low", "high"} → str tel quel.
          * Toute autre valeur → erreur.
      - Cross-type strict :
          * low/high sur analog → erreur.
          * CbusMinVal/Neutral/MaxVal sur digital → erreur.
          * entier sur digital → erreur.

    Retourne :
      - None si absent.
      - str (token) ou int (entier brut) si présent et valide.
    """
    # Cas absent : champ non présent dans le YAML.
    if raw_val is None:
        # Distinction absent / null :
        # PyYAML retourne None pour `key: null` ET pour clé absente.
        # On ne peut PAS distinguer les deux ici sans contexte.
        # La distinction est faite par validate_channel() qui vérifie
        # explicitement `raw_val is None AND "value" in raw` via
        # ChannelValidationError séparé. Voir validate_channel().
        return None

    # analog
    if type_ == "analog":
        if isinstance(raw_val, str):
            if raw_val in VALID_ANALOG_VALUES:
                return raw_val
            raise ChannelValidationError(
                path, channel_id,
                f"`value` for analog must be one of "
                f"{sorted(VALID_ANALOG_VALUES)} or an integer in "
                f"[{ANALOG_VALUE_MIN}..{ANALOG_VALUE_MAX}]; "
                f"got unknown token {raw_val!r}",
            )
        if isinstance(raw_val, bool):
            # bool est sous-classe de int en Python : on l'exclut explicitement.
            raise ChannelValidationError(
                path, channel_id,
                f"`value` for analog must be a token or an integer, "
                f"got boolean {raw_val!r}",
            )
        if isinstance(raw_val, int):
            if ANALOG_VALUE_MIN <= raw_val <= ANALOG_VALUE_MAX:
                return raw_val
            raise ChannelValidationError(
                path, channel_id,
                f"`value` for analog must be in [{ANALOG_VALUE_MIN}.."
                f"{ANALOG_VALUE_MAX}] (uint16_t range), got {raw_val!r}",
            )
        # Autres types YAML : float, list, dict, etc.
        raise ChannelValidationError(
            path, channel_id,
            f"`value` for analog must be a string token or an integer, "
            f"got {type(raw_val).__name__} ({raw_val!r})",
        )

    # digital
    if type_ == "digital":
        if isinstance(raw_val, str):
            if raw_val in VALID_DIGITAL_VALUES:
                return raw_val
            raise ChannelValidationError(
                path, channel_id,
                f"`value` for digital must be one of "
                f"{sorted(VALID_DIGITAL_VALUES)}; "
                f"got unknown token {raw_val!r}",
            )
        raise ChannelValidationError(
            path, channel_id,
            f"`value` for digital must be a string token ('low' or 'high'), "
            f"got {type(raw_val).__name__} ({raw_val!r})",
        )

    # type inconnu (ne devrait pas arriver : validate_channel filtre avant).
    raise ChannelValidationError(
        path, channel_id,
        f"`value` validation called with unknown `type`={type_!r}",
    )


def _check_value_explicit_null(
    path: Path,
    channel_id: str | None,
    raw: dict[str, Any],
) -> None:
    """
    Rejette explicitement `value: null` (≠ champ absent).

    PyYAML retourne None à la fois pour clé absente et pour clé=None.
    On distingue les deux cas ici : si "value" est dans le mapping ET
    que raw["value"] est None, c'est une erreur explicite.
    """
    if "value" in raw and raw["value"] is None:
        raise ChannelValidationError(
            path, channel_id,
            "`value` is explicitly null; expected a token (e.g. "
            "'CbusNeutral', 'low') or an integer for analog. "
            "Omit the field to use the generator default.",
        )


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

    # --- direction (contrat par scope + type, A2.1) ---
    direction, direction_or = _normalize_direction(
        path, cid, cscope, ctype, raw.get("direction"),
    )

    # --- requires ---
    requires = _validate_requires(path, cid, raw.get("requires"))

    # --- value (optionnel) ---
    # Distinction explicite absent / null AVANT _validate_value (qui retourne
    # None pour les deux cas).
    _check_value_explicit_null(path, cid, raw)
    value = _validate_value(path, cid, ctype, raw.get("value"))

    return ChannelDefinition(
        id=cid,
        info_name=info_name,
        type=ctype,
        scope=cscope,
        theme=ctheme,
        direction=direction,
        direction_or=direction_or,
        requires=requires,
        source_path=path,
        raw=dict(raw),
        value=value,
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

class ChannelValueConflictError(ChannelError):
    """Deux définitions d'un même channel portent des `value` incompatibles."""

    def __init__(self, channel_id: str, first_path: Path, second_path: Path,
                 first_value: Any, second_value: Any):
        self.channel_id = channel_id
        self.first_path = first_path
        self.second_path = second_path
        self.first_value = first_value
        self.second_value = second_value
        super().__init__(
            f"channel id {channel_id!r} is defined in two files with "
            f"incompatible `value`:\n"
            f"  - {first_path}: value={first_value!r}\n"
            f"  - {second_path}: value={second_value!r}\n"
            f"`value` must match across definitions of the same channel id. "
            f"Resolve the conflict before continuing."
        )


def merge_channels(
    sections: list[tuple[Path, list[Any]]],
) -> list[ChannelDefinition]:
    """
    Valide chaque channel, détecte les conflits d'ID, retourne la liste
    fusionnée (non triée).

    Lève ChannelValidationError, ChannelConflictError ou
    ChannelValueConflictError.

    Note :
      - Un même id avec des `value` différents est une erreur explicite
        (pas de fusion silencieuse).
      - Si l'un des deux `value` est None (= défaut générateur), l'autre
        gagne ; la présence explicite de `value` reste cohérente avec
        "même id = même contrat".
    """
    by_id: dict[str, ChannelDefinition] = {}
    for path, section in sections:
        for idx, raw in enumerate(section):
            ch = validate_channel(path, raw, idx)
            if ch.id in by_id:
                existing = by_id[ch.id]
                if existing.value != ch.value:
                    raise ChannelValueConflictError(
                        channel_id=ch.id,
                        first_path=existing.source_path,
                        second_path=ch.source_path,
                        first_value=existing.value,
                        second_value=ch.value,
                    )
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
