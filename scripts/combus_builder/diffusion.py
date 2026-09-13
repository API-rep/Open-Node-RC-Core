#!/usr/bin/env python3
"""
combus_builder/diffusion.py — ND2: automatic diffusion of LOCAL channels
between boards of the same node.

Scope (ND2 only):
  - For each channel id with scope LOCAL, if exactly one board of the
    node declares it explicitly, the OTHER board receives a synthetic
    declaration with the direction MIRRORED (uplink<->downlink,
    both->both, none->none).
  - If both boards declare it explicitly, validate compatibility:
      * {uplink, downlink}  -> OK (complementary pair)
      * {uplink, uplink}    -> FATAL (conflict)
      * {downlink, downlink} -> FATAL (conflict)
      * {both, both}        -> OK (BOTH_OR aggregation, runtime wiring
                                 out of ND2 scope)
      * {none, none}        -> OK
      * {none, X}           -> OK (X = uplink/downlink/both)
      * {both, uplink} or {both, downlink} -> FATAL (semantic conflict)
  - If a single board has MULTIPLE declarations of the same id in its
    own .cb files -> FATAL (local conflict, distinct error class).
  - Channels with scope REMOTE or SYSTEM are NOT touched by diffusion
    (ND5 / ND7 are out of scope here). They are passed through to every
    board unchanged.

Out of scope (ND2):
  - REMOTE diffusion (ND5).
  - MD5 ONE_WAY canonisation (ND7).
  - Runtime BOTH_OR aggregation wiring (ND6).
  - merge_channels() in canon/channels.py is NOT touched: the
    per-board lists produced here contain at most one declaration per
    id (by construction), so merge_channels() never sees a duplicate.

Pipeline position (ND2):
  - discover_and_parse() runs once (repo-wide, board-agnostic).
  - diffuse_local_channels() runs ONCE, BEFORE canonize(), producing
    a dict[board_id -> list[tuple[Path, str, Any]]] in the same format
    as the output of discover_and_parse().
  - canonize() runs PER BOARD on its own resolved list.
  - generate() runs PER BOARD (already in place since ND1).

Node topology (ND2):
  - Strictly point-to-point (2 boards). The current handshake is not
    designed for more than 2 cards, so we don't generalise to N boards.
"""
from __future__ import annotations

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

    Raised when two boards of the same node declare the same LOCAL
    channel id with INCOMPATIBLE directions (e.g. both uplink, or
    both + uplink).
    """

    def __init__(
        self,
        channel_id: str,
        board_a: str | None,
        board_b: str | None,
        direction_a: str,
        direction_b: str,
        path_a: Path,
        path_b: Path,
        *,
        semantic: bool = False,
    ):
        self.channel_id = channel_id
        self.board_a = board_a
        self.board_b = board_b
        self.direction_a = direction_a
        self.direction_b = direction_b
        self.path_a = path_a
        self.path_b = path_b
        self.semantic = semantic
        if semantic:
            msg = (
                f"conflit de semantique sur le channel LOCAL {channel_id!r} "
                f"entre {board_a!r} ({direction_a}) et {board_b!r} ({direction_b}): "
                f"both (agregation OR) et une direction simple ne sont pas "
                f"melangeables sur un meme canal.\n"
                f"  - {board_a}: {path_a}\n"
                f"  - {board_b}: {path_b}"
            )
        else:
            msg = (
                f"conflit sur le channel LOCAL {channel_id!r} entre "
                f"{board_a!r} ({direction_a}) et {board_b!r} ({direction_b}).\n"
                f"  - {board_a}: {path_a}\n"
                f"  - {board_b}: {path_b}"
            )
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


# =============================================================================
# DATA STRUCTURE
# =============================================================================

@dataclass(frozen=True)
class _RawChannelDecl:
    """A single raw channel declaration extracted from a .cb file.

    `raw` is the dict as parsed by PyYAML (with `id`, `infoName`,
    `type`, `scope`, `theme`, `direction`, `requires`, `value` keys).
    `source_path` is the .cb file the declaration came from.
    `board_id` is the board this declaration belongs to (None for the
    mother env in legacy mode, or the board_id in node mode).
    """

    raw: dict[str, Any]
    source_path: Path
    board_id: str | None


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
# REQUIRES FILTERING (raw variant)
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
# EXTRACTION
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
        - LOCAL channels: explicit declarations + synthetic mirror
          declarations for boards that didn't declare them.
        - REMOTE / SYSTEM channels: passed through unchanged, duplicated
          into every board (no diffusion applied).

    Raises:
      DiffusionLocalConflictError: a single board has multiple
        declarations of the same LOCAL channel id in its own .cb files.
      DiffusionConflictError: two boards declare the same LOCAL channel
        id with INCOMPATIBLE directions.

    NOTE: this function does NOT call validate_channel(). The returned
    per-board lists are still raw (pre-validation). canonize() will
    validate them downstream; merge_channels() will see at most one
    declaration per id (by construction), so its uniqueness constraint
    is respected.
    """
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

    # Step 3: detect LOCAL conflicts (intra-board and inter-board).
    # Intra-board: a single board has >1 declaration of the same id.
    # Inter-board: two boards declare the same id with incompatible
    # directions.
    for (cid, scope), entries in by_id_scope.items():
        # Group by board_id to detect intra-board duplicates.
        per_board: dict[str | None, list[tuple[str, Path]]] = {}
        for board_id, direction, path in entries:
            per_board.setdefault(board_id, []).append((direction, path))

        for board_id, decls in per_board.items():
            if len(decls) > 1:
                # Intra-board conflict.
                _, path_a = decls[0]
                _, path_b = decls[1]
                raise DiffusionLocalConflictError(
                    channel_id=cid,
                    board_id=board_id,
                    path_a=path_a,
                    path_b=path_b,
                )

        # Inter-board: at most one entry per board at this point.
        if len(entries) == 2:
            (board_a, dir_a, path_a), (board_b, dir_b, path_b) = entries
            _check_inter_board_compatibility(
                cid, board_a, dir_a, path_a, board_b, dir_b, path_b,
            )

    # Step 4: build the per-board resolved lists.
    # For each board, we need:
    #   - Its own LOCAL declarations (unchanged).
    #   - Synthetic LOCAL declarations for ids declared by OTHER boards
    #     (with mirrored direction).
    #   - All REMOTE/SYSTEM declarations (pass-through, one per id).
    result: dict[str | None, list[tuple[Path, str, Any]]] = {}

    for board_id in node.boards:
        # Collect this board's own LOCAL declarations (by id).
        own_local_ids: dict[str, _RawChannelDecl] = {}
        for decl in decls_by_board.get(board_id, []):
            cid = decl.raw.get("id")
            scope = decl.raw.get("scope")
            if cid in own_local_ids:
                continue  # already caught as intra-board conflict
            if scope == "LOCAL" and _is_active_for_board(decl.raw, node.boards[board_id]):
                own_local_ids[cid] = decl

        # Build the list of LOCAL declarations for this board.
        local_decls_for_board: list[tuple[Path, str, Any]] = []

        # 4a. Own LOCAL declarations (unchanged).
        for cid, decl in own_local_ids.items():
            local_decls_for_board.append((
                decl.source_path,
                "channel_def",
                {"channels": [decl.raw]},
            ))

        # 4b. Synthetic LOCAL declarations for ids declared by OTHER boards.
        for (cid, scope), entries in by_id_scope.items():
            if scope != "LOCAL":
                continue
            if cid in own_local_ids:
                continue  # already declared by this board
            # Find the OTHER board that declared it.
            for other_board_id, direction, source_path in entries:
                if other_board_id == board_id:
                    continue
                # Mirror the direction.
                mirrored = _mirror_direction(direction)
                # Re-derive the original raw dict from decls_by_board.
                original_decl = None
                for d in decls_by_board.get(other_board_id, []):
                    if d.raw.get("id") == cid and d.raw.get("scope") == "LOCAL":
                        if _is_active_for_board(d.raw, node.boards[other_board_id]):
                            original_decl = d
                            break
                if original_decl is None:
                    continue
                synthetic_raw = dict(original_decl.raw)
                synthetic_raw["direction"] = mirrored
                # Use a virtual Path to mark the synthetic origin.
                virtual_path = Path(f"<diffusion:{board_id}>")
                local_decls_for_board.append((
                    virtual_path,
                    "channel_def",
                    {"channels": [synthetic_raw]},
                ))
                break  # only one other board in 2-board topology

        # 4c. REMOTE / SYSTEM pass-through (one declaration per id).
        # We pick the first declaration seen for each (id, scope) pair.
        # In practice, all boards see the same REMOTE/SYSTEM declarations
        # (they come from the same repo-wide .cb files), so picking the
        # first is fine.
        seen_passthrough: set[tuple[str, str]] = set()
        for (cid, scope), entries in passthrough.items():
            if (cid, scope) in seen_passthrough:
                continue
            seen_passthrough.add((cid, scope))
            # Pick the first entry (any board will do, content is identical).
            _, raw_decl, source_path = entries[0]
            local_decls_for_board.append((
                source_path,
                "channel_def",
                {"channels": [raw_decl]},
            ))

        result[board_id] = local_decls_for_board

    return result


