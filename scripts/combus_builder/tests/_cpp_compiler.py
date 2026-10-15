"""
Shared helper for tests that need a C++ compiler.

`find_cpp_compiler()` returns the first available compiler, or None so the
caller can `pytest.skip(...)` cleanly instead of failing on a machine without
a toolchain in PATH.
"""

from __future__ import annotations

import shutil
from pathlib import Path


def find_cpp_compiler() -> tuple[list[str], str] | None:
    """
    Return (argv_prefix, label) for an available C++ compiler, or None.

    Order of preference:
      1. `g++` in PATH (native Linux/macOS or mingw).
      2. `clang++` in PATH.
      3. PlatformIO's bundled `xtensa-esp32-elf-g++` (cross-compiler).
         This is the EXACT toolchain used by PlatformIO for this project
         (platformio.ini: platform = espressif32@6.7.0, gcc 12.2.0).
         For cross-compilation callers should use -fsyntax-only (no
         linking), which is enough to validate designated initializers,
         enum values, struct field order and template instantiation.
    """
    if shutil.which("g++"):
        return (["g++"], "g++")
    if shutil.which("clang++"):
        return (["clang++"], "clang++")
    # PlatformIO's toolchain is at ~/.platformio/packages/ on Windows/Linux/macOS.
    pio_gpp = (Path.home() / ".platformio" / "packages"
               / "toolchain-xtensa-esp32" / "bin" / "xtensa-esp32-elf-g++.exe")
    if pio_gpp.exists():
        return ([str(pio_gpp)], "xtensa-esp32-elf-g++ (PlatformIO)")
    return None
