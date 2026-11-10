#!/usr/bin/env python3
"""
combus_builder/canon/channels.py — contract and canonisation of `channels:`.

This module holds ALL the vocabulary and the rules of the `channels:` format.
It is monolithic on purpose (one module per format, not per constant), to stay
readable and easy to edit.

Vocabularies (edit them here, nothing else to touch):
  - ALLOWED_FIELDS       : keys allowed in a channel.
  - VALID_TYPES          : values of `type`.
  - VALID_SCOPES         : values of `scope`.
  - VALID_THEMES         : values of `theme`.
  - SCOPE_ORDER          : canonical order of the scopes.
  - DIRECTION_TOKENS     : surface tokens of `direction`.
  - DIRECTION_BY_SCOPE   : tokens allowed per scope (`both` and `none` included).

Validation rules:
  - unknown key → explicit error;
  - `id` required, non-empty, unique across all files;
  - `infoName` REQUIRED, non-empty;
  - `type` ∈ VALID_TYPES;
  - `scope` ∈ VALID_SCOPES;
  - `theme` ∈ VALID_THEMES;
  - `direction`: contract per scope (see _normalize_direction);
  - `requires`: optional, AND, no duplicates.

Internal form of `direction`:
  - `both`  → frozenset({"uplink", "downlink"})
  - `none`  → frozenset()
  - `uplink` / `downlink` → frozenset({"uplink"}) / frozenset({"downlink"})
  - inconsistent combinations (e.g. `both` + `uplink`) → error.

Output:
  - ChannelDefinition (frozen dataclass), with `direction: frozenset[str]`
    normalised (only `uplink` and/or `downlink`, never `both` or `none`).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


# =============================================================================
# VOCABULARIES (the only place to edit to extend them)
# =============================================================================

# Keys allowed in a channel. Any other key is an error.
ALLOWED_FIELDS: frozenset[str] = frozenset({
    "id",
    "infoName",
    "type",
    "scope",
    "theme",
    "direction",
    "requires",
    "value",
    "candidate",   # BASE channel on trial (its id gets CANDIDATE_SUFFIX)
    "zeroAt",      # place of the logical zero of an analog channel
})

# Values of `zeroAt`: where the LOGICAL zero of an analog channel sits in its
# raw range — at 0 (`start`, the default) or at `CbusNeutral` (`neutral`).
# Storage (raw uint16_t) and frame are unchanged.
VALID_ZERO_AT: frozenset[str] = frozenset({"start", "neutral"})

# Suffix of a candidate channel, added by the builder (reserved in the `.cb`).
CANDIDATE_SUFFIX: str = "_NEXT"

# Tokens accepted for `value` on an analog channel.
# They map to uint16_t C++ literals (see src/core/system/combus/combus_res.h).
VALID_ANALOG_VALUES: frozenset[str] = frozenset({
    "CbusMinVal",
    "CbusNeutral",
    "CbusMaxVal",
})

# Tokens accepted for `value` on a digital channel.
VALID_DIGITAL_VALUES: frozenset[str] = frozenset({
    "low",
    "high",
    "fault",       # true, the safe value of a failsafe alarm (theme failsafe only)
})

# Bounds of a raw numeric analog value (integer accepted in [0..65535]).
ANALOG_VALUE_MIN: int = 0
ANALOG_VALUE_MAX: int = 65535

# Allowed types.
VALID_TYPES: frozenset[str] = frozenset({"analog", "digital"})

# Allowed scopes.
VALID_SCOPES: frozenset[str] = frozenset({"SYSTEM", "BASE", "BOARD"})

# Allowed themes. Extend here when a new theme appears.
VALID_THEMES: frozenset[str] = frozenset({
    "core",        # the controls of the machine (steering, throttle, gears...)
    "failsafe",    # the failsafe group: aggregators FAILSAFE_*, contributors *_FAIL
    "vbat",
    "light",
    "input",
    "sim",        # values computed by the simulation chains (HAS_SIM_PROC): ESC_SPEED_BUS, ESC_RPM_BUS, DRIVE_STATE_BUS
    "coupler",    # local channels of a coupler (command, engaged switch...)
    "remote",     # inputs sent by the remote (buttons, sticks); TODO(Simulation chain): the existing inputs move here
    # Add the new legitimate themes here.
})

# Canonical order of the scopes (used by the sort).
SCOPE_ORDER: dict[str, int] = {
    "SYSTEM": 0,
    "BASE": 1,
    "BOARD": 2,
}

# Surface tokens allowed in `direction`.  The `*_OR` variants add a logical OR
# merge policy to the wire direction.
DIRECTION_TOKENS: frozenset[str] = frozenset({
    "uplink", "downlink", "both", "none",
    "uplink_or", "downlink_or", "both_or",
})

# Surface tokens allowed per scope.  The `*_OR` variants are accepted on SYSTEM
# and BASE only: a BOARD channel has a single possible writer, so an OR merge
# has no meaning (last write wins).
DIRECTION_BY_SCOPE: dict[str, frozenset[str]] = {
    "SYSTEM": frozenset({"uplink", "downlink", "both", "none",
                        "uplink_or", "downlink_or", "both_or"}),
    "BASE": frozenset({"uplink", "downlink", "both", "none",
                         "uplink_or", "downlink_or", "both_or"}),
    "BOARD": frozenset({"none"}),  # the only surface value accepted
}

# Passthrough: `downlink_pt` is a BASE `downlink` that the board connected to
# another node redistributes inside the node.  The only `_pt` token accepted;
# `both_pt` is refused (several sources by nature).
PASSTHROUGH_TOKENS: frozenset[str] = frozenset({"downlink_pt"})

# The `*_OR` tokens, a subset of DIRECTION_TOKENS.
DIRECTION_OR_TOKENS: frozenset[str] = frozenset({"uplink_or", "downlink_or", "both_or"})



# =============================================================================
# ERRORS
# =============================================================================

class ChannelError(Exception):
    """Base class of the `channels:` errors."""


class ChannelValidationError(ChannelError):
    """A channel definition is invalid (structure or meaning)."""

    def __init__(self, path: Path, channel_id: str | None, message: str):
        self.path = path
        self.channel_id = channel_id
        loc = f" (channel id={channel_id!r})" if channel_id else ""
        super().__init__(f"{path}: {message}{loc}")


class ChannelConflictError(ChannelError):
    """Two definitions share the same id."""

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
    A validated and normalised channel definition.

    `direction` is ALWAYS a frozenset of `{"uplink", "downlink"}` (empty for
    `none` and BOARD).  The surface tokens `both` and `none` are normalised
    at validation and never appear here.

    `direction_or` is ALWAYS a frozenset of `{"uplink", "downlink"}` (empty
    without a `*_OR` token).  The surface tokens `uplink_or`, `downlink_or`
    and `both_or` are normalised at validation and never appear here.  The
    two are exclusive: a channel cannot have both `direction` and
    `direction_or` non-empty (an OR merge only makes sense when several
    sources write the same direction).

    `value` is `None` when the field is absent from the YAML; otherwise:
      - analog: one of the tokens `CbusMinVal` / `CbusNeutral` / `CbusMaxVal`,
                OR an integer in `[ANALOG_VALUE_MIN..ANALOG_VALUE_MAX]`;
      - digital: one of the tokens `low` / `high` / `fault`.
    The canonical form is kept as is (no implicit conversion), so that the
    C++ generator decides on the exact literal to emit.
    """

    id: str
    info_name: str
    type: str
    scope: str
    theme: str
    direction: frozenset[str]
    direction_or: frozenset[str]  # OR merge policy (empty without a `*_OR` token)
    requires: frozenset[str]
    source_path: Path
    raw: dict[str, Any] = field(default_factory=dict)
    value: str | int | None = None
    candidate: bool = False  # BASE channel on trial (id ends with _NEXT)
    passthrough: bool = False  # `downlink_pt`: redistributed in the node
    zero_at_neutral: bool = False  # `zeroAt: neutral`: logical zero at CbusNeutral

    def sort_key(self) -> tuple[int, str, str, str]:
        """Canonical sort key: (scope, type, theme, id)."""
        return (SCOPE_ORDER.get(self.scope, len(SCOPE_ORDER)),
                self.type, self.theme, self.id)

    @property
    def effective_direction(self) -> frozenset[str]:
        """Effective wire direction = direction | direction_or.

        The ONLY direction to use for the C++ bitmask and for the membership
        of the base and system views.  The `*_OR` tokens (uplink_or,
        downlink_or, both_or) fill `direction_or` only; on the wire (bitmask,
        transport) they behave as their plain equivalent.  This property
        unifies both forms for the consumers (generator, md5).

        `direction` and `direction_or` are exclusive (only one is non-empty),
        so the union is simply the one that is set.
        """
        return self.direction | self.direction_or


