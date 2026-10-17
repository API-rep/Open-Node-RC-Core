"""
Tests of the helper functions of scripts/combus_scons_hook.py.

The hook is a SCons extra_script: at import it calls Import("env") and runs
main(env). Here the source is exec'd with a no-op `Import`, a fake `env`, and
WITHOUT the final module-level `main(env)` call, so only the definitions are
loaded. No SCons, no PlatformIO, no build.

Covered: skip policy, expected artefacts (single board and node), staging
completeness check, atomic publication, node board collection, output
directory resolution, `[env:...]` parsing, cb source discovery, and the
SKIP action.
"""

from __future__ import annotations

import sys
import types
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))


class FakeEnv(dict):
    """Minimal stand-in for the SCons env injected by PlatformIO."""

    def get(self, k, d=None):
        return dict.get(self, k, d)


def _load_hook() -> types.ModuleType:
    hook_path = REPO_ROOT / "scripts" / "combus_scons_hook.py"
    source = hook_path.read_text(encoding="utf-8")
    tail = "\nmain(env)"
    idx = source.rstrip().rfind(tail)
    assert idx != -1, "final `main(env)` call not found in the hook"
    source = source[:idx] + "\n"

    module = types.ModuleType("combus_scons_hook_for_test")
    module.__file__ = str(hook_path)
    env = FakeEnv(PROJECT_DIR=str(REPO_ROOT))
    # @dataclass in the hook needs the module to be registered by name.
    sys.modules[module.__name__] = module
    ns = module.__dict__
    ns["Import"] = lambda *_a, **_k: None
    ns["env"] = env
    ns["projenv"] = None
    exec(compile(source, str(hook_path), "exec"), ns)
    return module


@pytest.fixture(scope="module")
def hook() -> types.ModuleType:
    return _load_hook()


# =============================================================================
# Skip policy
# =============================================================================

@pytest.mark.parametrize("value", ["1", "true", "TRUE", "yes", "on", " 1 "])
def test_skip_requested_truthy(hook, monkeypatch, value):
    monkeypatch.setenv("COMBUS_BUILDER_SKIP", value)
    assert hook._skip_requested() is True


@pytest.mark.parametrize("value", ["", "0", "no", "off", "false"])
def test_skip_requested_falsy(hook, monkeypatch, value):
    monkeypatch.setenv("COMBUS_BUILDER_SKIP", value)
    assert hook._skip_requested() is False


def test_skip_not_requested_when_unset(hook, monkeypatch):
    monkeypatch.delenv("COMBUS_BUILDER_SKIP", raising=False)
    assert hook._skip_requested() is False


# =============================================================================
# Expected artefacts
# =============================================================================

NINE = {
    "combus.cpp", "combus.h", "combus_ids.h",
    "combus_system_ids.h", "combus_base_ids.h",
    "combus_system_md5.h", "combus_base_md5.h", "combus_md5.h",
    "combus_wire_common.h",
}


def test_expected_artifacts_are_the_nine_files(hook):
    assert set(hook.EXPECTED_ARTIFACTS) == NINE
    assert len(hook.EXPECTED_ARTIFACTS) == 9


def test_expected_artifacts_single_board_is_unchanged(hook):
    got = hook._expected_artifacts_for_node([(None, "volvo")], is_node=False)
    assert got == list(hook.EXPECTED_ARTIFACTS)


def test_expected_artifacts_node_prefixes_only_the_other_boards(hook):
    boards = [(None, "volvo"), ("mainboard", "volvo_mainboard"),
              ("soundboard", "volvo_soundboard")]
    got = hook._expected_artifacts_for_node(
        boards, is_node=True, current_env="volvo_mainboard")
    # 9 unprefixed (current board) + 8 prefixed for the one other board.
    assert len(got) == 9 + 8
    assert set(hook.EXPECTED_ARTIFACTS) <= set(got)
    prefix = "soundboard"
    assert {
        f"combus_{prefix}.h", f"combus_{prefix}.cpp", f"combus_{prefix}_ids.h",
        f"combus_system_{prefix}_ids.h", f"combus_base_{prefix}_ids.h",
        f"combus_{prefix}_md5.h", f"combus_system_{prefix}_md5.h",
        f"combus_base_{prefix}_md5.h",
    } <= set(got)
    # The current board is never prefixed; the wire header is never prefixed.
    assert not any("mainboard" in n for n in got)
    assert not any("wire_common" in n and n != "combus_wire_common.h" for n in got)


def test_expected_artifacts_node_follows_the_current_env(hook):
    boards = [(None, "volvo"), ("mainboard", "volvo_mainboard"),
              ("soundboard", "volvo_soundboard")]
    got = hook._expected_artifacts_for_node(
        boards, is_node=True, current_env="volvo_soundboard")
    assert any("mainboard" in n for n in got)
    assert not any("soundboard" in n for n in got)


# =============================================================================
# Staging check and atomic publication
# =============================================================================

def _touch_all(directory: Path, names) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for n in names:
        (directory / n).write_text("x", encoding="utf-8")


