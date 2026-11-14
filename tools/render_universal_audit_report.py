#!/usr/bin/env python3
"""Render HM2 audit documents as local HTML; never read or copy original assets."""
import argparse
import gzip
import hashlib
import html
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import quote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
PAGES = {
    'docs/experimental-hud-plan-20261114.md': 'experimental-hud.html',
    'docs/lower-camera-simulator-20261114.md': 'lower-camera.html',
    'docs/world-hud-comparison-20261114.md': 'world-hud.html',
    'docs/iphone-surface-art-fix-20261114.md': 'surface-art.html',
    'docs/common-camera-plan-20261114.md': 'camera-plan.html',
    'docs/camera-baseline-investigation-20261114.md': 'camera-baseline.html',
    'docs/iphone-fixes-implementation-20261114.md': 'fixes.html',
    'docs/iphone-fixes-and-profiling-plan-20261114.md': 'fixes-plan.html',
    'docs/iphone-apple-runtime-audit-20261114.md': 'apple-audit.html',
    'docs/original-resource-loading-audit-20261114.md': 'resource-loading.html',
    'docs/iphone-path-jump-investigation-20261114.md': 'path-jump.html',
    'docs/iphone-background-asset-research-20261114.md': 'background-art.html',
    'docs/iphone-implementation-20261113.md': 'implementation.html',
    'docs/universal-design-audit-results-20261113.md': 'index.html',
    'docs/universal-design-audit-plan-20261113.md': 'plan.html',
    'docs/universal-package-differences-20261113.md': 'packages.html',
    'docs/standard-hd-logic-differences-20261113.md': 'logic.html',
    'docs/iphone-followup-investigation-20261113.md': 'followup.html',
    'docs/iphone-development-contract.md': 'development.html',
    'HANDOVER-IPHONE-UNIVERSAL.md': 'handover.html',
    'docs/iphone-universal-native-followup-20261113.md': 'native-followup.html',
    'docs/iphone-universal-runtime-followup-20261113.md': 'runtime-followup.html',
    'docs/iphone-universal-ui-followup-20261113.md': 'ui-followup.html',
}
LABELS = ['ExperimentalHUD 계획', '하단 카메라 시뮬레이터', '월드·HUD·포털 비교', '화면 늘림·배경·HUD 수정', '공통 줌 수정 계획', '기본 줌 조사', '11/14 수정·검사', '수정·측정 계획', 'Apple 자원·음향', '원본 에셋 수명', '점프 조사', '배경·버튼 조사',
          'iPhone 구현·검사', '초기 조사', '조사 범위', '패키지 비교', '초기 로직 비교', '추가 조사',
          '개발 계획·금지사항', '작업 인계', '카메라·전투', '시간·저장·음향', 'UI·입력']