# =============================================================================
# VALIDATION: direction
# =============================================================================

def _normalize_direction(
    path: Path,
    channel_id: str | None,
    scope: str,
    type_: str,
    raw_dir: Any,
) -> tuple[frozenset[str], frozenset[str]]:
    """
    Validate and normalise `direction` according to the scope and the type.

    Rules:
      - field absent on BOARD → (frozenset(), frozenset()) (implicit none);
      - otherwise a single STRING ∈ {uplink, downlink, both, none,
                                     uplink_or, downlink_or, both_or};
      - BOARD: only 'none' (or the field absent) is accepted;
      - SYSTEM/BASE: 'uplink', 'downlink', 'both', 'none', per the policy
        of the scope;
      - `both` is a shortcut for {uplink, downlink};
      - `none` is a shortcut for {}.

    Rules of the `*_OR` variants:
      - digital only (refused on analog);
      - SYSTEM/BASE only (refused on BOARD);
      - exclusive with a plain direction: a channel cannot have both a
        classic wire direction and an OR merge;
      - normalisation:
          * `uplink_or`   → direction_or = {uplink},   direction = {}
          * `downlink_or` → direction_or = {downlink}, direction = {}
          * `both_or`     → direction_or = {uplink, downlink}, direction = {}

    Why a string and not a list: the values are mutually exclusive.  A list
    adds nothing and makes the YAML syntax heavier.

    Returns a tuple (direction, direction_or) of normalised frozensets (only
    uplink/downlink).  Only one of them is non-empty.
    """
    # BOARD: field absent → (frozenset(), frozenset())
    if raw_dir is None:
        if scope == "BOARD":
            return (frozenset(), frozenset())
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` is required for scope={scope!r} "
            "(BOARD may omit it and defaults to 'none')",
        )

    # Must be a string.
    if not isinstance(raw_dir, str):
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` must be a string, got {type(raw_dir).__name__} "
            f"({raw_dir!r}); accepted values: 'uplink', 'downlink', 'both', "
            f"'none', 'uplink_or', 'downlink_or', 'both_or'",
        )

    # BOARD: only 'none' is accepted (the `*_OR` tokens are refused here).
    if scope == "BOARD":
        if raw_dir == "none":
            return (frozenset(), frozenset())
        raise ChannelValidationError(
            path, channel_id,
            f"scope=BOARD does not have a wire direction; "
            f"only `direction: none` (or absent) is accepted, "
            f"got {raw_dir!r}",
        )

    # SYSTEM/BASE: the value must be a known token.
    if raw_dir not in DIRECTION_TOKENS:
        raise ChannelValidationError(
            path, channel_id,
            f"`direction` must be one of {sorted(DIRECTION_TOKENS)}, "
            f"got {raw_dir!r}",
        )

    # Tokens allowed per scope.
    allowed = DIRECTION_BY_SCOPE[scope]
    if raw_dir not in allowed:
        raise ChannelValidationError(
            path, channel_id,
            f"scope={scope!r} does not accept direction token {raw_dir!r}; "
            f"allowed for this scope: {sorted(allowed)}",
        )

    # `*_OR` on `type: digital` only.
    if raw_dir in DIRECTION_OR_TOKENS and type_ != "digital":
        raise ChannelValidationError(
            path, channel_id,
            f"`direction: {raw_dir!r}` (a `*_OR` variant) is only valid "
            f"on `type: digital` channels (OR-fusion has no meaning on "
            f"analog magnitudes); got `type: {type_!r}`",
        )

    # Normalisation: `both` → {uplink, downlink}, `none` → {},
    # `uplink`/`downlink` → {uplink}/{downlink}; `*_OR` → direction_or set,
    # direction empty.
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
# VALIDATION: requires
# =============================================================================

