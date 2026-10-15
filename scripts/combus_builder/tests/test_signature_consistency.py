"""
Test: discover_and_parse supports both signatures (env= and project_root=).

Two callers use different keyword arguments:
- combus_scons_hook.py calls discover_and_parse(env=env)
- scripts/combus_builder.py (CLI) calls discover_and_parse(project_root=...)

Both must resolve to the same buildroot and find the same files. Priority
order in resolve_buildroot() (parser.py): env["PROJECT_SRC_DIR"], then
env["PROJECT_DIR"] + "/src", then project_root + "/src", then cwd + "/src".
"""

from __future__ import annotations

import sys
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.parser import discover_and_parse  # noqa: E402


class FakeEnv(dict):
    """Minimal stand-in for the SCons env injected by PlatformIO."""

    def get(self, k, d=None):
        return dict.get(self, k, d)


def _env_with_src() -> FakeEnv:
    env = FakeEnv()
    env["PROJECT_SRC_DIR"] = str(REPO_ROOT / "src")
    return env


def test_env_signature_resolves_src():
    buildroot, files, _ = discover_and_parse(env=_env_with_src())
    assert buildroot == (REPO_ROOT / "src").resolve()
    assert files, "no .cb/.cbch file found under src/"


def test_project_root_signature_resolves_src():
    buildroot, files, _ = discover_and_parse(project_root=REPO_ROOT)
    assert buildroot == (REPO_ROOT / "src").resolve()
    assert files


def test_env_wins_over_project_root(tmp_path):
    """When both are given, env takes priority (resolve_buildroot order)."""
    buildroot, _, _ = discover_and_parse(env=_env_with_src(), project_root=tmp_path)
    assert buildroot == (REPO_ROOT / "src").resolve()


def test_all_signatures_agree_on_buildroot_and_files(tmp_path):
    br1, files1, _ = discover_and_parse(env=_env_with_src())
    br2, files2, _ = discover_and_parse(project_root=REPO_ROOT)
    br3, files3, _ = discover_and_parse(env=_env_with_src(), project_root=tmp_path)
    assert br1 == br2 == br3
    assert len(files1) == len(files2) == len(files3)
    assert sorted(files1) == sorted(files2) == sorted(files3)
