#!/usr/bin/env python3
"""
flatten_cb.py — Remove SimProc/SimChain/CtrlProc/CtrlChain aliases,
replace all usages by the canonical Cb* types.

Substitutions (word-boundary, case-sensitive):
  SimProcFn  -> CbProcFn
  SimProc    -> CbProc
  SimChain   -> CbChain
  CtrlProcFn -> CbProcFn
  CtrlProc   -> CbProc
  CtrlChain  -> CbChain

Also removes the 'using Sim* = Cb*;' alias blocks from simulation_struct.h
and the '// 1. TYPE ALIASES' section + aliases from ctrl_struct.h.
"""

import os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIRS = {'.pio', '.git', 'libdeps', '.platformio', 'memories'}
EXTENSIONS = {'.h', '.cpp'}

# ── 1. Token substitutions ─────────────────────────────────────────────────────
SUBS = [
    (re.compile(r'\bSimProcFn\b'),  'CbProcFn'),
    (re.compile(r'\bSimProc\b'),    'CbProc'),
    (re.compile(r'\bSimChain\b'),   'CbChain'),
    (re.compile(r'\bCtrlProcFn\b'), 'CbProcFn'),
    (re.compile(r'\bCtrlProc\b'),   'CbProc'),
    (re.compile(r'\bCtrlChain\b'),  'CbChain'),
]

# ── 2. Block removals (exact multi-line strings to delete) ─────────────────────
# simulation_struct.h: remove the three 'using Sim* = Cb*;' blocks
# We match from the Doxygen comment above each alias to the alias line itself.
SIM_ALIAS_BLOCK = re.compile(
    r'/\*\*\n'
    r' \* @brief Simulation proc function pointer.*?'
    r'\*/\nusing SimProcFn = CbProcFn;\n'
    r'\n'
    r'/\*\*\n'
    r' \* @brief Simulation proc descriptor.*?'
    r'\*/\nusing SimProc = CbProc;\n'
    r'\n'
    r'/\*\*\n'
    r' \* @brief Simulation channel descriptor.*?'
    r'\*/\nusing SimChain = CbChain;\n',
    re.DOTALL
)

# ctrl_struct.h: remove the '// 1. TYPE ALIASES' section (header + 3 using lines + blank)
CTRL_ALIAS_BLOCK = re.compile(
    r'// =+\n'
    r'// 1\. TYPE ALIASES\n'
    r'// =+\n'
    r'\n'
    r'using CtrlProcFn.*\n'
    r'using CtrlProc.*\n'
    r'using CtrlChain.*\n'
    r'\n',
    re.MULTILINE
)

# ctrl_struct.h: renumber sections 2→1, 3→2 after removing section 1
CTRL_SEC2 = re.compile(
    r'(// =+\n// )2(\. SPEED GATE PROC CONFIG)',
    re.MULTILINE
)
CTRL_SEC3 = re.compile(
    r'(// =+\n// )3(\. TOGGLE PROC STATE)',
    re.MULTILINE
)

# ── helpers ────────────────────────────────────────────────────────────────────
def read(path):
    for enc in ('utf-8', 'latin-1'):
        try:
            with open(path, 'r', encoding=enc) as f:
                return f.read(), enc
        except UnicodeDecodeError:
            continue
    return None, None

def write(path, content, enc):
    with open(path, 'w', encoding=enc, newline='') as f:
        f.write(content)

# ── main walk ──────────────────────────────────────────────────────────────────
updated = []

for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
    for fname in filenames:
        if os.path.splitext(fname)[1] not in EXTENSIONS:
            continue
        path = os.path.join(dirpath, fname)
        orig, enc = read(path)
        if orig is None:
            print(f'  SKIP (encoding) {os.path.relpath(path, ROOT)}')
            continue

        content = orig
        rel = os.path.relpath(path, ROOT)

        # Per-file structural removals first
        if fname == 'simulation_struct.h':
            new = SIM_ALIAS_BLOCK.sub('', content)
            if new != content:
                content = new
                print(f'  [sim aliases removed] {rel}')

        if fname == 'ctrl_struct.h':
            new = CTRL_ALIAS_BLOCK.sub('', content)
            if new != content:
                content = new
                print(f'  [ctrl alias block removed] {rel}')
            # renumber sections
            content = CTRL_SEC2.sub(r'\g<1>1\2', content)
            content = CTRL_SEC3.sub(r'\g<1>2\2', content)

        # Token substitutions (all files)
        for pat, repl in SUBS:
            content = pat.sub(repl, content)

        if content != orig:
            write(path, content, enc)
            updated.append(rel)

print(f'\nUpdated {len(updated)} files:')
for f in updated:
    print(f'  {f}')
