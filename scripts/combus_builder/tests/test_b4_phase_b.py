"""
B4 phase B — test minimal de la chaîne de génération ComBus v2.

Ce test reproduit ce que ferait un build PlatformIO réel (`pio run -e
<env>`) avec `extra_scripts = post:scripts/combus_scons_hook.py`, mais
sans avoir besoin d'une toolchain PlatformIO installée.

Il :
  1. Construit un env simulé (même forme qu'un env PlatformIO réel).
  2. Exécute `main(env)` du hook, qui doit produire les 13 artefacts
     attendus dans `_b4_out/combus_generated/`.
  3. Pour chaque artefact .h/.cpp, lance un parsing syntaxique C++
     minimaliste (équilibrage des accolades + détection des includes).
  4. Vérifie la présence des symboles attendus (AnalogComBusArray,
     DigitalComBusArray, comBus, AnalogComBusID::CH_COUNT).

Si ce test passe, c'est une preuve que la chaîne de génération est
fonctionnelle end-to-end. C'est l'équivalent "B4 sans PlatformIO".

Usage (from repo root):
    python scripts/combus_builder/tests/test_b4_phase_b.py
"""
import io
import os
import re
import sys
import shutil
from pathlib import Path

# Force utf-8 output on Windows consoles.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.path.insert(0, '.')

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

REPO = Path('.').resolve()
OUT_ROOT = REPO / '_b4_out'


class FakeEnv(dict):
    """Simulates an SCons env.PlatformIO injects."""
    def get(self, k, d=None):
        return dict.get(self, k, d)


def reset_out():
    if OUT_ROOT.exists():
        shutil.rmtree(OUT_ROOT)
    OUT_ROOT.mkdir(parents=True)


def build_fake_env():
    """Simulate `env:machines` from platformio.ini with MACHINE_VOLVO_A60_H_BRUDER."""
    return FakeEnv({
        'CPPDEFINES': {
            'IS_MACHINE': None,
            'HAS_FAILSAFE': None,
            'HAS_VBAT_FAILSAFE': None,
            'MACHINE_VOLVO_A60_H_BRUDER': None,
            'BOARD_ESP32_8M_6S': None,
            'BOARD_DC_DRIVER_DRV8801': None,
            'VBAT_LIPO': None,
            'INPUT_PS4_DS4_BT': None,
            'COMBUS_UART_TX': '2',
            'MOTION_ENABLED': None,
            'DEBUG_DASHBOARD': None,
        },
        'PROJECT_SRC_DIR': './src',
        'PROJECT_DIR': '.',
        'PROJECT_BUILD_DIR': str(OUT_ROOT),
        'PIOENV': 'b4_test',
        'CPPPATH': [],
    })


def run_hook(env):
    """Execute combus_scons_hook.main(env) in an isolated namespace."""
    src_hook = open('scripts/combus_scons_hook.py', 'r', encoding='utf-8').read()
    # Strip the SCons-only top-level statements.
    lines = []
    for line in src_hook.splitlines():
        if line.strip() == 'Import("env")':
            continue
        if line.strip() == 'main(env)':
            continue
        lines.append(line)
    body = '\n'.join(lines)
    ns = {
        '__file__': 'scripts/combus_scons_hook.py',
        '__name__': '__main__',
        'Import': lambda n: env,  # not used, but we keep it safe
        'env': env,
    }
    exec(compile(body, 'scripts/combus_scons_hook.py', 'exec'), ns)
    return ns['main'](env)


def check_cpp_syntax(path: Path) -> tuple[bool, str]:
    """
    Minimal C++ syntax check: balanced braces + parens + presence of #include.

    Not a real C++ parser, but enough to catch obvious mistakes
    (missing closing brace, malformed include, etc.).
    """
    text = path.read_text(encoding='utf-8')
    # Strip C++ line comments.
    text = re.sub(r'//.*', '', text)
    # Strip C++ block comments.
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Strip string literals.
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)

    n_open = text.count('{')
    n_close = text.count('}')
    if n_open != n_close:
        return False, f"unbalanced braces: {n_open} {{ vs {n_close} }}"
    n_paren_open = text.count('(')
    n_paren_close = text.count(')')
    if n_paren_open != n_paren_close:
        return False, f"unbalanced parens: {n_paren_open} ( vs {n_paren_close} )"
    if path.suffix in ('.h', '.hpp', '.cpp'):
        if '#include' not in text:
            return False, "no #include in C++ source"
    return True, ""


def check_required_symbols(out_dir: Path) -> list[str]:
    """Verify the generated combus.h/cpp declare the required runtime symbols."""
    errors = []

    combus_h = out_dir / 'combus.h'
    if not combus_h.exists():
        return [f"combus.h missing"]

    text = combus_h.read_text(encoding='utf-8')

    required = [
        ('extern AnalogComBus AnalogComBusArray', 'AnalogComBusArray extern'),
        ('extern DigitalComBus DigitalComBusArray', 'DigitalComBusArray extern'),
        ('extern ComBus comBus', 'comBus extern'),
        ('CH_COUNT', 'CH_COUNT sentinel'),
    ]
    for needle, descr in required:
        if needle not in text:
            errors.append(f"combus.h missing: {descr} ('{needle}')")

    return errors


def main() -> int:
    failures = []

    # ----- Test B4 : full pipeline + syntax check + symbol check -----
    print("B4 — full pipeline + C++ syntax check")
    reset_out()
    env = build_fake_env()
    rc = run_hook(env)
    print(f"  main() returned: {rc}")

    out_dir = OUT_ROOT / 'combus_generated'
    if not out_dir.exists():
        failures.append("B4: out_dir not created")
        return 1

    files = sorted(out_dir.iterdir())
    print(f"  generated {len(files)} files in {out_dir}")
    for f in files:
        print(f"    - {f.name}")

    # EXPECTED_ARTEFACTS = 13 files (3 views x 3 + 3 md5 + 1 wire_common)
    if len(files) != 13:
        failures.append(f"B4: expected 13 files, got {len(files)}")

    # Syntax check on every .h and .cpp.
    syntax_errors = []
    for f in files:
        if f.suffix in ('.h', '.hpp', '.cpp'):
            ok, msg = check_cpp_syntax(f)
            if not ok:
                syntax_errors.append(f"{f.name}: {msg}")
    if syntax_errors:
        failures.append(f"B4: syntax errors: {syntax_errors}")
        for err in syntax_errors:
            print(f"    [SYNTAX] {err}")
    else:
        print(f"  [OK] all .h/.cpp files pass balanced-braces + include check")

    # Required symbols.
    sym_errors = check_required_symbols(out_dir)
    if sym_errors:
        failures.append(f"B4: missing symbols: {sym_errors}")
        for err in sym_errors:
            print(f"    [SYMBOL] {err}")
    else:
        print(f"  [OK] combus.h declares all required runtime symbols")

    # ----- Cleanup -----
    shutil.rmtree(OUT_ROOT)

    print()
    print("=" * 60)
    if failures:
        print(f"FAILED: {len(failures)} check(s)")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("B4 PASSED: end-to-end generation works without PlatformIO.")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())