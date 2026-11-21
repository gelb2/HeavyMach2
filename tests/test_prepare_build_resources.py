import importlib.util
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    'prepare_build_resources', Path(__file__).resolve().parents[1] / 'tools/prepare_build_resources.py')
resources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(resources)


class PublishedResourcesTests(unittest.TestCase):
    loader = ('({files:[{filename:"/campaign/Main_BG.png",start:0,end:1e1},'
              '{filename:"/campaign/MapInfo.mcb",start:10,end:11},'
              '{filename:"/campaign/ItemInfo.tble",start:11,end:12},'
              '{filename:"/campaign/manifest.json",start:12,end:13}],remote_package_size:13})')

    def test_export_can_be_reused_and_refuses_to_replace_a_different_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / 'game.data'
            data.write_bytes(b'1234567890ABC')
            output = root / 'Campaign' / 'HD'
            files = resources.file_ranges(self.loader, data.stat().st_size)
            resources.prepare_campaign(data, files, output)
            self.assertEqual((output / 'Main_BG.png').read_bytes(), b'1234567890')
            resources.prepare_campaign(data, files, output)
            path = output / 'MapInfo.mcb'
            path.write_bytes(b'X')
            with self.assertRaisesRegex(ValueError, 'different resources'):
                resources.prepare_campaign(data, files, output)
            self.assertEqual(path.read_bytes(), b'X')

    def test_refuses_paths_outside_campaign_and_out_of_bounds(self):
        for loader in (
            self.loader.replace('MapInfo.mcb', '../MapInfo.mcb'),
            self.loader.replace('MapInfo.mcb', 'main_bg.png'),
            self.loader.replace('end:11', 'end:14'),
        ):
            with self.subTest(loader=loader), self.assertRaises(ValueError):
                resources.file_ranges(loader, 13)
