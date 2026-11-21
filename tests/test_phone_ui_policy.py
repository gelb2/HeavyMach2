"""Parser regression fixtures. These do not certify a game screenshot."""
import base64
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('phone_policy',ROOT/'tools/check_phone_ui_policy.py')
POLICY=importlib.util.module_from_spec(spec);spec.loader.exec_module(POLICY)
class PhonePolicyTests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);self.root=Path(temp.name).resolve()
        stack=contextlib.ExitStack();self.addCleanup(stack.close);stack.enter_context(patch.object(POLICY,'ROOT',self.root));stack.enter_context(patch.object(POLICY,'HOME',self.root/'.local/evidence/check'))
        app=self.root/'build/apple/Build/Products/Debug-iphonesimulator/HeavyMach2Source.app';(app/'HD').mkdir(parents=True)
        for name in ('HeavyMach2Source','HeavyMach2Source.debug.dylib','HD/Main_BG.png'):(app/name).write_bytes(name.encode())
        stack.enter_context(patch.object(POLICY,'HD_TITLE_SHA',POLICY.sha(app/'HD/Main_BG.png')))
        for name in POLICY.INPUTS:
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(name)
        evidence=self.root/'.local/evidence';evidence.mkdir(parents=True);png=evidence/'fixture.png'
        png.write_bytes(base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aX1sAAAAASUVORK5CYII='))
        self.data={'sources':{p:POLICY.sha(self.root/p) for p in POLICY.INPUTS},'binarySHA256':POLICY.sha(app/'HeavyMach2Source'),'gameSHA256':POLICY.sha(app/'HeavyMach2Source.debug.dylib'),'rows':[]}
        names=['hub-Base00'+str(i) for i in range(1,7)]+['shop','shop-detail','shop-last-page','shop-trade','command','command-detail','command-last-page','title','logo','stage-name']
        for name in names:
            self.data['rows'].append({'case':name,'png':str(png),'sha256':POLICY.sha(png),'canvas':[1,1],'reviewedSHA256':POLICY.sha(png),'reviewEvidence':'parser fixture only','scrollBefore':[0,0],'scrollAfterHorizontal':[0,0],'scrollAfterVertical':[0,0],'controlsUnmovedAfterDrag':True,'originalPhonePolicyMatches':True,'buttons':[{'id':1,'bounds':[0,0,1,1]}],'image':'Main_BG.png','artworkSHA256':POLICY.HD_TITLE_SHA,'artworkBounds':[-.25,-1,.75*2,2],'clip':[0,0,1,1],'entersAtRightEdge':True,'exitsAtLeftEdge':True})
        last=next(x for x in self.data['rows'] if x['case']=='shop-last-page');last['buttons']=[{'id':i,'bounds':[0,0,1,1]} for i in (2223,6024)]
    def test_complete_parser_fixture(self):self.assertEqual(POLICY.verify(self.data)['status'],'PASS')
    def test_horizontal_or_vertical_pan_fails(self):
        for field in ('scrollBefore','scrollAfterHorizontal','scrollAfterVertical'):
            data=copy.deepcopy(self.data);data['rows'][0][field]=[0,1];self.assertEqual(POLICY.verify(data)['status'],'FAIL')
    def test_missing_or_unreviewed_case_never_passes(self):
        data=copy.deepcopy(self.data);data['rows'].pop()
        with self.assertRaisesRegex(ValueError,'Missing cases'):POLICY.verify(data)
        self.data['rows'][0].pop('reviewedSHA256');self.assertEqual(POLICY.verify(self.data)['status'],'FAIL')
    def test_clipped_button_fails(self):
        self.data['rows'][0]['buttons'][0]['bounds']=[-1,0,1,1];self.assertEqual(POLICY.verify(self.data)['status'],'FAIL')
    def test_stale_candidate_fails(self):
        (self.root/POLICY.INPUTS[0]).write_text('changed')
        with self.assertRaisesRegex(ValueError,'Stale candidate'):POLICY.verify(self.data)
    def test_invalid_picture_fails(self):
        self.data['rows'][0]['canvas']=[2,1]
        with self.assertRaisesRegex(ValueError,'picture extent'):POLICY.verify(self.data)
    def test_harness_cannot_block_tools_or_repair(self):
        for command in ('git stash push','git commit -m backup','delete check_phone_ui_policy.py','edit Sources/Core/UI/Screens.cpp'):
            event={'cwd':str(self.root),'hook_event_name':'PreToolUse','tool_input':{'cmd':command}}
            with patch.object(sys,'argv',['check','--hook']),patch.object(sys,'stdin',io.StringIO(json.dumps(event))),contextlib.redirect_stdout(io.StringIO()) as out:self.assertEqual(POLICY.main(),0)
            self.assertNotIn('permissionDecision',out.getvalue())
if __name__=='__main__':unittest.main()
