#!/usr/bin/env python3
"""
combus_builder/diffusion.py — ND2: automatic diffusion of LOCAL channels
between boards of the same node.

Scope (ND2):
  - Generalised to N >= 2 boards per node.
  - For each channel id with scope LOCAL, look at the set of boards
    for which the declaration is ACTIVE (via the existing
    `_is_active_for_board()` / `requires` logic — that part is
    unchanged). Apply the generalised rule below to decide what
    each board's final direction is, and what synthetic
    declarations (if any) to inject on boards that didn't declare
    the channel.

Generalised rule (ND2, per `id`):
  Let
    n_up   = number of boards active with direction `uplink`
    n_down = number of boards active with direction `downlink`
    n_both = number of boards active with direction `both`
    n_none = number of boards active with direction `none`
    absent = boards of the node where the channel is NOT active.

  1. n_up == n_down == n_both == n_none == 0
       -> channel inactive for the whole node, ignored.

  2. n_both >= 1 AND (n_up >= 1 OR n_down >= 1)
       -> DiffusionConflictError FATAL, flag "semantic". Message
          names ALL boards involved (not just 2).

  3. n_both >= 1, n_up == 0, n_down == 0
       -> mode agrégation OR: every ABSENT board receives `both`.
          Boards explicitly `none` stay `none` (they do NOT join
          the OR pool). No error.

  4. n_up >= 2 (n_both == 0 by case 2)
       -> DiffusionConflictError FATAL. Message names ALL boards
          that declared `uplink` (not just 2).

  5. n_up == 1, n_down >= 0 (n_both == 0 by case 2)
       -> compatible. Every absent board AND every board already
          `downlink` explicit stays/becomes `downlink`. The single
          `uplink` board is unchanged.

  6. n_up == 0, n_down >= 1 (n_both == 0 by case 2)
       -> CHANGED BEHAVIOUR vs ND2 2-board: not a fatal error
          anymore. Emit a WARNING ("canal mort" — no producer
          uplink in the node). Absent boards receive `none`
          (we never invent a producer). Build continues.

  7. n_up == 0, n_down == 0, n_both == 0, n_none >= 1
       -> every active board is `none`. No diffusion, no warning,
          no error. Absent boards also stay absent (no synthetic
          declaration needed; downstream canonisation treats
          absent as not present).

  Intra-board conflict (same `id` declared twice on the SAME
  board) is handled separately as DiffusionLocalConflictError — it
  is detected BEFORE the inter-board rule, so it always wins over
  the rule above.

Out of scope (deferred):
  - REMOTE diffusion (ND5).
  - MD5 ONE_WAY canonisation (ND7).
  - Runtime BOTH_OR aggregation wiring (ND6).
  - merge_channels() in canon/channels.py is NOT touched: the
    per-board lists produced here contain at most one declaration
    per id (by construction), so merge_channels() never sees a
    duplicate.

Pipeline position (ND2):
  - discover_and_parse() runs once (repo-wide, board-agnostic).
  - diffuse_local_channels() runs ONCE, BEFORE canonize(), producing
    a dict[board_id -> list[tuple[Path, str, Any]]] in the same
    format as the output of discover_and_parse().
  - canonize() runs PER BOARD on its own resolved list.
  - generate() runs PER BOARD (already in place since ND1).

Node topology (ND2, generalised):
  - N >= 2 boards. The "point-to-point" restriction of the
    previous version is dropped. The rule is symmetric in the
    sense that it only depends on the multiset of active
    directions across the node.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .flags import BuildContext
from .node_context import NodeContext


# =============================================================================
# ERRORS
# =============================================================================

class DiffusionError(Exception):
    """Base class for ND2 diffusion errors."""


class DiffusionConflictError(DiffusionError):
    """Inter-board conflict on a LOCAL channel direction.

    Raised when N >= 2 boards of the same node declare the same
    LOCAL channel id with INCOMPATIBLE directions (e.g. two
    `uplink`, or `both` mixed with a simple direction).

    The message names ALL boards involved (not just 2).
    """

    def __init__(
        self,
        channel_id: str,
        involved: list[tuple[str | None, str, Path]],
        *,
        semantic: bool = False,
    ):
        self.channel_id = channel_id
        self.involved = involved  # [(board_id, direction, source_path), ...]
        self.semantic = semantic
        if semantic:
            preview = ", ".join(
                f"{b!r}({d})" for b, d, _p in involved
            )
            msg = (
                f"conflit de semantique sur le channel LOCAL {channel_id!r}: "
                f"both (agregation OR) et une direction simple ne sont pas "
                f"melangeables sur un meme canal.\n"
                f"  boards impliquees: {preview}\n"
            )
        else:
            preview = ", ".join(
                f"{b!r}({d})" for b, d, _p in involved
            )
            msg = (
                f"conflit sur le channel LOCAL {channel_id!r} entre les "
                f"boards suivantes:\n"
                f"  {preview}\n"
            )
        # Append per-board source paths for traceability.
        for board_id, _direction, path in involved:
            msg += f"    - {board_id}: {path}\n"
        super().__init__(msg)


class DiffusionLocalConflictError(DiffusionError):
    """A single board has multiple declarations of the same channel id.

    This is a LOCAL conflict (the board's own .cb files disagree with
    each other), distinct from DiffusionConflictError (inter-board).
    """

    def __init__(
        self,
        channel_id: str,
        board_id: str | None,
        path_a: Path,
        path_b: Path,
    ):
        self.channel_id = channel_id
        self.board_id = board_id
        self.path_a = path_a
        self.path_b = path_b
        super().__init__(
            f"channel {channel_id!r} declare plusieurs fois dans les .cb "
            f"de la board {board_id!r}, voir {path_a} et {path_b}. "
            f"C'est un conflit LOCAL (intra-board), pas un conflit "
            f"inter-board : corriger les .cb de cette board avant de "
            f"continuer."
        )


class DiffusionDeadChannelWarning(DiffusionError):
    """Non-fatal: a LOCAL channel has only `downlink` consumers
    and no `uplink` producer in the node (canal mort).

    NOTE: this is a WARNING, not a fatal error. It inherits from
    DiffusionError only for typing convenience; the orchestrator
    catches it BEFORE re-raising if it wants to continue the build.
    The default orchestrator behaviour is to log it via
    `_warn()` and continue.
    """


# =============================================================================
# DATA STRUCTURE
# =============================================================================

@dataclass(frozen=True)
class _RawChannelDecl:
    """A single raw channel declaration extracted from a .cb file.

    `raw` is the dict as parsed by PyYAML (with `id`, `infoName`,
    `type`, `scope`, `theme`, `direction`, `requires`, `value` keys).
    `source_path` is the .cb file the declaration came from.
    `board_id` is the board this declaration belongs to (None for
    the mother env in legacy mode, or the board_id in node mode).
    """

    raw: dict[str, Any]
    source_path: Path
    board_id: str | None


# =============================================================================
# WARNING CHANNEL
# =============================================================================

def _warn(msg: str) -> None:
    """Emit a non-fatal warning to stderr.

    Uses a simple stderr print prefixed with "[combus_diffusion]
    WARNING:" — no new logging system is introduced. The orchestrator
    in combus_scons_hook.py catches DiffusionDeadChannelWarning and
    forwards it here.
    """
    sys.stderr.write(f"[combus_diffusion] WARNING: {msg}\n")
    sys.stderr.flush()


# =============================================================================
# DIRECTION HELPERS
# =============================================================================

# Mirror table for diffusion (ND2).
# When a channel is declared on exactly one board, the other board
# receives a synthetic declaration with the direction MIRRORED.
_DIRECTION_MIRROR: dict[str, str] = {
    "uplink": "downlink",
    "downlink": "uplink",
    "both": "both",
    "none": "none",
}


def _mirror_direction(direction: str) -> str:
    """Return the mirrored direction for diffusion."""
    try:
        return _DIRECTION_MIRROR[direction]
    except KeyError as e:
        raise DiffusionError(
            f"unknown direction token {direction!r}; expected one of "
            f"{sorted(_DIRECTION_MIRROR)}"
        ) from e


# =============================================================================
# REQUIRES FILTERING (raw variant) — UNCHANGED
# =============================================================================

def _is_active_for_board(
    raw: dict[str, Any],
    ctx: BuildContext,
) -> bool:
    """Return True iff the raw declaration is active for the given board.

    Mirrors the logic of resolve_requires() in generator.py, but
    operates on the raw dict (pre-validation) since at this stage
    validate_channel() has not been called yet.

    A declaration is active iff every flag in its `requires` list is
    present in ctx (with or without value). An absent or empty
    `requires` means "active by default".
    """
    raw_req = raw.get("requires")
    if raw_req is None:
        return True
    if not isinstance(raw_req, list):
        # Malformed requires will be caught later by validate_channel().
        # For diffusion purposes, treat as "active" so the conflict
        # surfaces at the right layer (canonisation, not diffusion).
        return True
    for req in raw_req:
        if not isinstance(req, str) or not req:
            continue  # let validate_channel() reject it
        if not (ctx.has(req) or ctx.value_of(req) is not None):
            return False
    return True


# =============================================================================
# EXTRACTION — UNCHANGED
# =============================================================================

def _extract_raw_declarations(
    parsed: list[tuple[Path, str, Any]],
    board_id: str | None,
) -> list[_RawChannelDecl]:
    """Extract raw channel declarations from parsed files for one board.

    Iterates over every (path, type_label, raw_dict) tuple, picks the
    ones with type_label == "channel_def" and a `channels:` section,
    and returns a flat list of _RawChannelDecl.

    NOTE: this function does NOT filter by scope. The caller decides
    which declarations to keep (LOCAL for diffusion, REMOTE/SYSTEM
    pass-through).
    """
    out: list[_RawChannelDecl] = []
    for path, type_label, raw in parsed:
        if type_label != "channel_def":
            continue
        if not isinstance(raw, dict):
            continue
        section = raw.get("channels")
        if section is None:
            continue
        if not isinstance(section, list):
            # Malformed; let canonisation reject it later.
            continue
        for decl in section:
            if isinstance(decl, dict):
                out.append(_RawChannelDecl(
                    raw=decl,
                    source_path=path,
                    board_id=board_id,
                ))
    return out


# =============================================================================
# GROUP RESOLUTION (N-board rule)
# =============================================================================

@dataclass(frozen=True)
class _ResolvedGroup:
    """Result of resolving the directions of one LOCAL channel id
    across all boards of a node.

    `explicit`     = (board_id, direction, source_path) for every
                     board where the declaration is EXPLICITLY
                     active. Already de-duplicated (intra-board
                     conflicts have been raised before this
                     struct is built).
    `absent`       = list of board_ids where the channel is NOT
                     active. These are the boards that may receive
                     a synthetic declaration depending on the rule.
    `synth_for_absent` = (board_id, synthetic_direction) for every
                     absent board that must receive a synthetic
                     declaration, with the direction to inject.
                     Empty if no synthesis is needed.
    `warning`      = optional human-readable warning message
                     (currently only used for the "canal mort"
                     case, rule 6).
    """

    explicit: list[tuple[str | None, str, Path]]
    absent: list[str | None]
    synth_for_absent: list[tuple[str | None, str]]
    warning: str | None = None


def _resolve_group(
    channel_id: str,
    explicit: list[tuple[str | None, str, Path]],
    all_board_ids: list[str | None],
) -> _ResolvedGroup:
    """Apply the generalised ND2 rule to one LOCAL channel id.

    Args:
      channel_id:  the channel id (only used for error/warning messages).
      explicit:    list of (board_id, direction, source_path) for every
                   board where the declaration is EXPLICITLY active.
                   By the time we get here, intra-board duplicates
                   have already been raised as DiffusionLocalConflictError,
                   so each board_id appears at most once.
      all_board_ids: the full list of board_ids in the node (in
                     declaration order, with None for the mother
                     env if present). Used to compute the `absent`
                     set.

    Returns:
      _ResolvedGroup. If the rule mandates a fatal error, raises
      DiffusionConflictError instead of returning.

    Notes on the rule (see module docstring for full spec):
      - Rule 2 (both + simple): FATAL.
      - Rule 3 (both only):     absent -> both.
      - Rule 4 (n_up >= 2):     FATAL.
      - Rule 5 (n_up == 1):     absent + explicit downlink -> downlink.
      - Rule 6 (n_up == 0,
                n_down >= 1):   WARNING ("canal mort"),
                                absent -> none (never invent
                                a producer).
      - Rule 7 (n_none only):   no synthesis, no warning.
    """
    explicit_set = {b for b, _d, _p in explicit}
    absent = [b for b in all_board_ids if b not in explicit_set]

    # Tally directions across explicit declarations.
    n_up = sum(1 for _b, d, _p in explicit if d == "uplink")
    n_down = sum(1 for _b, d, _p in explicit if d == "downlink")
    n_both = sum(1 for _b, d, _p in explicit if d == "both")
    n_none = sum(1 for _b, d, _p in explicit if d == "none")

    # --- Rule 2: both + (uplink or downlink) -> semantic FATAL --------
    if n_both >= 1 and (n_up >= 1 or n_down >= 1):
        # Build the involved list: all explicit declarations (the
        # both ones and the simple-direction ones are the conflict).
        involved = list(explicit)
        raise DiffusionConflictError(
            channel_id=channel_id,
            involved=involved,
            semantic=True,
        )

    # --- Rule 3: both only (no uplink, no downlink) -> OR aggregation -
    if n_both >= 1 and n_up == 0 and n_down == 0:
        synth = [(b, "both") for b in absent]
        return _ResolvedGroup(
            explicit=explicit,
            absent=absent,
            synth_for_absent=synth,
        )

    # --- Rule 4: n_up >= 2 -> FATAL -----------------------------------
    if n_up >= 2:
        # Only the uplink boards are the actual conflict. Naming all
        # `uplink` boards (not just 2) in the message.
        involved = [(b, d, p) for b, d, p in explicit if d == "uplink"]
        raise DiffusionConflictError(
            channel_id=channel_id,
            involved=involved,
            semantic=False,
        )

    # --- Rule 5: n_up == 1, n_down >= 0 -> compatible -----------------
    if n_up == 1:
        # The single uplink board stays as is. Every absent board AND
        # every explicit downlink board is a consumer (downlink). No
        # synthesis needed on explicit downlink boards (they already
        # have the right direction); synthesis is only needed on
        # absent boards.
        synth = [(b, "downlink") for b in absent]
        return _ResolvedGroup(
            explicit=explicit,
            absent=absent,
            synth_for_absent=synth,
        )

    # --- Rule 6: n_up == 0, n_down >= 1 -> WARNING "canal mort" -------
    if n_up == 0 and n_down >= 1:
        # CHANGED BEHAVIOUR vs the 2-board version: no longer fatal.
        # Emit a warning naming the downlink boards.
        down_boards = [
            f"{b!r}" for b, d, _p in explicit if d == "downlink"
        ]
        warning = (
            f"channel {channel_id!r}: {len(down_boards)} board(s) en "
            f"downlink sans aucun uplink producteur dans le node — "
            f"canal mort. boards concernees: {', '.join(down_boards)}. "
            f"les boards absentes recoivent `none` (pas d'inventeur)."
        )
        # Absent boards receive `none` (we never invent a producer).
        # Synthesize a `none` declaration for each absent board so
        # the downstream canon layer sees a uniform picture; if no
        # synthesis is desired, just return synth=[] — but we
        # choose to synthesise `none` so per-board output is
        # consistent (every board has a declaration for the id).
        synth = [(b, "none") for b in absent]
        return _ResolvedGroup(
            explicit=explicit,
            absent=absent,
            synth_for_absent=synth,
            warning=warning,
        )

    # --- Rule 7: only n_none (or nothing) -----------------------------
    # No uplink, no downlink, no both — only `none` declarations
    # (or no explicit declarations at all, but in that case we
    # would have already returned earlier with synth=[] and no
    # warning, since explicit would be empty and absent would be
    # the full node; but the outer caller skips the diffusion rule
    # for empty explicit lists).
    # Either way, nothing to synthesise, no warning.
    return _ResolvedGroup(
        explicit=explicit,
        absent=absent,
        synth_for_absent=[],
    )


# =============================================================================
# DIFFUSION (main entry point)
# =============================================================================

def diffuse_local_channels(
    parsed: list[tuple[Path, str, Any]],
    node: NodeContext,
) -> dict[str | None, list[tuple[Path, str, Any]]]:
    """
    Diffuse LOCAL channels across the boards of a node.

    Args:
      parsed: output of discover_and_parse() (repo-wide, board-agnostic).
      node:   NodeContext with boards populated (board_id -> BuildContext).
              The mother env (board_id=None) is included if present.

    Returns:
      dict[board_id -> list[tuple[Path, str, Any]]] in the same format
      as the input `parsed`, but restricted to the channels resolved
      for THAT board after diffusion:
        - LOCAL channels: explicit declarations + synthetic
          declarations (mirrored or `both` or `none` depending on
          the generalised rule) for boards that didn't declare them.
        - REMOTE / SYSTEM channels: passed through unchanged,
          duplicated into every board (no diffusion applied).

    Raises:
      DiffusionLocalConflictError: a single board has multiple
        declarations of the same LOCAL channel id in its own .cb files.
      DiffusionConflictError: N >= 2 boards declare the same LOCAL
        channel id with INCOMPATIBLE directions (rule 2 or rule 4).

    Side effects:
      May emit non-fatal warnings to stderr (rule 6, "canal mort").
      These do NOT abort the build.

    NOTE: this function does NOT call validate_channel(). The returned
    per-board lists are still raw (pre-validation). canonize() will
    validate them downstream; merge_channels() will see at most one
    declaration per id (by construction), so its uniqueness constraint
    is respected.
    """
    # The full list of board_ids in the node, in stable order. We
    # use this to compute the `absent` set per (id, scope) below.
    all_board_ids: list[str | None] = list(node.boards.keys())

    # Step 1: collect raw declarations per board.
    decls_by_board: dict[str | None, list[_RawChannelDecl]] = {}
    for board_id, ctx in node.boards.items():
        decls_by_board[board_id] = _extract_raw_declarations(parsed, board_id)

    # Step 2: bucket declarations by (id, scope).
    # We need to know, for each LOCAL id, which boards have an active
    # declaration and what direction each one declares.
    # Key: (channel_id, scope) -> list of (board_id, direction, path)
    by_id_scope: dict[tuple[str, str], list[tuple[str | None, str, Path]]] = {}

    # Also collect REMOTE/SYSTEM declarations for pass-through.
    # Key: (channel_id, scope) -> list of (board_id, raw_decl, path)
    passthrough: dict[tuple[str, str], list[tuple[str | None, dict[str, Any], Path]]] = {}

    for board_id, decls in decls_by_board.items():
        ctx = node.boards[board_id]
        for decl in decls:
            cid = decl.raw.get("id")
            scope = decl.raw.get("scope")
            if not isinstance(cid, str) or not cid:
                continue  # let canonisation reject it
            if scope not in ("LOCAL", "REMOTE", "SYSTEM"):
                continue  # let canonisation reject it

            if scope == "LOCAL":
                if not _is_active_for_board(decl.raw, ctx):
                    continue
                direction = decl.raw.get("direction", "none")
                if not isinstance(direction, str):
                    direction = "none"
                by_id_scope.setdefault((cid, scope), []).append(
                    (board_id, direction, decl.source_path)
                )
            else:
                # REMOTE / SYSTEM: pass through, no diffusion.
                passthrough.setdefault((cid, scope), []).append(
                    (board_id, decl.raw, decl.source_path)
                )

    # Step 3: detect LOCAL conflicts (intra-board) and resolve
    # the inter-board rule via _resolve_group() for every (id, scope).
    #
    # For each LOCAL id, we:
    #   1. Group the explicit declarations by board_id, raise
    #      DiffusionLocalConflictError on intra-board duplicates.
    #   2. Collect the (board_id, direction, path) list, at most
    #      one per board.
    #   3. Call _resolve_group() to apply the N-board rule.
    #   4. Record the resolved syntheses in
    #      `resolved_by_id[(cid, LOCAL)]` so step 4 can use them.
    resolved_by_id: dict[tuple[str, str], _ResolvedGroup] = {}

    for (cid, scope), entries in by_id_scope.items():
        if scope != "LOCAL":
            continue  # REMOTE/SYSTEM pass-through is separate
        # Group by board_id to detect intra-board duplicates.
        per_board: dict[str | None, tuple[str, Path]] = {}
        for board_id, direction, path in entries:
            if board_id in per_board:
                # Intra-board conflict.
                _dir_a, path_a = per_board[board_id]
                raise DiffusionLocalConflictError(
                    channel_id=cid,
                    board_id=board_id,
                    path_a=path_a,
                    path_b=path,
                )
            per_board[board_id] = (direction, path)

        # Build the explicit list in all_board_ids order for
        # deterministic error messages.
        explicit: list[tuple[str | None, str, Path]] = []
        for b in all_board_ids:
            if b in per_board:
                d, p = per_board[b]
                explicit.append((b, d, p))

        # Apply the N-board rule. May raise DiffusionConflictError
        # (fatal) or emit a warning (rule 6, "canal mort") which is
        # non-fatal — we capture the warning message and route it
        # to _warn() below.
        group = _resolve_group(cid, explicit, all_board_ids)
        resolved_by_id[(cid, scope)] = group

        if group.warning is not None:
            _warn(group.warning)

    # Step 4: build the per-board resolved lists.
    # For each board, we need:
    #   - Its own LOCAL declarations (unchanged).
    #   - Synthetic LOCAL declarations for ids declared by OTHER
    #     boards (with the direction dictated by _resolve_group()).
    #   - All REMOTE/SYSTEM declarations (pass-through, one per id).
    result: dict[str | None, list[tuple[Path, str, Any]]] = {}

    for board_id in node.boards:
        # Collect this board's own LOCAL declarations (by id), so
        # we know which ids to NOT synthesise on this board.
        own_local_ids: set[str] = set()
        for decl in decls_by_board.get(board_id, []):
            cid = decl.raw.get("id")
            scope = decl.raw.get("scope")
            if not isinstance(cid, str) or not cid:
                continue
            if scope == "LOCAL" and _is_active_for_board(
                decl.raw, node.boards[board_id]
            ):
                own_local_ids.add(cid)

        # Build the list of LOCAL declarations for this board.
        local_decls_for_board: list[tuple[Path, str, Any]] = []

        # 4a. Own LOCAL declarations (unchanged).
        for decl in decls_by_board.get(board_id, []):
            cid = decl.raw.get("id")
            scope = decl.raw.get("scope")
            if not isinstance(cid, str) or not cid:
                continue
            if scope != "LOCAL":
                continue
            if not _is_active_for_board(decl.raw, node.boards[board_id]):
                continue
            local_decls_for_board.append((
                decl.source_path,
                "channel_def",
                {"channels": [decl.raw]},
            ))

        # 4b. Synthetic LOCAL declarations dictated by _resolve_group().
        for (cid, scope), group in resolved_by_id.items():
            if scope != "LOCAL":
                continue
            if cid in own_local_ids:
                continue  # already declared by this board
            # Find the synthetic direction for THIS board, if any.
            for synth_board, synth_dir in group.synth_for_absent:
                if synth_board != board_id:
                    continue
                # Find the original raw dict to mirror the metadata
                # (theme, type, infoName, value, etc.) from the
                # board that originally declared the channel. We
                # pick the first explicit declaration in the group
                # (any board's content is fine for metadata; only
                # the direction is overridden by the rule).
                if not group.explicit:
                    # No explicit declaration to copy metadata from
                    # (e.g. rule 6 with all-n_none: shouldn't reach
                    # here, but be defensive).
                    continue
                source_board, _source_dir, source_path = group.explicit[0]
                source_decl = None
                for d in decls_by_board.get(source_board, []):
                    if (
                        d.raw.get("id") == cid
                        and d.raw.get("scope") == "LOCAL"
                        and _is_active_for_board(
                            d.raw, node.boards[source_board]
                        )
                    ):
                        source_decl = d
                        break
                if source_decl is None:
                    continue
                synthetic_raw = dict(source_decl.raw)
                synthetic_raw["direction"] = synth_dir
                # Use a virtual Path to mark the synthetic origin.
                virtual_path = Path(f"<diffusion:{board_id}>")
                local_decls_for_board.append((
                    virtual_path,
                    "channel_def",
                    {"channels": [synthetic_raw]},
                ))
                break  # at most one synthesis per board per id

        # 4c. REMOTE / SYSTEM pass-through (one declaration per id).
        # We pick the first declaration seen for each (id, scope)
        # pair. In practice, all boards see the same REMOTE/SYSTEM
        # declarations (they come from the same repo-wide .cb files),
        # so picking the first is fine.
        seen_passthrough: set[tuple[str, str]] = set()
        for (cid, scope), entries in passthrough.items():
            if (cid, scope) in seen_passthrough:
                continue
            seen_passthrough.add((cid, scope))
            # Pick the first entry (any board will do, content is
            # identical).
            _bd, raw_decl, source_path = entries[0]
            local_decls_for_board.append((
                source_path,
                "channel_def",
                {"channels": [raw_decl]},
            ))

        result[board_id] = local_decls_for_board

    return result
