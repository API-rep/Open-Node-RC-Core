"""
Property / stress tests of the ComBus Builder.

Instead of fixed examples, these tests generate random but valid `.cb`
declarations (seeded, so every failure is reproducible) and check invariants
that must hold for ANY input:

  1. Determinism and order independence of the whole pipeline.
  2. ID numbering contract (per type, BASE < SYSTEM < BOARD, view prefixes).
  3. MD5 sensitivity matrix (which scope change moves which view hash).
  4. MD5 insensitivity (`value`, satisfied `requires` do not move the hash).
  5. Duplicate ids are always rejected.
  6. Fuzzing: junk field values and junk YAML never raise anything but the
     documented validation / parse errors.
  7. SYSTEM diffusion against an independent oracle of the documented rules.

Knobs (environment):
  COMBUS_STRESS_SEEDS   number of random seeds per test (default 25).
                        Example: set COMBUS_STRESS_SEEDS=2000 for a long run.
  COMBUS_STRESS_SEED0   first seed (default 0), to replay a failure.

A failing parametrised test id contains its seed: test_xxx[seed=17].
"""

from __future__ import annotations

import copy
import os
import random
import sys
import tempfile
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize
from scripts.combus_builder.canon.channels import (
    ChannelError,
    ChannelValidationError,
    VALID_THEMES,
)
from scripts.combus_builder.diffusion import (
    DiffusionConflictError,
    diffuse_system_channels,
)
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.generator import generate, select_views
from scripts.combus_builder.md5 import compute_view_hash
from scripts.combus_builder.node_context import NodeContext
from scripts.combus_builder.parser import ParseError, parse_yaml_file

N_SEEDS = int(os.environ.get("COMBUS_STRESS_SEEDS", "25"))
SEED0 = int(os.environ.get("COMBUS_STRESS_SEED0", "0"))
SEEDS = [pytest.param(s, id=f"seed={s}") for s in range(SEED0, SEED0 + N_SEEDS)]

THEMES = sorted(VALID_THEMES)
FLAG_POOL = ["HAS_A", "HAS_B", "HAS_C", "HAS_D"]


# =============================================================================
# Generators
# =============================================================================

def _ctx(defines) -> BuildContext:
    return BuildContext(
        buildroot=REPO_ROOT / "src",
        defines=frozenset(defines),
        defines_with_value={},
        cppdefines_source="override",
    )


def _random_channel(rng: random.Random, idx: int) -> dict:
    type_ = rng.choice(["analog", "digital"])
    scope = rng.choice(["BASE", "SYSTEM", "BOARD"])
    ch = {
        "id": f"CH_{idx}",
        "infoName": f"channel {idx}",
        "type": type_,
        "scope": scope,
        "theme": rng.choice(THEMES),
    }
    if scope == "BOARD":
        if rng.random() < 0.5:
            ch["direction"] = "none"
    else:
        tokens = ["uplink", "downlink", "both", "none"]
        if type_ == "digital":
            tokens += ["uplink_or", "downlink_or", "both_or"]
        ch["direction"] = rng.choice(tokens)
        if scope == "SYSTEM" and rng.random() < 0.4:
            ch["requires"] = rng.sample(FLAG_POOL, rng.randint(1, 2))
    if rng.random() < 0.4:
        if type_ == "analog":
            ch["value"] = rng.choice(["CbusMinVal", "CbusNeutral", "CbusMaxVal",
                                      rng.randint(0, 65535)])
        else:
            ch["value"] = rng.choice(["low", "high"])
    return ch


def _random_channels(rng: random.Random) -> list[dict]:
    return [_random_channel(rng, i) for i in range(rng.randint(3, 25))]