def test_check_staging_complete_accepts_full_set(hook, tmp_path):
    staging = tmp_path / "combus_generated.new"
    _touch_all(staging, hook.EXPECTED_ARTIFACTS)
    hook._check_staging_complete(staging, list(hook.EXPECTED_ARTIFACTS))
    assert staging.is_dir()


def test_check_staging_complete_rejects_missing_and_cleans_staging(hook, tmp_path):
    staging = tmp_path / "combus_generated.new"
    names = list(hook.EXPECTED_ARTIFACTS)
    _touch_all(staging, names[:-1])
    with pytest.raises(RuntimeError) as exc:
        hook._check_staging_complete(staging, names)
    assert names[-1] in str(exc.value)
    assert not staging.exists(), "incomplete staging must be removed"


def test_publish_replaces_old_directory(hook, tmp_path):
    src = tmp_path / "combus_generated.new"
    dst = tmp_path / "combus_generated"
    _touch_all(dst, ["old.h"])
    _touch_all(src, ["new.h"])
    hook._publish(src, dst)
    assert (dst / "new.h").is_file()
    assert not (dst / "old.h").exists(), "previous artefacts must be gone"
    assert not src.exists(), "staging is moved, not copied"


def test_publish_when_destination_missing(hook, tmp_path):
    src = tmp_path / "combus_generated.new"
    dst = tmp_path / "combus_generated"
    _touch_all(src, ["new.h"])
    hook._publish(src, dst)
    assert (dst / "new.h").is_file()


def test_fatal_exits_with_code_1(hook, capsys):
    with pytest.raises(SystemExit) as exc:
        hook._fatal("boom")
    assert exc.value.code == 1
    assert "boom" in capsys.readouterr().err


# =============================================================================
# Node boards
# =============================================================================

def test_collect_node_boards_mother_first_then_sorted(hook):
    names = ["volvo_soundboard", "other", "volvo", "volvo_mainboard", "volvo_"]
    got = hook._collect_node_boards("volvo", names)
    assert got == [
        (None, "volvo"),
        ("mainboard", "volvo_mainboard"),
        ("soundboard", "volvo_soundboard"),
    ]


def test_collect_node_boards_without_mother_env(hook):
    got = hook._collect_node_boards("volvo", ["volvo_mainboard"])
    assert got == [("mainboard", "volvo_mainboard")]


# =============================================================================
# Output directory
# =============================================================================

def test_default_out_dir_uses_project_build_dir(hook, tmp_path):
    env = FakeEnv(PROJECT_BUILD_DIR=str(tmp_path / "build"), PIOENV="x")
    assert hook._default_out_dir(env) == tmp_path / "build" / "combus_generated"


def test_default_out_dir_falls_back_to_pio_build_env(hook, tmp_path):
    env = FakeEnv(PROJECT_DIR=str(tmp_path), PIOENV="volvo")
    assert hook._default_out_dir(env) == (
        tmp_path / ".pio" / "build" / "volvo" / "combus_generated"
    )


def test_default_out_dir_raises_without_any_dir(hook):
    with pytest.raises(RuntimeError):
        hook._default_out_dir(FakeEnv())


# =============================================================================
# platformio.ini parsing and .cb discovery
# =============================================================================

def test_read_all_env_names(hook, tmp_path):
    (tmp_path / "platformio.ini").write_text(
        "[platformio]\nsrc_dir = src\n\n[env]\nbuild_flags = -D A\n\n"
        "[env:machines]\n\n[env:volvo_A60H_bruder]\nextends = env:machines\n",
        encoding="utf-8",
    )
    assert hook._read_all_env_names(tmp_path) == ["machines", "volvo_A60H_bruder"]


def test_read_all_env_names_missing_file(hook, tmp_path):
    assert hook._read_all_env_names(tmp_path) == []


def test_discover_cb_sources_finds_cb_and_cbch_sorted(hook, tmp_path):
    (tmp_path / "b").mkdir()
    (tmp_path / "b" / "z.cb").write_text("channels: []", encoding="utf-8")
    (tmp_path / "a.cbch").write_text("channels: []", encoding="utf-8")
    (tmp_path / "ignored.yaml").write_text("x: 1", encoding="utf-8")
    got = hook._discover_cb_sources(tmp_path)
    assert got == sorted(got)
    assert [Path(p).name for p in got] == ["a.cbch", "z.cb"]


# =============================================================================
# SKIP action
# =============================================================================

def test_skip_action_fails_when_artefacts_missing(hook, tmp_path, capsys):
    env = FakeEnv(PROJECT_BUILD_DIR=str(tmp_path))
    rc = hook._skip_action(None, None, env)
    assert rc == 1
    assert "COMBUS_BUILDER_SKIP" in capsys.readouterr().err


def test_skip_action_fails_on_partial_artefacts(hook, tmp_path):
    out = tmp_path / "combus_generated"
    _touch_all(out, list(hook.EXPECTED_ARTIFACTS)[:-1])
    assert hook._skip_action(None, None, FakeEnv(PROJECT_BUILD_DIR=str(tmp_path))) == 1


