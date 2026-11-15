#!/usr/bin/env python3
"""Independent Phone menu/title/banner receipt check. Never denies a tool."""
import argparse, hashlib, json, struct, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
HOME=ROOT/'.local/evidence/phone-ui-policy-check'
INPUTS=('Sources/Core/UI/Screens.cpp','Sources/Core/UI/Screens.hpp','Sources/Core/Presentation/DisplayLayout.cpp','Sources/Core/Presentation/DisplayLayout.hpp','Sources/Core/Presentation/MenuArtwork.hpp','Sources/Core/Presentation/MenuGridLayout.hpp')
HD_TITLE_SHA='d34b109ac8ccdb73f90a419b60840d350ab7c3d53acad40194f0c8eb3b809a36'
SCOPE=('Screens.cpp','Screens.hpp','DisplayLayout.cpp','DisplayLayout.hpp','MenuArtwork.hpp','MenuGridLayout.hpp','check_phone_ui_policy.py')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(data):
    if data.get('sources')!={p:sha(ROOT/p) for p in INPUTS}: raise ValueError('Stale candidate sources')
    app=ROOT/'build/apple/Build/Products/Debug-iphonesimulator/HeavyMach2Source.app'
    if data.get('binarySHA256')!=sha(app/'HeavyMach2Source') or data.get('gameSHA256')!=sha(app/'HeavyMach2Source.debug.dylib'): raise ValueError('Stale candidate build')
    expected={'hub-Base00'+str(i) for i in range(1,7)}|{'shop','shop-detail','shop-last-page','shop-trade','command','command-detail','command-last-page','title','logo','stage-name'}
    seen=set(); failures=[]
    for row in data['rows']:
        name=row['case']; seen.add(name)
        if name not in expected or list(x['case'] for x in data['rows']).count(name)!=1: raise ValueError('Unexpected/duplicate case')
        path=Path(row['png'])
        if path.is_symlink() or not path.resolve().is_relative_to((ROOT/'.local/evidence').resolve()) or sha(path)!=row['sha256']: raise ValueError('Missing/changed simulator picture')
        width,height=row['canvas']; clip=row.get('clip'); png=path.read_bytes()
        if png[:8]!=b'\x89PNG\r\n\x1a\n' or list(struct.unpack('>II',png[16:24]))!=[width,height]: raise ValueError('Invalid simulator picture extent')
        if row.get('reviewedSHA256')!=row['sha256'] or not row.get('reviewEvidence'): failures.append(name+': picture not reviewed')
        if name.startswith(('hub-','shop','command')):
            if any(row.get(k)!=[0,0] for k in ('scrollBefore','scrollAfterHorizontal','scrollAfterVertical')): failures.append(name+': horizontal/vertical scrolling/unverified')
            if row.get('controlsUnmovedAfterDrag') is not True: failures.append(name+': drag moved controls/page')
            if row.get('originalPhonePolicyMatches') is not True: failures.append(name+': original Phone controls/panels unverified')
            for button in row.get('buttons',[]):
                x,y,w,h=button['bounds']
                if x<-.1 or y<-.1 or x+w>width+.1 or y+h>height+.1: failures.append(name+': clipped button '+str(button['id']))
            if not row.get('buttons'): failures.append(name+': missing button observations')
            if name=='shop-last-page' and not {2223,6024}.issubset({b['id'] for b in row.get('buttons',[])}): failures.append(name+': last stock/inventory cells not captured')
        elif name=='title':
            x,y,w,h=row['artworkBounds']
            if row.get('image')!='Main_BG.png' or sha(app/'HD/Main_BG.png')!=HD_TITLE_SHA or row.get('artworkSHA256')!=HD_TITLE_SHA: failures.append(name+': HD illustration missing')
            if abs(y+h-height)>.1 or x>.1 or y>.1 or x+w<width-.1 or abs(w/h-.75)>.001: failures.append(name+': top aligned uniform cover failed')
        else:
            if clip!=[0,0,width,height]: failures.append(name+': banner clipped to safe area')
            if row.get('entersAtRightEdge') is not True or row.get('exitsAtLeftEdge') is not True: failures.append(name+': entry/exit path unverified')
    if seen!=expected: raise ValueError('Missing cases: '+str(sorted(expected-seen)))
    return {'status':'FAIL' if failures else 'PASS','cases':len(seen),'failures':failures}
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--hook',action='store_true');parser.add_argument('--input',type=Path,default=HOME/'latest-input.json');args=parser.parse_args()
    event=json.load(sys.stdin) if args.hook else {}
    if args.hook:
        payload=json.dumps(event.get('tool_input',{}))
        if not (str(event.get('cwd','')).startswith(str(ROOT)) or str(ROOT) in payload) or not any(x in payload for x in SCOPE): print('{}');return 0
    try: result=verify(json.loads(args.input.read_text())) if args.input.exists() else {'status':'NOT_RUN','reason':'Capture the current simulator menu/title/banner cases and review them'}
    except Exception as error: result={'status':'ERROR','reason':str(error)}
    HOME.mkdir(parents=True,exist_ok=True);prior=sorted(p for p in HOME.glob('*.json') if p.stem.isdigit());old=json.loads(prior[-1].read_text()) if prior else {}
    result['repeatedFailure']=result['status'] in ('FAIL','ERROR') and all(old.get(k)==result.get(k) for k in ('status','failures','reason'))
    if result['repeatedFailure']:result['nextAction']='Correct the first mismatch; stop repeating the same approach. Editing and recovery remain allowed.'
    result.update(time_ns=time.time_ns(),hook_event=event.get('hook_event_name'),tool_id=event.get('tool_use_id'))
    path=HOME/(str(result['time_ns'])+'.json');path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    if args.input.exists():path.with_suffix('.input.json').write_bytes(args.input.read_bytes())
    if args.hook:print(json.dumps({'hookSpecificOutput':{'hookEventName':event['hook_event_name'],'additionalContext':'Phone menu/title/banner only: '+result['status']+'; '+str(path)+'. No tools/files blocked.'}}));return 0
    print(json.dumps(result,ensure_ascii=False));return 0 if result['status']=='PASS' else 1
if __name__=='__main__':
    try: code=main()
    except Exception as error:print(json.dumps({'systemMessage':'Phone UI check ERROR: '+str(error)}));code=0 if '--hook' in sys.argv else 1
    sys.exit(code)