def _validate_requires(
    path: Path,
    channel_id: str | None,
    raw_req: Any,
) -> frozenset[str]:
    """
    Validate `requires`.

    Rules:
      - optional; absent or [] → frozenset() (active by default);
      - when present: a list of non-empty strings;
      - no duplicates;
      - meaning: AND (all the flags must be defined).  It is resolved
        against the build flags by generator.resolve_requires().
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
# VALIDATION: value
# =============================================================================

def _validate_value(
    path: Path,
    channel_id: str | None,
    type_: str,
    raw_val: Any,
) -> str | int | None:
    """
    Validate and normalise `value` according to the `type` of the channel.

    Rules:
      - field absent (`raw_val is None`) → None (generator default; parser.py
        refuses it in a `.cb`, where the value is the safe value);
      - explicit `value: null` → error (null is not an absent value);
      - analog:
          * token ∈ {"CbusMinVal", "CbusNeutral", "CbusMaxVal"} → str as is;
          * integer in [ANALOG_VALUE_MIN..ANALOG_VALUE_MAX] → int as is;
          * any other type, or out of bounds → error;
      - digital:
          * token ∈ VALID_DIGITAL_VALUES → str as is;
          * any other value → error;
      - strict across types:
          * low/high/fault on analog → error;
          * CbusMinVal/Neutral/MaxVal on digital → error;
          * integer on digital → error.

    Returns:
      - None when absent;
      - str (token) or int (raw integer) when present and valid.
    """
    # Absent: the field is not in the YAML.
    if raw_val is None:
        # Absent or null: PyYAML returns None for `key: null` AND for an
        # absent key, so the two cannot be told apart here.
        # validate_channel() does it first, with _check_value_explicit_null().
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
            # bool is a subclass of int in Python: excluded explicitly.
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
        # Other YAML types: float, list, dict...
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
            f"`value` for digital must be a string token ('low', 'high' or 'fault'), "
            f"got {type(raw_val).__name__} ({raw_val!r})",
        )

    # Unknown type (should not happen: validate_channel filters it first).
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
    Refuse an explicit `value: null` (not the same as an absent field).

    PyYAML returns None both for an absent key and for key=None.  The two
    are told apart here: "value" in the mapping AND raw["value"] None is an
    explicit error.
    """
    if "value" in raw and raw["value"] is None:
        raise ChannelValidationError(
            path, channel_id,
            "`value` is explicitly null; expected a token (e.g. "
            "'CbusNeutral', 'low') or an integer for analog. "
            "Omit the field to use the generator default.",
        )


