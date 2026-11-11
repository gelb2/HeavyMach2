"""Boundary tests use synthetic paths and files; never require an original IPA."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('reference_package', ROOT / 'Compatibility/package_reference.py')
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)


class ReferenceOutputTests(unittest.TestCase):
    def test_external_child_allowed(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            self.assertEqual(MOD.external_output(output / 'Phone.app', output), output.resolve() / 'Phone.app')

    def test_parent_escape_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'allowed'
            with self.assertRaises(ValueError):
                MOD.external_output(output / '../escaped.app', output)

    def test_sibling_prefix_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                MOD.external_output(Path(tmp) / 'allowed-extra/a.app', Path(tmp) / 'allowed')

    def test_symlink_escape_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); allowed = root / 'allowed'; allowed.mkdir()
            outside = root / 'outside'; outside.mkdir()
            (allowed / 'link').symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                MOD.external_output(allowed / 'link/a.app', allowed)

    def test_repository_output_always_rejected(self):
        with self.assertRaises(ValueError):
            MOD.external_output(ROOT / 'build/Phone.app', ROOT)

    def test_sha_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'synthetic'; p.write_bytes(b'abc')
            self.assertEqual(MOD.sha(p), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')


if __name__ == '__main__':
    unittest.main()
