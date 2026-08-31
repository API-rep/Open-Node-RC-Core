#!/usr/bin/env python3
"""
combus_builder/parser.py — A3: Discovery + minimal YAML parsing.

Scope (A3 only):
  - Determine the buildroot of the node currently built by PlatformIO.
  - Recursively walk the buildroot and collect every file whose extension
    matches the centralized CONFIG_EXTENSIONS set.
  - Exclude .pio/ explicitly.
  - Do NOT sort or canonize the results at this stage (raw order is kept).
  - For each discovered file: identify its type by extension, load YAML,
    verify minimal structural coherence, store the raw/parsed data with
    its source path.

Out of scope (A4+):
  - Canonical sort (scope, type, theme, id).
  - CPPDEFINES resolution as business logic.
  - C++ header generation.
  - MD5 computation.
  - Full field validation.
  - Processor lookup.
  - .cbch wiring.

Buildroot strategy:
  Prefer the value already resolved by PlatformIO / SCons when available
  (env["PROJECT_SRC_DIR"] or env["PROJECT_DIR"] + "/src"). Fall back to
  "<project_root>/src" if the env is not provided (CLI / standalone mode).
  The buildroot is NEVER hardcoded to a specific node (machines/,
  remotes/, sound_module/, ...): the actual src_dir is taken from the
  PlatformIO configuration for the environment currently being built.

Usage as PlatformIO extra_script:
    extra_scripts = pre:scripts/combus_builder.py
where scripts/combus_builder.py is a thin wrapper that calls
discover_and_parse(env).

Standalone usage:
    python -m combus_builder.parser /path/to/project
"""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Any

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.stderr.write(
        "[combus_builder] FATAL: PyYAML is required. "
        "Install it via `pip install pyyaml`.\n"
    )
    raise


# =============================================================================
# CONFIG EXTENSIONS (centralized, easy to extend)
# =============================================================================

CONFIG_EXTENSIONS: set[str] = {
    ".cb",
    ".cbch",
}

# Recognized extension -> type label (used only for diagnostic/routing).
# The parser does NOT impose a structural mapping between extension and
# YAML schema beyond routing to the right parser entry point.
EXTENSION_TYPE: dict[str, str] = {
    ".cb": "channel_def",
    ".cbch": "chain_def",
}

# Explicitly excluded top-level directories (in addition to .pio/).
EXCLUDED_DIRS: set[str] = {
    ".pio",
    ".git",
    "__pycache__",
    "node_modules",
}


# =============================================================================
# BUILDROOT RESOLUTION
# =============================================================================

def resolve_buildroot(env: Any | None, project_root: Path | None = None) -> Path:
    """
    Resolve the buildroot (src dir) of the current PlatformIO environment.

    Priority:
      1. env["PROJECT_SRC_DIR"]  (canonical PlatformIO 6.x variable)
      2. env["PROJECT_DIR"] + "/src"  (fallback)
      3. project_root + "/src"  (explicit override, e.g. CLI mode)
      4. cwd + "/src"  (last fallback)

    The buildroot is ALWAYS the project src_dir, never a per-node
    subdirectory. The parser does not need to know which node is being
    built — it only needs to know where source code lives.
    """
    if env is not None:
        try:
            src_dir = env.get("PROJECT_SRC_DIR") if hasattr(env, "get") else env["PROJECT_SRC_DIR"]
            if src_dir:
                return Path(src_dir).resolve()
        except (KeyError, AttributeError):
            pass
        try:
            proj_dir = env["PROJECT_DIR"]
            return (Path(proj_dir) / "src").resolve()
        except (KeyError, AttributeError):
            pass

    if project_root is not None:
        return (project_root / "src").resolve()

    return (Path.cwd() / "src").resolve()


# =============================================================================
# DISCOVERY
# =============================================================================

def discover_config_files(buildroot: Path) -> list[Path]:
    """
    Recursively walk `buildroot` and return every file whose extension
    is in CONFIG_EXTENSIONS.

    - .pio/ is excluded explicitly.
    - Other noisy directories (git, pycache, node_modules) are excluded.
    - Results are returned in raw os.walk order (no sort, no canonicalization).
    """
    if not buildroot.is_dir():
        raise FileNotFoundError(
            f"buildroot does not exist or is not a directory: {buildroot}"
        )

    found: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(buildroot):
        # In-place prune of excluded directories (avoids descending into them).
        dirnames[:] = [d for d in dirnames if d not in EXCLUDED_DIRS]

        for name in filenames:
            ext = os.path.splitext(name)[1].lower()
            if ext in CONFIG_EXTENSIONS:
                found.append(Path(dirpath) / name)

    return found


