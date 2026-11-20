import base64
import zipfile
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module); return module
ASSET, SCREEN = load('check_menu_background_assets'), load('check_menu_screen_evidence')

class MenuCheckTests(unittest.TestCase):
    def setUp(self):
        # Isolated parser fixtures, never product acceptance evidence. No IPA,
        # app build, screenshot, or private local state is required in CI.
        temp = tempfile.TemporaryDirectory(); self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve(); self.package = self.root / 'build/apple/Build/Products/Debug-iphonesimulator/HeavyMach2Source.app'
        (self.package / 'HD/PhoneMenu').mkdir(parents=True); (self.root / 'docs/evidence').mkdir(parents=True)
        (self.root / '.local/evidence').mkdir(parents=True)
        patches = contextlib.ExitStack(); self.addCleanup(patches.close)
        for module in (ASSET, SCREEN):
            patches.enter_context(patch.object(module, 'ROOT', self.root))
            patches.enter_context(patch.object(module, 'HOME', self.root / '.local/evidence' / module.__name__))
        patches.enter_context(patch.object(ASSET, 'PACKAGE', self.package))
        for path in set(ASSET.INPUTS + SCREEN.INPUTS):
            f = self.root / path; f.parent.mkdir(parents=True, exist_ok=True); f.write_text('test source ' + path)
        for name in ('HeavyMach2Source', 'HeavyMach2Source.debug.dylib'): (self.package / name).write_bytes(name.encode())
        phone_shop = self.root / 'Resources/PhoneShopArt/PhoneShopHangar.jpg'
        phone_shop.parent.mkdir(parents=True)
        phone_shop.write_bytes(b'authored phone shop fixture')
        (self.package / 'HD' / phone_shop.name).write_bytes(phone_shop.read_bytes())
        bases = ['Base00' + str(i) for i in range(1, 7)]; entries = []; provenance = []; pins = {}
        for device in ('iphone', 'ipad'):
            suffix = '.jpg' if device == 'iphone' else '.png'; prefix = 'PhoneMenu/' if device == 'iphone' else ''
            archive = self.root / (device + '.zip')
            with zipfile.ZipFile(archive, 'w') as z:
                for name in bases + ['ShopBG', 'CmdCenterBG']:
                    data = (device + name).encode(); f = self.package / 'HD' / (prefix + name + suffix); f.write_bytes(data)
                    z.writestr('Payload/Test.app/' + name + suffix, data)
            pins[device] = ASSET.sha(archive.read_bytes()); provenance.append({'sha256': pins[device], 'relative_path': archive.name})
            entries.extend({'profile': device, 'base': base, 'background': prefix + base + suffix} for base in bases)
        patches.enter_context(patch.object(ASSET, 'SHA', pins))
        (self.root / 'docs/source-provenance.json').write_text(json.dumps({'source_root': str(self.root), 'files': provenance}))
        (self.root / 'docs/evidence/hub-artwork-census-20261115.json').write_text(json.dumps({'baseIds': bases, 'entries': entries}))

    def hook(self, module, command, broken=False):
        with tempfile.TemporaryDirectory(dir=self.root / '.local/evidence') as directory:
            home = Path(directory); path = home / 'input.json'
            if broken: path.write_text('{')
            event = {'hook_event_name': 'PreToolUse', 'cwd': str(self.root), 'tool_name': 'exec_command', 'tool_input': {'cmd': command}}
            output = io.StringIO()
            with patch.object(module, 'HOME', home), patch.object(sys, 'argv', ['check', '--hook', '--input', str(path)]), patch.object(sys, 'stdin', io.StringIO(json.dumps(event))), contextlib.redirect_stdout(output):
                self.assertEqual(module.main(), 0)
            reply = json.loads(output.getvalue()); self.assertNotIn('permissionDecision', json.dumps(reply)); return reply

    def test_read_stash_finder_commit_and_uninstall_are_not_blocked(self):
        for module in (ASSET, SCREEN):
            for command in ('git stash push', 'git commit -m backup', 'Finder', 'remove .codex/hooks.json', 'read ThirdParty/OpenALSoft'):
                with self.subTest(module=module.__name__, command=command): self.assertEqual(self.hook(module, command), {})

    def test_relevant_work_runs_and_missing_evidence_never_passes(self):
        for module, file in ((ASSET, 'Screens.cpp'), (ASSET, 'GameSession.cpp'), (SCREEN, 'Screens.cpp'), (SCREEN, 'HM2SourceView.mm')): self.assertIn('NOT_RUN', json.dumps(self.hook(module, 'edit Sources/' + file)))

    def test_broken_evidence_is_error_without_blocking_repair(self):
        for module in (ASSET, SCREEN): self.assertIn('ERROR', json.dumps(self.hook(module, 'edit Sources/Core/UI/Screens.cpp', True)))

    def test_repeated_failure_keeps_repair_allowed(self):
        for module in (ASSET, SCREEN):
            with tempfile.TemporaryDirectory(dir=self.root / '.local/evidence') as directory:
                home = Path(directory); path = home / 'input.json'; path.write_text('{')
                event = {'hook_event_name': 'PostToolUse', 'cwd': str(self.root), 'tool_input': {'cmd': 'edit Screens.cpp'}}
                with patch.object(module, 'HOME', home), patch.object(sys, 'argv', ['check', '--hook', '--input', str(path)]):
                    for _ in range(2):
                        output = io.StringIO()
                        with patch.object(sys, 'stdin', io.StringIO(json.dumps(event))), contextlib.redirect_stdout(output): self.assertEqual(module.main(), 0)
                    records = sorted(p for p in home.glob('*.json') if p.stem.isdigit())
                    self.assertTrue(json.loads(records[-1].read_text())['repeatedFailure'])
                    self.assertNotIn('permissionDecision', output.getvalue())

    def test_stale_source_evidence_is_rejected(self):
        for module in (ASSET, SCREEN):
            with self.assertRaisesRegex(ValueError, 'stale'): module.verify({'sources': {}})

    def asset_input(self):
        census = json.loads((self.root / 'docs/evidence/hub-artwork-census-20261115.json').read_text())
        package = ASSET.PACKAGE / 'HD'; rows = []
        for device in ('iphone', 'ipad'):
            for base in census['baseIds']:
                for screen in ('hub', 'shop', 'command'):
                    if screen == 'hub':
                        image = next(x['background'] for x in census['entries'] if x['profile'] == device and x['base'] == base)
                    elif device == 'iphone':
                        image = {'shop': 'PhoneShopHangar.jpg', 'command': 'PhoneMenu/CmdCenterBG.jpg'}[screen]
                    else:
                        image = {'shop': 'ShopBG.png', 'command': 'CmdCenterBG.png'}[screen]
                    rows.append({'device': device, 'base': base, 'screen': screen, 'backgrounds': [{'image': image, 'sha256': ASSET.sha((package / image).read_bytes())}]})
        return {'sources': {p: ASSET.sha((self.root / p).read_bytes()) for p in ASSET.INPUTS}, 'binarySHA256': ASSET.sha((ASSET.PACKAGE / 'HeavyMach2Source').read_bytes()), 'gameSHA256': ASSET.sha((ASSET.PACKAGE / 'HeavyMach2Source.debug.dylib').read_bytes()), 'rows': rows}

    def test_original_file_comparison_accepts_correct_sources_only(self):
        data = self.asset_input(); self.assertEqual(ASSET.verify(data, self.package)['status'], 'PASS')
        data['rows'][0]['backgrounds'][0] = {'image': 'Base001.png', 'sha256': ASSET.sha((ASSET.PACKAGE / 'HD/Base001.png').read_bytes())}
        self.assertEqual(ASSET.verify(data, self.package)['status'], 'FAIL')

    def test_missing_device_or_base_cannot_pass(self):
        data = self.asset_input(); data['rows'].pop()
        with self.assertRaisesRegex(ValueError, 'Missing contexts'): ASSET.verify(data, self.package)

    def screen_fixture(self):
        # A one-pixel PNG tests receipt integrity, not visual correctness.
        path = self.root / '.local/evidence/parser-fixture.png'
        path.write_bytes(base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aMZkAAAAASUVORK5CYII='))
        identity = SCREEN.sha(path.read_bytes()); data = self.asset_input(); data['sources'] = {p: SCREEN.sha((self.root / p).read_bytes()) for p in SCREEN.INPUTS}
        data['rows'] = [{'device': r['device'], 'base': r['base'], 'screen': r['screen'], 'png': str(path), 'sha256': identity,
                        'review': {'reviewedSHA256': identity, 'evidence': 'unit parser fixture only', 'blackFiller': r['device'] == 'ipad', 'edgeStretch': False}} for r in data['rows']]
        return data

    def test_reviewed_black_margins_are_rejected(self):
        result = SCREEN.verify(self.screen_fixture()); self.assertEqual(result['status'], 'FAIL')
        self.assertEqual(len(result['failures']), 18)

    def test_missing_visual_review_cannot_pass(self):
        data = self.screen_fixture(); data['rows'][0].pop('review')
        self.assertIn('NOT_REVIEWED', '\n'.join(SCREEN.verify(data)['failures']))

    def test_changed_capture_is_rejected(self):
        data = self.screen_fixture(); data['rows'][0]['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'changed'): SCREEN.verify(data)

if __name__ == '__main__': unittest.main()
