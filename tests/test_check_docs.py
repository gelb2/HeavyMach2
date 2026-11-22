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

    def test_reports_editorial_regressions_in_both_languages(self):
        self.write('docs/public/play.md', '''# 플레이
사격마다 파일·DB를 쓰거나 오디오 컨텍스트를 만들지 않는다.
원본 파츠와 신규 파츠는 공통 데이터·조립·렌더 경로를 따른다.
광고를 표시하거나 요청하지 않는다.
이 실행은 모든 임무의 검증 완료를 뜻하지 않는다.
''')
        self.write('docs/en/public/play.md', '''# Play
Firing does not create a file/database write or a new audio context for each shot.
Original and new parts follow the shared data, assembly and rendering paths.
Ads are neither displayed nor requested.
This test does not establish full campaign verification.
''')
        self.manifest['excerpts'].append('docs/en/public/play.md')
        errors, _, _ = self.inspect()
        self.assertEqual(len(errors), 8)
        self.assertTrue(all('public prose:' in error for error in errors), errors)
        self.assertTrue(any('docs/public/play.md:2:' in error for error in errors), errors)

    def test_allows_observed_behavior_and_protected_text(self):
        self.write('README.md', '''# HeavyMach 2

## HeavyMach 2를 다시 만드는 이유
광고를 표시하거나 요청하지 않는다.

## 프로젝트 소개
효과음은 준비한 OpenAL 소스와 버퍼를 재사용한다.
랭킹을 열면 현재 랭킹 서비스를 이용할 수 없다고 표시한다.

> 광고를 표시하거나 요청하지 않는다.
사용자는 “광고를 표시하거나 요청하지 않는다.”라고 적었다.
원문 규칙의 예시는 `광고를 표시하거나 요청하지 않는다`이다.

```text
광고를 표시하거나 요청하지 않는다.
```
''')
        self.write('docs/runtime-contract.md', '# Runtime\n광고를 표시하거나 요청하지 않는다.\n')
        self.write('docs/RESTORATION.md', '# 개발자의 변\n광고를 표시하거나 요청하지 않는다.\n')
        self.assertEqual(self.inspect()[0], [])


if __name__ == '__main__':
    unittest.main()
