"""
A12 rev4 — full test suite.

Runs combus_scons_hook.main() under different env var combinations to
verify the failure policy and anti-stale guarantee.

The helper script (_a12_helper_v2.py pattern, inlined in the comment
below) exec's the hook source with mocked Import("env") and env. We
mock the SCons bindings so the hook's module-level Import("env") and
main(env) calls work without a real SCons.

Usage (from repo root):
    python scripts/combus_builder/tests/test_a12_rev4.py
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent.parent
HELPER = REPO / "_a12_helper_v2.py"

# Helper content (re-created here if missing). The helper exec's the
# hook source into globals() so that 'main' is accessible after exec.
_HELPER_SRC = r'''import os, sys

sys.path.insert(0, '.')

src_hook = open('scripts/combus_scons_hook.py', 'r', encoding='utf-8').read()
src_hook = '\n'.join(l for l in src_hook.splitlines()
                if 'Import("env")' not in l and l.strip() != 'main(env)')

class FakeEnv(dict):
    def get(self, k, d=None):
        return dict.get(self, k, d)

env = FakeEnv()

if os.environ.get('A12_EMPTY_CPPDEFINES', ''):
    env['CPPDEFINES'] = {}
else:
    env['CPPDEFINES'] = {
        'IS_MACHINE': None,
        'HAS_FAILSAFE': None,
        'HAS_VBAT_FAILSAFE': None,
        'MACHINE_VOLVO_A60_H_BRUDER': None,
        'BOARD_ESP32_8M_6S': None,
    }

env['PROJECT_SRC_DIR'] = './src'
env['PROJECT_DIR'] = '.'
env['PROJECT_BUILD_DIR'] = os.environ.get('A12_BUILD_DIR', './out_a12_rev4')
env['PIOENV'] = 'test'
env['CPPPATH'] = []

ns = globals()
ns['__file__'] = 'scripts/combus_scons_hook.py'
ns['Import'] = lambda n: env
ns['env'] = env
exec(compile(src_hook, 'scripts/combus_scons_hook.py', 'exec'), ns)
main(env)
'''


def _ensure_helper():
    if not HELPER.exists() or HELPER.read_text(encoding='utf-8') != _HELPER_SRC:
        HELPER.write_text(_HELPER_SRC, encoding='utf-8')


def main() -> int:
    _ensure_helper()
    failures = []

    # ----- Test 1 : success -----
    print("Test 1: success path -> exit 0")
    out_dir = REPO / "out_a12_rev4"
    if out_dir.exists():
        shutil.rmtree(out_dir)
    env_overlay = os.environ.copy()
    env_overlay.pop('A12_EMPTY_CPPDEFINES', None)
    env_overlay['A12_BUILD_DIR'] = str(out_dir)
    res = subprocess.run([sys.executable, str(HELPER)],
                         cwd=REPO, capture_output=True, text=True,
                         env=env_overlay)
    print(f"  exit={res.returncode}")
    try:
        assert res.returncode == 0, f"exit={res.returncode}, stderr={res.stderr}"
        assert "FATAL" not in res.stderr
        assert "CPPPATH" in res.stdout
        assert (out_dir / "combus_generated" / "combus.h").is_file()
        print("  OK")
    except AssertionError as e:
        failures.append(f"Test 1: {e}")
        print(f"  FAIL: {e}")

    # ----- Test 2 : empty CPPDEFINES -----
    print()
    print("Test 2: empty CPPDEFINES -> exit 1 + FATAL")
    if (out_dir / "combus_generated").exists():
        shutil.rmtree(out_dir / "combus_generated")
    env_overlay = os.environ.copy()
    env_overlay['A12_EMPTY_CPPDEFINES'] = "1"
    env_overlay['A12_BUILD_DIR'] = str(out_dir)
    res = subprocess.run([sys.executable, str(HELPER)],
                         cwd=REPO, capture_output=True, text=True,
                         env=env_overlay)
    print(f"  exit={res.returncode}")
    try:
        assert res.returncode == 1
        assert "FATAL" in res.stderr
        print("  OK")
    except AssertionError as e:
        failures.append(f"Test 2: {e}")
        print(f"  FAIL: {e}")

    # ----- Test 3 : anti-stale -----
    print()
    print("Test 3: anti-stale — failed build leaves no stale artefacts")
    dst = REPO / "out_a12_rev4" / "combus_generated"
    try:
        assert not dst.exists() or not any(dst.iterdir())
        print("  OK")
    except AssertionError as e:
        failures.append(f"Test 3: stale artefacts present")
        print(f"  FAIL: stale artefacts present in {dst}")

    # ----- Test 4 : SKIP no artefacts -----
    print()
    print("Test 4: COMBUS_BUILDER_SKIP=1 with no artefacts -> FATAL")
    skip_dir = REPO / "out_a12_rev4_skip"
    if skip_dir.exists():
        shutil.rmtree(skip_dir)
    env_overlay = os.environ.copy()
    env_overlay['COMBUS_BUILDER_SKIP'] = "1"
    env_overlay.pop('A12_EMPTY_CPPDEFINES', None)
    env_overlay['A12_BUILD_DIR'] = str(skip_dir)
    res = subprocess.run([sys.executable, str(HELPER)],
                         cwd=REPO, capture_output=True, text=True,
                         env=env_overlay)
    print(f"  exit={res.returncode}")
    try:
        assert res.returncode == 1
        assert "FATAL" in res.stderr
        assert "COMBUS_BUILDER_SKIP" in res.stderr
        print("  OK")
    except AssertionError as e:
        failures.append(f"Test 4: {e}")
        print(f"  FAIL: {e}")

    # ----- Test 5 : SKIP with full artefact set -----
    # Phase 2 / A12: the hook checks the FULL EXPECTED_ARTIFACTS list
    # (9 files: combus {h,cpp,_ids} + combus_local_ids + combus_remote_ids
    #  + 3 MD5 + combus_wire_common). We populate all 9 sentinel files.
    print()
    print("Test 5: COMBUS_BUILDER_SKIP=1 with full artefact set -> success")
    sentinel_dir = skip_dir / "combus_generated"
    sentinel_dir.mkdir(parents=True, exist_ok=True)
    EXPECTED_ARTEFACT_NAMES = (
        "combus.h", "combus.cpp", "combus_ids.h",
        "combus_local_ids.h",
        "combus_remote_ids.h",
        "combus_local_md5.h", "combus_remote_md5.h", "combus_md5.h",
        "combus_wire_common.h",
    )
    for name in EXPECTED_ARTEFACT_NAMES:
        (sentinel_dir / name).write_text(f"// pre-existing {name}")
    env_overlay = os.environ.copy()
    env_overlay['COMBUS_BUILDER_SKIP'] = "1"
    env_overlay.pop('A12_EMPTY_CPPDEFINES', None)
    env_overlay['A12_BUILD_DIR'] = str(skip_dir)
    res = subprocess.run([sys.executable, str(HELPER)],
                          cwd=REPO, capture_output=True, text=True,
                          env=env_overlay)
    print(f"  exit={res.returncode}")
    try:
        assert res.returncode == 0
        assert "FATAL" not in res.stderr
        assert "using pre-existing" in res.stdout
        assert "9 files verified" in res.stdout
        print("  OK")
    except AssertionError as e:
        failures.append(f"Test 5: {e}")
        print(f"  FAIL: {e}")
        print(f"  stdout: {res.stdout[-300:]!r}")
        print(f"  stderr: {res.stderr[-300:]!r}")

    # ----- Test 5b : SKIP with PARTIAL artefact set -> FATAL -----
    # Skip path must reject a partial set, not just missing all artefacts.
    print()
    print("Test 5b: COMBUS_BUILDER_SKIP=1 with PARTIAL artefacts -> FATAL")
    partial_dir = REPO / "out_a12_rev4_skip_partial"
    if partial_dir.exists():
        shutil.rmtree(partial_dir)
    partial_dst = partial_dir / "combus_generated"
    partial_dst.mkdir(parents=True, exist_ok=True)
    # Provide only combus.h, leave the other 11 missing.
    (partial_dst / "combus.h").write_text("// only combus.h")
    env_overlay = os.environ.copy()
    env_overlay['COMBUS_BUILDER_SKIP'] = "1"
    env_overlay.pop('A12_EMPTY_CPPDEFINES', None)
    env_overlay['A12_BUILD_DIR'] = str(partial_dir)
    res = subprocess.run([sys.executable, str(HELPER)],
                         cwd=REPO, capture_output=True, text=True,
                         env=env_overlay)
    print(f"  exit={res.returncode}")
    try:
        assert res.returncode == 1
        assert "FATAL" in res.stderr
        assert "incomplete" in res.stderr
        # Verify that out_dir is left untouched (only combus.h is there,
        # no new files were created).
        files_in_partial = sorted(p.name for p in partial_dst.iterdir())
        assert files_in_partial == ["combus.h"], (
            f"out_dir was modified by failed SKIP: {files_in_partial}"
        )
        print("  OK")
    except AssertionError as e:
        failures.append(f"Test 5b: {e}")
        print(f"  FAIL: {e}")
        print(f"  stderr: {res.stderr[-300:]!r}")

    # ----- Test 6 : 2-step success then failure preserves out_dir -----
    # This is the rev 6 rewrite of the anti-stale test. Scenario:
    #   1. A successful build produces artefacts A in out_dir.
    #   2. A second build attempt fails (CPPDEFINES empty), the hook
    #      must sys.exit(1), the staging is cleaned up, and the
    #      artefacts A are STILL present in out_dir (because we only
    #      remove out_dir as part of a SUCCESSFUL publish, never on
    #      a failed run).
    #   3. No partial publication happened.
    print()
    print("Test 6: 2-step failure preserves previous artefacts (anti-stale)")
    twostep_dir = REPO / "out_a12_rev4_twostep"
    if twostep_dir.exists():
        shutil.rmtree(twostep_dir)
    # Step 1: successful build.
    env_overlay = os.environ.copy()
    env_overlay.pop('A12_EMPTY_CPPDEFINES', None)
    env_overlay.pop('COMBUS_BUILDER_SKIP', None)
    env_overlay['A12_BUILD_DIR'] = str(twostep_dir)
    res = subprocess.run([sys.executable, str(HELPER)],
                         cwd=REPO, capture_output=True, text=True,
                         env=env_overlay)
    assert res.returncode == 0, f"step 1 failed: {res.stderr}"
    twostep_dst = twostep_dir / "combus_generated"
    files_after_step1 = sorted(p.name for p in twostep_dst.iterdir())
    print(f"  step 1: success, {len(files_after_step1)} files in out_dir")
    # Step 2: failed build (CPPDEFINES empty -> fatal).
    env_overlay = os.environ.copy()
    env_overlay['A12_EMPTY_CPPDEFINES'] = "1"
    env_overlay.pop('COMBUS_BUILDER_SKIP', None)
    env_overlay['A12_BUILD_DIR'] = str(twostep_dir)
    res = subprocess.run([sys.executable, str(HELPER)],
                         cwd=REPO, capture_output=True, text=True,
                         env=env_overlay)
    print(f"  step 2: exit={res.returncode}")
    try:
        assert res.returncode == 1, f"step 2 must exit 1, got {res.returncode}"
        assert "FATAL" in res.stderr
        # Staging was cleaned up.
        staging_dir = twostep_dir / "combus_generated.new"
        assert not staging_dir.exists(), (
            f"staging directory {staging_dir} was not cleaned up after "
            "fatal failure"
        )
        # out_dir is left UNTOUCHED: the artefacts from step 1 are still
        # there. This is the key invariant — a failed generation must
        # never delete a previously valid out_dir.
        files_after_step2 = sorted(p.name for p in twostep_dst.iterdir())
        assert files_after_step2 == files_after_step1, (
            f"out_dir was modified by failed build. "
            f"Before: {files_after_step1}, after: {files_after_step2}"
        )
        print("  OK")
    except AssertionError as e:
        failures.append(f"Test 6: {e}")
        print(f"  FAIL: {e}")

    # Cleanup
    for d in (REPO / "out_a12_rev4", REPO / "out_a12_rev4_skip",
              REPO / "out_a12_rev4_skip_partial",
              REPO / "out_a12_rev4_twostep"):
        if d.exists():
            shutil.rmtree(d)
    if HELPER.exists():
        HELPER.unlink()

    # Cleanup moved into the test loop above (rev 6).

    print()
    print("=" * 60)
    if failures:
        print(f"FAILED: {len(failures)} test(s)")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("All A12 rev6 tests PASSED.")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())