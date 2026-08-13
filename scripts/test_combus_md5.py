"""test_combus_md5.py — minimal test harness for scripts/combus_md5.py.

Loads `combus_md5.py` with a mocked PlatformIO `env`, then exercises the
pure functions (`resolve_file`, `resolve_include`, `_process_line`,
`build_hash_input`, `get_defined_flags`) against a temporary directory tree.

Run from the project root:
    python scripts/test_combus_md5.py
"""

import hashlib
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock

# ---------------------------------------------------------------------------
# Mock SCons.Script.Import so combus_md5.py can be imported outside PIO.
# ---------------------------------------------------------------------------

class _FakeSConsScript:
    @staticmethod
    def Import(name):
        pass


class _FakeSCons:
    Script = _FakeSConsScript


sys.modules.setdefault("SCons", _FakeSCons)
sys.modules.setdefault("SCons.Script", _FakeSConsScript)

# ---------------------------------------------------------------------------
# Build a fake PlatformIO env.
# ---------------------------------------------------------------------------

PROJECT_ROOT = Path(__file__).resolve().parent.parent
FAKE_ENV = MagicMock()
FAKE_ENV.__getitem__ = lambda self, key: {
    "PROJECT_DIR": str(PROJECT_ROOT),
}[key]
FAKE_ENV.GetProjectOption = lambda key, default=None: {
    "build_flags": "-D DEBUG_HW -D DEBUG_SYSTEM -D IS_MACHINE",
}.get(key, default)

# Patch Import to inject our fake env.
_FakeSConsScript.Import = staticmethod(lambda name: None)


def _load_combus_md5():
    """Load combus_md5.py as a module with a mocked env."""
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "combus_md5", PROJECT_ROOT / "scripts" / "combus_md5.py"
    )
    mod = importlib.util.module_from_spec(spec)
    # Inject BOTH `Import` (the SCons symbol) and `env` into the module's
    # namespace BEFORE exec.  combus_md5.py calls `Import("env")` at module
    # scope; we provide a no-op Import and pre-set `env` so the subsequent
    # `env["PROJECT_DIR"]` works.
    mod.__dict__["Import"] = lambda name: None
    mod.__dict__["env"] = FAKE_ENV
    spec.loader.exec_module(mod)
    return mod


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestResolveFile(unittest.TestCase):
    """Test the transitive #include resolver."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.defined_flags: set = set()

    def tearDown(self):
        self.tmp.cleanup()

    def _write(self, rel: str, content: str) -> Path:
        p = self.root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")
        return p

    def _load(self):
        """Load combus_md5 with PROJECT_ROOT/INCLUDE_PATHS redirected."""
        mod = _load_combus_md5()
        mod.PROJECT_ROOT = self.root
        mod.INCLUDE_PATHS = [self.root]
        return mod

    def test_no_include_passthrough(self):
        """A file with no #include returns its content unchanged."""
        p = self._write("a.inc", "X = 1,\nY = 2,\n")
        mod = self._load()
        out = mod.resolve_file(p, set(), self.defined_flags)
        self.assertEqual(out, b"X = 1,\nY = 2,\n")

    def test_single_include(self):
        """A file with one #include inlines the included content."""
        self._write("b.inc", "B_TOKEN,\n")
        p = self._write("a.inc", "#include <b.inc>\nREST,\n")
        mod = self._load()
        out = mod.resolve_file(p, set(), self.defined_flags)
        self.assertEqual(out, b"B_TOKEN,\nREST,\n")

    def test_transitive_include(self):
        """A -> B -> C: all three are inlined in order."""
        self._write("c.inc", "C_TOKEN,\n")
        self._write("b.inc", "B_TOKEN,\n#include <c.inc>\n")
        p = self._write("a.inc", "A_TOKEN,\n#include <b.inc>\n")
        mod = self._load()
        out = mod.resolve_file(p, set(), self.defined_flags)
        self.assertEqual(out, b"A_TOKEN,\nB_TOKEN,\nC_TOKEN,\n")

    def test_cycle_guard(self):
        """A -> B -> A: cycle is broken, no infinite recursion."""
        self._write("b.inc", "#include <a.inc>\n")
        p = self._write("a.inc", "#include <b.inc>\n")
        mod = self._load()
        # Should terminate quickly (no infinite recursion).
        out = mod.resolve_file(p, set(), self.defined_flags)
        # Cycle broken: a.inc visited twice, second visit yields b"".
        # Result contains the verbatim #include from b.inc (unresolved cycle).
        self.assertIsInstance(out, bytes)
        # No exception = success.

    def test_ifdef_included_when_defined(self):
        """#ifdef FLAG block is included when FLAG is in defined_flags."""
        p = self._write("a.inc",
                        "#ifdef HAS_FAILSAFE\n"
                        "FAILSAFE_TOKEN,\n"
                        "#endif\n"
                        "ALWAYS,\n")
        mod = self._load()
        out = mod.resolve_file(p, set(), {"HAS_FAILSAFE"})
        self.assertEqual(out, b"FAILSAFE_TOKEN,\nALWAYS,\n")

    def test_ifdef_excluded_when_undefined(self):
        """#ifdef FLAG block is excluded when FLAG is NOT in defined_flags."""
        p = self._write("a.inc",
                        "#ifdef HAS_FAILSAFE\n"
                        "FAILSAFE_TOKEN,\n"
                        "#endif\n"
                        "ALWAYS,\n")
        mod = self._load()
        out = mod.resolve_file(p, set(), set())
        self.assertEqual(out, b"ALWAYS,\n")

    def test_unresolved_include_kept_verbatim(self):
        """A missing #include is kept as-is (not silently dropped)."""
        p = self._write("a.inc", "#include <nonexistent.inc>\nREST,\n")
        mod = self._load()
        out = mod.resolve_file(p, set(), self.defined_flags)
        self.assertEqual(out, b"#include <nonexistent.inc>\nREST,\n")


