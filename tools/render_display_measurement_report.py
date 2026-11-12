#!/usr/bin/env python3
"""Render recorded HM2 screenshot measurements; never edit source captures."""
import argparse
import hashlib
import html
import json
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    data_path = ROOT / 'docs/evidence/reference-display-measurements-20261110.json'
    data = json.loads(data_path.read_text())
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    frames = data['frames']
    def esc(s): return html.escape(str(s))
    def pair(v): return ' × '.join(f'{x:.1f}' for x in v)
    cards = []
    for key, f in frames.items():
        src = ROOT / '.local/evidence/reference-port' / f['file']
        raw = src.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == f['sha256'], src
        assert list(struct.unpack('>II', raw[16:24])) == f['capture_px'], src
        shutil.copyfile(src, out / f['file'])
        w, h = f['capture_px']
        rects, rows = [], []
        for i, row in enumerate(data['measurements'], 1):
            if row['frame'] != key: continue
            x0,y0,x1,y1 = row['box_xyxy']
            assert 0 <= x0 < x1 <= w and 0 <= y0 < y1 <= h
            size = [x1-x0,y1-y0]
            expected = [round(size[j]*f['guest'][j]/f['viewport'][j+2],2) for j in (0,1)]
            assert row['size_px'] == size and row['guest_size'] == expected
            color = '#ffbe55' if row['key'].startswith('player') else '#62eff5'
            rects.append(f'<g class="bounds"><rect x="{x0}" y="{y0}" width="{x1-x0}" height="{y1-y0}" stroke="{color}"/><text x="{x0+3}" y="{max(22,y0-5)}" fill="{color}">{i}</text><title>{esc(row["label"])}</title></g>')
            rows.append(f'<tr><td>{i}</td><td>{esc(row["label"])}<small>{esc(row["note"])}</small></td><td>{pair(size)}</td><td>{pair(row["guest_size"])}</td><td>{pair(row["host_points"])}</td><td>{pair(row["viewport_percent"])}%</td><td>({x0},{y0})–({x1},{y1})</td></tr>')
        cards.append(f'''<section><h2>{esc(f['label'])}</h2><p>{esc(f['scene'])}</p><p>원본 {w}×{h}px · 게임 내부 {pair(f['guest'])} · 표시 영역 {esc(f['viewport'])}</p>
<div class="actions"><a href="{f['file']}" target="_blank">원본 PNG 열기</a><button onclick="this.closest('section').classList.toggle('hide-bounds')">측정선 켜기/끄기</button><button onclick="this.closest('section').classList.toggle('native')">1:1 픽셀 / 화면 맞춤</button></div>
<div class="image-scroll"><svg viewBox="0 0 {w} {h}" style="--native-width:{w}px" role="img" aria-label="{esc(f['label'])} 측정 영역"><image href="{f['file']}" width="{w}" height="{h}"/>{''.join(rects)}</svg></div>
<div class="table-scroll"><table><thead><tr><th>번호</th><th>대상</th><th>출력 px</th><th>게임 내부 단위</th><th>호스트 논리 크기</th><th>화면 폭·높이 비율</th><th>원본 좌표</th></tr></thead><tbody>{''.join(rows)}</tbody></table></div><details><summary>캡처 SHA-256</summary><code>{f['sha256']}</code></details></section>''')
    intro = '''<header><p class="eyebrow">HEAVYMACH 2 · REFERENCE MEASUREMENTS · 2026.11.10</p><h1>포팅판 메크·적·HUD 크기 측정</h1><p>일반판 iPhone과 HD iPad의 기존 전장 캡처 4장, 측정 영역 29개.</p><p><strong>측정·기록만 수행했습니다.</strong> 새 앱 빌드, 게임 코드·HUD 수정, 실기기 설치는 하지 않았습니다.</p></header>
<section><h2>먼저 확인한 결과</h2><p>HD HUD는 일반판을 통째로 확대한 구성이 아닙니다. 미니맵·메뉴·상태 막대의 위치와 요소별 크기가 다릅니다. 하단 HUD의 최대 높이는 일반판 화면의 약 20%, HD 화면의 약 13%입니다.</p>
<div class="table-scroll"><table><thead><tr><th>측정 대상</th><th>일반판 2.0.1 · iPhone</th><th>HD 1.0.3 · iPad</th></tr></thead><tbody>
<tr><td>게임 내부 화면</td><td>480 × 320 · 가로</td><td>768 × 1024 · 세로</td></tr>
<tr><td>주인공 메크, 전투 표본</td><td>약 28 × 31 내부 단위</td><td>약 46 × 53 내부 단위</td></tr>
<tr><td>메크 화면 점유율</td><td>폭 5.82% / 높이 9.76%</td><td>폭 5.98% / 높이 5.17%</td></tr>
<tr><td>붉은 궤도 적, 세로 방향</td><td>약 23 × 37 내부 단위</td><td>약 35 × 60 내부 단위</td></tr>
<tr><td>하단 HUD 최대 외곽</td><td>480 × 64 내부 단위</td><td>약 768 × 135 내부 단위</td></tr>
<tr><td>하단 HUD의 높이 비율</td><td>20.0%</td><td>13.2%</td></tr></tbody></table></div>
<p>내부 좌표 단위는 실물 길이가 아닙니다. 화면비·카메라·방향·줌이 완전히 일치하지 않아 이 표로 기기 간 고정 확대율이나 정확한 월드 시야 배수를 정할 수 없습니다.</p></section>
<section><h2>측정 방법과 정확도</h2><p>원본 PNG에 측정선을 별도로 겹쳤습니다. 원본 이미지 파일은 수정하지 않았습니다. 번호는 아래 표와 대응합니다. 기본 화면 맞춤은 열람용이며 크기를 비교할 때는 수치표나 1:1 픽셀 보기를 사용하십시오.</p>
<p>일반판 출력은 2736×1260px, 실제 게임 영역은 x=423부터 1890×1260px입니다. HD 출력은 1640×2360px, 실제 게임 영역은 y=87부터 1640×2187px입니다. 검은 여백은 점유율 계산에서 제외했습니다.</p>
<p>메크·적 경계는 그림자·HP·발사광을 제외한 수동 측정입니다. 경계마다 보통 ±4 출력px, 폭·높이 환산 오차는 일반판 약 ±2, HD 약 ±4 내부 단위입니다. HUD 외곽에는 빈 공간이 포함되며 실제 불투명 가림 면적이 아닙니다. 막대는 캡처 당시 표시량입니다.</p>
<p>좌표 환산은 기존 로그의 게임 화면 크기와 실행기 viewport 공식, PNG 표시 경계를 대조했습니다. 호스트 논리 크기는 일반판 출력px÷3, HD 출력px÷2입니다. 게임 내부 단위와 호스트 논리 크기를 혼용하지 않습니다.</p></section>'''
    limits = '<section><h2>아직 측정하지 못한 항목</h2><ul>' + ''.join('<li>'+esc(s)+'</li>' for s in data['limits']) + '</ul><p>파츠별 치수·애니메이션 최소/최대·동일 줌 기준과 실제 터치 판정은 남아 있습니다. 초기 장비와 적 종류의 내부 ID도 확정하지 않았습니다. 원작 출시 당시 기기에서 재현한 결과는 아닙니다.</p><p>이번 수치는 첫 전장의 표본입니다. 복원판 표시 배율 확정과 구현은 진행하지 않았습니다.</p><a href="measurements.json">전체 측정 데이터 JSON</a></section>'
    css = '''*{box-sizing:border-box}body{margin:0;background:#10151d;color:#e4ecf4;font:17px/1.7 system-ui,sans-serif}main{max-width:1320px;margin:auto;padding:32px 24px}header,section{padding:28px;margin-bottom:24px;background:#18212d;border:1px solid #344255;border-radius:12px}h1{font-size:clamp(28px,4vw,46px);line-height:1.25}h2{font-size:24px}p{max-width:100ch}.eyebrow{font-size:13px;letter-spacing:.08em;color:#7bd8de}a{color:#8cebf4}button{background:#253548;border:1px solid #6c879c;border-radius:6px;color:white;padding:10px 14px;font:inherit;cursor:pointer}.actions{display:flex;gap:16px;align-items:center;flex-wrap:wrap;margin:16px 0}.image-scroll{overflow:auto;background:black;max-height:900px}svg{display:block;width:100%;height:auto}.native svg{width:var(--native-width);max-width:none}.bounds rect{fill:none;stroke-width:2;vector-effect:non-scaling-stroke}.bounds text{font:24px monospace;paint-order:stroke;stroke:#000;stroke-width:5px}.hide-bounds .bounds{display:none}.table-scroll{overflow:auto}table{width:100%;border-collapse:collapse;margin:18px 0;font-size:15px}td,th{padding:12px;border-bottom:1px solid #344255;text-align:left;vertical-align:top;white-space:nowrap}td:nth-child(2){white-space:normal;min-width:180px}small{display:block;color:#b0becd;font-size:13px;line-height:1.5}code{word-break:break-all;font-size:12px}li{margin:8px 0}details{font-size:14px}@media(max-width:600px){main{padding:12px}header,section{padding:16px}h2{font-size:21px}}'''
    page = '<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>HeavyMach 2 포팅판 표시 크기 측정</title><style>'+css+'</style><main>'+intro+''.join(cards)+limits+'</main></html>'
    (out/'index.html').write_text(page)
    shutil.copyfile(data_path,out/'measurements.json')
    print(json.dumps({'frames':len(frames),'measurements':len(data['measurements']),'output':str(out)},ensure_ascii=False))
if __name__ == '__main__': main()