# =============================================================================
# VALIDATION: a whole channel
# =============================================================================

def validate_channel(
    path: Path,
    raw: Any,
    index: int,
) -> ChannelDefinition:
    """
    Validate a raw channel and return a normalised ChannelDefinition.

    Raises ChannelValidationError on any problem (structure, meaning,
    unknown key, duplicate, inconsistent combination).
    """
    if not isinstance(raw, dict):
        raise ChannelValidationError(
            path, None,
            f"channel #{index} must be a mapping, got {type(raw).__name__}",
        )

    # --- unknown keys (refused explicitly) ---
    unknown = set(raw.keys()) - ALLOWED_FIELDS
    if unknown:
        raise ChannelValidationError(
            path, None,
            f"channel #{index} has unknown field(s) {sorted(unknown)!r}; "
            f"allowed fields are {sorted(ALLOWED_FIELDS)}",
        )

    # --- id (required, non-empty) ---
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

    # --- infoName (REQUIRED, non-empty) ---
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
    if not isinstance(ctype, str) or ctype not in VALID_TYPES:
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
    if not isinstance(cscope, str) or cscope not in VALID_SCOPES:
        raise ChannelValidationError(
            path, cid,
            f"`scope` must be one of {sorted(VALID_SCOPES)}, got {cscope!r}",
        )

    # --- BASE refuses `requires` ---
    # A BASE channel belongs to a fixed frame, from the generic profile of a
    # machine type, shared as is by the machine node and the remote.
    # `requires` (activation at compile time) would give frame variants
    # depending on the built config, while the frame must stay fixed per
    # machine type.  A non-empty `requires` on a BASE channel is refused,
    # with a message that points to the alternatives (SYSTEM/BOARD scope, or
    # no requires).
    if cscope == "BASE":
        raw_req = raw.get("requires")
        if raw_req is not None:
            # An empty list is accepted (same as no requires), anything else
            # is refused.
            if isinstance(raw_req, list) and len(raw_req) == 0:
                pass  # requires: [] is OK (same as absent)
            else:
                raise ChannelValidationError(
                    path, cid,
                    f"channel BASE {cid!r} declares requires={raw_req!r} "
                    f"— BASE channels represent a fixed machine-type "
                    f"frame and cannot be conditionally compiled; move "
                    f"this channel to SYSTEM/BOARD scope, or remove the "
                    f"requires",
                )

    # --- theme (from VALID_THEMES) ---
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
    if not isinstance(ctheme, str) or ctheme not in VALID_THEMES:
        raise ChannelValidationError(
            path, cid,
            f"`theme` must be one of {sorted(VALID_THEMES)}, got {ctheme!r}; "
            f"add the new theme to VALID_THEMES in canon/channels.py if "
            f"this is intentional",
        )

    # --- direction (contract per scope and type) ---
    # `downlink_pt` is a `downlink` redistributed inside the node by the board
    # connected to another node; BASE only (SYSTEM is already diffused);
    # `both_pt` refused (several sources by nature).
    raw_direction = raw.get("direction")
    passthrough = False
    if isinstance(raw_direction, str) and raw_direction.endswith("_pt"):
        if raw_direction == "both_pt":
            raise ChannelValidationError(
                path, cid,
                "`direction: both_pt` is refused: a channel relayed in both directions "
                "has several sources; use `both` (two nodes) or `both_or`",
            )
        if raw_direction not in PASSTHROUGH_TOKENS:
            raise ChannelValidationError(
                path, cid,
                f"`direction: {raw_direction}` is not supported; the only `_pt` token "
                f"is {sorted(PASSTHROUGH_TOKENS)}",
            )
        if cscope != "BASE":
            raise ChannelValidationError(
                path, cid,
                f"`_pt` is for BASE channels only (scope is {cscope}): a SYSTEM "
                f"channel is already diffused in the node",
            )
        passthrough = True
        raw_direction = raw_direction[:-len("_pt")]
    direction, direction_or = _normalize_direction(
        path, cid, cscope, ctype, raw_direction,
    )

    # --- requires ---
    requires = _validate_requires(path, cid, raw.get("requires"))

    # --- value (optional) ---
    # Absent and null are told apart BEFORE _validate_value (which returns
    # None for both).
    _check_value_explicit_null(path, cid, raw)
    value = _validate_value(path, cid, ctype, raw.get("value"))

    # --- candidate (optional) ---
    # A BASE channel on trial: out of the stable set and of its MD5, sent in
    # next mode only.  The `.cb` gives the final name and the builder adds
    # the suffix: the status shows everywhere in the code, and removing
    # `candidate` promotes the channel under its final name.  The suffix is
    # reserved to the builder.
    candidate = raw.get("candidate", False)
    if not isinstance(candidate, bool):
        raise ChannelValidationError(
            path, cid, f"`candidate` must be true or false, got {candidate!r}",
        )
    if candidate and cscope != "BASE":
        raise ChannelValidationError(
            path, cid,
            f"only a BASE channel can be a candidate (scope is {cscope})",
        )
    if cid.endswith(CANDIDATE_SUFFIX):
        raise ChannelValidationError(
            path, cid,
            f"the suffix {CANDIDATE_SUFFIX!r} is reserved: write the final name and "
            f"`candidate: true`, the builder adds the suffix",
        )
    if candidate:
        cid = cid + CANDIDATE_SUFFIX

    # --- zeroAt (optional) ---
    # Place of the LOGICAL zero in the raw range of an analog channel: at the
    # start (`start`, the default) or at neutral (`neutral`).  Changes neither
    # the storage (raw uint16_t) nor the frame.
    raw_zero_at = raw.get("zeroAt", "start")
    if not isinstance(raw_zero_at, str) or raw_zero_at not in VALID_ZERO_AT:
        raise ChannelValidationError(
            path, cid,
            f"`zeroAt` is one of {sorted(VALID_ZERO_AT)}, got {raw_zero_at!r}",
        )
    if "zeroAt" in raw and ctype != "analog":
        raise ChannelValidationError(
            path, cid,
            f"`zeroAt` is for an analog channel only (type is {ctype})",
        )
    zero_at_neutral = raw_zero_at == "neutral"

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
        candidate=candidate,
        passthrough=passthrough,
        zero_at_neutral=zero_at_neutral,
    )


