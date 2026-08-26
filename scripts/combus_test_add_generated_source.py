#!/usr/bin/env python3
"""
combus_test_add_generated_source.py — Phase 3 / A13 helper.

PlatformIO extra_script that adds the generated `combus_generated/combus.cpp`
to the test environment's source list. This is required for the C++ runtime
identity tests (Group C of test_combus_loopback.cpp) which reference
`AnalogComBusArray` and `DigitalComBusArray` — those symbols are defined
in the generated combus.cpp, not in any static .cpp file in the repo.

The combus_scons_hook.py (registered globally via [env] in platformio.ini)
already generates the artefacts and adds the directory to CPPPATH. This
script is a SECONDARY hook that ONLY adds the generated .cpp as a build
source. It is registered ONLY for the test_combus_loopback env (via
`extra_scripts = pre:scripts/combus_test_add_generated_source.py` in
that env's section).

Why a separate script (not a modification of combus_scons_hook.py)?
  - The hook is shared across all envs (machines, sound_node, remotes,
    test). Adding the generated .cpp as a source there would force
    every env to compile it, which is already the case for the
    non-test envs (they include combus.cpp via their own build_src_filter
    or via the runtime). For the test env, we want to opt in explicitly.
  - This keeps the hook's contract clean: it generates + publishes +
    adds CPPPATH. Source-list manipulation is a separate concern.

Usage in platformio.ini (test env only):

    [env:test_combus_loopback]
    extra_scripts =
        pre:scripts/combus_test_add_generated_source.py

The script is idempotent: if the generated combus.cpp is not present
yet (e.g. the hook hasn't run), it does nothing. The build will then
fail at link time with a clear "undefined reference" error, which is
the expected behaviour when the hook is disabled (COMBUS_BUILDER_SKIP=1
without pre-existing artefacts).
"""
from __future__ import annotations

import os
from pathlib import Path

# SCons / PlatformIO injects `env` at top level.
Import("env")  # noqa: F821 — SCons inject
try:
    Import("projenv")  # noqa: F821 — SCons inject, may not exist
except Exception:  # pragma: no cover
    projenv = None  # type: ignore[assignment]
# PlatformIO test env is a separate SCons environment. Try to import it
# so we can also add the generated directory to its CPPPATH (the test
# source files need to find combus.h, combus_local_ids.h, etc.).
try:
    Import("testenv")  # noqa: F821 — SCons inject, may not exist
except Exception:  # pragma: no cover
    testenv = None  # type: ignore[assignment]


def _generated_combus_cpp(env) -> Path | None:
    """Locate the generated combus.cpp produced by combus_scons_hook.py."""
    build_dir = env.get("PROJECT_BUILD_DIR")
    if build_dir:
        candidate = Path(build_dir) / "combus_generated" / "combus.cpp"
        if candidate.is_file():
            return candidate

    project_dir = env.get("PROJECT_DIR")
    pio_env = env.get("PIOENV") or "default"
    if project_dir:
        candidate = Path(project_dir) / ".pio" / "build" / pio_env / "combus_generated" / "combus.cpp"
        if candidate.is_file():
            return candidate

    return None


def _add_source(target_env, source_path: Path) -> None:
    """Add `source_path` to `target_env`'s source list."""
    if target_env is None:
        return
    sources = target_env.get("PIOBUILDFILES", [])
    sources = [sources] if isinstance(sources, str) else list(sources)
    src_str = str(source_path)
    if src_str not in sources:
        sources.append(src_str)
        target_env.Replace(PIOBUILDFILES=sources)
        print(f"[combus_test_add_generated_source] PIOBUILDFILES += {src_str}")


def _add_common_defs_cpppath(target_env) -> None:
    """
    Add `common_defs/include/` to CPPPATH so the generated combus.cpp
    (which transitively includes <pin_defs.h> via <machines_defs.h>) can
    be compiled in the test env.

    The test env (test_combus_loopback) doesn't `extends = env:...`, so
    it doesn't inherit the lib_deps' auto-CPPPATH that the production
    envs get. We add it explicitly here.
    """
    if target_env is None:
        return
    project_dir = env.get("PROJECT_DIR")
    if not project_dir:
        return
    pio_env = env.get("PIOENV") or "default"
    # PlatformIO stores resolved libraries under .pio/libdeps/<pio_env>/
    for lib_name in ("common_defs",):
        candidate = Path(project_dir) / ".pio" / "libdeps" / pio_env / lib_name / "include"
        if candidate.is_dir():
            cpppath = target_env.get("CPPPATH", [])
            cpppath = [cpppath] if isinstance(cpppath, str) else list(cpppath)
            if str(candidate) not in cpppath:
                cpppath.append(str(candidate))
                target_env["CPPPATH"] = cpppath
                print(f"[combus_test_add_generated_source] CPPPATH += {candidate}")


def _add_generated_cpppath(target_env, generated_dir: Path) -> None:
    """Add the generated combus_generated/ directory to CPPPATH."""
    if target_env is None:
        return
    cpppath = target_env.get("CPPPATH", [])
    cpppath = [cpppath] if isinstance(cpppath, str) else list(cpppath)
    if str(generated_dir) not in cpppath:
        cpppath.append(str(generated_dir))
        target_env["CPPPATH"] = cpppath
        print(f"[combus_test_add_generated_source] CPPPATH += {generated_dir}")


def main() -> int:
    src = _generated_combus_cpp(env)
    if src is None:
        # The hook hasn't run yet, or COMBUS_BUILDER_SKIP=1 without
        # pre-existing artefacts. Don't fail here — let the build
        # proceed and fail at link time with a clear error.
        print(
            "[combus_test_add_generated_source] generated combus.cpp not "
            "found yet — skipping. The build will fail at link time if "
            "the test references AnalogComBusArray / DigitalComBusArray."
        )
        return 0

    generated_dir = src.parent

    _add_source(env, src)
    _add_source(projenv, src)
    _add_common_defs_cpppath(env)
    _add_common_defs_cpppath(projenv)
    # The test source files (test_combus_loopback.cpp) include
    # "combus.h", "combus_local_ids.h", "combus_remote_ids.h" — they
    # need the generated directory in their CPPPATH. The test env is a
    # separate SCons environment from `env`/`projenv`, so we add it
    # explicitly here.
    _add_generated_cpppath(env, generated_dir)
    _add_generated_cpppath(projenv, generated_dir)
    _add_generated_cpppath(testenv, generated_dir)
    return 0


main()
