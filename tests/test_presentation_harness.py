"""Failure-first workflow tests in owned, disposable HM2-local repositories.

These simulate hook event contracts. They are not proof that the native agent
runtime loaded or trusted the hooks; that is a separate integration probe.
"""
import copy
from datetime import datetime,timezone
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.agents/harness'))
from presentation import Harness,Rejected,candidate,classify,digest,deny,UNIT_CHECK,rule_text,requirements_identity
from visual import png_rgb,black_edge_bands,validate_observations

def shell(root,*args):return subprocess.check_output(['git','-C',str(root),*args],stderr=subprocess.DEVNULL)

def png(path,w=8,h=4,pixel=(0,0,0)):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+bytes(pixel)*w)*h))+chunk(b'IEND',b''))

class HarnessTests(unittest.TestCase):
    def setUp(self):
        (ROOT/'.local/evidence').mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix='hm2-harness-fixture-',dir=ROOT/'.local/evidence')
        self.root=Path(self.temp.name);shell(self.root,'init','-q');shell(self.root,'config','user.email','fixture@example.invalid');shell(self.root,'config','user.name','HM2 local fixture')
        (self.root/'docs').mkdir();(self.root/'Sources').mkdir();(self.root/'tests').mkdir()
        (self.root/'.gitignore').write_text('.local/\n__pycache__/\n*.pyc\n')
        (self.root/'docs/design-and-asset-rules.md').write_text('| D01 | No letterbox |\n- **V05:** Inspect actual output\n')
        (self.root/'docs/presentation-harness-plan-20261115.md').write_text('Require native start before change.\n')
        self.file=self.root/'Sources/menu.cpp';self.file.write_text('original\n')
        shell(self.root,'add','.');shell(self.root,'commit','-qm','fixture baseline')
        self.h=Harness(self.root)
        self.native_root=patch('presentation.NATIVE_SESSIONS',self.root);self.native_root.start()

    def tearDown(self):self.native_root.stop();self.temp.cleanup()

    def event(self,name,tool='apply_patch',command=None,identity='one',**extra):
        value={'hook_event_name':name,'session_id':'native-fixture','turn_id':'turn1','cwd':str(self.root),'tool_name':tool,'tool_use_id':identity,'tool_input':{'command':command or '*** Begin Patch\n*** Update File: '+str(self.file)+'\n*** End Patch'}}
        value.update(extra);return value

    def start(self):return self.h.hook(self.event('UserPromptSubmit',prompt='Keep original assets; no letterbox'))

    def observed_edit(self):
        self.start();self.assertEqual(self.h.hook(self.event('PreToolUse')),{});self.file.write_text('changed\n');self.h.hook(self.event('PostToolUse'))

    def test_first_mutation_denied_and_bytes_preserved(self):
        before=self.file.read_bytes();result=self.h.hook(self.event('PreToolUse'))
        self.assertEqual(result['hookSpecificOutput']['permissionDecision'],'deny');self.assertEqual(before,self.file.read_bytes())

    def test_request_delivers_original_rule_context(self):
        result=self.start();self.assertIn('No letterbox',result['hookSpecificOutput']['additionalContext'])

    def test_observed_change_has_pre_and_post_identity(self):
        self.observed_edit();events=self.h.events();pre=next(x for x in events if x['event']=='pre');post=next(x for x in events if x['event']=='post')
        self.assertEqual(pre['toolId'],post['toolId']);self.assertEqual(post['changed'],['Sources/menu.cpp']);self.assertFalse(self.h.state('native-fixture')['unattested'])

    def test_external_change_rejected(self):
        self.start();self.file.write_text('not observed');self.assertIn('UNATTESTED',str(self.h.hook(self.event('PreToolUse'))))

    def test_unexpected_second_file_not_silently_attested(self):
        self.start();self.h.hook(self.event('PreToolUse'));(self.root/'Sources/other.cpp').write_text('unexpected');self.h.hook(self.event('PostToolUse'))
        self.assertEqual(self.h.state('native-fixture')['unattested'],['Sources/other.cpp'])

    def test_missing_post_blocks_next_mutation(self):
        self.start();self.h.hook(self.event('PreToolUse'));self.assertIn('unfinished',str(self.h.hook(self.event('PreToolUse',identity='two'))))

    def test_duplicate_patch_target_rejected_before_pending_is_created(self):
        self.start()
        command='*** Begin Patch\n*** Delete File: '+str(self.file)+'\n*** Add File: '+str(self.file)+'\n+replacement\n*** End Patch'
        result=self.h.hook(self.event('PreToolUse',command=command))
        self.assertIn('duplicate',str(result).lower())
        self.assertFalse(self.h.state('native-fixture')['pending'])

    def failure_transcript(self,command=None,output=None):
        command=command or self.event('PreToolUse')['tool_input']['command']
        path=self.root/'failed-tool.jsonl'
        records=[{'type':'session_meta','payload':{'id':'native-fixture'}},
                 {'type':'response_item','payload':{'type':'custom_tool_call','name':'exec','call_id':'outer-call','input':'text(await tools.apply_patch('+json.dumps(command)+'));'}},
                 {'type':'response_item','timestamp':datetime.now(timezone.utc).isoformat(),'payload':{'type':'custom_tool_call_output','call_id':'outer-call','output':[{'type':'input_text','text':output or 'Script error:\napply_patch verification failed: Failed to find expected lines'}]}}]
        path.write_text(''.join(json.dumps(r)+'\n' for r in records))
        return path

    def test_failed_tool_recovery_preserves_failure_and_allows_next_edit(self):
        self.start();self.h.hook(self.event('PreToolUse'))
        transcript=self.failure_transcript()
        result=self.h.recover_failed('native-fixture','one',transcript,'outer-call')
        self.assertEqual(result['status'],'RECOVERED_FAILED_NO_MUTATION')
        self.assertFalse(result['productAcceptance'])
        self.assertFalse(self.h.state('native-fixture')['pending'])
        self.assertEqual(self.h.hook(self.event('PreToolUse',identity='two')), {})
        self.assertTrue(any(r['event']=='maintenance-recovery' for r in self.h.events()))

    def test_recovery_rejects_a_changed_file(self):
        self.start();self.h.hook(self.event('PreToolUse'));self.file.write_text('changed after pre')
        with self.assertRaisesRegex(Rejected,'changed'):self.h.recover_failed('native-fixture','one',self.failure_transcript(),'outer-call')
        self.assertIn('one',self.h.state('native-fixture')['pending'])

    def test_recovery_rejects_missing_successful_or_unrelated_tool_output(self):
        self.start();self.h.hook(self.event('PreToolUse'))
        for output in ('Success. Updated files','Script error: unknown failure'):
            with self.subTest(output=output):
                with self.assertRaises(Rejected):self.h.recover_failed('native-fixture','one',self.failure_transcript(output=output),'outer-call')
        with self.assertRaises(Rejected):self.h.recover_failed('native-fixture','one',self.failure_transcript(command='*** Begin Patch\n*** End Patch'),'outer-call')

    def test_recovery_command_can_run_with_pending_without_creating_another_pending(self):
        self.start();self.h.hook(self.event('PreToolUse'))
        command='node .agents/harness/cli.mjs recover-failed --tool-id one --transcript failed-tool.jsonl --call-id outer-call'
        event=self.event('PreToolUse','exec_command',command,identity='recovery')
        self.assertEqual(self.h.hook(event),{})
        self.assertEqual(list(self.h.state('native-fixture')['pending']),['one'])

    def test_old_failed_call_cannot_close_a_new_pending_retry(self):
        self.start();self.h.hook(self.event('PreToolUse'));transcript=self.failure_transcript()
        rows=[json.loads(line) for line in transcript.read_text().splitlines()];rows[-1]['timestamp']='2000-01-01T00:00:00+00:00'
        transcript.write_text(''.join(json.dumps(row)+'\n' for row in rows))
        with self.assertRaisesRegex(Rejected,'predates'):self.h.recover_failed('native-fixture','one',transcript,'outer-call')
        self.assertIn('one',self.h.state('native-fixture')['pending'])

    def test_prompt_during_pending_preserves_latest_original_correction(self):
        self.start();before=requirements_identity(self.h.state('native-fixture'));self.h.hook(self.event('PreToolUse'))
        self.h.hook(self.event('UserPromptSubmit',prompt='Latest original correction while a tool is unresolved',turn_id='turn2'))
        state=self.h.state('native-fixture')
        self.assertEqual(state['requests'][-1]['original'],'Latest original correction while a tool is unresolved')
        self.assertNotEqual(before,requirements_identity(state));self.assertIn('one',state['pending'])

    def test_direct_gate_does_not_create_a_self_blocking_pending_check(self):
        self.accepted_infrastructure_candidate()
        event=self.event('PreToolUse','exec_command','node .agents/harness/cli.mjs gate',identity='gate')
        self.assertEqual(self.h.hook(event),{})
        self.assertEqual(self.h.gate('native-fixture')['status'],'COMMIT_ALLOWED')

    def test_document_only_commit_records_its_actual_tree_for_push(self):
        self.start();self.h.hook(self.event('PreToolUse','exec_command','git status'));(self.root/'docs/notes.md').write_text('Recorded user observation, no product completion claim')
        shell(self.root,'add','docs/notes.md');result=self.h.gate('native-fixture')
        self.assertEqual(result['kind'],'documentation');self.assertFalse(result['visualPass'])
        shell(self.root,'commit','-qm','Record observation')
        self.assertEqual(self.h.committed_gate()['tree'],shell(self.root,'rev-parse','HEAD^{tree}').decode().strip())

    def test_document_only_acceptance_cannot_cover_subsequent_product_change(self):
        self.start();self.h.hook(self.event('PreToolUse','exec_command','git status'));(self.root/'docs/notes.md').write_text('Observation');shell(self.root,'add','docs/notes.md');self.h.gate('native-fixture');shell(self.root,'commit','-qm','Observation')
        self.file.write_text('unapproved product change')
        with self.assertRaisesRegex(Rejected,'STALE'):self.h.committed_gate()

    def test_push_acceptance_uses_actual_outgoing_tip(self):
        self.accepted_infrastructure_candidate();self.h.gate('native-fixture');shell(self.root,'commit','-qm','Accepted infrastructure')
        oid=shell(self.root,'rev-parse','HEAD').decode().strip();zero='0'*40
        result=self.h.push_gate(f'refs/heads/feature {oid} refs/heads/feature {zero}\n')
        self.assertEqual(result['updates'][0]['oid'],oid);self.assertFalse(result['visualPass'])

    def test_approved_head_cannot_authorize_an_unverified_other_branch(self):
        self.accepted_infrastructure_candidate();self.h.gate('native-fixture');shell(self.root,'commit','-qm','Accepted infrastructure')
        accepted=shell(self.root,'rev-parse','HEAD').decode().strip()
        shell(self.root,'checkout','-qb','unverified');(self.root/'Sources/menu.cpp').write_text('Unverified product implementation');shell(self.root,'add','Sources/menu.cpp');shell(self.root,'commit','-qm','Unverified tip')
        outgoing=shell(self.root,'rev-parse','HEAD').decode().strip();shell(self.root,'checkout','-q',accepted)
        with self.assertRaisesRegex(Rejected,'Outgoing ref'):self.h.push_gate(f'refs/heads/unverified {outgoing} refs/heads/unverified '+('0'*40)+'\n')

    def test_chain_tampering_detected(self):
        self.start();p=self.h.home/'events.jsonl';p.write_text(p.read_text().replace('native-fixture','forged'))
        with self.assertRaises(Rejected):self.h.events()

    def test_no_runtime_record_is_not_active(self):
        with self.assertRaisesRegex(Rejected,'HOOK_NOT_ACTIVE'):self.h.gate('absent')

    def test_just_hook_call_is_not_game_pass(self):
        self.observed_edit();shell(self.root,'add','Sources/menu.cpp')
        with self.assertRaisesRegex(Rejected,'NOT_RUN'):self.h.gate('native-fixture')

    def test_index_worktree_mismatch_fails(self):
        self.observed_edit();shell(self.root,'add','Sources/menu.cpp');self.file.write_text('another change')
        self.h.hook(self.event('PostToolUse'))
        with self.assertRaises(Rejected):self.h.gate('native-fixture')

    def test_policy_change_invalidates_permission(self):
        self.start();(self.root/'docs/design-and-asset-rules.md').write_text('| D01 | Changed rule |')
        self.assertIn('policy changed',str(self.h.hook(self.event('PreToolUse'))))

    def test_observed_policy_edit_delivers_new_context_without_requiring_a_user_restart(self):
        self.start();policy=self.root/'docs/design-and-asset-rules.md'
        event=self.event('PreToolUse',command='*** Update File: '+str(policy))
        self.assertEqual(self.h.hook(event),{});policy.write_text('| D01 | Latest original requirement |')
        event['hook_event_name']='PostToolUse';result=self.h.hook(event)
        self.assertIn('Latest original requirement',str(result))
        self.assertEqual(self.h.state('native-fixture')['policy'],self.h.policy())
        self.assertEqual(self.h.hook(self.event('PreToolUse',identity='after-policy')), {})

    def test_installer_preserves_other_handlers_in_same_group(self):
        import install_hooks
        path=self.root/'hooks.json';command='python3 '+str(install_hooks.ROOT/'.agents/harness/presentation.py')+' --source project hook'
        path.write_text(json.dumps({'notify':['existing'],'hooks':{'PreToolUse':[{'matcher':'*','hooks':[{'type':'command','command':command},{'type':'command','command':'existing-tool'}]}]}}))
        with patch.object(install_hooks,'ROOT',self.root):
            # Generate the matching command from the isolated root, never user configuration.
            value=json.loads(path.read_text());value['hooks']['PreToolUse'][0]['hooks'][0]['command']='python3 '+str(self.root/'.agents/harness/presentation.py')+' --source project hook';path.write_text(json.dumps(value))
            install_hooks.install(path,'project',retire=True)
        value=json.loads(path.read_text())
        self.assertEqual(value['notify'],['existing'])
        self.assertEqual(value['hooks']['PreToolUse'],[{'matcher':'*','hooks':[{'type':'command','command':'existing-tool'}]}])

    def test_scope_preserves_source_and_forbids_external_link(self):
        external=self.root.parent/'not-a-source';self.file.unlink();self.file.symlink_to(external)
        self.assertIn('leaves HM2 root',str(self.h.hook(self.event('PreToolUse'))))

    def test_unknown_shell_write_and_bypass_rejected(self):
        for cmd in ['python3 -c "open(\'Sources/menu.cpp\',\'w\').write(\'x\')"','sed -i x Sources/menu.cpp','rg --pre=evil x Sources','git commit --no-verify','git -c core.hooksPath=/tmp commit','cat x > Sources/menu.cpp']:
            with self.subTest(cmd=cmd):self.assertEqual(classify(self.root,self.event('PreToolUse','Bash',cmd))[0],'unknown')

    def test_patch_outside_owned_root_rejected(self):
        with self.assertRaises(Rejected):classify(self.root,self.event('PreToolUse',command='*** Update File: ../HeavyMach/file.cpp'))

    def test_arbitrary_acceptance_command_rejected(self):
        with self.assertRaisesRegex(Rejected,'fixed'):self.h.run('native-fixture','harness',['true'])

    def test_uninspected_product_review_rejected(self):
        self.start()
        with self.assertRaisesRegex(Rejected,'actual simulator'):self.h.check_product_review('native-fixture','candidate')

    def test_requirement_set_cannot_be_narrowed(self):
        self.start();self.h.save(self.h.home/'visual-review.json',{'session':'native-fixture','candidate':'candidate','requirements':[{'id':'D01','status':'PASS'}]})
        with self.assertRaisesRegex(Rejected,'narrowed'):self.h.check_product_review('native-fixture','candidate')

    def test_ninety_percent_does_not_waive_mandatory_failure(self):
        self.start();state=self.h.state('native-fixture');state['requirements']={f'D{i}':'Original '+str(i) for i in range(10)};self.h.save(self.h.session_path('native-fixture'),state)
        req=[{'id':f'D{i}','status':'PASS' if i<9 else 'FAIL'} for i in range(10)]
        self.h.save(self.h.home/'visual-review.json',{'session':'native-fixture','candidate':'candidate','originalRequirementsHash':requirements_identity(state),'requirements':req})
        with self.assertRaisesRegex(Rejected,'Mandatory'):self.h.check_product_review('native-fixture','candidate')

    def test_eighty_percent_rejected_before_artifact_claims(self):
        self.start();state=self.h.state('native-fixture');state['requirements']={f'D{i}':'Original '+str(i) for i in range(10)};self.h.save(self.h.session_path('native-fixture'),state)
        req=[{'id':f'D{i}','status':'PASS' if i<8 else 'FAIL','mandatory':False} for i in range(10)]
        self.h.save(self.h.home/'visual-review.json',{'session':'native-fixture','candidate':'candidate','originalRequirementsHash':requirements_identity(state),'requirements':req})
        with self.assertRaisesRegex(Rejected,'below 90'):self.h.check_product_review('native-fixture','candidate')

    def test_stop_does_not_force_continuation(self):self.start();self.assertEqual(self.h.hook(self.event('Stop')), {})

    def test_actual_canonical_rules_are_all_bound(self):
        rules=rule_text(ROOT)
        self.assertEqual(len(rules),50)
        for key in ['D01','A05','H11','P10','V05','V11']:self.assertIn(key,rules)

    def accepted_infrastructure_candidate(self):
        self.file=self.root/'tests/test_presentation_harness.py'
        self.start();self.h.hook(self.event('PreToolUse'))
        self.file.write_text('import unittest\nclass Check(unittest.TestCase):\n def test_fixture(self): self.assertEqual(2+2,4)\n')
        self.h.hook(self.event('PostToolUse'));shell(self.root,'add','tests/test_presentation_harness.py')
        check=self.h.run('native-fixture','harness',UNIT_CHECK);self.assertEqual(check['status'],'PASS')

    def test_same_candidate_real_check_allows_infrastructure_only(self):
        self.accepted_infrastructure_candidate();result=self.h.gate('native-fixture')
        self.assertEqual(result['status'],'COMMIT_ALLOWED');self.assertFalse(result['visualPass'])

    def test_observed_policy_only_change_uses_harness_checks_without_claiming_game_pass(self):
        self.accepted_infrastructure_candidate();policy=self.root/'docs/design-and-asset-rules.md'
        event=self.event('PreToolUse',command='*** Update File: '+str(policy),identity='policy')
        self.h.hook(event);policy.write_text('| D01 | No letterbox, preserve original dimensions |')
        event['hook_event_name']='PostToolUse';self.h.hook(event);shell(self.root,'add','docs/design-and-asset-rules.md')
        self.assertEqual(self.h.run('native-fixture','harness',UNIT_CHECK)['status'],'PASS')
        result=self.h.gate('native-fixture');self.assertEqual(result['kind'],'harness');self.assertFalse(result['visualPass'])

    def test_old_pass_cannot_cover_a_later_observed_change(self):
        self.accepted_infrastructure_candidate()
        self.h.hook(self.event('PreToolUse',identity='second'));self.file.write_text(self.file.read_text()+'# after check\n');self.h.hook(self.event('PostToolUse',identity='second'));shell(self.root,'add','tests/test_presentation_harness.py')
        with self.assertRaisesRegex(Rejected,'NOT_RUN'):self.h.gate('native-fixture')

    def test_changed_log_rejected_at_gate(self):
        self.accepted_infrastructure_candidate();log=next(self.h.home.glob('check-*.log'));log.write_text('forged PASS')
        with self.assertRaisesRegex(Rejected,'NOT_RUN'):self.h.gate('native-fixture')

    def test_git_connected_shell_and_output_option_are_mutations(self):
        for cmd in ['git status && touch Sources/unsafe.cpp','git diff --output=Sources/menu.cpp','git status; rm Sources/menu.cpp']:
            with self.subTest(cmd=cmd):self.assertEqual(classify(self.root,self.event('PreToolUse','Bash',cmd))[0],'unknown')

    def test_unstaged_helper_cannot_be_part_of_a_commit_candidate(self):
        (self.root/'Sources/helper.cpp').write_text('required helper')
        self.accepted_infrastructure_candidate()
        with self.assertRaises(Rejected):self.h.gate('native-fixture')

    def test_mandatory_flag_cannot_be_overridden_in_review(self):
        self.start();state=self.h.state('native-fixture');state['requirements']={f'D{i}':'Original '+str(i) for i in range(10)};self.h.save(self.h.session_path('native-fixture'),state)
        req=[{'id':f'D{i}','status':'PASS' if i<9 else 'FAIL','mandatory':False} for i in range(10)]
        self.h.save(self.h.home/'visual-review.json',{'session':'native-fixture','candidate':'candidate','originalRequirementsHash':requirements_identity(state),'requirements':req})
        with self.assertRaisesRegex(Rejected,'Mandatory'):self.h.check_product_review('native-fixture','candidate')

    def test_next_user_correction_invalidates_old_visual_review(self):
        self.start();state=self.h.state('native-fixture')
        req=[{'id':k,'status':'PASS'} for k in state['requirements']]
        self.h.save(self.h.home/'visual-review.json',{'session':'native-fixture','candidate':'candidate','originalRequirementsHash':requirements_identity(state),'requirements':req})
        self.h.hook(self.event('UserPromptSubmit',prompt='New requirement: phone button positions and HD products'))
        with self.assertRaisesRegex(Rejected,'requirement text mismatch'):self.h.check_product_review('native-fixture','candidate')

class VisualTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='hm2-visual-fixture-',dir=ROOT/'.local/evidence');self.root=Path(self.temp.name)
        (self.root/'.local/prepared/HD/PhoneMenu').mkdir(parents=True);self.capture=self.root/'actual.png';self.resource=self.root/'.local/prepared/HD/PhoneMenu/Base001.png';png(self.capture);png(self.resource)
        draw={'resource':'PhoneMenu/Base001.png','resourceSha256':digest(self.resource.read_bytes()),'role':'backdrop','rect':[0,0,8,4],'sourcePixels':[8,4],'uv':[0,0,1,1]}
        self.case={'id':'base1','base':'Base001','screen':'hub','device':'iphone','png':'actual.png','pngSha256':digest(self.capture.read_bytes()),'drawable':[8,4],'actualInputResults':[{'passed':True}],'requiredContent':[draw],'layoutReference':'phone-original','resourceTraceComplete':True}
        self.report={'origin':'candidate-native-renderer','candidate':'c','binarySha256':'sha','cases':[self.case]}
        self.catalog=SimpleNamespace(records={'PhoneMenu/Base001.png':{'sha256':digest(self.resource.read_bytes()),'pixels':[8,4]}},backdrop=lambda device,base,screen:'PhoneMenu/'+base+'.png',base_refs={})
        self.original=patch('visual.original_catalog',return_value=self.catalog);self.original.start()

    def tearDown(self):self.original.stop();self.temp.cleanup()
    def validate(self):return validate_observations(self.report,self.root)
    def test_real_black_art_not_automatically_a_letterbox(self):self.assertEqual(self.validate()['status'],'PASS')
    def test_narrow_art_and_side_black_void_rejected(self):self.case['requiredContent'][0]['rect']=[2,0,4,4];self.assertEqual(self.validate()['status'],'FAIL')
    def test_nonuniform_scale_rejected(self):self.case['requiredContent'][0]['rect']=[0,0,8,3];self.assertIn('nonuniform',str(self.validate()))
    def test_hd_art_in_phone_menu_rejected(self):
        p=self.resource.parent.parent/'Base001.png';shutil.copyfile(self.resource,p);self.case['requiredContent'][0]['resource']='Base001.png';self.catalog.records['Base001.png']={'sha256':digest(p.read_bytes()),'pixels':[8,4]};self.assertIn('wrong device',str(self.validate()))
    def test_phone_product_art_rejected(self):self.case['requiredContent'][0]['role']='product';self.assertIn('must be HD',str(self.validate()))
    def test_filler_panel_with_buttons_still_rejected(self):self.case['fillerPanels']=[{'buttons':4}];self.assertIn('filler',str(self.validate()))
    def test_capture_or_source_tampering_rejected(self):self.resource.write_bytes(b'not original');self.assertEqual(self.validate()['status'],'FAIL')
    def test_scroll_or_reveal_rejected(self):self.case['revealButtonUsed']=True;self.assertIn('workaround',str(self.validate()))
    def test_missing_actual_input_not_passed(self):self.case['actualInputResults']=[];self.assertIn('input',str(self.validate()))
    def test_png_crc_errors_fail(self):
        b=bytearray(self.capture.read_bytes());b[-1]^=1;self.capture.write_bytes(b)
        with self.assertRaises(ValueError):png_rgb(self.capture)
    def test_black_pixel_detector_only_reports_candidates(self):self.assertEqual(black_edge_bands(self.capture)['vertical'],[8,8])

    def test_black_gpu_output_cannot_hide_behind_correct_geometry(self):
        png(self.resource,pixel=(200,200,200));sha=digest(self.resource.read_bytes());self.catalog.records['PhoneMenu/Base001.png']['sha256']=sha;self.case['requiredContent'][0]['resourceSha256']=sha
        self.assertIn('rendered black',str(self.validate()))

    def test_original_pixel_size_cannot_be_self_reported(self):
        self.case['requiredContent'][0]['sourcePixels']=[16,8]
        self.assertIn('dimensions',str(self.validate()))

    def test_another_bases_approved_background_is_not_valid(self):
        self.case['base']='Base006';self.assertIn('base/screen',str(self.validate()))

    def test_normal_hd_product_tile_uv_is_preserved(self):
        product=self.root/'.local/prepared/HD/Items.png';png(product)
        sha=digest(product.read_bytes());self.catalog.records['Items.png']={'sha256':sha,'pixels':[8,4]}
        self.case['requiredContent'].append({'resource':'Items.png','resourceSha256':sha,'role':'product','rect':[0,0,4,4],'sourcePixels':[8,4],'tile':[2,1,1],'uv':[.5,0,1,1]})
        self.assertEqual(self.validate()['status'],'PASS')

    def test_hd_copy_renamed_to_phone_is_not_original_phone_art(self):
        png(self.resource,pixel=(250,0,0));self.case['requiredContent'][0]['resourceSha256']=digest(self.resource.read_bytes())
        self.assertIn('original edition',str(self.validate()))

if __name__=='__main__':unittest.main()
