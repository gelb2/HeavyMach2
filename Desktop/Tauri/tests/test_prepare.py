import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


project = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("prepare", project / "scripts/prepare.py")
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class PackagingTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        self.source = root / "web"
        self.output = root / "desktop"
        self.source.mkdir()
        self.output.mkdir()
        entries = []
        for name in prepare.REQUIRED_FILES:
            data = name.encode()
            (self.source / name).write_bytes(data)
            entries.append({"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        self.manifest = {"validation": False, "files": entries}

    def save_manifest(self):
        (self.source / "asset-manifest.json").write_text(json.dumps(self.manifest))

    def test_copies_only_verified_files(self):
        self.save_manifest()
        (self.source / "unused.log").write_text("ignore")
        prepare.copy_verified(self.source, self.output)
        for entry in self.manifest["files"]:
            name = entry["path"]
            self.assertEqual((self.output / name).read_bytes(), (self.source / name).read_bytes())
        self.assertFalse((self.output / "unused.log").exists())

    def test_rejects_validation_build(self):
        self.manifest["validation"] = True
        self.save_manifest()
        with self.assertRaisesRegex(ValueError, "normal PWA"):
            prepare.copy_verified(self.source, self.output)

    def test_validation_requires_matching_web_build(self):
        self.save_manifest()
        with self.assertRaisesRegex(ValueError, "validation PWA"):
            prepare.copy_verified(self.source, self.output, validation=True)
        self.manifest["validation"] = True
        self.save_manifest()
        prepare.copy_verified(self.source, self.output, validation=True)
        self.assertEqual(json.loads((self.output / "asset-manifest.json").read_text()), self.manifest)

    def test_rejects_changed_payload(self):
        self.save_manifest()
        (self.source / "game.wasm").write_bytes(b"FAIL.wasm")
        with self.assertRaisesRegex(ValueError, "did not match"):
            prepare.copy_verified(self.source, self.output)

    def test_requires_game_data(self):
        self.manifest["files"] = [entry for entry in self.manifest["files"] if entry["path"] != "game.data"]
        self.save_manifest()
        with self.assertRaisesRegex(ValueError, "Missing desktop web dependency: game.data"):
            prepare.copy_verified(self.source, self.output)

    def test_rejects_path_outside_artifact(self):
        self.manifest["files"][0]["path"] = "../index.html"
        self.save_manifest()
        with self.assertRaisesRegex(ValueError, "Invalid packaged path"):
            prepare.copy_verified(self.source, self.output)