CSS = '''
:root{--paper:#f3f1eb;--ink:#222923;--muted:#687069;--line:#d5d9ce;--accent:#ad471d}
*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:17px/1.75 system-ui,sans-serif}
main{max-width:1160px;margin:auto;padding:28px 24px 64px}nav{display:flex;gap:10px 24px;flex-wrap:wrap;border-bottom:1px solid var(--line);padding:16px 0}
a{color:var(--accent);overflow-wrap:anywhere}h1{font-size:clamp(28px,4vw,42px);line-height:1.3;margin:32px 0 24px}h2{margin:38px 0 14px;font-size:25px}h3{font-size:20px}
p,li{overflow-wrap:anywhere}code{font-size:.88em;background:#e7e8df;padding:2px 5px;border-radius:3px}pre{overflow:auto;background:#e7e8df;padding:18px}pre code{padding:0;background:none}
.table-scroll{overflow:auto;margin:20px 0;border:1px solid var(--line)}table{border-collapse:collapse;width:100%;font-size:15px}th,td{padding:12px 14px;vertical-align:top;text-align:left;border:1px solid var(--line);min-width:140px}th{background:#e9ece1}
.meta{font-size:14px;color:var(--muted)}footer{border-top:1px solid var(--line);margin-top:36px;padding-top:16px}.scope{padding:16px 20px;border-left:4px solid var(--accent);background:#e9ece1}
@media(max-width:600px){main{padding:16px 14px 40px}body{font-size:16px}nav{gap:8px 18px}th,td{padding:9px;min-width:120px}}
'''
SCRIPT = '''
(async()=>{
 const status=document.getElementById('load-status');
 document.getElementById('report-navigation').innerHTML=__REPORT_NAVIGATION__;
 document.getElementById('report-footer-note').textContent='화면·데이터·함수 비교 결과와 미확정 범위는 본문에 구분했습니다. 원본 IPA·실행파일·전체 디컴파일·사용자 저장은 이 사이트에 포함하지 않습니다.';
 try{
  const response=await fetch('report-content.bin');
  if(!response.ok)throw new Error('HTTP '+response.status);
  const bytes=new Uint8Array(await response.arrayBuffer());
  let text;
  if(bytes[0]===31&&bytes[1]===139){
   if(typeof DecompressionStream!=='function')throw new Error('decoder unavailable');
   text=await new Response(new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'))).text();
  }else text=new TextDecoder().decode(bytes);
  const item=JSON.parse(text)[document.body.dataset.page];
  if(!item)throw new Error('page missing');
  document.getElementById('report-content').innerHTML=item.html;
  status.remove();
  document.documentElement.dataset.reportLoaded='true';
  if(location.hash){const id=decodeURIComponent(location.hash.slice(1));document.getElementById(id)?.scrollIntoView();}
 }catch(error){status.textContent='문서를 불러오지 못했습니다. 아래 GitHub 원고 링크에서 확인할 수 있습니다.';}
})();
'''


