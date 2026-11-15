#!/usr/bin/env python3
"""Install scoped definitions without granting or bypassing native hook trust."""
import argparse
from datetime import datetime,timezone
import json
from pathlib import Path
import shlex
import shutil

ROOT=Path(__file__).resolve().parents[2]
EVENTS=['SessionStart','UserPromptSubmit','PreToolUse','PostToolUse','Stop']

def install(path,source,retire=False):
    if path.is_symlink():raise ValueError('Refusing symbolic hook configuration')
    path.parent.mkdir(parents=True,exist_ok=True)
    old=path.read_bytes() if path.exists() else None
    value=json.loads(old) if old else {'hooks':{}}
    command='python3 '+shlex.quote(str(ROOT/'.agents/harness/presentation.py'))+' --source '+source+' hook'
    for event in EVENTS:
        groups=value.setdefault('hooks',{}).setdefault(event,[])
        kept=[]
        for group in groups:
            handlers=[h for h in group.get('hooks',[]) if h.get('command')!=command]
            if handlers:kept.append({**group,'hooks':handlers})
        groups[:]=kept
        if retire:
            if not groups:value['hooks'].pop(event,None)
            continue
        handler={'type':'command','command':command,'timeout':20,'statusMessage':'HeavyMach2 local presentation policy'}
        if event!='Stop':handler['additionalContextLimit']=14000
        groups.append({'hooks':[handler]})
    if old:
        backup=ROOT/'.local/evidence/presentation-harness-implementation-20261115/hook-config-backups';backup.mkdir(parents=True,exist_ok=True)
        (backup/(source+'-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')+'.json')).write_bytes(old)
    tmp=path.with_suffix('.hm2.tmp');tmp.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n');tmp.replace(path)
    return {'path':str(path),'events':[] if retire else EVENTS,'source':source,'trustGranted':False,'retired':retire}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--install',action='store_true');args=parser.parse_args()
    targets=[(ROOT/'.codex/hooks.json','project'),(Path.home()/'.codex/hooks.json','user')]
    if not args.install:print(json.dumps({'planned':[str(p) for p,_ in targets],'nativeTrustBypassed':False}));raise SystemExit(0)
    print(json.dumps({'installed':[install(p,s,retire=s=='project') for p,s in targets],'nativeTrustRequired':True,'gameSourceChanged':False}))
