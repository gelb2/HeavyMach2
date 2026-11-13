#!/usr/bin/env python3
"""Compact the report list without changing its HTML body or existing URLs."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = '''
(async()=>{
 const status=document.getElementById('report-index-status');
 try{
  const response=await fetch('report-index-content.bin');
  if(!response.ok)throw new Error('HTTP '+response.status);
  const bytes=new Uint8Array(await response.arrayBuffer());
  const text=bytes[0]===31&&bytes[1]===139
   ?await new Response(new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'))).text()
   :new TextDecoder().decode(bytes);
  document.body.innerHTML=text;
  document.documentElement.dataset.reportIndexLoaded='true';
  if(location.hash)document.getElementById(decodeURIComponent(location.hash.slice(1)))?.scrollIntoView();
 }catch(error){status.textContent='개발 보고 목록을 불러오지 못했습니다. 보고서의 직접 주소로 접속해 주세요.';}
})();
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    args = parser.parse_args()
    directory = args.directory.resolve()
    if not directory.is_relative_to(ROOT / '.local'):
        parser.error('Only this repository local Site checkout is allowed')
    index = directory / 'index.html'
    original = index.read_text()
    if 'src="report-index-loader.js"' in original:
        raise ValueError('Index already compacted; preserve and update its body payload explicitly')
    match = re.fullmatch(r'([\s\S]*?<body\b[^>]*>)([\s\S]*?)(</body>[\s\S]*)', original, re.I)
    if not match or re.search(r'<script\b', match[2], re.I):
        raise ValueError('Expected static body without scripts')
    packed = gzip.compress(match[2].encode(), mtime=0)
    assert gzip.decompress(packed).decode() == match[2]
    (directory / 'report-index-content.bin').write_bytes(packed)
    (directory / 'report-index-loader.js').write_text(SCRIPT)
    prefix = match[1].replace('</head>', '<script src="report-index-loader.js" defer></script></head>', 1)
    replacement = (prefix + '<p id="report-index-status">개발 보고 목록을 불러오는 중입니다.</p>'
                   '<noscript><p>목록 표시에는 JavaScript가 필요합니다. 각 보고서의 직접 주소로 접속할 수 있습니다.</p></noscript>' + match[3])
    index.write_text(replacement)
    record = {'original_index_sha256': hashlib.sha256(original.encode()).hexdigest(),
              'original_body_sha256': hashlib.sha256(match[2].encode()).hexdigest(),
              'packed_body_sha256': hashlib.sha256(packed).hexdigest(),
              'decoded_body_identical': True, 'existing_body_scripts': 0,
              'original_index_bytes': len(original.encode()),
              'replacement_index_bytes': len(replacement.encode()),
              'packed_body_bytes': len(packed), 'existing_urls_changed': False}
    (directory / 'report-index-source.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps(record))


if __name__ == '__main__':
    main()