# =============================================================================
# INTER-BOARD COMPATIBILITY CHECK
# =============================================================================

def _check_inter_board_compatibility(
    channel_id: str,
    board_a: str | None,
    direction_a: str,
    path_a: Path,
    board_b: str | None,
    direction_b: str,
    path_b: Path,
) -> None:
    """Raise DiffusionConflictError if the two directions are incompatible.

    Compatibility table (ND2):
      - {uplink, downlink}  -> OK (complementary pair)
      - {uplink, uplink}    -> FATAL
      - {downlink, downlink} -> FATAL
      - {both, both}        -> OK (BOTH_OR aggregation)
      - {none, none}        -> OK
      - {none, X}           -> OK (X = uplink/downlink/both)
      - {both, uplink} or {both, downlink} -> FATAL (semantic conflict)
    """
    pair = frozenset({direction_a, direction_b})

    # OK cases.
    if pair == frozenset({"uplink", "downlink"}):
        return
    if pair == frozenset({"both"}):
        return
    if pair == frozenset({"none"}):
        return
    if pair == frozenset({"none", "uplink"}):
        return
    if pair == frozenset({"none", "downlink"}):
        return
    if pair == frozenset({"none", "both"}):
        return

    # FATAL cases.
    if pair == frozenset({"uplink"}):
        raise DiffusionConflictError(
            channel_id=channel_id,
            board_a=board_a,
            board_b=board_b,
            direction_a=direction_a,
            direction_b=direction_b,
            path_a=path_a,
            path_b=path_b,
        )
    if pair == frozenset({"downlink"}):
        raise DiffusionConflictError(
            channel_id=channel_id,
            board_a=board_a,
            board_b=board_b,
            direction_a=direction_a,
            direction_b=direction_b,
            path_a=path_a,
            path_b=path_b,
        )
    if pair == frozenset({"both", "uplink"}) or pair == frozenset({"both", "downlink"}):
        raise DiffusionConflictError(
            channel_id=channel_id,
            board_a=board_a,
            board_b=board_b,
            direction_a=direction_a,
            direction_b=direction_b,
            path_a=path_a,
            path_b=path_b,
            semantic=True,
        )

    # Unknown combination (should not happen with valid direction tokens).
    raise DiffusionError(
        f"unknown direction pair {pair!r} for channel {channel_id!r} "
        f"between {board_a!r} and {board_b!r}"
    )
