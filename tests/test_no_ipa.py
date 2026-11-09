import hashlib
import importlib.util
import io
from pathlib import Path
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('guard', Path(__file__).parents[1]/'tools/check_no_ipa.py')
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)


def zipped(name):
    b = io.BytesIO()
    with zipfile.ZipFile(b, 'w') as z:
        z.writestr(name, b'synthetic fixture, not original game data')
    return b.getvalue()


class GuardTests(unittest.TestCase):
    def test_ipa_case_and_directory(self):
        for name in ['game.ipa','GAME.IPA','backup/game.IpA','game.ipa/part']:
            self.assertIsNotNone(guard.violation(name,b'fixture'))

    def test_renamed_package(self):
        self.assertIsNotNone(guard.violation('renamed.bin',zipped('Payload/Test.app/Info.plist')))

    def test_wrapped_ipa(self):
        self.assertIsNotNone(guard.violation('backup.zip',zipped('folder/Game.IPA')))

    def test_original_bytes_by_hash(self):
        b=b'synthetic original';h=hashlib.sha256(b).hexdigest()
        self.assertIsNotNone(guard.violation('hidden.dat',b,{h}))

    def test_lfs_original(self):
        h='a'*64;b=('version https://git-lfs.github.com/spec/v1\noid sha256:'+h+'\nsize 100\n').encode()
        self.assertIsNotNone(guard.violation('hidden.bin',b,{h}))

    def test_allowed_sources_and_metadata(self):
        self.assertIsNone(guard.violation('docs/source.json',b'{"name":"original.ipa"}'))
        self.assertIsNone(guard.violation('Sources/Core/Game.cpp',b'// source'))
        self.assertIsNone(guard.violation('fixtures/data.zip',zipped('sample.txt')))


if __name__ == '__main__':
    unittest.main()
