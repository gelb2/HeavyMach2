#!/usr/bin/env python3
"""Append unchanged classic-HUD capture figures to the generated plan page."""
from pathlib import Path
import argparse,gzip,hashlib,html,json,shutil
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();assert out.is_relative_to(ROOT/'.local')
 evidence=json.loads((ROOT/'docs/evidence/experimental-hud-plan-20261114.json').read_text());captures=evidence['captures'];assets=out/'experimental-hud-assets';assets.mkdir(exist_ok=True)
 colors=['#f5df46','#ff8f9c','#bc9cff','#71e7e6','#ffb86b','#8aee88','#f5f5f5','#5fd8ff','#f4a9ff']
 # Approximate visible regions, used only for explanatory outlines. Actual
 # input positions/IDs are in the capture metadata, not derived from boxes.
 def regions(c):
  phone=c['profile']=='phone';w,h=c['width'],c['height'];s=.75 if phone else 1;ox=(w-768*s)/2;oy=0 if phone else 20
  specs=[(1,0,0,300,104),(2,299,56,72,72),(3,444,56,72,72),(4,487,0,281,59),(5,581,74,187,37),(6,369,0,80,80)]
  result=[(i,ox+x*s,h-oy-(y+height)*s,width*s,height*s) for i,x,y,width,height in specs]
  if c.get('frame',0)>1:result.append((7,359 if phone else 384,43 if phone else 59,194 if phone else 256,16))
  if c.get('weaponChoiceType') in [2,3]:
   side=c['weaponChoiceType'];result.append((8 if side==2 else 9,249 if phone and side==2 else 376 if phone else 236 if side==2 else 405,274 if phone else 1147,321 if phone else 427,56 if phone else 72))
  return result
 blocks=['<section id="hud-capture-gallery"><h2>실제 캡처와 조각 구분</h2><p>현재 클래식 HUD입니다. 색 테두리와 번호는 보고서에만 겹쳐 표시한 분석 영역입니다. Experimental 구현 화면이 아닙니다. 번호1~7은 계획 표의 조각,8·9는 좌우 선택 패널입니다. 테두리는 설명용 대략 영역이며 터치 경계 측정값이 아닙니다.</p>']
 order=['phone-classic-default.png','phone-classic-left-weapons.png','phone-classic-right-weapons.png','ipad-classic-default.png','ipad-classic-left-weapons.png','ipad-classic-right-weapons.png','phone-portal-classic-1.png','phone-portal-classic-2.png']
 labels=['iPhone 기본 HUD','iPhone 왼쪽 무기 선택','iPhone 오른쪽 무기 선택 — 검정 영역 관측','iPad 기본 HUD','iPad 왼쪽 무기 선택','iPad 오른쪽 무기 선택','이전 iPhone 포털 검사 캡처1','이전 iPhone 포털 검사 캡처2']
 for name,label in zip(order,labels):
  c=next(c for c in captures if c['file']==name);source=ROOT/c['report_local_copy'];assert hashlib.sha256(source.read_bytes()).hexdigest()==c['sha256'];shutil.copy2(source,assets/name);w,h=c['width'],c['height'];svg=[]
  if c['kind']=='actualClassicConcurrentGPU':
   for i,x,y,bw,bh in regions(c):
    color=colors[i-1];svg.append(f'<rect x="{x:g}" y="{y:g}" width="{bw:g}" height="{bh:g}" fill="none" stroke="{color}" stroke-width="2"/><rect x="{x:g}" y="{max(0,y-18):g}" width="22" height="18" fill="{color}"/><text x="{x+11:g}" y="{max(0,y-18)+13:g}" text-anchor="middle" fill="#111" font-size="12" font-weight="bold">{i}</text>')
  blocks.append(f'<figure class="hud-figure"><h3>{html.escape(label)}</h3><div class="hud-canvas" style="max-width:{w}px"><img loading="lazy" src="experimental-hud-assets/{name}" width="{w}" height="{h}" alt="{html.escape(label)}의 현재 클래식 HUD 실제 GPU 캡처"><svg aria-hidden="true" viewBox="0 0 {w} {h}">{"".join(svg)}</svg></div><figcaption>{w}×{h} 원본 PNG · <a target="_blank" rel="noopener" href="experimental-hud-assets/{name}">원본 전체 크기로 보기</a><br>SHA-256 <code>{c["sha256"]}</code></figcaption></figure>')
 blocks.append('</section>');payloadPath=out/'report-content.bin';payload=json.loads(gzip.decompress(payloadPath.read_bytes()));payload['experimental-hud.html']['html']+='\n'+''.join(blocks);payloadPath.write_bytes(gzip.compress(json.dumps(payload,ensure_ascii=False,separators=(',',':')).encode(),mtime=0))
 css=out/'report.css';s=css.read_text();s+='\n.hud-figure{margin:30px 0;padding:14px;border:1px solid var(--line);background:#e7e8df}.hud-canvas{position:relative;margin:auto;line-height:0}.hud-canvas img{display:block;width:100%;height:auto}.hud-canvas svg{position:absolute;inset:0;width:100%;height:100%;pointer-events:none}.hud-figure figcaption{font-size:13px;line-height:1.6;margin-top:10px;overflow-wrap:anywhere}.hud-figure code{overflow-wrap:anywhere}\n';css.write_text(s)
 sourcesPath=out/'sources.json';sources=json.loads(sourcesPath.read_text());sources['payload_sha256']=hashlib.sha256(payloadPath.read_bytes()).hexdigest();sources['hud_captures']=[{k:c[k] for k in ['file','width','height','sha256','kind']} for c in captures];sources['hud_figures_body_sha256']=hashlib.sha256(payload['experimental-hud.html']['html'].encode()).hexdigest();sourcesPath.write_text(json.dumps(sources,ensure_ascii=False,separators=(',',':'))+'\n');print(json.dumps({'captures':len(captures),'unchangedPNG':True,'experimentalImplemented':False,'payload_sha256':sources['payload_sha256']}))
if __name__=='__main__':main()
