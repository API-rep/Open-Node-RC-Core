"""Nature of a node: one structural flag per env (link rework, step 3.2b).

Rules validated by the owner (2026-10-25; couplers: 2026-10-31, step 3.4a):
  - exactly one structural flag per env: IS_VEHICLE, IS_ATTACHMENT,
    IS_MACHINE or IS_REMOTE;
  - IS_VEHICLE: the prime mover (tractor, dumper, excavator), the hub of the
    star: zero or several couplers (-D COMBUS_COUPLER_<NAME>);
  - IS_ATTACHMENT: what is hitched to a vehicle (trailer, plough, dolly), an
    electron: exactly one coupler, the one of its vehicle;
  - IS_MACHINE: a stand-alone machine, isolated: no coupler (refused explicitly);
  - IS_REMOTE: unchanged, no coupler before the remote work (chantier 4).
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.links import LinkError, node_nature  # noqa: E402

P1, P2 = "COMBUS_COUPLER_TEST_HITCH", "COMBUS_COUPLER_FIFTH_WHEEL"


@pytest.mark.parametrize("defines,nature", [
    ({"IS_VEHICLE"}, "IS_VEHICLE"),
    ({"IS_VEHICLE", P1}, "IS_VEHICLE"),
    ({"IS_VEHICLE", P1, P2}, "IS_VEHICLE"),
    ({"IS_ATTACHMENT", P1}, "IS_ATTACHMENT"),
    ({"IS_MACHINE", "HAS_FAILSAFE"}, "IS_MACHINE"),
    ({"IS_REMOTE"}, "IS_REMOTE"),
])
def test_accepted_natures(defines, nature):
    assert node_nature(defines) == nature


@pytest.mark.parametrize("defines,match", [
    (set(), "no structural flag"),
    ({"IS_VEHICLE", "IS_MACHINE"}, "IS_MACHINE, IS_VEHICLE"),
    ({"IS_ATTACHMENT", "IS_VEHICLE", P1}, "IS_ATTACHMENT, IS_VEHICLE"),
    ({"IS_MACHINE", P1}, "IS_MACHINE"),
    ({"IS_ATTACHMENT"}, "exactly one"),
    ({"IS_ATTACHMENT", P1, P2}, "exactly one"),
    ({"IS_REMOTE", P1}, "IS_REMOTE"),
])
def test_refused_natures(defines, match):
    with pytest.raises(LinkError, match=match):
        node_nature(defines)


def test_the_hook_checks_the_nature():
    """The hook refuses an env through node_nature(), not through its old IS_MACHINE / IS_REMOTE list."""
    text = (REPO_ROOT / "scripts" / "combus_scons_hook.py").read_text(encoding="utf-8")
    assert "node_nature(" in text
    assert 'structural_flags = {"IS_MACHINE", "IS_REMOTE"}' not in text