class TestBuildHashInput(unittest.TestCase):
    """Test the hash input builder."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def _write(self, rel: str, content: str) -> Path:
        p = self.root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")
        return p

    def _load(self):
        mod = _load_combus_md5()
        mod.PROJECT_ROOT = self.root
        mod.INCLUDE_PATHS = [self.root]
        # Patch the module-level VER_MAJOR/VER_MINOR to known values.
        mod.VER_MAJOR = 1
        mod.VER_MINOR = 0
        return mod

    def test_hash_changes_with_transitive_include(self):
        """Changing a transitively-included file changes the hash."""
        self._write("b.inc", "B_v1,\n")
        a = self._write("a.inc", "#include <b.inc>\n")
        mod = self._load()
        h1 = hashlib.md5(mod.build_hash_input([a])).hexdigest()
        # Mutate b.inc.
        (self.root / "b.inc").write_text("B_v2,\n", encoding="utf-8")
        h2 = hashlib.md5(mod.build_hash_input([a])).hexdigest()
        self.assertNotEqual(h1, h2)

    def test_hash_changes_with_ifdef_flag(self):
        """Toggling HAS_FAILSAFE changes the hash when #ifdef is present."""
        p = self._write("a.inc",
                        "#ifdef HAS_FAILSAFE\n"
                        "FAILSAFE_TOKEN,\n"
                        "#endif\n"
                        "ALWAYS,\n")
        mod = self._load()
        mod.DEFINED_FLAGS = set()
        h_off = hashlib.md5(mod.build_hash_input([p])).hexdigest()
        mod.DEFINED_FLAGS = {"HAS_FAILSAFE"}
        h_on = hashlib.md5(mod.build_hash_input([p])).hexdigest()
        self.assertNotEqual(h_off, h_on)

    def test_hash_stable_for_files_without_includes(self):
        """Files without #include produce a stable hash (no flag effect)."""
        p = self._write("a.inc", "X = 1,\n")
        mod = self._load()
        mod.DEFINED_FLAGS = set()
        h_off = hashlib.md5(mod.build_hash_input([p])).hexdigest()
        mod.DEFINED_FLAGS = {"HAS_FAILSAFE"}
        h_on = hashlib.md5(mod.build_hash_input([p])).hexdigest()
        self.assertEqual(h_off, h_on)

    def test_hash_changes_with_comment(self):
        """A comment change modifies the hash (current semantics)."""
        p = self._write("a.inc", "// v1\nX = 1,\n")
        mod = self._load()
        mod.DEFINED_FLAGS = set()
        h1 = hashlib.md5(mod.build_hash_input([p])).hexdigest()
        (self.root / "a.inc").write_text("// v2\nX = 1,\n", encoding="utf-8")
        h2 = hashlib.md5(mod.build_hash_input([p])).hexdigest()
        self.assertNotEqual(h1, h2)


class TestGetDefinedFlags(unittest.TestCase):
    """Test the build_flags parser."""

    def test_parses_dash_d_space_flag(self):
        env = MagicMock()
        env.GetProjectOption = lambda k, d=None: "-D HAS_FAILSAFE -D DEBUG_HW"
        mod = _load_combus_md5()
        flags = mod.get_defined_flags(env)
        self.assertIn("HAS_FAILSAFE", flags)
        self.assertIn("DEBUG_HW", flags)

    def test_parses_dash_d_no_space_flag(self):
        env = MagicMock()
        env.GetProjectOption = lambda k, d=None: "-DHAS_FAILSAFE"
        mod = _load_combus_md5()
        flags = mod.get_defined_flags(env)
        self.assertIn("HAS_FAILSAFE", flags)

    def test_parses_list_form(self):
        env = MagicMock()
        env.GetProjectOption = lambda k, d=None: [
            "-D", "HAS_FAILSAFE", "-D", "DEBUG_HW"
        ]
        mod = _load_combus_md5()
        flags = mod.get_defined_flags(env)
        self.assertIn("HAS_FAILSAFE", flags)
        self.assertIn("DEBUG_HW", flags)

    def test_empty_when_no_flags(self):
        env = MagicMock()
        env.GetProjectOption = lambda k, d=None: ""
        mod = _load_combus_md5()
        flags = mod.get_defined_flags(env)
        self.assertEqual(flags, set())


if __name__ == "__main__":
    unittest.main(verbosity=2)
