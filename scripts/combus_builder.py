#!/usr/bin/env python3
"""
combus_builder.py — Standalone CLI wrapper for the combus_builder pipeline.

Runs the same pipeline as the PlatformIO hook (combus_scons_hook.py),
but without a live SCons env. Use cases:

  - CLI generation: `python scripts/combus_builder.py`
  - CI smoke tests
  - IDE integration that does not run a real PlatformIO build
  - Manual inspection of generated headers before wiring the build

Pipeline (identical to combus_scons_hook.main()):
  1. BuildContext from --define flags (or empty if none given).
  2. Discover + parse .cb files under <project-root>/src.
  3. Canonise.
  4. Generate headers + MD5 artefacts into --out-dir.

Usage:
  python scripts/combus_builder.py
      [--project-root DIR]   # default: current directory
      [--out-dir DIR]        # default: <project-root>/out/combus_generated
      [--define TOKEN]       # e.g. "-D IS_MACHINE". May be repeated.

Exit codes:
  0  success
  1  generation error
  2  CLI error
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parent.parent
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Run the combus_builder pipeline (standalone CLI)."
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=_REPO_ROOT,
        help="Project root (where to find src/, etc.).",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Where to write generated headers. "
             "Default: <project-root>/out/combus_generated.",
    )
    parser.add_argument(
        "--define",
        action="append",
        default=[],
        help="CPPDEFINE token, e.g. '-D IS_MACHINE' or '-D KEY=VAL'. "
             "May be repeated.",
    )

    args = parser.parse_args(argv)

    project_root = args.project_root.resolve()
    out_dir = (args.out_dir or project_root / "out" / "combus_generated").resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    # Late imports.
    from scripts.combus_builder.canon import canonize
    from scripts.combus_builder.flags import (
        BuildContextError,
        acquire_build_context,
    )
    from scripts.combus_builder.generator import GeneratorError, generate
    from scripts.combus_builder.parser import ParseError, discover_and_parse

    # 1. BuildContext from override.
    try:
        ctx = acquire_build_context(
            env=None,
            project_root=project_root,
            override_cppdefines=args.define or None,
            require_non_empty=bool(args.define),
        )
    except BuildContextError as e:
        print(f"combus_builder: FATAL: {e}", file=sys.stderr)
        return 1

    print(
        f"[combus_builder] BuildContext acquired "
        f"(source={ctx.cppdefines_source}, "
        f"{len(ctx.defines)} defines, "
        f"{len(ctx.defines_with_value)} valued)"
    )

    # 2. Discover + parse.
    try:
        _, _, parsed = discover_and_parse(project_root=project_root)
    except ParseError as e:
        print(f"combus_builder: FATAL: {e}", file=sys.stderr)
        return 1

    # 3. Canonise.
    try:
        canon = canonize(parsed)
    except Exception as e:  # ChannelError family
        print(f"combus_builder: FATAL: canonisation failed: {e}", file=sys.stderr)
        return 1

    # 4. Generate.
    try:
        sel = generate(canon.canonical_definitions, ctx, out_dir)
    except GeneratorError as e:
        print(f"combus_builder: FATAL: generation failed: {e}", file=sys.stderr)
        return 1

    print(
        f"[combus_builder] generated "
        f"combus ({sel.full.ch_count} ch, WIRE_END={sel.full.wire_end}), "
        f"combus_local ({sel.local.ch_count} ch), "
        f"combus_remote ({sel.remote.ch_count} ch) "
        f"-> {out_dir}"
    )
    print(
        f"[combus_builder] Run `pio run -e <env>` or include {out_dir} in your "
        "build system to consume the generated headers."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())