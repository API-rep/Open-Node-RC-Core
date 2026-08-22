"""
Test : discover_and_parse supports both signatures (env= and project_root=).

The A12 diff shows two callers using different kwargs:
- combus_scons_hook.py calls discover_and_parse(env=env)
- combus_builder.py (CLI) calls discover_and_parse(project_root=project_root)

This test verifies that:
1. discover_and_parse(env=env) works without TypeError.
2. discover_and_parse(project_root=project_root) works without TypeError.
3. discover_and_parse(env=env, project_root=...) works (env wins per
   resolve_buildroot priority).
4. All three produce the same buildroot and the same file count.

If this test ever fails, it means the two entry points have diverged
and the diff has introduced a regression.

Usage (from repo root):
    python scripts/combus_builder/tests/test_signature_consistency.py
"""
import io
import sys
from pathlib import Path

# Force utf-8 output on Windows consoles.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.path.insert(0, '.')

from scripts.combus_builder.parser import discover_and_parse  # noqa: E402


class FakeEnv(dict):
    def get(self, k, d=None):
        return dict.get(self, k, d)


REPO = Path('.').resolve()


def main() -> int:
    failures = []

    # Case 1: env=env
    env = FakeEnv()
    env['PROJECT_SRC_DIR'] = './src'
    try:
        br1, files1, _ = discover_and_parse(env=env)
        print(f"[OK] env=env          -> {br1.name}, {len(files1)} files")
    except TypeError as e:
        failures.append(f"env=env TypeError: {e}")
        print(f"[FAIL] env=env         -> TypeError: {e}")

    # Case 2: project_root=project_root
    try:
        br2, files2, _ = discover_and_parse(project_root=REPO)
        print(f"[OK] project_root=... -> {br2.name}, {len(files2)} files")
    except TypeError as e:
        failures.append(f"project_root TypeError: {e}")
        print(f"[FAIL] project_root=... -> TypeError: {e}")

    # Case 3: both (env wins)
    env = FakeEnv()
    env['PROJECT_SRC_DIR'] = './src'
    try:
        br3, files3, _ = discover_and_parse(env=env, project_root=Path('/tmp'))
        print(f"[OK] env + project_root (env wins) -> {br3.name}, {len(files3)} files")
    except TypeError as e:
        failures.append(f"both TypeError: {e}")
        print(f"[FAIL] both -> TypeError: {e}")

    # Consistency: all three produce the same buildroot + file count.
    if not failures:
        if br1 == br2 == br3 and len(files1) == len(files2) == len(files3):
            print(f"\n[PASS] All three signatures route to the same buildroot.")
        else:
            failures.append(
                f"inconsistent buildroots: {br1} / {br2} / {br3}, "
                f"file counts: {len(files1)} / {len(files2)} / {len(files3)}"
            )
            print(f"\n[FAIL] Inconsistent buildroots.")

    if failures:
        print("\n" + "=" * 60)
        print("FAIL")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("\n" + "=" * 60)
    print("PASS: discover_and_parse signature is consistent across callers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())