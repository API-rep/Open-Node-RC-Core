import os

base = os.path.join(os.path.dirname(__file__), '..', 'src', 'core', 'system', 'simulation')
base = os.path.normpath(base)

files = {
    'sim.cpp':       '// EOF sim.cpp',
    'sim.h':         '// EOF sim.h',
    'ctrl.cpp':      '// EOF ctrl.cpp',
    'ctrl.h':        '// EOF ctrl.h',
    'sim_math.cpp':  '// EOF sim_math.cpp',
}

# Also fix ctrl_config.cpp
extra = {
    os.path.join(os.path.dirname(__file__), '..', 'src', 'machines', 'config', 'machines',
                 'volvo_A60H_bruder', 'inputs_map', 'ctrl_config.cpp'):
        '// EOF ctrl_config.cpp',
}

all_files = {os.path.join(base, k): v for k, v in files.items()}
all_files.update({os.path.normpath(k): v for k, v in extra.items()})

for path, marker in all_files.items():
    if not os.path.exists(path):
        print(f'MISSING: {path}')
        continue
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    count = content.count(marker)
    idx = content.find(marker)
    if idx == -1:
        print(f'No marker: {os.path.basename(path)}')
        continue
    new_content = content[:idx + len(marker)] + '\n'
    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f'{os.path.basename(path)}: {count} markers -> truncated OK')
