#!/usr/bin/env python3
"""Package byte-identical original phone menu art beside the shared HD data.
Offline preparation only; the game runtime remains C++ and consumes no IPA.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile
ROOT=Path(__file__).resolve().parents[1]
PHONE_SHA='23ff9c74c7d890cc6f28952b4845f6c616bae4bb7207a393f80ed9ed76062d32'
MENU={'Btn_InvenPage.png','Btn_Disarm.png','Btn_ArrangeItem.png','Btn_ShopLeft.png','Btn_ShopRight.png','Btn_ShopBuy.png','Btn_ShopSell.png','TradeItemBG.png','Common_Btn_BUY.png','Common_Btn_SELL.png','Common_Btn_ACCEPT.png','Common_Btn_CANCEL.png','Common_Btn_DEC.png','Common_Btn_INC.png','NumTile01.png','NumTile03.png','NumTile04.png','BaseTextBG.png','Btn_ShopExit.png','Btn_Mission.png','AcceptedMark.png','ItemDesc.png','ShopCategory.png','MissionNotice.png','Main_BG.png','Main_Copyright.png','Main_Episode.png','Main_TapToStart.png','Main_Title.png','Main_TitleBig.png','StageClearBG.png','SelSlot_BG.jpg','CmdCenterBG.jpg','ShopBG.jpg','MaintenanceBG.jpg','Btn_CmdCenter.png','Btn_Shop.png','Btn_Maintenance.png','Btn_ExitBase.png','Btn_GoRuinField.png','Btn_GoSkyway.png','Btn_GoWasteland.png','Btn_GoSeaside.png'}
def original_source():
    provenance=json.loads((ROOT/'docs/source-provenance.json').read_text())
    entry=next(x for x in provenance['files'] if x['sha256']==PHONE_SHA)
    source=Path(provenance['source_root'])/entry['relative_path']
    with source.open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest()!=PHONE_SHA:raise ValueError('Original phone IPA hash mismatch')
    return source

def approved_name(name):
    return name in MENU or re.fullmatch(r'Base00[1-6](?:\.(?:jpg|tble)|_(?:CmdCenter|Shop|Maintenance)\.png)',name)

def verify_prepared(output):
    if output.is_symlink() or not output.is_dir():raise ValueError('PhoneMenu must be a real directory')
    manifest=json.loads((output/'manifest.json').read_text())
    if manifest.get('ipa_sha256')!=PHONE_SHA or manifest.get('profile')!='phone201-menu-only' or manifest.get('original_executable_included') is not False:raise ValueError('Unapproved menu manifest')
    records=manifest['files'];names={record['name'] for record in records}
    if len(names)!=len(records) or not MENU.issubset(names):raise ValueError('Missing or duplicate menu records')
    if {p.name for p in output.iterdir()}!=names|{'manifest.json'}:raise ValueError('Unlisted menu resource')
    with zipfile.ZipFile(original_source()) as archive:
        originals={Path(name).name:archive.read(name) for name in archive.namelist() if len(Path(name).parts)==3 and Path(name).parts[:2]==('Payload','HeavyMach2.app') and approved_name(Path(name).name)}
    if set(originals)!=names:raise ValueError('Menu files differ from the approved original')
    for record in records:
        name=record['name']
        if not approved_name(name):raise ValueError('Unapproved menu resource')
        if record['sha256']!=hashlib.sha256(originals[name]).hexdigest() or record['bytes']!=len(originals[name]):raise ValueError('Menu manifest differs from original '+name)
        path=output/name
        if path.is_symlink() or not path.is_file() or path.stat().st_size!=record['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest()!=record['sha256']:raise ValueError('Changed menu resource '+name)

def prepare(output):
    output=output.resolve()
    if not output.is_relative_to(ROOT/'.local') or output.name!='PhoneMenu':raise ValueError('Use an HM2-local PhoneMenu directory')
    source=original_source()
    output.mkdir(parents=True,exist_ok=True);records=[];wanted=set(MENU)
    with zipfile.ZipFile(source) as archive:
        for name in archive.namelist():
            path=Path(name)
            if path.parts[:2]!=('Payload','HeavyMach2.app') or len(path.parts)!=3:continue
            if not(path.name in MENU or re.fullmatch(r'Base00[1-6](?:\.(?:jpg|tble)|_(?:CmdCenter|Shop|Maintenance)\.png)',path.name)):continue
            data=archive.read(name);target=output/path.name
            if target.is_symlink():raise ValueError('No linked menu resources')
            digest=hashlib.sha256(data).hexdigest()
            if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest()!=digest:raise ValueError('Refusing changed menu asset '+path.name)
            if not target.exists():target.write_bytes(data)
            records.append({'name':path.name,'bytes':len(data),'sha256':digest});wanted.discard(path.name)
    if wanted:raise ValueError('Missing approved menu resources: '+str(sorted(wanted)))
    names={x['name'] for x in records}
    if {x.name for x in output.iterdir()}-names-{'manifest.json'}:raise ValueError('Unexpected resource in PhoneMenu')
    manifest={'profile':'phone201-menu-only','ipa_sha256':PHONE_SHA,'original_executable_included':False,'unchanged_resource_bytes':True,'files':sorted(records,key=lambda x:x['name'])}
    (output/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n');verify_prepared(output);print(json.dumps({'output':str(output),'files':len(records),'bytes':sum(x['bytes'] for x in records)}))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=ROOT/'.local/prepared/HD/PhoneMenu');prepare(parser.parse_args().output)