# =============================================================================
# PARSING
# =============================================================================

class ParseError(Exception):
    """Raised when a YAML file is invalid or structurally inexploitable."""

    def __init__(self, path: Path, message: str, line: int | None = None,
                 column: int | None = None):
        self.path = path
        self.message = message
        self.line = line
        self.column = column
        loc = f" (line {line}, col {column})" if line is not None else ""
        super().__init__(f"{path}: {message}{loc}")


def parse_yaml_file(path: Path) -> tuple[Path, str, Any]:
    """
    Load a YAML file and return (path, type_label, parsed_data).

    `type_label` is derived from the extension via EXTENSION_TYPE and is
    provided for downstream routing only. This function does NOT validate
    business fields — it only checks that the YAML loaded to a usable
    Python object (dict at the top level for our schemas).

    Raises ParseError with file path and (when available) line/column.
    """
    ext = path.suffix.lower()
    type_label = EXTENSION_TYPE.get(ext, "unknown")
    if type_label == "unknown":
        # Should not happen since we filter by CONFIG_EXTENSIONS, but guard
        # against a future extension being added without a type label.
        raise ParseError(path, f"unknown extension: {ext}")

    try:
        text = path.read_text(encoding="utf-8")
    except OSError as e:
        raise ParseError(path, f"cannot read file: {e}") from e

    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as e:
        mark = getattr(e, "problem_mark", None)
        line = mark.line + 1 if mark is not None else None
        column = mark.column + 1 if mark is not None else None
        raise ParseError(path, f"invalid YAML: {e}", line=line,
                         column=column) from e

    if data is None:
        raise ParseError(path, "empty YAML document")
    if not isinstance(data, dict):
        raise ParseError(
            path,
            f"top-level YAML must be a mapping, got {type(data).__name__}",
        )

    return path, type_label, data


def parse_all(files: list[Path]) -> list[tuple[Path, str, Any]]:
    """
    Parse every file in `files`. Returns the list of (path, type, data)
    tuples in the same order as input. Collects errors but does not
    swallow them: the first error is re-raised so the build fails loudly.
    """
    results: list[tuple[Path, str, Any]] = []
    for path in files:
        results.append(parse_yaml_file(path))
    return results


# =============================================================================
# TOP-LEVEL ENTRY POINT
# =============================================================================

def discover_and_parse(
    env: Any | None = None,
    project_root: Path | None = None,
) -> tuple[Path, list[Path], list[tuple[Path, str, Any]]]:
    """
    A3 pipeline: resolve buildroot, discover config files, parse them.

    Returns (buildroot, discovered_files, parsed_data).
    `parsed_data` is a list of (path, type_label, raw_dict) tuples in
    discovery order. No sorting, no filtering, no canonization.
    """
    buildroot = resolve_buildroot(env, project_root)
    files = discover_config_files(buildroot)
    parsed = parse_all(files)
    return buildroot, files, parsed


# =============================================================================
# ENTRY POINTS (PlatformIO + CLI)
# =============================================================================

def _print_report(buildroot: Path, files: list[Path],
                  parsed: list[tuple[Path, str, Any]]) -> None:
    print(f"[combus_builder] buildroot = {buildroot}")
    print(f"[combus_builder] discovered {len(files)} config file(s):")
    for f in files:
        print(f"  - {f}")
    print(f"[combus_builder] parsed {len(parsed)} file(s) successfully")


def main(env: Any) -> int:
    """A3 entry point usable from a PlatformIO extra_script."""
    try:
        buildroot, files, parsed = discover_and_parse(env)
    except ParseError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1
    except FileNotFoundError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1

    _print_report(buildroot, files, parsed)
    return 0


def main_with_root(project_root: Path) -> int:
    """CLI entry point for standalone testing (no PlatformIO env)."""
    try:
        buildroot, files, parsed = discover_and_parse(project_root=project_root)
    except ParseError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1
    except FileNotFoundError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1

    _print_report(buildroot, files, parsed)
    return 0


if __name__ == "__main__":
    # CLI mode: optional positional arg = project root.
    if len(sys.argv) > 1:
        root = Path(sys.argv[1]).resolve()
    else:
        root = None
    if root is not None:
        sys.exit(main_with_root(root))
    else:
        sys.exit(main(env=None))
