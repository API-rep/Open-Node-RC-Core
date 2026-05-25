"""
rename_chain.py  --  CbChannel → CbChain rename sweep
Run from project root.
"""
import os
import re
import shutil

root = os.getcwd()

# Step 1 — rename the header file (idempotent)
old_path = os.path.join(root, 'include', 'struct', 'cb_struct.h')
new_path = os.path.join(root, 'include', 'struct', 'combus_proc_struct.h')
if os.path.exists(old_path) and not os.path.exists(new_path):
    shutil.move(old_path, new_path)
    print('Renamed:', os.path.relpath(old_path, root), '->', os.path.relpath(new_path, root))
elif os.path.exists(new_path):
    print('Already renamed:', os.path.relpath(new_path, root))
else:
    print('WARNING: neither source nor destination found')

# Step 2 — ordered text replacements (specific/longer patterns first)
replacements = [
    # Type names
    (r'\bCbChannel\b',            'CbChain'),
    (r'\bSimChannel\b',           'SimChain'),
    (r'\bCtrlChannel\b',          'CtrlChain'),
    # Function names
    (r'\bsim_channel_update\b',   'sim_chain_update'),
    # Struct field names — count variant before base to avoid partial match
    (r'\bsimChannelCount\b',      'simChainCount'),
    (r'\bsimChannel\b',           'simChain'),
    (r'\bctrlChannelCount\b',     'ctrlChainCount'),
    (r'\bctrlChannel\b',          'ctrlChain'),
    # Config array names
    (r'\bkCtrlChannels\b',        'kCtrlChains'),
    (r'\bkCtrlChannelCount\b',    'kCtrlChainCount'),
    # chanOwner field (lowercase c -- does NOT touch ChanOwner type)
    (r'\bchanOwner\b',            'chainOwner'),
    # Include path (angle and quote variants)
    (r'<struct/cb_struct\.h>',    '<struct/combus_proc_struct.h>'),
    (r'"struct/cb_struct\.h"',    '"struct/combus_proc_struct.h"'),
    # @file and EOF markers inside the renamed header
    (r'@file\s+cb_struct\.h',     '@file  combus_proc_struct.h'),
    (r'// EOF cb_struct\.h',      '// EOF combus_proc_struct.h'),
    # Doc/comment patterns
    (r'\bcb_channel_update\b',    'cb_chain_update'),
    (r'4\. CHANNEL DESCRIPTOR',   '4. CHAIN DESCRIPTOR'),
]

skip_dirs = {'.pio', '.git', 'libdeps', '.platformio'}
updated = []

for dirpath, dirnames, filenames in os.walk(root):
    dirnames[:] = [d for d in dirnames if d not in skip_dirs]
    for fname in filenames:
        if not (fname.endswith('.h') or fname.endswith('.cpp')):
            continue
        path = os.path.join(dirpath, fname)
        try:
            with open(path, 'r', encoding='utf-8') as f:
                original = f.read()
        except Exception as e:
            print(f'  SKIP {os.path.relpath(path, root)}: {e}')
            continue
        content = original
        for pattern, repl in replacements:
            content = re.sub(pattern, repl, content)
        if content != original:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(content)
            updated.append(os.path.relpath(path, root))

print(f'\nUpdated {len(updated)} files:')
for p in sorted(updated):
    print(f'  {p}')
