"""project_version.h -> kProjectVersionMajor/Minor in combus_wire_common.h.

The builder reads the project contract version from <project_root>/
project_version.h (read_project_version) and the generator writes it in the
shared header combus_wire_common.h.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize  # noqa: E402
from scripts.combus_builder.flags import BuildContext  # noqa: E402
from scripts.combus_builder.generator import generate  # noqa: E402
from scripts.combus_builder.md5 import (  # noqa: E402
    ProjectVersionError,
    read_project_version,
)
from scripts.combus_builder.parser import discover_and_parse  # noqa: E402


def _write(tmp_path: Path, text: str) -> Path:
    (tmp_path / "project_version.h").write_text(text, encoding="utf-8")
    return tmp_path


TEMPLATE = """
namespace project {
static constexpr uint8_t PROJECT_VERSION_MAJOR = @MAJOR@u;
static constexpr uint8_t PROJECT_VERSION_MINOR = @MINOR@u;
static constexpr uint8_t PROJECT_VERSION_DEV = 0u;
}
"""


def _header(major, minor) -> str:
    return TEMPLATE.replace("@MAJOR@", str(major)).replace("@MINOR@", str(minor))


def test_reads_both_numbers(tmp_path):
    root = _write(tmp_path, _header(3, 17))
    assert read_project_version(root) == (3, 17, 0)


def test_real_project_header_is_readable():
    major, minor, _dev = read_project_version(REPO_ROOT)
    assert 0 <= major <= 255 and 0 <= minor <= 255


def test_commented_definitions_are_ignored(tmp_path):
    text = (
        "// static constexpr uint8_t PROJECT_VERSION_MAJOR = 9u;\n"
        "/* PROJECT_VERSION_MINOR = 9u; */\n"
        + _header(1, 2)
    )
    assert read_project_version(_write(tmp_path, text)) == (1, 2, 0)


@pytest.mark.parametrize("major,minor", [(256, 0), (0, 256), (1000, 1)])
def test_out_of_range_is_an_error(tmp_path, major, minor):
    root = _write(tmp_path, _header(major, minor))
    with pytest.raises(ProjectVersionError):
        read_project_version(root)


def test_missing_constant_is_an_error(tmp_path):
    text = "static constexpr uint8_t PROJECT_VERSION_MAJOR = 1u;\n"
    with pytest.raises(ProjectVersionError):
        read_project_version(_write(tmp_path, text))


def test_missing_file_is_an_error(tmp_path):
    with pytest.raises(ProjectVersionError):
        read_project_version(tmp_path)


FLAGS = frozenset({
    "IS_MACHINE", "HAS_FAILSAFE", "MACHINE_VOLVO_A60_H_BRUDER", "IS_MAINBOARD",
    "BOARD_ESP32_8M_6S", "HAS_VBAT_FAILSAFE", "VBAT_LIPO",
    "HAS_REMOTE_LINK_LOST_FALLBACK", "HAS_RUNLEVEL_MANAGER", "HAS_IGNIT_KEY",
})


def _generate(tmp_path: Path, **kwargs) -> str:
    _, _, parsed = discover_and_parse(project_root=REPO_ROOT, machine_type="dumper_truck")
    ctx = BuildContext(
        buildroot=REPO_ROOT / "src", defines=FLAGS,
        defines_with_value={}, cppdefines_source="override",
    )
    generate(canonize(parsed).canonical_definitions, ctx, tmp_path, **kwargs)
    return (tmp_path / "combus_wire_common.h").read_text(encoding="utf-8")


def test_generate_writes_the_given_version(tmp_path):
    text = _generate(tmp_path, project_version=(7, 42, 0))
    assert "kProjectVersionMajor = 7u;" in text
    assert "kProjectVersionMinor = 42u;" in text


def test_generate_writes_the_given_version_with_a_prefix(tmp_path):
    text = _generate(tmp_path, project_version=(5, 6, 0), filename_prefix="board")
    assert "kProjectVersionMajor = 5u;" in text
    assert "kProjectVersionMinor = 6u;" in text
    assert not (tmp_path / "_md5_scratch").exists()


def test_generate_without_version_keeps_the_placeholder(tmp_path):
    text = _generate(tmp_path)
    assert "kProjectVersionMajor = 0u;" in text
    assert "kProjectVersionMinor = 1u;" in text
