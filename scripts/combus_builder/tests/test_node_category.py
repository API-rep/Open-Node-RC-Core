"""Category of a node: one per env.

Rules validated by the owner (2026-11-01):
  - a node is a compiled firmware with a ComBus; exactly one category per env:
    IS_MACHINE (a node that is driven) or IS_REMOTE (a node that drives);
  - the place of a machine in the star is the side of each of its couplers
    (HAS_COUPLER_SOCKET_<NAME>: it carries; HAS_COUPLER_PLUG_<NAME>: it is
    carried), never a nature of the node: see test_couplers.py (T2);
  - a remote carries no coupler (refused explicitly).
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.links import LinkError, node_category  # noqa: E402

SOCKET, PLUG = "HAS_COUPLER_SOCKET_TEST_COUPLER", "HAS_COUPLER_PLUG_FIFTH_WHEEL"


@pytest.mark.parametrize("defines,category", [
    ({"IS_MACHINE", "HAS_FAILSAFE"}, "IS_MACHINE"),
    ({"IS_MACHINE", SOCKET}, "IS_MACHINE"),
    ({"IS_MACHINE", SOCKET, PLUG}, "IS_MACHINE"),
    ({"IS_REMOTE"}, "IS_REMOTE"),
])
def test_accepted_categories(defines, category):
    assert node_category(defines) == category


@pytest.mark.parametrize("defines,match", [
    (set(), "no category"),
    ({"IS_MACHINE", "IS_REMOTE"}, "several categories"),
    ({"IS_REMOTE", SOCKET}, "IS_REMOTE"),
])
def test_refused_categories(defines, match):
    with pytest.raises(LinkError, match=match):
        node_category(defines)


def test_the_hook_checks_the_category():
    """The hook refuses an env through node_category()."""
    text = (REPO_ROOT / "scripts" / "combus_scons_hook.py").read_text(encoding="utf-8")
    assert "node_category(" in text
