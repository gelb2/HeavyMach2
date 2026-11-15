#!/usr/bin/env python3
"""Local lifecycle records and evidence gates; never infer visual PASS from logs.

The same OS user can change these files. This is a workflow guard, not a
tamper-proof security boundary. Native lifecycle activation is tested separately.
"""
from __future__ import annotations
import argparse
import contextlib
from datetime import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
POLICY = ('docs/design-and-asset-rules.md', 'docs/presentation-harness-plan-20261115.md')
PRODUCT = ('Sources/', 'Apple/', 'tests/', 'tools/', 'CMakeLists.txt')
INFRA = ('.agents/harness/', '.codex/', '.githooks/', 'tests/test_presentation_harness.py')
READ_COMMANDS = {'pwd','rg','cat','head','tail','ls','stat','du','df','ps','which','uname','sw_vers','wc'}
UNIT_CHECK = ['python3','-m','unittest','discover','-s','tests','-p','test_presentation_harness.py','-v']
NATIVE_SESSIONS = Path.home()/'.codex/sessions'

class Rejected(Exception):
    pass

def digest(value):
    data=value if isinstance(value,bytes) else json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()
    return hashlib.sha256(data).hexdigest()

def read_json(path):
    return json.loads(Path(path).read_text())

def git(root,*args):
    return subprocess.check_output(['git','-C',str(root),*args],stderr=subprocess.PIPE)

def inside(root,path):
    return Path(path).resolve().is_relative_to(root.resolve())

def protected(name):
    return name in POLICY or name in {'AGENTS.md','docs/source-provenance.json','docs/evidence/hub-artwork-census-20261115.json'} or name.casefold().startswith(tuple(x.casefold() for x in PRODUCT+INFRA))

def infrastructure(name):
    return name in POLICY or name=='AGENTS.md' or name.startswith(INFRA)

def content(path):
    if path.is_symlink():
        return {'kind':'symlink','sha':digest(os.readlink(path).encode())}
    if path.is_file():
        return {'kind':'file','sha':digest(path.read_bytes()),'mode':path.stat().st_mode&0o777}
    return None

def snapshot(root):
    names=git(root,'ls-files','-z','--cached','--others','--exclude-standard').decode().split('\0')
    result={}
    for name in names:
        if not name or not protected(name):continue
        p=root/name
        if p.is_symlink() and not inside(root,p):raise Rejected('External symlink in protected input: '+name)
        if (c:=content(p)) is not None:result[name]=c
    return result

def delta(before,after):
    return sorted(k for k in before.keys()|after.keys() if before.get(k)!=after.get(k))

def asset_inputs(root):
    base=root/'.local/prepared/HD'
    if not base.exists():return {'status':'NOT_PREPARED'}
    result={}
    for p in sorted(base.rglob('*')):
        if p.is_symlink():raise Rejected('Linked prepared asset: '+str(p))
        if p.is_file():result[p.relative_to(base).as_posix()]=digest(p.read_bytes())
    return result

def candidate(root,assets=True):
    return digest({'files':snapshot(root),'assets':asset_inputs(root) if assets else {},'policy':{p:content(root/p) for p in POLICY}})

def require_file(path,root=None):
    path=Path(path)
    if root is not None and not inside(root,path):raise Rejected('Evidence outside approved root')
    if path.is_symlink() or not path.is_file():raise Rejected('Missing/linked evidence: '+str(path))
    return path