def _to_parsed(channels: list[dict], rng: random.Random | None = None):
    """Spread channels over several virtual files (optionally shuffled)."""
    chans = list(channels)
    if rng is not None:
        rng.shuffle(chans)
    n_files = 1 + (len(chans) // 5)
    parsed = [(Path(f"f{i}.cb"), "channel_def", {"channels": []}) for i in range(n_files)]
    for i, ch in enumerate(chans):
        parsed[i % n_files][2]["channels"].append(copy.deepcopy(ch))
    if rng is not None:
        rng.shuffle(parsed)
    return parsed


def _generate_files(channels, rng=None) -> dict[str, bytes]:
    result = canonize(_to_parsed(channels, rng))
    with tempfile.TemporaryDirectory() as td:
        generate(result.canonical_definitions, _ctx(FLAG_POOL), Path(td))
        return {p.name: p.read_bytes() for p in sorted(Path(td).iterdir())}


def _view_hashes(channels) -> dict[str, str]:
    result = canonize(_to_parsed(channels))
    # requires are satisfied by the context; resolve like generate() does
    from scripts.combus_builder.generator import resolve_requires
    active = resolve_requires(result.canonical_definitions, _ctx(FLAG_POOL))
    sel = select_views(active)
    return {
        "base": compute_view_hash(sel.base).md5_hex,
        "system": compute_view_hash(sel.system).md5_hex,
        "full": compute_view_hash(sel.full).md5_hex,
    }


# =============================================================================
# 1. Determinism and order independence
# =============================================================================

@pytest.mark.parametrize("seed", SEEDS)
def test_output_is_deterministic_and_order_independent(seed):
    rng = random.Random(seed)
    channels = _random_channels(rng)
    reference = _generate_files(channels)
    again = _generate_files(channels)
    shuffled = _generate_files(channels, random.Random(seed + 10_000))
    assert reference == again, "two runs differ"
    assert reference == shuffled, "file / channel order changed the output"


# =============================================================================
# 2. ID numbering contract
# =============================================================================

@pytest.mark.parametrize("seed", SEEDS)
def test_id_numbering_contract(seed):
    channels = _random_channels(random.Random(seed))
    from scripts.combus_builder.generator import resolve_requires
    result = canonize(_to_parsed(channels))
    sel = select_views(resolve_requires(result.canonical_definitions, _ctx(FLAG_POOL)))

    order = {"BASE": 0, "SYSTEM": 1, "BOARD": 2}
    ids_by_view = {}
    for name, view in (("base", sel.base), ("system", sel.system), ("full", sel.full)):
        by_type: dict[str, list] = {"analog": [], "digital": []}
        for vc in view.channels:
            by_type[vc.ch.type].append(vc)
        for type_, vcs in by_type.items():
            ids = [vc.numeric_id for vc in vcs]
            assert ids == list(range(len(vcs))), f"{name}/{type_}: ids not 0..n-1 in order"
            scopes = [order[vc.ch.scope] for vc in vcs]
            assert scopes == sorted(scopes), f"{name}/{type_}: scope order broken"
        ids_by_view[name] = {vc.ch.id: vc.numeric_id for vc in view.channels}
        assert view.wire_end == sum(1 for vc in view.channels if vc.ch.scope == "BASE")

    # A channel keeps the same id in every view that contains it.
    for small, big in (("base", "system"), ("system", "full")):
        for cid, nid in ids_by_view[small].items():
            assert ids_by_view[big][cid] == nid, f"{cid}: id differs {small} -> {big}"
    assert sel.base.ch_count <= sel.system.ch_count <= sel.full.ch_count
    assert not any(vc.ch.scope == "BOARD" for vc in sel.system.channels)
    assert all(vc.ch.scope == "BASE" for vc in sel.base.channels)


# =============================================================================
# 3 / 4. MD5 sensitivity matrix and insensitivity
# =============================================================================

# scope of the modified channel -> views whose hash must change
EXPECTED_CHANGED = {
    "BASE": {"base", "system", "full"},
    "SYSTEM": {"system", "full"},
    "BOARD": {"full"},
}


@pytest.mark.parametrize("seed", SEEDS)
def test_hash_changes_exactly_in_views_containing_the_channel(seed):
    rng = random.Random(seed)
    channels = _random_channels(rng)
    before = _view_hashes(channels)
    for scope, changed in EXPECTED_CHANGED.items():
        # A channel that is present in the views (no unsatisfied requires here).
        candidates = [c for c in channels if c["scope"] == scope]
        if not candidates:
            continue
        mutated = copy.deepcopy(channels)
        target = rng.choice([c for c in mutated if c["scope"] == scope])
        target["infoName"] += " (renamed)"
        after = _view_hashes(mutated)
        really = {v for v in before if before[v] != after[v]}
        assert really == changed, f"renaming a {scope} channel moved {really}, expected {changed}"


@pytest.mark.parametrize("seed", SEEDS)
def test_hash_ignores_value_and_satisfied_requires(seed):
    rng = random.Random(seed)
    channels = _random_channels(rng)
    before = _view_hashes(channels)
    mutated = copy.deepcopy(channels)
    for ch in mutated:
        if ch["type"] == "analog":
            ch["value"] = rng.choice(["CbusMinVal", "CbusNeutral", "CbusMaxVal", 7])
        else:
            ch["value"] = rng.choice(["low", "high"])
        if ch["scope"] == "SYSTEM":
            ch["requires"] = ["HAS_A"]          # satisfied by the context
    assert _view_hashes(mutated) == before


@pytest.mark.parametrize("seed", SEEDS)
def test_hash_changes_when_direction_changes_on_base(seed):
    """On the BASE view uplink and downlink are NOT equivalent."""
    rng = random.Random(seed)
    channels = [c for c in _random_channels(rng)]
    remotes = [c for c in channels if c["scope"] == "BASE"
               and c["direction"] in ("uplink", "downlink")]
    if not remotes:
        extra = {"id": "CH_DIR", "infoName": "dir", "type": "digital", "scope": "BASE",
                 "theme": "core", "direction": "uplink"}
        channels.append(extra)
        remotes = [extra]
    before = _view_hashes(channels)
    target = rng.choice(remotes)
    target["direction"] = "downlink" if target["direction"] == "uplink" else "uplink"
    after = _view_hashes(channels)
    assert after["base"] != before["base"]


# =============================================================================
# 5. Duplicate ids
# =============================================================================

@pytest.mark.parametrize("seed", SEEDS)
def test_duplicate_id_is_always_rejected(seed):
    rng = random.Random(seed)
    channels = _random_channels(rng)
    dup = copy.deepcopy(rng.choice(channels))
    if rng.random() < 0.5:
        dup["type"] = "digital" if dup["type"] == "analog" else "analog"
        dup.pop("value", None)
        dup.pop("direction_or", None)
    parsed = _to_parsed(channels)
    parsed.append((Path("dup.cb"), "channel_def", {"channels": [dup]}))
    with pytest.raises(ChannelError):
        canonize(parsed)


# =============================================================================
# 6. Fuzzing
# =============================================================================

JUNK = [None, 0, 1, -1, 1.5, True, False, [], [1], ["uplink"], {}, {"a": 1},
        "", " ", "x", "UPLINK", "uplink ", "é", "a" * 300, float("inf")]
FIELDS = ["id", "infoName", "type", "scope", "theme", "direction", "requires", "value"]


def _is_unhashable(value) -> bool:
    try:
        hash(value)
    except TypeError:
        return True
    return False


@pytest.mark.parametrize("seed", SEEDS)
def test_junk_field_values_only_raise_validation_errors(seed):
    rng = random.Random(seed)
    base = _random_channel(rng, 0)
    for field in FIELDS:
        for junk in rng.sample(JUNK, 8):
            if field in ("type", "scope") and _is_unhashable(junk):
                continue        # covered by test_unhashable_type_or_scope_is_a_validation_error
            ch = copy.deepcopy(base)
            ch[field] = copy.deepcopy(junk)
            try:
                canonize([(Path("f.cb"), "channel_def", {"channels": [ch]})])
            except ChannelValidationError:
                pass        # documented behaviour
            # any other exception type propagates and fails the test


@pytest.mark.parametrize("field", ["type", "scope"])
@pytest.mark.parametrize("junk", [[], ["analog"], {}, {"a": 1}], ids=repr)
def test_unhashable_type_or_scope_is_a_validation_error(field, junk):
    ch = _random_channel(random.Random(0), 0)
    ch[field] = junk
    with pytest.raises(ChannelValidationError):
        canonize([(Path("f.cb"), "channel_def", {"channels": [ch]})])


@pytest.mark.parametrize("seed", SEEDS)
def test_unknown_or_missing_keys_are_rejected(seed):
    rng = random.Random(seed)
    base = _random_channel(rng, 0)
    extra = copy.deepcopy(base)
    extra[rng.choice(["module", "layer", "default", "info_name", "typo", "uplink"])] = 1
    with pytest.raises(ChannelValidationError):
        canonize([(Path("f.cb"), "channel_def", {"channels": [extra]})])
    for required in ("id", "infoName", "type", "scope", "theme"):
        missing = copy.deepcopy(base)
        del missing[required]
        with pytest.raises(ChannelValidationError):
            canonize([(Path("f.cb"), "channel_def", {"channels": [missing]})])


@pytest.mark.parametrize("seed", SEEDS)
def test_junk_yaml_files_only_raise_parse_errors(seed, tmp_path):
    rng = random.Random(seed)
    alphabet = list("abc:-[]{}#&*!|>'\"%@`,?\n \t") + ["  ", "- ", "key: ", "\\x00"]
    for n in range(10):
        text = "".join(rng.choice(alphabet) for _ in range(rng.randint(0, 200)))
        path = tmp_path / f"junk{n}.cb"
        path.write_bytes(text.encode("utf-8", "replace"))
        try:
            parse_yaml_file(path)
        except ParseError:
            pass


def test_non_utf8_file_is_a_parse_error(tmp_path):
    path = tmp_path / "binary.cb"
    path.write_bytes(bytes([0xEA, 0xFF, 0xFE]) + b" channels: []")
    with pytest.raises(ParseError):
        parse_yaml_file(path)


# =============================================================================
# 7. SYSTEM diffusion against an independent oracle
# =============================================================================

DECLARABLE = [None, None, "uplink", "downlink", "both", "none",
              "uplink_or", "downlink_or", "both_or"]
UP = {"uplink", "uplink_or"}
DOWN = {"downlink", "downlink_or"}


def _oracle(declared: dict[str, str | None]):
    """Documented rules (diffusion.py docstring, rules 1-7), re-implemented
    independently. Returns ("error", None, False) or
    ("ok", {board: direction-or-None-for-absent}, warning_expected)."""
    dirs = list(declared.values())
    n_up = sum(d in UP for d in dirs)
    n_down = sum(d in DOWN for d in dirs)
    n_both = sum(d == "both" for d in dirs)
    n_bor = sum(d == "both_or" for d in dirs)
    bothish = n_both + n_bor
    absent = [b for b, d in declared.items() if d is None]
    out = dict(declared)
    # ND6: `both_or` explicitly declared while fewer than 2 boards explicitly
    # declare a writer (anything but `none`) -> single-writer warning.
    explicit_writers = sum(d not in (None, "none") for d in dirs)
    single_writer_warn = n_bor >= 1 and explicit_writers < 2
    warn = False

    if bothish >= 1 and (n_up >= 1 or n_down >= 1):
        return ("error", None, False)
    if bothish >= 1:
        synth = "both_or" if n_bor >= 1 else "both"
        for b in absent:
            out[b] = synth
        warn = (n_both >= 1 and n_bor >= 1) or single_writer_warn
        return ("ok", out, warn)
    if n_up >= 2:
        return ("error", None, False)
    if n_up == 1:
        for b in absent:
            out[b] = "downlink"
        return ("ok", out, single_writer_warn)
    if n_down >= 1:
        for b in absent:
            out[b] = "none"
        return ("ok", out, True)
    return ("ok", out, single_writer_warn)   # only `none`: no synthesis


def _extract(decls) -> dict[str, str]:
    found = {}
    for _path, _label, data in decls:
        for raw in data.get("channels", []):
            found[raw["id"]] = raw["direction"]
    return found


@pytest.mark.parametrize("seed", SEEDS)
def test_diffusion_matches_documented_rules(seed, capsys):
    rng = random.Random(seed)
    n_boards = rng.randint(2, 5)
    boards = [f"b{i}" for i in range(n_boards)]
    declared = {b: rng.choice(DECLARABLE) for b in boards}
    if all(d is None for d in declared.values()):
        declared[boards[0]] = rng.choice([d for d in DECLARABLE if d])

    parsed = []
    for b, d in declared.items():
        if d is None:
            continue
        parsed.append((Path(f"{b}.cb"), "channel_def", {"channels": [{
            "id": "X", "infoName": "x", "type": "digital", "scope": "SYSTEM",
            "theme": "core", "direction": d, "requires": [f"IS_{b.upper()}"],
        }]}))
    contexts = {b: _ctx({f"IS_{b.upper()}"}) for b in boards}
    if rng.random() < 0.5:
        contexts[None] = _ctx(set())             # the mother, to be excluded
    node = NodeContext(mother_env="m", boards=contexts, is_node=True)

    verdict, expected, warn = _oracle(declared)
    capsys.readouterr()                           # drop earlier output
    if verdict == "error":
        with pytest.raises(DiffusionConflictError):
            diffuse_system_channels(parsed, node)
        return

    result = diffuse_system_channels(parsed, node)
    err = capsys.readouterr().err

    assert None not in result, "the mother must never get a board result"
    assert set(result) == set(boards)
    for b in boards:
        got = _extract(result[b]).get("X")
        assert got == expected[b], (
            f"board {b}: declared={declared[b]!r} expected={expected[b]!r} got={got!r}"
            f" (all declared: {declared})"
        )
    assert ("WARNING" in err) == warn, f"warning mismatch for {declared}: stderr={err!r}"
    final_up = [b for b in boards if _extract(result[b]).get("X") in UP]
    assert len(final_up) <= 1, "a node must never end with two producers"
