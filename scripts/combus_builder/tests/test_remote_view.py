"""The ComBus of a remote: the BASE view of the machine type it drives, mirrored (chantier 4, embryo).

Rules validated by the owner (2026-11-01):
  - R1 a remote (IS_REMOTE) drives a machine type (-D MACHINE_TYPE_<TYPE>);
    one type for the embryo, the wiring ready for several (step 4.1);
  - R2 its ComBus is the BASE view of that type, mirrored: what the machine
    receives (`downlink`, `downlink_pt`: received then relayed) the remote
    sends (`uplink`); what the machine sends (`uplink`) the remote receives
    (`downlink`, a feedback); `both` and `both_or` unchanged; the SYSTEM and
    BOARD channels of the type stay inside the machine;
  - R3 the contract of the link is the BASE of the machine type as written:
    the BASE MD5 of the remote is hashed on the machine view (the generated
    code keeps the mirrored directions), so both ends announce the same MD5
    and the handshake of a plain BASE link between nodes works as is;
  - R5 the remote sends only the inputs: the mirror guarantees it.
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

from scripts.combus_builder.canon import canonize  # noqa: E402
from scripts.combus_builder.flags import BuildContext  # noqa: E402
from scripts.combus_builder.generator import GeneratorError, generate  # noqa: E402
from scripts.combus_builder.links import remote_channels  # noqa: E402
from scripts.combus_builder.parser import discover_config_files, parse_all  # noqa: E402

SRC = REPO_ROOT / "src"
TYPE = "MACHINE_TYPE_DUMPER_TRUCK"


def _ctx(*flags):
    return BuildContext(buildroot=SRC, defines=frozenset(flags), defines_with_value={},
                        cppdefines_source="override")


def _ch(id_, direction, scope="BASE", type_="analog"):
    raw = {"id": id_, "infoName": id_.lower(), "type": type_, "scope": scope, "theme": "core",
           "value": "low" if type_ == "digital" else "CbusNeutral"}
    if direction is not None:
        raw["direction"] = direction
    return raw


def _canon(*raws, path="ch"):
    parsed = [(Path(f"{path}{i}.cb"), "channel_def", {"channels": [r]}) for i, r in enumerate(raws)]
    return canonize(parsed).canonical_definitions


@pytest.fixture(scope="module")
def dumper():
    """Every channel a dumper truck build discovers: core and the dumper_truck type."""
    return canonize(parse_all(discover_config_files(SRC, machine_type="dumper_truck"))).canonical_definitions


def _md5(out: Path, header: str) -> str:
    text = (out / header).read_text(encoding="utf-8")
    return " ".join(re.findall(r"0x[0-9A-Fa-f]{2}", text))


# =============================================================================
# R2: the mirror
# =============================================================================

@pytest.mark.parametrize("machine,remote", [
    ("downlink", frozenset({"uplink"})),      # row 2: the machine receives it, the remote sends it
    ("downlink_pt", frozenset({"uplink"})),   # row 4: received then relayed by the machine
    ("uplink", frozenset({"downlink"})),      # row 3: the machine sends it, the remote receives it
    ("both", frozenset({"uplink", "downlink"})),
])
def test_row2_to_row4_base_directions_are_mirrored(machine, remote):
    out = {c.id: c for c in remote_channels(_canon(_ch("X", machine)), "dumper_truck")}
    assert out["X"].direction == remote
    assert not out["X"].passthrough


def test_row2_or_directions_are_swapped():
    out = {c.id: c for c in remote_channels(_canon(_ch("F", "uplink_or", type_="digital"),
                                                   _ch("G", "both_or", type_="digital")), "dumper_truck")}
    assert out["F"].direction_or == frozenset({"downlink"})
    assert out["G"].direction_or == frozenset({"uplink", "downlink"})


def test_row1_the_system_and_board_channels_of_the_type_stay_in_the_machine(dumper):
    ids = {c.id for c in remote_channels(dumper, "dumper_truck")}
    assert {"BRAKE_BUS", "THROTTLE_BUS", "KEY_BTN", "LIGHTS"} <= ids         # BASE of the type
    assert not ids & {"THROTTLE_STICK", "KEY_ACTIVE", "ESC_RPM_BUS"}        # SYSTEM of the type
    assert "RUNLEVEL" in ids                                                # the core stays


# =============================================================================
# R1 / R3: through the generator
# =============================================================================

def test_row1_the_remote_combus_holds_the_base_of_the_type(tmp_path, dumper):
    generate(dumper, _ctx("IS_REMOTE", TYPE), tmp_path)
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "BRAKE_BUS" in ids and "KEY_BTN" in ids
    assert "THROTTLE_STICK" not in ids and "KEY_ACTIVE" not in ids


def test_row5_the_remote_and_the_machine_agree_on_the_link(tmp_path, dumper):
    """Step 3.6: the contract is the MD5 of the remote link, no longer the BASE one."""
    generate(dumper, _ctx("IS_MACHINE", TYPE), tmp_path / "machine")
    generate(dumper, _ctx("IS_REMOTE", TYPE), tmp_path / "remote")
    def md5s(folder):
        text = (tmp_path / folder / "combus_peer_links.h").read_text(encoding="utf-8")
        block = text[text.index("namespace RemoteDumperTruck {"):]
        return tuple(re.search(rf"{name}\[16\]\s*=\s*\{{([^}}]*)\}}", block).group(1).strip()
                     for name in ("kAnnouncedMd5", "kExpectedMd5"))
    machine, remote = md5s("machine"), md5s("remote")
    assert machine[0] != machine[1]                                         # the two sides differ
    assert remote == (machine[1], machine[0])


def test_row6_a_remote_drives_a_machine_type(tmp_path, dumper):
    with pytest.raises(GeneratorError, match="MACHINE_TYPE"):
        generate(dumper, _ctx("IS_REMOTE"), tmp_path)


def test_a_machine_is_not_mirrored(tmp_path, dumper):
    """Only a remote is mirrored: a machine keeps its own directions."""
    generate(dumper, _ctx("IS_MACHINE", TYPE), tmp_path)
    ids = (tmp_path / "combus_ids.h").read_text(encoding="utf-8")
    assert "THROTTLE_STICK" in ids
