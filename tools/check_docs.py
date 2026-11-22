#!/usr/bin/env python3
"""Check tracked Markdown links and Korean/English document pairs."""

import argparse
from functools import cache
import html
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import subprocess
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
PAIRS = 'docs/translations/pairs.json'
FENCE = re.compile(r'^ {0,3}(`{3,}|~{3,})(.*)$')
INLINE_CODE = re.compile(r'(?<!`)(`+)(?!`)(.*?)\1(?!`)', re.DOTALL)
DESTINATION = r'(<[^<>\n]+>|(?:\\[^\n]|[^\s()\\]|\([^()\n]*\))+)'
INLINE_LINK = re.compile(r'(?<!\\)\]\(\s*' + DESTINATION)
REFERENCE = re.compile(r'^ {0,3}\[(?!\^)[^\]\n]+\]:[ \t]*' + DESTINATION, re.MULTILINE)
HTML_TAG = re.compile(r'<[A-Za-z][^<>]*>')
HTML_LINK = re.compile(r'''\b(?:href|src)\s*=\s*(["'])(.*?)\1''', re.IGNORECASE)
HTML_ANCHOR = re.compile(r'''\b(?:id|name)\s*=\s*(["'])(.*?)\1''', re.IGNORECASE)
ATX = re.compile(r'^ {0,3}#{1,6}[ \t]+(.*?)(?:[ \t]+#+)?[ \t]*$')
SETEXT = re.compile(r'^ {0,3}(?:=+|-+)[ \t]*$')


def markdown_body(text):
    """Remove fenced snippets and HTML comments before looking for links."""
    text = re.sub(r'<!--.*?-->', lambda match: '\n' * match[0].count('\n'), text, flags=re.DOTALL)
    fence = None
    body = []
    for line in text.splitlines():
        marker = FENCE.match(line)
        if fence:
            if marker and marker[1].startswith(fence) and not marker[2].strip():
                fence = None
            body.append('')
        elif marker:
            fence = marker[1]
            body.append('')
        else:
            body.append(line)
    return '\n'.join(body)


def link_urls(text):
    body = INLINE_CODE.sub(lambda match: '\n' * match[0].count('\n'), markdown_body(text))
    for match in INLINE_LINK.findall(body) + REFERENCE.findall(body):
        yield match.strip('<>')
    for tag in HTML_TAG.findall(body):
        for _, url in HTML_LINK.findall(tag):
            yield url


def heading_slug(title):
    title = re.sub(r'!?\[([^\]]*)\]\([^)]*\)', r'\1', title)
    title = HTML_TAG.sub('', title)
    title = html.unescape(title).lower().strip()
    return re.sub(r'[^\w\s-]', '', title).replace(' ', '-').replace('\t', '-')


def anchors(text):
    body = markdown_body(text)
    result = {html.unescape(value) for tag in HTML_TAG.findall(body)
              for _, value in HTML_ANCHOR.findall(tag)}
    slugs = set()
    previous = ''
    for line in body.splitlines():
        heading = ATX.match(line)
        if heading or (previous and SETEXT.fullmatch(line)):
            base = heading_slug(heading[1] if heading else previous)
            slug, suffix = base, 1
            while slug in slugs:
                slug = f'{base}-{suffix}'
                suffix += 1
            slugs.add(slug)
            previous = ''
        else:
            previous = line.strip()
    return result | slugs


def local_target(source, destination):
    url = urlsplit(html.unescape(destination))
    if url.scheme or url.netloc:
        return None
    path = re.sub(r'\\(.)', r'\1', unquote(url.path))
    if path.startswith('/'):
        target = posixpath.normpath(path.lstrip('/'))
    elif path:
        target = posixpath.normpath(posixpath.join(posixpath.dirname(source), path))
    else:
        target = source
    return target, unquote(url.fragment)


def check_documents(root, files, manifest, changed):
    errors = []
    directories = {str(parent) for file in files for parent in PurePosixPath(file).parents}

    @cache
    def read(name):
        return (root / name).read_text(encoding='utf-8')

    @cache
    def local_links(name):
        links = []
        for url in link_urls(read(name)):
            try:
                destination = local_target(name, url)
            except ValueError as error:
                errors.append(f'{name}: invalid link {url}: {error}')
                continue
            if destination is not None:
                links.append((url, *destination))
        return links

    @cache
    def file_anchors(name):
        return anchors(read(name))

    documents = [name for name in sorted(files)
                 if name.endswith('.md') and not name.startswith('ThirdParty/')]
    link_count = 0
    for name in documents:
        for url, target, fragment in local_links(name):
            link_count += 1
            if target not in files and target not in directories:
                errors.append(f'{name}: missing local target: {url}')
            elif target in files and fragment and target.endswith('.md'):
                if fragment not in file_anchors(target):
                    errors.append(f'{name}: missing heading or anchor: {url}')

    pairs = manifest['pairs']
    excerpts = set(manifest['excerpts'])
    english = {name for name in documents if name.endswith('.en.md') or name.startswith('docs/en/')}
    for name in sorted(english - set(pairs.values()) - excerpts):
        errors.append(f'{name}: English document has no translation pair or excerpt entry')
    for name in sorted(excerpts - english):
        errors.append(f'{name}: listed English excerpt is missing')
    if len(set(pairs.values())) != len(pairs) or excerpts.intersection(pairs.values()):
        errors.append(f'{PAIRS}: an English document must have one source or one excerpt entry')
    for korean, english_doc in pairs.items():
        if korean not in files or english_doc not in files:
            errors.append(f'Missing Korean/English document: {korean} / {english_doc}')
            continue
        for name, other in ((korean, english_doc), (english_doc, korean)):
            if not any(target == other and not fragment for _, target, fragment in local_links(name)):
                errors.append(f'{name}: missing language link to {other}')
        if (korean in changed) != (english_doc in changed):
            edited, other = (korean, english_doc) if korean in changed else (english_doc, korean)
            errors.append(f'{edited}: only one language changed. Check and update {other} in the same change.')
    return errors, len(documents), link_count


def git_paths(*args):
    output = subprocess.check_output(['git', '-C', str(ROOT), *args])
    return {os.fsdecode(value) for value in output.split(b'\0') if value}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', help='Compare changed documents with this Git revision')
    args = parser.parse_args()
    files = git_paths('ls-files', '--cached', '-z')
    changed = set()
    if args.base:
        base = subprocess.check_output(
            ['git', '-C', str(ROOT), 'rev-parse', '--verify', '--end-of-options', f'{args.base}^{{commit}}'],
            text=True,
        ).strip()
        changed = git_paths('diff', '--name-only', '-z', '--no-renames', base, '--')
    manifest = json.loads((ROOT / PAIRS).read_text(encoding='utf-8'))
    errors, doc_count, link_count = check_documents(ROOT, files, manifest, changed)
    if errors:
        parser.exit(1, '\n'.join(errors) + '\n')
    print(f'Checked {doc_count} Markdown files, {link_count} local links, and {len(manifest["pairs"])} translation pairs.')


if __name__ == '__main__':
    main()
