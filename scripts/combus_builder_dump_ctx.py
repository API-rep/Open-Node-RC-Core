#!/usr/bin/env python3
"""
combus_builder_dump_ctx.py — A4 integration helper.

PlatformIO `post:` hook. Run AFTER a real build to dump the
acquired BuildContext (raw env["CPPDEFINES"], no override) into
`.pio/build/<env>/combus_ctx.json`.

This artifact is the input of the integration test:
  test_integration_against_idedata_machine_env
  test_integration_against_idedata_remote_env

The test then runs `pio run -t idedata -e <env>` separately and
compares the two values via compare_context_to_idedata(...). The
two sources are produced by two SEPARATE pio invocations, which is
the only structurally sound way to close the "is the production
path the same as the idedata path" question.

Usage in platformio.ini:

    extra_scripts =
        scripts/combus_builder_dump_ctx.py
        ; + any post: scripts that consume combus_ctx.json

This script is a SCons post: hook. SCons injects `env` automatically.
"""

import sys

# SCons / PlatformIO injects `env` at top level.
Import("env")  # noqa: F821 — SCons inject

from pathlib import Path

try:
    from scripts.combus_builder.flags import (
        dump_ctx_to_json,
        BuildContextError,
    )
except ImportError as e:
    sys.stderr.write(
        f"[combus_builder_dump_ctx] cannot import combus_builder: {e}. "
        "Make sure the project's Python path includes the repo root.\n"
    )
    raise


def dump_ctx_post_action(target, source, env):  # noqa: ARG001 — SCons signature
    """SCons post-action: dump the build context to JSON."""
    # PIO 6.x exposes PROJECT_BUILD_DIR / PROJECT_DATA_DIR or simply
    # the build dir via the env. We use the canonical PROJECT_BUILD_DIR.
    # Falling back to "pio/build/<env>" if the variable is missing.
    project_build_dir = env.get("PROJECT_BUILD_DIR")
    if not project_build_dir:
        # Fallback: derive from PROJECT_DIR + the current env name.
        project_dir = env.get("PROJECT_DIR")
        pio_env = env.get("PIOENV") or env.get("PROGNAME") or "default"
        if not project_dir:
            sys.stderr.write("[combus_builder_dump_ctx] no PROJECT_BUILD_DIR / PROJECT_DIR\n")
            return 1
        project_build_dir = Path(project_dir) / ".pio" / "build" / pio_env

    out_path = Path(project_build_dir) / "combus_ctx.json"

    try:
        ctx = dump_ctx_to_json(env, out_path)
    except BuildContextError as e:
        sys.stderr.write(f"[combus_builder_dump_ctx] FATAL: {e}\n")
        # Do NOT env.Exit: this is a post-action, exiting would corrupt
        # the build. Just log and let the integration test fail loudly.
        return 1

    sys.stdout.write(
        f"[combus_builder_dump_ctx] dumped {len(ctx.defines)} defines + "
        f"{len(ctx.defines_with_value)} values to {out_path}\n"
    )
    return 0


# Register as a post-action so it fires AFTER a real build completes.
# post: actions run AFTER `pio run` finishes its work, but BEFORE data
# is purged. Importantly, by that point env["CPPDEFINES"] is resolved.
env.AddPostAction("$PROG_PATH", dump_ctx_post_action)