class Harness:
    def __init__(self,root=ROOT):
        self.root=Path(root).resolve()
        self.home=self.root/'.local/presentation-harness'
        if self.home.is_symlink():raise Rejected('Harness storage must not be a symlink')
        self.home.mkdir(parents=True,exist_ok=True)

    @contextlib.contextmanager
    def locked(self):
        with (self.home/'lock').open('a') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX)
            try:yield
            finally:fcntl.flock(lock,fcntl.LOCK_UN)

    def save(self,path,value):
        if path.is_symlink():raise Rejected('Linked record')
        tmp=path.with_suffix('.tmp')
        tmp.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
        tmp.replace(path)

    def session_path(self,session):
        return self.home/('session-'+digest(str(session).encode())+'.json')

    def state(self,session):
        p=self.session_path(session)
        return read_json(p) if p.exists() else None

    def policy(self):
        return {p:digest(require_file(self.root/p).read_bytes()) for p in POLICY}

    def context(self):
        return 'SOURCE '+POLICY[0]+'\n'+require_file(self.root/POLICY[0]).read_text()+'\nExecution contract: '+POLICY[1]+'\n'+json.dumps(self.policy())

    def append(self,event):
        path=self.home/'events.jsonl';previous='0'*64;seq=0
        if path.exists():
            with path.open() as stream:
                for line in stream:
                    row=json.loads(line);claimed=row.pop('hash')
                    if row['previous']!=previous or row['sequence']!=seq+1 or digest(row)!=claimed:raise Rejected('Broken event chain')
                    previous=claimed;seq+=1
        row={'sequence':seq+1,'previous':previous,'time_ns':time.time_ns(),**event}
        row['hash']=digest(row)
        with path.open('a') as stream:stream.write(json.dumps(row,ensure_ascii=False,sort_keys=True)+'\n');stream.flush();os.fsync(stream.fileno())
        return row

    def events(self):
        path=self.home/'events.jsonl';rows=[];previous='0'*64
        if not path.exists():return rows
        for line in path.read_text().splitlines():
            row=json.loads(line);copy=dict(row);claimed=copy.pop('hash')
            if copy['previous']!=previous or copy['sequence']!=len(rows)+1 or digest(copy)!=claimed:raise Rejected('Broken event chain')
            rows.append(row);previous=claimed
        return rows

    def begin(self,session,turn,prompt):
        files=snapshot(self.root)
        start=self.append({'event':'start','session':session,'turn':turn,'head':git(self.root,'rev-parse','HEAD').decode().strip(),'policy':self.policy(),'files':files,'promptHash':digest(prompt.encode())})
        state={'session':session,'turn':turn,'start':start['hash'],'policy':self.policy(),'files':files,'pending':{},'unattested':[], 'requests':[{'turn':turn,'original':prompt}],'requirements':rule_text(self.root)}
        self.save(self.session_path(session),state)
        return state

    def hook(self,event):
        name=event.get('hook_event_name','');session=event.get('session_id','');turn=event.get('turn_id','')
        if not session:raise Rejected('Missing native session identity')
        with self.locked():
            self.append({'event':'native','name':name,'session':session,'turn':turn,'toolId':event.get('tool_use_id'),'tool':event.get('tool_name'), 'pid':os.getpid(),'parentPid':os.getppid()})
            state=self.state(session)
            if name=='SessionStart':
                return {'hookSpecificOutput':{'hookEventName':name,'additionalContext':'HM2 presentation harness is loaded. Related writes require a recorded requirement context; product completion requires simulator and actual visual review.'}}
            if name=='UserPromptSubmit':
                # Preserve unresolved traces across turns instead of quietly rebaselining.
                if state and (state['pending'] or state['unattested']):
                    prompt=event.get('prompt','')
                    state['turn']=turn;state['requests'].append({'turn':turn,'original':prompt});state['requirements']=rule_text(self.root)
                    self.append({'event':'requirements-while-unresolved','session':session,'turn':turn,'promptHash':digest(prompt.encode()),'policy':self.policy()})
                    self.save(self.session_path(session),state)
                    return {'hookSpecificOutput':{'hookEventName':name,'additionalContext':'HM2 unresolved execution records exist. Repair their evidence; do not call this work complete.\n'+self.context()}}
                if state:
                    now=snapshot(self.root)
                    if delta(state['files'],now):state['unattested']=delta(state['files'],now);self.save(self.session_path(session),state)
                    self.append({'event':'requirements','session':session,'turn':turn,'promptHash':digest(event.get('prompt','').encode()),'policy':self.policy()})
                    state['turn']=turn;state['policy']=self.policy();state['requirements']=rule_text(self.root);state['requests'].append({'turn':turn,'original':event.get('prompt','')});self.save(self.session_path(session),state)
                else:self.begin(session,turn,event.get('prompt',''))
                return {'hookSpecificOutput':{'hookEventName':name,'additionalContext':self.context()}}
            if name=='PreToolUse':
                try:action,paths=classify(self.root,event)
                except Rejected as error:return deny(str(error))
                if action=='read':return {}
                if action=='recovery':return {} # recover_failed independently checks the finished native call and unchanged files.
                if action=='unknown':return deny('Unclassified HM2 mutation. Use apply_patch or the bounded harness runner.')
                if '.agents/harness/probes/blocked.txt' in paths:return deny('HM2_NATIVE_PROBE_DENIED: owned sentinel must remain unchanged')
                if not state:
                    self.begin(session,turn,'Late entry: requirements must be delivered before first mutation')
                    return deny('HM2 first mutation held while rules are delivered.\n'+self.context())
                if state['policy']!=self.policy():return deny('Rules/harness policy changed; obtain a current request context before mutation.')
                now=snapshot(self.root);unseen=delta(state['files'],now)
                if unseen or state['unattested']:
                    state['unattested']=sorted(set(state['unattested']+unseen));self.save(self.session_path(session),state)
                    return deny('UNATTESTED changes: '+', '.join(state['unattested']))
                tool_id=event.get('tool_use_id')
                previous=state['pending'].get(tool_id)
                if previous and previous['inputHash']==digest(event.get('tool_input')) and previous['before']==now:return {}
                if state['pending']:return deny('A previous protected tool is unfinished. If it failed before mutation, use recover-failed with its native transcript and call ID.')
                if action=='commit':
                    try:self.gate(session)
                    except Rejected as error:return deny(str(error))
                if not tool_id:return deny('Missing tool-call identity')
                state['pending'][tool_id]={'before':now,'paths':paths,'action':action,'inputHash':digest(event.get('tool_input'))}
                record=self.append({'event':'pre','session':session,'turn':turn,'toolId':tool_id,'paths':paths,'before':now,'inputHash':digest(event.get('tool_input'))})
                state['pending'][tool_id]['preTimeNs']=record['time_ns']
                self.save(self.session_path(session),state);return {}
            if name=='PostToolUse':
                if event.get('tool_name')=='view_image':
                    data=event.get('tool_input') or {};path=data.get('path')
                    response=event.get('tool_response')
                    if path and inside(self.root,path) and Path(path).is_file() and not (isinstance(response,dict) and (response.get('isError') or response.get('error'))):
                        self.append({'event':'image-view','session':session,'turn':turn,'toolId':event.get('tool_use_id'),'path':str(Path(path).resolve().relative_to(self.root)),'imageSha':digest(Path(path).read_bytes())})
                tool_id=event.get('tool_use_id');pending=state and state['pending'].pop(tool_id,None)
                if not pending:return {}
                now=snapshot(self.root);changed=delta(pending['before'],now)
                unexpected=[p for p in changed if not any(p==x or p.startswith(x.rstrip('/')+'/') for x in pending['paths'])]
                if unexpected:state['unattested']=sorted(set(state['unattested']+unexpected))
                self.append({'event':'post','session':session,'turn':turn,'toolId':tool_id,'changed':changed,'after':now,'unexpected':unexpected,'responseHash':digest(event.get('tool_response'))})
                refreshed=bool(set(changed)&set(POLICY)) and not unexpected
                if refreshed:
                    state['policy']=self.policy();state['requirements']=rule_text(self.root)
                    self.append({'event':'policy-refresh','session':session,'turn':turn,'toolId':tool_id,'policy':state['policy']})
                state['files']=now;self.save(self.session_path(session),state)
                if refreshed:return {'hookSpecificOutput':{'hookEventName':'PostToolUse','additionalContext':self.context()}}
                return {'systemMessage':'HM2 UNATTESTED: '+', '.join(unexpected)} if unexpected else {}
            if name=='Stop':
                if state:self.append({'event':'stop','session':session,'turn':turn,'pending':len(state['pending']),'unattested':state['unattested']})
                return {} # Never suppress factual status or user interruption.
            return {}

    def recover_failed(self,session,tool_id,transcript,call_id):
        """Close one failed no-mutation patch; never invent a native PostToolUse."""
        transcript=require_file(transcript,NATIVE_SESSIONS)
        with self.locked():
            state=self.state(session)
            if not state or tool_id not in state['pending']:raise Rejected('No matching pending tool')
            pending=state['pending'][tool_id];now=snapshot(self.root)
            if state['unattested'] or now!=state['files'] or now!=pending['before']:raise Rejected('Protected files changed; no-mutation recovery is forbidden')
            if pending['action']!='write':raise Rejected('Recovery is limited to failed patches')
            identity=False;invocation=None;response=None;failure_ordinal=None;finished=None
            with transcript.open() as stream:
                for ordinal,line in enumerate(stream):
                    row=json.loads(line);payload=row.get('payload',{})
                    if row.get('type')=='session_meta' and payload.get('id')==session:identity=True
                    if row.get('type')!='response_item' or payload.get('call_id')!=call_id:continue
                    if payload.get('type')=='custom_tool_call':invocation=payload
                    elif payload.get('type')=='custom_tool_call_output' and invocation:
                        response=payload;failure_ordinal=ordinal;finished=row.get('timestamp');break
            if not identity or not invocation or not response:raise Rejected('Missing same-session completed native call')
            if not finished or not pending.get('preTimeNs'):raise Rejected('Missing native failure/current Pre timestamp')
            if int(datetime.fromisoformat(finished.replace('Z','+00:00')).timestamp()*1_000_000_000)<pending['preTimeNs']:raise Rejected('Native failure predates the pending invocation')
            raw=invocation.get('input','')
            if invocation.get('name')=='apply_patch':patches=[raw]
            elif invocation.get('name')=='exec':
                patches=[json.loads(m[1]) for m in re.finditer(r'tools\.apply_patch\(("(?:\\.|[^"\\])*")\)',raw)]
            else:patches=[]
            if sum(pending['inputHash'] in {digest(p),digest({'command':p})} for p in patches)!=1:raise Rejected('Native failed patch input does not match pending input')
            output=response.get('output',[])
            text='\n'.join(x.get('text','') for x in output if isinstance(x,dict)) if isinstance(output,list) else str(output)
            if 'apply_patch verification failed:' not in text:raise Rejected('Native result does not prove patch validation failure')
            proof={'callId':call_id,'ordinal':failure_ordinal,'invocationHash':digest(invocation),'responseHash':digest(response),'transcript':str(transcript),'result':'FAILED_NO_MUTATION'}
            backup=self.home/('failed-tool-state-'+str(time.time_ns())+'.json');self.save(backup,state)
            self.append({'event':'maintenance-recovery','session':session,'toolId':tool_id,'inputHash':pending['inputHash'],'stateBackup':backup.name,'proof':proof,'snapshotHash':digest(now),'productAcceptance':False})
            del state['pending'][tool_id];self.save(self.session_path(session),state)
            return {'status':'RECOVERED_FAILED_NO_MUTATION','toolId':tool_id,'productAcceptance':False}

    def run(self,session,kind,argv):
        if kind not in {'harness','product'}:raise Rejected('Unknown check kind')
        if not argv:raise Rejected('A concrete check command is required')
        if kind=='harness' and argv!=UNIT_CHECK:raise Rejected('Harness acceptance requires the fixed unit/integration suite')
        recipe='unit';output=None
        if kind=='product':recipe,output=product_recipe(self.root,argv)
        before=candidate(self.root,assets=kind=='product');started=time.time_ns()
        process=subprocess.run(argv,cwd=self.root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        after=candidate(self.root,assets=kind=='product')
        out=self.home/('check-'+str(started)+'.log');out.write_bytes(process.stdout)
        tests=re.search(rb'Ran (\d+) tests?',process.stdout)
        checked=int(tests[1]) if tests else 0
        if recipe=='core':
            total=re.search(rb'100% tests passed, 0 tests failed out of (\d+)',process.stdout);checked=int(total[1]) if total else 0
        if recipe=='simulators' and process.returncode==0:
            summary=read_json(output/'summary.json');checked=0
            if summary.get('success') and (summary.get('observed_test_overlap_seconds') or 0)>0:
                for device in ('iphone','ipad'):
                    log=require_file(output/device/'tests.log',self.root).read_bytes();counts=re.findall(rb'Executed (\d+) tests?',log)
                    if b'** TEST EXECUTE SUCCEEDED **' not in log or not counts:checked=0;break
                    checked+=max(map(int,counts))
        record={'kind':kind,'recipe':recipe,'session':session,'candidate':before,'start':started,'end':time.time_ns(),'argv':argv,'tests':checked,'returncode':process.returncode,'log':str(out.relative_to(self.root)),'logSha':digest(process.stdout),'status':'PASS' if process.returncode==0 and before==after and checked>0 else 'FAIL'}
        self.save(out.with_suffix('.json'),record)
        return record

    def gate(self,session):
        state=self.state(session)
        if not state:raise Rejected('HOOK_NOT_ACTIVE: no native work start')
        rows=self.events()
        if not any(r['event']=='native' and r['session']==session and r['name']=='PreToolUse' for r in rows):raise Rejected('HOOK_NOT_ACTIVE: no pre-tool event')
        if any(p['action']!='commit' for p in state['pending'].values()) or state['unattested']:raise Rejected('UNATTESTED or unfinished tool')
        if state['policy']!=self.policy():raise Rejected('STALE policy')
        now=snapshot(self.root)
        if now!=state['files']:raise Rejected('UNATTESTED current files')
        changed=git(self.root,'diff','--cached','--name-only','-z').decode().split('\0');changed=[x for x in changed if x]
        guarded=[x for x in changed if protected(x)]
        if not guarded:
            if not changed:return {'status':'NO_PRODUCT_CHANGE','visualPass':False}
            result={'status':'COMMIT_ALLOWED','kind':'documentation','session':session,'tree':git(self.root,'write-tree').decode().strip(),'candidate':candidate(self.root,assets=False),'checkLogs':[],'visualPass':False,'reason':'No protected product, policy or harness paths are staged'}
            self.save(self.home/'gate.json',result);return result
        self.check_index_snapshot(now)
        kind='harness' if all(infrastructure(x) for x in guarded) else 'product'
        expected=candidate(self.root,assets=kind=='product')
        checks=[read_json(p) for p in self.home.glob('check-*.json')]
        valid=[r for r in checks if r['session']==session and r['kind']==kind and r['candidate']==expected and r['status']=='PASS' and digest(require_file(self.root/r['log'],self.root).read_bytes())==r['logSha']]
        if not valid:raise Rejected('NOT_RUN: no passing check for current candidate')
        if kind=='product':
            if not {'core','simulators'}.issubset({r.get('recipe') for r in valid}):raise Rejected('NOT_RUN: both common core and simultaneous simulator recipes are required')
            self.check_product_review(session,expected)
        tree=git(self.root,'write-tree').decode().strip()
        result={'status':'COMMIT_ALLOWED','kind':kind,'session':session,'tree':tree,'candidate':expected,'checkLogs':[r['logSha'] for r in valid],'visualPass':kind=='product'}
        self.save(self.home/'gate.json',result);return result

    def committed_gate(self):
        path=self.home/'gate.json'
        if not path.exists():raise Rejected('No staged acceptance record for committed tree')
        record=read_json(path)
        if git(self.root,'rev-parse','HEAD^{tree}').decode().strip()!=record.get('tree'):raise Rejected('Committed tree differs from approved index')
        if candidate(self.root,assets=record['kind']=='product')!=record['candidate']:raise Rejected('STALE committed candidate')
        return {'status':'COMMIT_ALLOWED','tree':record['tree'],'kind':record['kind'],'visualPass':record['visualPass']}

    def push_gate(self,updates):
        accepted=self.committed_gate();checked=[]
        for line in updates.splitlines():
            if not line.strip():continue
            fields=line.split()
            if len(fields)!=4:raise Rejected('Malformed native push update')
            local,oid,remote,previous=fields
            if not all(re.fullmatch(r'[0-9a-f]{40}|[0-9a-f]{64}',value) for value in (oid,previous)):raise Rejected('Invalid push object identity')
            if set(oid)=={'0'}:continue # Existing branch-flow protection still validates deletion rights.
            tree=git(self.root,'rev-parse',oid+'^{tree}').decode().strip()
            if tree!=accepted['tree']:raise Rejected('Outgoing ref has no matching tree acceptance: '+local)
            checked.append({'local':local,'remote':remote,'oid':oid,'tree':tree})
        return {**accepted,'updates':checked}

    def observe(self,input_path,binary):
        from visual import validate_observations
        source=require_file(input_path,self.root);program=require_file(binary,self.root)
        report=read_json(source);expected=candidate(self.root)
        if report.get('candidate')!=expected or report.get('binarySha256')!=digest(program.read_bytes()):raise Rejected('Observation source/binary differs from candidate')
        bases=census_bases(self.root);needed={(base,device,screen) for base in bases for device in ('iphone','ipad') for screen in ('hub','command','shop')}
        actual={(c.get('base'),c.get('device'),c.get('screen')) for c in report.get('cases',[])}
        outcome=validate_observations(report,self.root)
        if needed-actual:outcome['status']='FAIL';outcome['failures'].append('Incomplete original base/device/screen matrix')
        result={**outcome,'candidate':expected,'binarySha256':digest(program.read_bytes()),'source':{'path':str(source.relative_to(self.root)),'sha256':digest(source.read_bytes())}}
        self.save(self.home/'machine-observations.json',result);return result

    def check_product_review(self,session,expected):
        path=self.home/'visual-review.json'
        if not path.exists():raise Rejected('NOT_RUN: actual simulator image/original requirement review missing')
        review=read_json(path)
        if review.get('session')!=session or review.get('candidate')!=expected:raise Rejected('STALE visual review')
        requirements=review.get('requirements',[])
        if not requirements:raise Rejected('Missing original requirements')
        required_rules=self.state(session)['requirements']
        if len(requirements)!=len(required_rules) or {r.get('id') for r in requirements}!=set(required_rules):raise Rejected('Original requirement set was narrowed or duplicated')
        state=self.state(session)
        if review.get('originalRequirementsHash')!=requirements_identity(state):raise Rejected('Original requirement text mismatch')
        matches=sum(r.get('status')=='PASS' for r in requirements)
        score=matches/len(requirements)*100
        if score<90:raise Rejected('Visual match below 90%; inspect assets through missing code')
        if any(r.get('status')!='PASS' for r in requirements):raise Rejected('Mandatory requirement failed')
        if not review.get('contexts') or not review.get('artifacts'):raise Rejected('Missing full context/artifact matrix')
        required={(base,device,screen) for base in census_bases(self.root) for device in ('iphone','ipad') for screen in ('hub','command','shop')}
        actual={(x.get('base'),x.get('device'),x.get('screen')) for x in review['contexts'] if x.get('status')=='PASS'}
        if required-actual:raise Rejected('Incomplete base/device/screen coverage')
        viewed={r.get('imageSha') for r in self.events() if r.get('event')=='image-view' and r.get('session')==session}
        for artifact in review['artifacts']:
            file=require_file(self.root/artifact['path'],self.root)
            if digest(file.read_bytes())!=artifact['sha256']:raise Rejected('Changed artifact')
            if artifact.get('type')=='screenshot' and artifact['sha256'] not in viewed:raise Rejected('NOT_RUN: screenshot was not actually opened through an observed tool')
        machine=review.get('machineObservationReceipt')
        if not isinstance(machine,dict):raise Rejected('Missing renderer/resource/input observation validation')
        receipt=require_file(self.root/machine['path'],self.root)
        if digest(receipt.read_bytes())!=machine.get('sha256'):raise Rejected('Changed machine receipt')
        machine_result=read_json(receipt)
        if machine_result.get('candidate')!=expected or machine_result.get('status')!='PASS':raise Rejected('Invalid machine observation receipt')
        if not machine_result.get('source'):raise Rejected('Machine receipt has no source observations')
        source=require_file(self.root/machine_result['source']['path'],self.root)
        if digest(source.read_bytes())!=machine_result['source']['sha256']:raise Rejected('Changed renderer observations')
        from visual import validate_observations
        observations=read_json(source)
        if observations.get('candidate')!=expected:raise Rejected('Different captured candidate')
        binary=review.get('binary',{});binary_file=require_file(self.root/binary.get('path',''),self.root)
        actual_binary=digest(binary_file.read_bytes())
        if actual_binary!=binary.get('sha256') or observations.get('binarySha256')!=actual_binary:raise Rejected('Different captured binary')
        captured={(c.get('base'),c.get('device'),c.get('screen')) for c in observations.get('cases',[])}
        if captured!=actual:raise Rejected('Captured contexts and declared coverage differ')
        artifacts={a['path']:a['sha256'] for a in review['artifacts'] if a.get('type')=='screenshot'}
        if any(artifacts.get(c.get('png'))!=c.get('pngSha256') for c in observations.get('cases',[])):raise Rejected('Captured PNG does not match opened artifact')
        outcome=validate_observations(observations,self.root)
        if outcome['status']!='PASS':raise Rejected('Renderer observations do not pass: '+str(outcome['failures']))

    def check_index_snapshot(self,now):
        algorithm=git(self.root,'rev-parse','--show-object-format').decode().strip();indexed={}
        for row in git(self.root,'ls-files','--stage','-z').decode().split('\0'):
            if not row:continue
            meta,name=row.split('\t',1);mode,blob,stage=meta.split()
            if protected(name):
                if stage!='0':raise Rejected('Unmerged index')
                indexed[name]=(mode,blob)
        if set(indexed)!=set(now):raise Rejected('Candidate/index file set differs, including unstaged helper')
        for name,(mode,blob) in indexed.items():
            p=self.root/name;data=os.readlink(p).encode() if p.is_symlink() else p.read_bytes()
            actual=hashlib.new(algorithm,b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            expected_mode='120000' if p.is_symlink() else ('100755' if p.stat().st_mode&0o111 else '100644')
            if actual!=blob or mode!=expected_mode:raise Rejected('Index/worktree mismatch: '+name)

def census_bases(root):
    from provenance import original_catalog
    try:return original_catalog(root).verify_prepared_bases()
    except (ValueError,KeyError,OSError) as error:raise Rejected('Original/prepared base mismatch: '+str(error)) from error

def requirements_identity(state):return digest({'rules':state['requirements'],'requests':state['requests']})

def product_recipe(root,argv):
    if argv==['ctest','--test-dir','build/core','--output-on-failure']:return 'core',None
    prefix=['python3','tools/test_apple_parallel.py']
    if argv[:2]!=prefix:raise Rejected('Unregistered product command')
    parser=argparse.ArgumentParser(exit_on_error=False,add_help=False)
    for flag in ['--xctestrun','--iphone','--ipad','--output']:parser.add_argument(flag,required=True)
    # Full suites only. A missing selected test must not silently produce 0 tests.
    try:args,unknown=parser.parse_known_args(argv[2:])
    except (argparse.ArgumentError,SystemExit) as error:raise Rejected('Invalid simulator recipe') from error
    if unknown or args.iphone==args.ipad:raise Rejected('Invalid simultaneous simulator selection')
    for identifier in [args.iphone,args.ipad]:
        if not re.fullmatch(r'[0-9a-fA-F-]{36}',identifier):raise Rejected('Invalid simulator identifier')
    run=Path(args.xctestrun);run=run if run.is_absolute() else root/run
    require_file(run,root)
    output=(root/args.output).resolve()
    if not inside(root/'.local/evidence',output) or output.exists():raise Rejected('Use a new local simulator evidence directory')
    return 'simulators',output

def rule_text(root):
    text=require_file(root/POLICY[0]).read_text();rules={}
    for line in text.splitlines():
        found=re.match(r'\| ([DAHPV]\d+) \|',line) or re.match(r'- \*\*([DAHPV]\d+)(?=[: *])',line)
        if found:rules[found[1]]=line
    if not rules:raise Rejected('No canonical requirement IDs')
    return rules

def deny(reason):
    return {'hookSpecificOutput':{'hookEventName':'PreToolUse','permissionDecision':'deny','permissionDecisionReason':reason}}

def classify(root,event):
    name=event.get('tool_name','');data=event.get('tool_input') or {}
    if name in {'view_image','mcp__images__open'}:return 'read',[]
    if name in {'get_goal','clock__curr_time'}:return 'read',[]
    if name in {'collaborationfollowup_task','collaborationsend_message','collaborationlist_agents','collaborationwait_agent','collaborationinterrupt_agent'}:return 'read',[]
    command=data.get('command',data.get('cmd','')) if isinstance(data,dict) else str(data)
    if name in {'apply_patch','Edit','Write'}:
        paths=[]
        for match in re.finditer(r'^\*\*\* (?:Add File|Update File|Delete File|Move to): (.+)$',command,re.M):
            p=Path(match[1]);p=p if p.is_absolute() else Path(event.get('cwd',root))/p
            if not inside(root,p):raise Rejected('Patch leaves HM2 root')
            if p.is_symlink():raise Rejected('Patch target is a symbolic link')
            paths.append(p.resolve().relative_to(root).as_posix())
        if len(paths)!=len(set(paths)):raise Rejected('Duplicate patch target; combine operations into one Update File')
        return ('write',paths) if paths else ('unknown',[])
    if name in {'Bash','exec_command'}:
        try:tokens=shlex.split(command)
        except ValueError:return 'unknown',[]
        if not tokens:return 'read',[]
        lexer=shlex.shlex(command,posix=True,punctuation_chars=';&|<>');lexer.whitespace_split=True
        if any(t in command for t in ('`','$(','\n')) or any(t and set(t)<=set(';&|<>') for t in lexer):return 'unknown',[]
        if tokens[0]=='git':
            if '--no-verify' in tokens or any(x.startswith(('core.hooksPath','--output','--ext-diff','--textconv','--config-env')) for x in tokens):return 'unknown',[]
            if any(x in tokens for x in ('commit','push')):return 'commit',[]
            if len(tokens)>1 and tokens[1]=='add':return 'index',[]
            if len(tokens)>1 and tokens[1] in {'status','diff','show','log','ls-files','ls-tree','rev-parse','ls-remote'}:return 'read',[]
            return 'unknown',[]
        if tokens[0] in READ_COMMANDS and not any(x.startswith(('--pre','--files-with-matches=')) for x in tokens[1:]):return 'read',[]
        if tokens[0]=='sed' and len(tokens)==4 and tokens[1]=='-n' and re.fullmatch(r'\d+(?:,\d+)?p',tokens[2]):return 'read',[]
        if len(tokens)>2 and tokens[0] in {'node','python3'}:
            cwd=Path(data.get('workdir',data.get('cwd',event.get('cwd',root))))
            executable=(cwd/tokens[1]).resolve()
            allowed={(root/'.agents/harness/cli.mjs').resolve(),(root/'.agents/harness/presentation.py').resolve()}
            if executable in allowed and tokens[2] in {'doctor','plan','status','gate'}:return 'read',[]
            if executable in allowed and tokens[2]=='setup' and len(tokens)==3:return 'write',['.codex/hooks.json']
            if executable in allowed and tokens[2]=='recover-failed':return 'recovery',[]
            if executable in allowed and tokens[2] in {'gate','check','observe','review'}:return 'check',[]
        return 'unknown',[]
    return 'unknown',[]

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--source',choices=['project','user'],default='project')
    sub=parser.add_subparsers(dest='mode',required=True)
    sub.add_parser('hook');sub.add_parser('doctor');sub.add_parser('plan');sub.add_parser('setup')
    status=sub.add_parser('status');status.add_argument('--session',default=os.environ.get('CODEX_THREAD_ID',''))
    recovery=sub.add_parser('recover-failed');recovery.add_argument('--session',default=os.environ.get('CODEX_THREAD_ID',''));recovery.add_argument('--tool-id',required=True);recovery.add_argument('--transcript',type=Path,required=True);recovery.add_argument('--call-id',required=True)
    gate=sub.add_parser('gate');gate.add_argument('--session',default=os.environ.get('CODEX_THREAD_ID',''))
    gate.add_argument('--committed',action='store_true')
    gate.add_argument('--push',action='store_true')
    check=sub.add_parser('check');check.add_argument('--session',default=os.environ.get('CODEX_THREAD_ID',''));check.add_argument('--kind',choices=['harness','product'],required=True);check.add_argument('argv',nargs=argparse.REMAINDER)
    observe=sub.add_parser('observe');observe.add_argument('--input',type=Path,required=True);observe.add_argument('--binary',type=Path,required=True)
    review=sub.add_parser('review');review.add_argument('--input',type=Path,required=True);review.add_argument('--session',default=os.environ.get('CODEX_THREAD_ID',''))
    args=parser.parse_args();h=Harness(args.root)
    try:
        if args.mode=='hook':
            event=json.load(sys.stdin)
            # User-level dispatcher leaves other repositories untouched, including
            # sessions begun elsewhere whose target is not this repository.
            cwd=Path(event.get('cwd',h.root));data=event.get('tool_input') or {}
            payload=json.dumps(data,ensure_ascii=False)
            local=inside(h.root,cwd)
            if local or str(h.root) in payload:
                with h.locked():
                    with (h.home/'dispatch-entry.jsonl').open('a') as audit:
                        audit.write(json.dumps({'time_ns':time.time_ns(),'session':event.get('session_id'),'event':event.get('hook_event_name'),'tool':event.get('tool_name'),'source':args.source,'cwd':str(cwd),'insideProject':local,'inputType':type(data).__name__,'inputKeys':list(data) if isinstance(data,dict) else None,'skipBecauseProjectAssumed':args.source=='user' and local})+'\n')
            # The user dispatcher also owns local tool calls from threads opened outside HM2.
            active=h.state(event.get('session_id','')) if event.get('session_id') else None
            data_cwd=data.get('workdir',data.get('cwd')) if isinstance(data,dict) else None
            resolved_cwd=(cwd/data_cwd).resolve() if data_cwd else cwd
            targets=[]
            command=data.get('command',data.get('cmd','')) if isinstance(data,dict) else str(data)
            if isinstance(data,dict):
                for m in re.finditer(r'^\*\*\* (?:Add File|Update File|Delete File|Move to): (.+)$',command,re.M):targets.append((resolved_cwd/m[1]).resolve())
                for key in ('path','file_path'): 
                    if data.get(key):targets.append((resolved_cwd/data[key]).resolve())
            else:
                for m in re.finditer(r'^\*\*\* (?:Add File|Update File|Delete File|Move to): (.+)$',command,re.M):targets.append((resolved_cwd/m[1]).resolve())
            if event.get('tool_name') in {'Bash','exec_command'}:
                try:
                    for token in shlex.split(command):
                        if '/' in token and not token.startswith('-'):targets.append((resolved_cwd/token).resolve())
                except ValueError:pass
            targeted=inside(h.root,resolved_cwd) or any(inside(h.root,p) for p in targets)
            if not local and not targeted and str(h.root) not in payload and not active:return 0
            print(json.dumps(h.hook(event),ensure_ascii=False));return 0
        if args.mode=='doctor':print(json.dumps({'status':'CONFIGURED_NOT_RUNTIME_PROOF','root':str(h.root),'policy':h.policy(),'hooksFile':(h.root/'.codex/hooks.json').exists(),'productAcceptance':False}));return 0
        if args.mode=='setup':return subprocess.run(['python3',str(h.root/'.agents/harness/install_hooks.py'),'--install'],cwd=h.root).returncode
        if args.mode=='plan':print(json.dumps({'steps':['native request and pre-tool hooks','record guarded mutations','same-candidate simultaneous cheat simulator','actual images and original requirement review','staged evidence gate'],'currentProductPass':False}));return 0
        if args.mode=='status':
            state=h.state(args.session);events=[r for r in h.events() if r.get('session')==args.session]
            print(json.dumps({'session':args.session,'nativeEvents':[r.get('name') for r in events if r.get('event')=='native'],'preTools':len([r for r in events if r.get('event')=='pre']),'postTools':len([r for r in events if r.get('event')=='post']),'pending':len(state['pending']) if state else None,'unattested':state.get('unattested') if state else None,'productAcceptance':False}));return 0
        if args.mode=='recover-failed':
            print(json.dumps(h.recover_failed(args.session,args.tool_id,args.transcript,args.call_id)));return 0
        if args.mode=='gate':
            with h.locked():result=h.push_gate(sys.stdin.read()) if args.push else h.committed_gate() if args.committed else h.gate(args.session)
        elif args.mode=='observe':result=h.observe(args.input,args.binary)
        elif args.mode=='review':
            original=require_file(args.input,h.root);incoming=read_json(original)
            h.save(h.home/'visual-review.json',incoming)
            h.check_product_review(args.session,candidate(h.root));result={'status':'PASS','scope':'same-candidate original-requirement visual review'}
        else:
            argv=args.argv[1:] if args.argv[:1]==['--'] else args.argv
            result=h.run(args.session,args.kind,argv)
        print(json.dumps(result,ensure_ascii=False));return 0 if result.get('status') in {'PASS','COMMIT_ALLOWED','NO_PRODUCT_CHANGE'} else 1
    except (Rejected,ValueError,KeyError,OSError,subprocess.CalledProcessError) as error:
        if args.mode=='hook':print(json.dumps(deny('HM2 hook failed safely: '+str(error)),ensure_ascii=False));return 0
        print(json.dumps({'status':'FAIL','reason':str(error)},ensure_ascii=False));return 1

if __name__=='__main__':sys.exit(main())