def test_skip_action_succeeds_with_all_artefacts(hook, tmp_path):
    out = tmp_path / "combus_generated"
    _touch_all(out, hook.EXPECTED_ARTIFACTS)
    assert hook._skip_action(None, None, FakeEnv(PROJECT_BUILD_DIR=str(tmp_path))) == 0


# =============================================================================
# Node mode: every expected artefact must actually be produced
# =============================================================================

ROOT_FLAGS = {"IS_MACHINE", "HAS_FAILSAFE", "MACHINE_VOLVO_A60_H_BRUDER"}
MAIN_FLAGS = ROOT_FLAGS | {
    "IS_MAINBOARD", "BOARD_ESP32_8M_6S", "HAS_VBAT_FAILSAFE", "VBAT_LIPO",
    "HAS_REMOTE_LINK_LOST_FALLBACK", "HAS_RUNLEVEL_MANAGER", "HAS_IGNIT_KEY",
}
EXT_FLAGS = ROOT_FLAGS | {"IS_EXT_BOARD", "BOARD_ESP32_8M_6S"}


def _node_staging(hook, tmp_path, current_env="test_node_mainboard", project_version=None):
    """Run the node-mode generation loop of the hook on the real .cb files,
    for a root env with two boards, as if `current_env` were being compiled.
    Returns (staging_dir, boards)."""
    from scripts.combus_builder.canon import canonize
    from scripts.combus_builder.diffusion import diffuse_system_channels
    from scripts.combus_builder.flags import BuildContext
    from scripts.combus_builder.node_context import NodeContext
    from scripts.combus_builder.parser import discover_and_parse

    def ctx(flags):
        return BuildContext(buildroot=REPO_ROOT / "src", defines=frozenset(flags),
                            defines_with_value={}, cppdefines_source="override")

    _, _, parsed = discover_and_parse(project_root=REPO_ROOT)
    node = NodeContext(
        mother_env="test_node",
        boards={None: ctx(ROOT_FLAGS), "mainboard": ctx(MAIN_FLAGS), "extboard": ctx(EXT_FLAGS)},
        is_node=True,
    )
    diffused = diffuse_system_channels(parsed, node)
    staging = tmp_path / "combus_generated.new"
    boards = hook._collect_node_boards(
        "test_node", ["test_node", "test_node_mainboard", "test_node_extboard"])
    for board_id, env_name in boards:
        if board_id is None:                        # the root is skipped, as in the hook
            continue
        hook._generate_per_board(
            board_id=board_id, board_env_name=env_name, pio_env=current_env,
            node_mother_env="test_node", live_ctx=None, project_dir=None,
            canonical_definitions=canonize(diffused[board_id]).canonical_definitions,
            staging_dir=staging, board_ctx=node.boards[board_id],
            unprefixed=(env_name == current_env),
            project_version=project_version,
        )
    return staging, boards


@pytest.mark.parametrize("current,other", [
    ("test_node_mainboard", "extboard"),
    ("test_node_extboard", "mainboard"),
])
def test_node_mode_produces_every_expected_artefact(hook, tmp_path, current, other):
    staging, boards = _node_staging(hook, tmp_path, current)
    expected = hook._expected_artifacts_for_node(boards, is_node=True, current_env=current)
    assert len(expected) == 9 + 8
    missing = [n for n in expected if not (staging / n).is_file()]
    assert not missing, f"files not generated: {missing}"
    # Nothing prefixed for the current board, nothing unexpected overall.
    own = current.removeprefix("test_node_")
    assert not any(own in p.name for p in staging.iterdir())
    assert {p.name for p in staging.iterdir()} == set(expected)


@pytest.mark.parametrize("current", ["test_node_mainboard", "test_node_extboard"])
def test_node_mode_completeness_check_passes(hook, tmp_path, current):
    staging, boards = _node_staging(hook, tmp_path, current)
    hook._check_staging_complete(
        staging, hook._expected_artifacts_for_node(boards, is_node=True, current_env=current))


def test_node_mode_unprefixed_files_belong_to_the_current_board(hook, tmp_path):
    """combus.h of a build must carry the current board's channels (the
    mainboard owns RUNLEVEL, the extboard does not)."""
    main, _ = _node_staging(hook, tmp_path / "m", "test_node_mainboard")
    ext, _ = _node_staging(hook, tmp_path / "e", "test_node_extboard")
    assert (main / "combus_ids.h").read_text(encoding="utf-8") !=         (ext / "combus_ids.h").read_text(encoding="utf-8")


def test_node_mode_writes_the_project_version(hook, tmp_path):
    """The version read from project_version.h reaches combus_wire_common.h."""
    staging, _ = _node_staging(hook, tmp_path, project_version=(8, 9))
    text = (staging / "combus_wire_common.h").read_text(encoding="utf-8")
    assert "kProjectVersionMajor = 8u;" in text
    assert "kProjectVersionMinor = 9u;" in text