def render(text, source, ref):
    def href(raw):
        parsed = urlsplit(raw)
        if parsed.scheme:
            return raw if parsed.scheme in {'https', 'http'} else '#'
        if raw.startswith('#'):
            return raw
        resolved = (source.parent / parsed.path).resolve()
        try:
            relative = str(resolved.relative_to(ROOT))
        except ValueError:
            return '#'
        if relative == 'docs/evidence/iphone-implementation-20261113.json':
            return 'implementation-results.json'
        if relative in PAGES:
            return PAGES[relative] + ('#' + parsed.fragment if parsed.fragment else '')
        return ('https://github.com/gelb2/HeavyMach2/blob/' + quote(ref, safe='') + '/' +
                quote(relative, safe='/') + ('#' + parsed.fragment if parsed.fragment else ''))

    def inline(raw):
        pieces = re.split(r'(`[^`]+`|\[[^\]]+\]\([^\s)]+\))', raw)
        out = []
        for piece in pieces:
            if piece.startswith('`') and piece.endswith('`'):
                out.append('<code>' + html.escape(piece[1:-1]) + '</code>')
            elif (link := re.fullmatch(r'\[([^\]]+)\]\(([^\s)]+)\)', piece)):
                out.append('<a href="' + html.escape(href(link[2]), quote=True) + '">' +
                           html.escape(link[1]) + '</a>')
            else:
                escaped = html.escape(piece)
                out.append(re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', escaped))
        return ''.join(out)

    lines = text.splitlines()
    result, paragraph, listing = [], [], None
    def flush():
        if paragraph:
            result.append('<p>' + inline(' '.join(paragraph)) + '</p>')
            paragraph.clear()
    def close_list():
        nonlocal listing
        if listing:
            result.append('</' + listing + '>')
            listing = None
    i = 0
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            flush(); close_list(); i += 1; continue
        if line.startswith('```'):
            flush(); close_list(); i += 1; code = []
            while i < len(lines) and not lines[i].startswith('```'):
                code.append(lines[i]); i += 1
            result.append('<pre><code>' + html.escape('\n'.join(code)) + '</code></pre>')
            i += 1; continue
        if (heading := re.match(r'^(#{1,6})\s+(.+)$', line)):
            flush(); close_list(); level = len(heading[1]); label = heading[2]
            anchor = ''.join(c for c in label.lower() if c.isalnum() or c in ' -_').replace(' ', '-')
            result.append(f'<h{level} id="{html.escape(anchor)}">{inline(label)}</h{level}>')
            i += 1; continue
        if line.startswith('|') and i + 1 < len(lines) and re.fullmatch(r'[| :\-]+', lines[i + 1]):
            flush(); close_list()
            cells = lambda row: [x.strip() for x in row.strip().strip('|').split('|')]
            result.append('<div class="table-scroll"><table><thead><tr>' + ''.join(
                '<th>' + inline(cell) + '</th>' for cell in cells(line)) + '</tr></thead><tbody>')
            i += 2
            while i < len(lines) and lines[i].startswith('|'):
                result.append('<tr>' + ''.join('<td>' + inline(cell) + '</td>' for cell in cells(lines[i])) + '</tr>')
                i += 1
            result.append('</tbody></table></div>'); continue
        if (item := re.match(r'^(?:([-*]) |(\d+)\. )(.*)$', line)):
            flush(); tag = 'ul' if item[1] else 'ol'
            if listing != tag:
                close_list(); result.append('<' + tag + '>'); listing = tag
            result.append('<li>' + inline(item[3]) + '</li>')
            i += 1; continue
        close_list(); paragraph.append(line); i += 1
    flush(); close_list()
    return '\n'.join(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source-ref', default=None)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / '.local'):
        parser.error('Report output must remain in this repository .local space')
    ref = args.source_ref or subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    output.mkdir(parents=True, exist_ok=True)
    nav = '<nav aria-label="조사 문서">' + ''.join('<a href="' + page + '">' + label + '</a>'
          for page, label in zip(PAGES.values(), LABELS)) + '</nav>'
    records, payload = [], {}
    for source_name, page in PAGES.items():
        source = ROOT / source_name
        text = source.read_text()
        title = re.search(r'^# (.+)$', text, re.M)[1]
        body = render(text, source, ref)
        payload[page] = {'html': body}
        source_url = 'https://github.com/gelb2/HeavyMach2/blob/' + quote(ref, safe='') + '/' + source_name
        navigation = ('<details><summary>관련 조사 문서</summary><div id="report-navigation"></div></details>'
                      if page in {'lower-camera.html', 'world-hud.html', 'surface-art.html', 'camera-baseline.html', 'camera-plan.html'} else '<div id="report-navigation"></div>')
        full = ('<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
                '<title>' + html.escape(title) + '</title><link rel="stylesheet" href="report.css"><script src="report.js" defer></script></head><body data-page="' + page + '"><main>'
                '<a href="../heavymach2-restoration-20261110/#report-log">HeavyMach2 누적 보고 목록</a>' + navigation +
                '<p class="meta" id="load-status">문서를 불러오는 중입니다.</p><div id="report-content"></div>' +
                '<noscript><p>문서 표시에는 JavaScript가 필요합니다. 아래 GitHub 원고에서도 읽을 수 있습니다.</p></noscript>' +
                '<footer><p id="report-footer-note"></p>'
                '<p><a href="' + source_url + '">GitHub 원고</a></p></footer></main></body></html>')
        (output / page).write_text(full)
        records.append({'source': source_name, 'page': page, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                        'html_sha256': hashlib.sha256(full.encode()).hexdigest(),
                        'rendered_body_sha256': hashlib.sha256(body.encode()).hexdigest(), 'bytes': len(full.encode())})
    packed = gzip.compress(json.dumps(payload, ensure_ascii=False, separators=(',', ':')).encode(), mtime=0)
    (output / 'report-content.bin').write_bytes(packed)
    (output / 'report.css').write_text(CSS)
    (output / 'report.js').write_text(SCRIPT.replace('__REPORT_NAVIGATION__', json.dumps(nav, ensure_ascii=False)))
    evidence = json.loads((ROOT / 'docs/evidence/iphone-implementation-20261113.json').read_text())
    (output / 'implementation-results.json').write_text(json.dumps(evidence, ensure_ascii=False, separators=(',', ':')) + '\n')
    (output / 'sources.json').write_text(json.dumps({'source_ref': ref, 'pages': records,
                                                   'payload_sha256': hashlib.sha256(packed).hexdigest()}, ensure_ascii=False, separators=(',', ':')) + '\n')
    print(json.dumps({'output': str(output), 'pages': records}, ensure_ascii=False))


if __name__ == '__main__':
    main()
