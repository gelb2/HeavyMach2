import importlib.util
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

spec = importlib.util.spec_from_file_location('check_docs', Path(__file__).parents[1] / 'tools/check_docs.py')
check_docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_docs)


class DocumentationTests(unittest.TestCase):
    def setUp(self):
        working = TemporaryDirectory()
        self.addCleanup(working.cleanup)
        self.root = Path(working.name)
        self.files = set()
        self.manifest = {'pairs': {}, 'excerpts': []}

    def write(self, name, contents):
        target = self.root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(contents, encoding='utf-8')
        self.files.add(name)

    def inspect(self, *changed):
        return check_docs.check_documents(self.root, self.files, self.manifest, set(changed))

    def pair(self):
        self.write('README.md', '# 소개\n\n[English](README.en.md)\n')
        self.write('README.en.md', '# Introduction\n\n[한국어](README.md)\n')
        self.manifest['pairs']['README.md'] = 'README.en.md'

    def test_recognizes_local_links_and_skips_examples(self):
        self.write('guide/Read me (1).md', '# 이동 경로\n\n## 이동 경로\n\n<a id="fixed"></a>\n')
        self.write('README.md', '''# Guide

[Heading](<guide/Read me (1).md#이동-경로-1>)
[Reference]: <guide/Read me (1).md#fixed>
<img src='guide/Read%20me%20%281%29.md' alt='Example'>
[External](https://example.com/missing.md)
`[Example](ignored.md)`
<!-- [Comment](ignored.md) -->
~~~~example
~~~
[Fence](ignored.md)
~~~~
''')
        self.assertEqual(self.inspect(), ([], 2, 3))

    def test_reports_missing_targets_and_anchors(self):
        self.write('guide.md', '# First heading\n')
        self.write('README.md', '''[Missing](absent.md)
[Case](Guide.md)
[Anchor](guide.md#not-a-heading)
[Outside](../outside.md)
''')
        errors, _, links = self.inspect()
        self.assertEqual(links, 4)
        self.assertEqual(len(errors), 4)
        self.assertIn('missing heading or anchor: guide.md#not-a-heading', errors[2])

    def test_requires_edits_in_both_languages(self):
        self.pair()
        errors, _, _ = self.inspect('README.md')
        self.assertEqual(len(errors), 1)
        self.assertIn('Check and update README.en.md', errors[0])
        self.assertEqual(self.inspect('README.md', 'README.en.md')[0], [])

    def test_requires_reciprocal_language_links(self):
        self.pair()
        self.write('README.en.md', '# Introduction\n')
        self.assertEqual(self.inspect()[0], ['README.en.md: missing language link to README.md'])

    def test_reports_missing_pair_and_unregistered_translation(self):
        self.manifest['pairs']['README.md'] = 'README.en.md'
        self.write('README.md', '# 소개\n')
        self.write('docs/en/extra.md', '# Unregistered\n')
        errors, _, _ = self.inspect()
        self.assertEqual(len(errors), 2)
        self.assertIn('Missing Korean/English document: README.md / README.en.md', errors)
        self.assertIn('docs/en/extra.md: English document has no translation pair or excerpt entry', errors)

    def test_accepts_tracked_english_excerpt(self):
        self.write('docs/en/excerpt.md', '# Excerpt\n')
        self.manifest['excerpts'].append('docs/en/excerpt.md')
        self.assertEqual(self.inspect(), ([], 1, 0))


if __name__ == '__main__':
    unittest.main()