# =============================================================================
# EXTRACTION FROM THE PARSED FILES
# =============================================================================

def extract_channels_sections(
    parsed: list[tuple[Path, str, Any]],
) -> list[tuple[Path, list[Any]]]:
    """
    Extract the `channels:` section of each document parsed by parser.py.

    The selection is by KEY, not by extension: a `.cbch` with a `channels:`
    section is handled as a `.cb` with a `channels:` section.

    Precisely:
      - key `channels:` absent   → skipped silently
      - `channels: null`         → explicit validation error
      - `channels: []`           → valid empty section
      - `channels: [...]`        → valid non-empty section

    The documents without `channels:` may hold other sections (such as
    `chains:`), which are not handled here.
    """
    out: list[tuple[Path, list[Any]]] = []
    for path, _type_label, raw in parsed:
        if not isinstance(raw, dict):
            continue
        if "channels" not in raw:
            # Key absent: the file declares no channel.
            continue
        section = raw["channels"]
        if section is None:
            # Key present but null: explicit error.
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
# MERGE AND CONFLICTS
# =============================================================================

class ChannelValueConflictError(ChannelError):
    """Two definitions of the same channel carry incompatible `value`s."""

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
    Validate each channel, detect the id conflicts, return the merged list
    (not sorted).

    Raises ChannelValidationError, ChannelConflictError or
    ChannelValueConflictError.

    An id defined twice is always an error: ChannelValueConflictError when
    the two `value`s differ, ChannelConflictError otherwise (no silent merge).
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

    # The final name of a candidate (without the suffix) must be free:
    # promoting it would otherwise create a duplicate.
    for ch in by_id.values():
        if not ch.candidate:
            continue
        final_name = ch.id[:-len(CANDIDATE_SUFFIX)]
        if final_name in by_id:
            raise ChannelValidationError(
                ch.source_path, ch.id,
                f"candidate {final_name!r} has the name of the channel defined in "
                f"{by_id[final_name].source_path}; promoting it would create a duplicate",
            )
    return list(by_id.values())


# =============================================================================
# CANONICAL SORT
# =============================================================================

def canonize_channels(channels: list[ChannelDefinition]) -> list[ChannelDefinition]:
    """Canonical sort: (scope, type, theme, id)."""
    return sorted(channels, key=lambda ch: ch.sort_key())
