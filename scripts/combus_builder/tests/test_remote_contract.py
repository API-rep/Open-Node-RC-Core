"""Machine type configuration and the link with the remote (link rework, step 3.6).

Rules validated by the owner (2026-11-02):
  - <type>.mtype lists the couplers the type supports (socket, plug): part of
    its contract; an instance (HAS_COUPLER_*) without its support is refused;
  - the remote link carries only what the remote sends: the inputs of the type
    and the commands the supported couplers relay (`downlink_pt`), never what
    a plug side sends; its own table and MD5, the same on both ends, with or
    without the coupler;
  - link channels have generic names shared between couplers: one channel, no
    more bytes; the same name agrees on type, value and zeroAt;
  - one source per channel: a channel of the type comes from the remote, a
    coupler that says the plug side sends it is refused.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import pytest

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder import links as links_module  # noqa: E402
from scripts.combus_builder.canon import canonize  # noqa: E402
from scripts.combus_builder.flags import BuildContext  # noqa: E402
from scripts.combus_builder.generator import GeneratorError, generate  # noqa: E402
from scripts.combus_builder.links import (  # noqa: E402
    LinkError,
    MachineTypeConfig,
    PLUG,
    SOCKET,
    check_supported_couplers,
    compute_remote_link,
    link_md5,
    load_machine_type_config,
    remote_commands,
)
from scripts.combus_builder.parser import discover_config_files, parse_all  # noqa: E402

SRC = REPO_ROOT / "src"
M, R = "IS_MACHINE", "IS_REMOTE"
TRACTOR = "MACHINE_TYPE_TEST_TRACTOR"
HITCH = "HAS_COUPLER_SOCKET_TEST_COUPLER"


def _ctx(*flags):
    return BuildContext(buildroot=SRC, defines=frozenset(flags), defines_with_value={},
                        cppdefines_source="override")


def _ch(id_, direction="downlink", type_="analog", value=None):
    return {"id": id_, "infoName": id_.lower(), "type": type_, "scope": "BASE", "theme": "core",
            "direction": direction,
            "value": value if value is not None else ("low" if type_ == "digital" else "CbusNeutral")}


def _canon(*raws):
    parsed = [(Path(f"ch{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


@pytest.fixture(scope="module")
def tractor():
    """Every channel a TEST tractor build discovers."""
    return canonize(parse_all(discover_config_files(SRC, machine_type="test_tractor"))).canonical_definitions


def _remote_md5s(out: Path, header: str = "combus_peer_links.h"):
    """(announced, expected) MD5 of the remote link in a generated header."""
    text = (out / header).read_text(encoding="utf-8")
    block = text[text.index("namespace RemoteTestTractor {"):]
    announced = re.search(r"kAnnouncedMd5\[16\]\s*=\s*\{([^}]*)\}", block).group(1)
    expected = re.search(r"kExpectedMd5\[16\]\s*=\s*\{([^}]*)\}", block).group(1)
    return announced.strip(), expected.strip()


# =============================================================================
# The .mtype file
# =============================================================================

def test_mtype_of_the_test_tractor():
    config = load_machine_type_config(SRC, "test_tractor")
    assert config.socket == ("test_coupler", "test_tool")
    assert config.plug == ()


def test_mtype_absent_supports_nothing(tmp_path):
    assert load_machine_type_config(tmp_path, "no_type") == MachineTypeConfig()


@pytest.mark.parametrize("body,match", [
    ("wheels: 4\n", "wheels"),
    ("couplers:\n  front: [x]\n", "front"),
])
def test_mtype_unknown_fields_are_refused(tmp_path, body, match):
    folder = tmp_path / "core" / "config" / "machines" / "t"
    folder.mkdir(parents=True)
    (folder / "t.mtype").write_text(body, encoding="utf-8")
    with pytest.raises(LinkError, match=match):
        load_machine_type_config(tmp_path, "t")


def test_an_instance_needs_its_support(tmp_path, monkeypatch, tractor):
    monkeypatch.setattr(links_module, "load_machine_type_config", lambda root, t: MachineTypeConfig())
    with pytest.raises(GeneratorError, match="not supported"):
        generate(tractor, _ctx(M, TRACTOR, HITCH), tmp_path)


def test_a_coupler_needs_a_machine_type(tmp_path, tractor):
    with pytest.raises(GeneratorError, match="MACHINE_TYPE"):
        generate(tractor, _ctx(M, HITCH), tmp_path)


# =============================================================================
# What travels on the remote link
# =============================================================================

def test_the_remote_link_carries_only_what_the_remote_sends():
    type_base = _canon(_ch("KEY", "downlink", "digital"), _ch("BRAKE", "downlink_pt"), _ch("OWN", "uplink"))
    coupler = _canon(_ch("BRAKE", "downlink_pt"), _ch("RAMP", "downlink_pt", "digital"), _ch("TILT", "downlink"),
                     _ch("MINE", "uplink"))
    ids = [c.id for c in remote_commands(type_base, {"c": coupler})]
    assert ids == ["BRAKE", "KEY", "RAMP"]            # not TILT (the plug side), not OWN / MINE (the machine)


def test_a_generic_name_shared_by_two_couplers_is_one_channel():
    a = _canon(_ch("TRAILER_RAMP", "downlink_pt", "digital"))
    b = _canon(_ch("TRAILER_RAMP", "downlink_pt", "digital"))
    ids = [c.id for c in remote_commands([], {"a": a, "b": b})]
    assert ids == ["TRAILER_RAMP"]


def test_a_command_relayed_from_a_plug_is_not_sent_by_the_remote():
    """One coupler is the source, the other relays it: the remote has no part in it."""
    source, relay = _canon(_ch("TILT", "downlink")), _canon(_ch("TILT", "downlink_pt"))
    assert remote_commands([], {"s": source, "r": relay}) == []


def test_what_the_plug_receives_is_not_sent_by_the_remote():
    """One source per channel, even on a carried machine: BRAKE arrives through its plug."""
    type_base = _canon(_ch("BRAKE", "downlink"), _ch("KEY", "downlink", "digital"))
    plug = {"c": _canon(_ch("BRAKE", "downlink_pt"), _ch("OWN", "uplink"), _ch("TILT", "downlink"))}
    assert [c.id for c in remote_commands(type_base, {}, plug)] == ["KEY"]


def test_a_type_with_no_command_has_no_remote_link(tmp_path):
    """The TEST trailer: everything it receives arrives through its plug."""
    trailer = canonize(parse_all(discover_config_files(SRC, machine_type="test_trailer"))).canonical_definitions
    generate(trailer, _ctx(M, "MACHINE_TYPE_TEST_TRAILER", "HAS_COUPLER_PLUG_TEST_COUPLER"), tmp_path)
    links = (tmp_path / "combus_peer_links.h").read_text(encoding="utf-8")
    assert "namespace TestCoupler {" in links
    assert "kRemoteLink" not in links


@pytest.mark.parametrize("a,b", [
    (_ch("X", "downlink_pt"), _ch("X", "downlink_pt", "digital")),
    (_ch("X", "downlink_pt"), _ch("X", "downlink_pt", value="CbusMaxVal")),
])
def test_the_same_name_in_two_couplers_must_agree(a, b):
    with pytest.raises(LinkError, match="disagree"):
        check_supported_couplers([], {"a": _canon(a), "b": _canon(b)})


def test_two_sources_are_refused():
    """The type says the remote sends BRAKE, the coupler says the plug side sends it."""
    with pytest.raises(LinkError, match="two sources"):
        check_supported_couplers(_canon(_ch("BRAKE", "downlink_pt")), {"c": _canon(_ch("BRAKE", "downlink"))})


# =============================================================================
# The contract: the same on both ends, with or without the coupler
# =============================================================================

def test_each_end_expects_the_md5_of_the_other():
    commands = _canon(_ch("BRAKE", "downlink_pt"))
    machine = compute_remote_link("t", commands, SOCKET)
    remote = compute_remote_link("t", commands, PLUG)
    assert link_md5(machine) == link_md5(remote, peer=True)
    assert link_md5(remote) == link_md5(machine, peer=True)
    assert link_md5(machine) != link_md5(remote)      # two machines, or two remotes, refuse each other


def test_the_machine_type_is_in_the_contract():
    commands = _canon(_ch("BRAKE", "downlink_pt"))
    assert link_md5(compute_remote_link("a", commands, SOCKET)) != link_md5(compute_remote_link("b", commands, SOCKET))


def test_machine_and_remote_agree_with_and_without_the_coupler(tmp_path, tractor):
    generate(tractor, _ctx(M, TRACTOR), tmp_path / "bare")
    generate(tractor, _ctx(M, TRACTOR, HITCH), tmp_path / "coupled")
    generate(tractor, _ctx(R, TRACTOR), tmp_path / "remote")
    bare, coupled, remote = (_remote_md5s(tmp_path / d) for d in ("bare", "coupled", "remote"))
    assert bare == coupled                            # the coupler does not change the contract
    assert remote == (bare[1], bare[0])               # each end announces what the other expects


def test_the_machine_receives_the_table_and_the_remote_sends_it(tmp_path, tractor):
    generate(tractor, _ctx(M, TRACTOR, HITCH), tmp_path / "m")
    generate(tractor, _ctx(R, TRACTOR), tmp_path / "r")
    machine = (tmp_path / "m" / "combus_peer_links.h").read_text(encoding="utf-8")
    remote = (tmp_path / "r" / "combus_peer_links.h").read_text(encoding="utf-8")
    assert "kRemoteLink = RemoteTestTractor::kLink" in machine
    block = machine[machine.index("namespace RemoteTestTractor {"):]
    analog_rx = re.search(r"kAnalogRx\[\] = \{([^}]*)\}", block).group(1)
    assert "false" not in analog_rx
    block = remote[remote.index("namespace RemoteTestTractor {"):]
    assert "true" not in re.search(r"kAnalogRx\[\] = \{([^}]*)\}", block).group(1)
    assert "TEST_TILT_ANGLE" not in block             # what the plug side sends never reaches the remote


def test_the_commands_of_a_supported_coupler_exist_without_it(tmp_path, monkeypatch, tractor):
    """Inert: the channel is there, at its safe value, even without the coupler."""
    real = links_module.load_coupler_channels
    only_here = _canon(_ch("TEST_ONLY_IN_COUPLER", "downlink_pt"))
    monkeypatch.setattr(links_module, "load_coupler_channels",
                        lambda root, name: real(root, name) + only_here if name == "test_coupler" else real(root, name))
    generate(tractor, _ctx(M, TRACTOR), tmp_path)
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "TEST_ONLY_IN_COUPLER" in ids              # the type does not declare it: it comes from the support
    assert "TEST_TILT_ANGLE" not in ids               # from the plug side: only with the coupler
    assert "TEST_COUPLER_CMD" not in ids              # local channel: only with the coupler
