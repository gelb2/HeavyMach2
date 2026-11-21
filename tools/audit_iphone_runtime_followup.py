#!/usr/bin/env python3
"""Inspect local original runtime metadata and cached REA evidence; never execute apps."""
from pathlib import Path
import argparse,json,re,hashlib,struct,collections
REPO=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--analysis-root',type=Path,default=Path('/Users/sokol/Documents/Codex/HeavyMach2/Analysis'))
parser.add_argument('--reference-root',type=Path,default=Path('/Users/sokol/Documents/Codex/HeavyMach2/ReferenceBuilds/inspection'))
parser.add_argument('--output',type=Path,default=REPO/'.local/evidence/iphone-universal-runtime-followup-20261113')
args=parser.parse_args()
OUT=args.output;OUT.mkdir(parents=True,exist_ok=True)
AN=args.analysis_root
DOM={'phone':AN/'Phone201/universal-audit-20261113/full-export','hd_methods':AN/'HD103/method-evidence','hd_native':AN/'HD103/native-evidence'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
M={k:json.loads((p/'export-manifest.json').read_text()) for k,p in DOM.items()}
DATA={k:{f.name.split('.')[0]:f.read_text().split('{',1)[-1] for f in (p/'functions').glob('*.pseudo.c')} for k,p in DOM.items()}
NAME={k:{m['address']:m.get('name') or m.get('owner','')+'::'+m.get('selector','') for m in v['methods']} for k,v in M.items()}
EVID={}
def ref(k,a):
 key=k+':'+a
 if key in EVID:return key
 entry=next(x for x in M[k]['methods'] if x['address']==a)
 fs=[]
 for ext,typ in [('pseudo.c','pseudo'),('asm','assembly')]:
  p=DOM[k]/'functions'/f'{a}.{ext}';data=p.read_bytes();e=entry.get('exports',{}).get(typ,{})
  dig=hashlib.sha256(data).hexdigest()
  if e.get('sha256'):assert e['sha256']==dig
  fs.append({'kind':typ,'path':str(p),'sha256':dig,'bytes':len(data),'evidence_id':e.get('evidence_id')})
 EVID[key]={'domain':k,'address':a,'name':NAME[k][a],'source_sha256':M[k]['source_sha256'],'files':fs};return key
def rows(k,term):
 return [{'address':a,'name':NAME[k][a],'occurrences':t.count(term),'evidence_ref':ref(k,a)} for a,t in DATA[k].items() if term in t]
def calls(k,pattern):
 return [{'address':a,'name':NAME[k][a],'calls':len(re.findall(pattern,t)),'evidence_ref':ref(k,a)} for a,t in DATA[k].items() if re.search(pattern,t)]
# Literal-offset inventory is an over-approximation, not alias or type recovery.
pt48=rows('phone','+ 0x48)');global_candidates=[r for r in pt48 if 'PTR_DAT_000653ec' in DATA['phone'][r['address']]]
known_phone_time={
 '0x7ca4':'airstrike_fire_and_expiry','0x80b4':'field_item_birth_timestamp','0x8584':'bullet_flight_expiry','0x8c64':'bullet_birth_timestamp','0x92e8':'field_item_animation_and_poll','0x9ec4':'enemy_birth_timestamp','0xa6ec':'bot_item_job_schedule','0xaab8':'bot_aircraft_wait','0xad54':'bot_hp_regeneration','0xaf50':'bot_birth_timestamp','0xb128':'enemy_attack_poll','0xbd7c':'enemy_train_wait','0xbf80':'enemy_aircraft_wait','0xc168':'enemy_vehicle_wait','0xc35c':'enemy_crash_effect_delay','0xcc24':'player_pickup_and_hp_regeneration','0xd974':'player_pickup_timestamp','0x10230':'target_position_refresh','0x12f54':'trail_lifetime','0x132a0':'trail_birth_timestamp','0x23be0':'weapon_ai_start_timestamp','0x23d70':'weapon_reload_and_shot_timing','0x2471c':'weapon_ai_scan','0x24b50':'weapon_reload_timestamp','0x2a9f0':'range_notice_sound_throttle','0x2abe4':'message_timestamp','0x2de68':'message_and_range_hud_tick','0x58ce4':'world_path_marker_animation','0x59cf0':'world_easy_move_touch_timing','0x5af50':'world_clock_producer'}
# These roles came from read local body flow, not from literal offset alone.
known=[{'address':a,'role':role,'evidence_ref':ref('phone',a)} for a,role in known_phone_time.items()]
time_inventory={'hd_world_time_selector_consumers':rows('hd_methods','"time"'),'hd_world_time_direct_fields':rows('hd_methods','_gameTime'),'hd_global_frame_clock_references':rows('hd_methods','PTR_DAT_00051408'),'phone_literal_offset48_candidates':pt48,'phone_candidates_also_referencing_world_singleton':global_candidates,'phone_selected_world_time_roles':known,'limitation':'Literal offsets can refer to scale, indices or unrelated structures. The selected roles do not establish all aliases/register-offset accesses. Whole-program time-consumer closure remains open.'}
rng={k:{'random':calls(k,r'(?<![A-Za-z])_random\('),'srandom':calls(k,r'(?<![A-Za-z])_srandom\('),'mach_absolute_time':rows(k,'_mach_absolute_time('),'CFAbsoluteTimeGetCurrent':rows(k,'_CFAbsoluteTimeGetCurrent(')} for k in DOM}
saves={'phone':calls('phone',r'FUN_00017f80\('),'hd':calls('hd_methods',r'(?:::)?saveDataFile\(')}
phone_events={'0x1a0b0':'existing_valid_slot_selected; initialize working slot, time and HP, then save','0x6b78':'base_screen_entry; normalize Base001 name, construct base UI, then save','0x3de68':'slot_delete_accept_branch; valid slot flag cleared and slot refreshed before save','0x2ca98':'system_menu buttonID0x408 branch; clears world pause, hides menu, then saves and requests screen change','0x5a5dc':'stage_open_complete; world state set1, refresh, unpause, then save','0x55cf4':'game_over_entry; guard world state!=4, set4, save, then create destruction alert'}
hd_events={'0x17be0':'valid_slot_selected; level/EXP/HP UI refresh before save','0x5644':'base_screen_entry; normalize Base001, copy stage name, construct base UI, then save','0x5468':'confirmed_slot_delete; alertID1 and selected valid slot -> clear used flag, refresh, save','0x281e0':'system_menu buttonID0x407; close dialogs, pause, test world.isSave, save if true, then screen change','0x13778':'stage_entry_complete; reseed RNG, load map/mission/player/linkgate, then save only if world._isSave'}
for k,rs in saves.items():
 for r in rs:r['event']=(phone_events if k=='phone' else hd_events).get(r['address'],'UNRESOLVED')
assert len(saves['phone'])==6 and len(saves['hd'])==5
# Resolve only direct immediate ARM BL/BLX edges from the complete phone assembly cache.
g={int(f.stem,16):{int(x,16) for x in re.findall(r'\bblx?\s+(0x[0-9a-f]+)',f.read_text())} for f in (DOM['phone']/'functions').glob('*.asm')}
paths=[]
for a in ['0x23d70','0x8c64','0x8584','0x8910']:
 start=int(a,16);q=collections.deque([start]);prev={start:None}
 while q:
  c=q.popleft()
  for to in g.get(c,[]):
   if to not in prev:prev[to]=c;q.append(to)
 paths.append({'start':a,'evidence_ref':ref('phone',a),'known_bodies_reachable':sum(v in g for v in prev),'save_0x17f80_reachable':0x17f80 in prev,'limitation':'Only direct immediate BL/BLX edges; excludes dynamic vtable/function-pointer/Objective-C dispatch.'})
audio_terms=['_alGenSources','_alDeleteSources','_alSourceStop','_alSourcePause','_alcMakeContextCurrent','_alcCreateContext','_alcDestroyContext','_AudioQueueNewOutput','_AudioQueuePrime','_AudioQueuePause','_AudioSession','interruption','Interruption','AVAudioSession']
audio_census={k:{term:rows(k,term) for term in audio_terms} for k in DOM}
# Parse only native metadata and four callback pointer words. No binary content is copied out.
def macho(b):
 assert struct.unpack_from('<I',b)[0]==0xfeedface
 n=struct.unpack_from('<I',b,16)[0];off=28;segments=[];st=None
 for _ in range(n):
  cmd,size=struct.unpack_from('<II',b,off)
  if cmd==1:
   name=b[off+8:off+24].split(b'\0')[0].decode();va,vs,fo,fs=struct.unpack_from('<IIII',b,off+24);segments.append({'name':name,'vmaddr':va,'vmsize':vs,'fileoff':fo,'filesize':fs})
  if cmd==2:st=struct.unpack_from('<IIII',b,off+8)
  off+=size
 imp=[]
 if st:
  sy,nsy,so,ss=st
  for i in range(nsy):
   ix,nt,ns,nd,val=struct.unpack_from('<IBBHI',b,sy+i*12)
   if nt&0xe==0 and ix:
    end=b.find(b'\0',so+ix,so+ss);imp.append(b[so+ix:end].decode(errors='replace'))
 return segments,imp
native_files={'phone':args.reference_root/'phone/original-executable','hd':args.reference_root/'hd/original-executable'}
imports={};callback_words=[]
for k,p in native_files.items():
 b=p.read_bytes();container_sha=hashlib.sha256(b).hexdigest();slice_info=None
 if b[:4]==bytes.fromhex('cafebabe'):
  for i in range(struct.unpack_from('>I',b,4)[0]):
   cpu,sub,o,l,al=struct.unpack_from('>IIIII',b,8+i*20)
   if cpu==12 and sub==9:slice_info={'cpu':cpu,'subtype':sub,'offset':o,'bytes':l};b=b[o:o+l];break
 expected=M['phone' if k=='phone' else 'hd_native']['source_sha256'];assert hashlib.sha256(b).hexdigest()==expected
 segs,imps=macho(b)
 selected={t:[s for s in imps if t in s] for t in ['AudioSession','AVAudioSession','AudioQueuePause','alSourcePause','AudioQueueNewOutput','alcCreateContext']}
 imports[k]={'path':str(p),'container_sha256':container_sha,'slice':slice_info,'native_sha256':expected,'undefined_symbol_count':len(imps),'selected_imports':selected}
 if k=='hd':
  for va,role,target in [(0x5deb0,'AudioQueue_output_callback',0x416d4),(0x5deac,'AudioQueue_isRunning_listener',0x41f3c),(0x5dea8,'pthread_entry',0x40ca8),(0x5deb4,'loop_effect_worker',0x41578)]:
   seg=next(x for x in segs if x['vmaddr']<=va<x['vmaddr']+x['filesize']);fo=va-seg['vmaddr']+seg['fileoff'];word=struct.unpack_from('<I',b,fo)[0];assert word&~1==target
   callback_words.append({'role':role,'pointer_va':hex(va),'file_offset':hex(fo),'raw_word':hex(word),'thumb_target':hex(word&~1),'segment':seg,'source_native_sha256':expected,'evidence_kind':'derived_local_MachO_section_mapping_and_4byte_read; target export evidence is separate','target_evidence_ref':ref('hd_native',hex(target))})
# Method-only selector searches distinguish SoundManager from unrelated world pause.
cache_entrypoints={'phone_getSound_callers':calls('phone',r'FUN_00049aec\('),'hd_getSound_selector_callers':rows('hd_methods','"getSound:"'),'hd_sound_manager_named_lifecycle': [ref('hd_methods',a) for a in ['0x42248','0x42268','0x42364','0x42318']],'hd_sound_manager_noarg_lifecycle_calls_in_cached_bodies':{t:rows('hd_methods','"'+t+'"') for t in ['setPause','setBackground','removeAllSounds']}}
claims=[]
def claim(id,fact,refs,expectation,forbidden,unknown):claims.append({'id':id,'observed':fact,'evidence_refs':[ref(k,a) for k,a in refs],'expected_restoration_behavior':expectation,'forbidden_shortcut':forbidden,'still_unresolved':unknown})
claim('time_precision_and_narrowing','Global frame clocks use f32; world clock is phone f32 versus HD f64. HD time consumers often explicitly narrow double world time into float timestamps, including weapon fireTime, BOT HP refresh, player pickup and GUI notices.', [('phone','0x5af50'),('hd_methods','0x110d8'),('hd_methods','0x14864'),('hd_methods','0x1cf38'),('hd_methods','0x8e28'),('hd_methods','0xb454'),('hd_methods','0x28dac')], 'Preserve one HD world-time accumulator and the observed per-field timestamp precision for both device families. Separate global animation time and host absolute time. Freeze world clock when its pause gate is set.', 'Do not globally change every time field to float or double; do not replace 0.0166f by1/60 or advance world time from host elapsed delta without an explicit behavior change.', 'All indirect and register-offset time aliases are not closed by literal-offset search; selected phone roles and all named HD time calls are indexed separately.')
claim('save_event_and_rng_order','Six phone and five HD direct writer caller bodies are resolved to guarded events. HD stage entry and save seed RNG with mach time; EncTbl loading seeds 0xb27f7 in both originals. Save/load consume the same process random API used by combat/AI.', [('phone','0x540fc'),('phone','0x17f80'),('phone','0x17df4'),('phone','0x5a5dc'),('hd_methods','0x3e38c'),('hd_methods','0x13778'),('hd_methods','0x17580'),('hd_methods','0x16d8c')], 'Keep HD 28204-byte save schema, checksum/sign behavior and event guards; preserve gameplay RNG call order against HD evidence. Capture save counts during transitions, completion and battle tests.', 'No per-shot/per-frame DB or save writes. Do not import phone26344-byte format or RNG reseeding schedule as an iPhone rule. Do not move all save writes to generic UI events.', 'Direct caller closure is not virtual/selector closure. Mission accept/reward modifies in-memory state but no additional direct saveDataFile caller is present; actual flush boundaries require runtime counters.')
claim('original_hd_checksum','Original HD save0x17580 sums signed plaintext bytes from offset4 through0x6e27 before storing the32-bit checksum at0x6e28. ASM0x1764e initializes with sxtb;0x1766a accumulates with sxtab;0x17690 stores at the terminal offset. Load0x16d8c decrypts, accumulates signed bytes with sxtab at0x16e2a, compares the trailing word at0x16e34/0x16e36, and only the equal branch copies the loaded state. Unequal checksum takes the invalid-save path.', [('hd_methods','0x17580'),('hd_methods','0x16d8c')], 'Retain HD signed-byte checksum and seed/XOR transform for all restored devices. Reject a checksum mismatch through the observed invalid-save path before accepting loaded state.', 'Do not infer this original contract from reconstructed SaveData.cpp; do not call XOR alone integrity validation or silently accept an invalid checksum.', 'This is original static ASM/pseudo evidence. No corrupted user save was created or loaded and no runtime corruption test was executed.')
claim('effect_cache_source_identity','The complete pseudo corpus contains one alGenSources call body per original. Both common getSound paths cache filename->one source ID and load only on miss. Replaying a cached one-shot source stops then restarts that same source; no round-robin pool selector appears in those traced paths.', [('phone','0x49aec'),('phone','0x49930'),('phone','0x18438'),('phone','0x1880c'),('phone','0x482d8'),('hd_methods','0x423a8'),('hd_native','0x41c20'),('hd_native','0x40cf4'),('hd_native','0x40aa8')], 'Preserve HD sound ID ownership/cache hit behavior and compare same-ID repeated triggers separately from different-ID simultaneous triggers. A restoration pool is an intentional extension unless justified by original callers.', 'Do not assert original polyphony from many gun events. Do not create context/source/decode PCM per shot, or replace SFX with AVAudioPlayer/AVAudioEngine.', 'Aliased writes and arbitrary native pointer mutation were not proven absent. No real audible polyphony/latency test occurred.')
claim('loop_worker_reachability','Both effect start implementations contain a separate attack/loop worker branch. HD common constructor0x40cf4 receives loop flag0 through0x41c20, and phone0x49930 initializes it0. Worker sequence plays, polls processed attack buffer, unqueues it, then sets AL_LOOPING. pthread entries and HD callback pointers are now resolved.', [('phone','0x49930'),('phone','0x482d8'),('phone','0x480ec'),('phone','0x4820c'),('phone','0x482a4'),('hd_native','0x41c20'),('hd_native','0x40cf4'),('hd_native','0x40aa8'),('hd_native','0x41578'),('hd_native','0x40ca8')], 'Classify effect looping separately from BGM looping. Test only genuinely reachable original effect-loop configuration; keep error and attack/decay semantics if activated.', 'Do not treat existence of dormant library loop code as proof that game weapons require it; do not enable loop by filename guess.', 'Other writes to the loop flag and worker priority/engine destruction race lifetimes need a complete alias audit before declaring loop unreachable globally.')
claim('bgm_callback_links','HD four literal callback pointers were independently verified from SHA-matched __DATA mapping. They target output0x416d4, isRunning0x41f3c, thread0x40ca8, effect-worker0x41578. BGM output EOF has continue/stop and format-transition cases. isRunning listener rebuilds queue+buffers and primes/starts after transition stop.', [('phone','0x48e70'),('phone','0x4925c'),('hd_native','0x416d4'),('hd_native','0x41f3c'),('hd_native','0x41d04'),('hd_native','0x40f50')], 'Preserve callback/lifetime sequencing, distinguish normal EOF stop from format-change recreation, preserve one/three buffer setup and0.5-second sizing request.', 'Do not recreate SFX context on BGM queue transitions; do not reduce stream callbacks to a single end-of-track event.', 'Exact output latency, underruns, stopping during callback and interruption/route-change timing require runtime tests. Numeric bounds do not prove measured0.5-second latency.')
claim('audio_lifecycle_limits','Both original undefined-symbol tables have no AudioSession/AVAudioSession/AudioQueuePause/alSourcePause import. Cached bodies also show none. HD SoundManager setPause only releases cache; setBackground and dealloc destroy engines. No cached no-argument selector caller to setPause/setBackground/removeAllSounds was found. World setPause: is a different selector.', [('hd_methods','0x42268'),('hd_methods','0x42364'),('hd_methods','0x42318'),('hd_native','0x40604'),('phone','0x2cd0'),('phone','0x2cf4'),('hd_methods','0x24d8'),('hd_methods','0x24dc')], 'Document host interruption/route handling as an explicit common adapter requirement. Preserve shared HD game state and resume without leaking engines or stale IDs; instrument pause, resume and rewarm.', 'Do not rename world pause as audio pause. Do not report complete original audio interruption behavior from method names or missing imports. Do not claim untested background sound continuity.', 'Dynamic selector strings/function pointers and OS behavior can bypass literal searches. Exact interruption/route registration and restart behavior remains unproven; no event simulation was performed.')
# Mission entrypoints are a bounded direct writer absence check, not in-memory behavior equivalence.
mission_refs=[ref('hd_methods',a) for a in ['0x1b280','0x1b510','0x25e90','0x279a8','0x15c78']]
result={'schema':1,'date':'2026-11-13','task':'Original phone2.0.1/HD1.0.3 runtime follow-up for universal design','repository_head_at_start':'7247972','scope':{'cache_bodies':{k:len(v) for k,v in DATA.items()},'native_queries':0,'app_executions':0,'builds':0,'device_operations':0,'game_source_changes':0,'original_file_writes':0},'sources':{k:{'native_sha256':v['source_sha256'],'manifest_path':str(DOM[k]/'export-manifest.json'),'manifest_sha256':sha(DOM[k]/'export-manifest.json')} for k,v in M.items()},'time_inventory':time_inventory,'rng_call_inventory':rng,'direct_save_callers':saves,'phone_direct_call_graph_weapon_to_save':paths,'audio_symbol_and_body_census':audio_census,'native_import_metadata':imports,'hd_callback_pointer_resolution':callback_words,'cache_entrypoint_inventory':cache_entrypoints,'mission_reward_memory_mutation_scope':{'reviewed_refs':mission_refs,'extra_direct_save_callers_found':0,'limitation':'No claim of automatic disk flush at every accept/reward; event-level write counters still required.'},'findings':claims,'evidence_catalog':EVID,'limits':['Cached full export coverage is not full semantic coverage.','No complete decompiled function, game asset, original binary or IPA is copied into this output.','Read original local executable metadata and four pointer words only; original native SHA and section mapping are preserved.','No new REA session or native query was executed.','Game behavior follows shared HD rules. Phone layout is reference; it does not create a second save, RNG, audio or time implementation.']}
(OUT/'findings.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
(REPO/'docs/evidence/iphone-universal-runtime-followup-20261113.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
verification={'findings':len(claims),'unique_functions':len(EVID),'evidence_files':sum(len(x['files']) for x in EVID.values()),'all_evidence_hashes_match':True,'callback_pointer_words_verified':4,'native_queries':0,'app_executions':0,'builds':0,'output_sha256':sha(OUT/'findings.json'),'repo_json_sha256':sha(REPO/'docs/evidence/iphone-universal-runtime-followup-20261113.json'),'metrics':{'hd_world_time_consumer_functions':len(time_inventory['hd_world_time_selector_consumers']),'hd_world_time_selector_calls':sum(x['occurrences'] for x in time_inventory['hd_world_time_selector_consumers']),'hd_global_frame_clock_functions':len(time_inventory['hd_global_frame_clock_references']),'phone_offset48_candidate_functions':len(pt48),'phone_selected_time_roles':len(known)}}
md=REPO/'docs/iphone-universal-runtime-followup-20261113.md'
if md.exists():
 missing=[target for target in re.findall(r'\]\(([^)]+)\)',md.read_text()) if not target.startswith(('http','#')) and not (md.parent/target.split('#')[0]).exists()]
 assert not missing,missing
 verification['markdown_sha256']=sha(md)
 verification['markdown_local_links_exist']=True
verification['analysis_tool_sha256']=sha(Path(__file__))
verification['repository_json_equals_local']=(REPO/'docs/evidence/iphone-universal-runtime-followup-20261113.json').read_bytes()==(OUT/'findings.json').read_bytes()
(OUT/'verification.json').write_text(json.dumps(verification,indent=2)+'\n');print(json.dumps(verification))
