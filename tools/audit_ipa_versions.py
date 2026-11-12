#!/usr/bin/env python3
"""Read-only local IPA content census. Never writes extracted game data.

EncTbl decoding follows HD 1.0.0 QTable initWithFile: at 0xf45a4:
6-byte marker, Darwin srandom(0xb27f7), byte XOR random()%227.
Output contains counts, hashes, identifiers and differences, not full tables.
"""
import argparse
import collections
import ctypes
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def sha(data):
    return hashlib.sha256(data).hexdigest()

def decode(data):
    if not data.startswith(b'EncTbl'):
        return data
    if sys.platform != 'darwin':
        raise RuntimeError('This audit requires Darwin libc random; do not substitute Linux random')
    libc = ctypes.CDLL('/usr/lib/libSystem.B.dylib')
    libc.srandom.argtypes = [ctypes.c_uint]
    libc.random.restype = ctypes.c_long
    libc.srandom(0xb27f7)
    return bytes(byte ^ (libc.random() % 227) for byte in data[6:])

def table(data, strict_keys=False):
    text = decode(data).decode('utf-8-sig')
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    comments = [line for line in lines if line.startswith('//')]
    lines = [line for line in lines if not line.startswith('//')]
    fields = [cell.strip() for cell in lines[0].split('\t')]
    rows = []
    short_rows = []
    for line in lines[1:]:
        cells = [cell.strip() for cell in line.split('\t')]
        # The original tables have optional trailing tabs; no data is discarded.
        if len(cells) > len(fields) and any(cells[len(fields):]):
            raise ValueError('Unexpected extra nonempty table columns')
        if len(cells) < len(fields):
            short_rows.append({'key': cells[0], 'expected_columns':len(fields), 'actual_columns':len(cells)})
        rows.append(dict(zip(fields, cells)))
    key = fields[0]
    ids = [row[key] for row in rows]
    if strict_keys and len(ids) != len(set(ids)):
        raise ValueError('Duplicate record keys need separate runtime resolution')
    return rows, {'rows':len(rows), 'unique_keys':len(set(ids)),
                  'comment_lines_excluded':len(comments), 'key_column':key, 'short_rows':short_rows,
                  'encoded_sha256':sha(data), 'decoded_sha256':sha(decode(data))}

def parts_records(data):
    count = struct.unpack_from('<I', data, 12)[0]
    offset = 16
    names = []
    for _ in range(count):
        size, children, payload = struct.unpack_from('<III',data,offset)
        if size < 16 or offset + size > len(data):
            raise ValueError('MCB chunk boundary mismatch')
        name = data[offset+12:offset+size].split(b'\0',1)[0].decode('ascii')
        names.append(name)
        offset += size
    if offset != len(data) or len(names) != len(set(names)):
        raise ValueError('MCB incomplete consumption or duplicate names')
    return names

def audit(manifest):
    versions = []
    for index, original in enumerate(manifest['files']):
        path = Path(manifest['source_root']) / original['relative_path']
        digest = sha(path.read_bytes())
        if digest != original['sha256']:
            raise ValueError(f'Original hash mismatch: {path.name}')
        with zipfile.ZipFile(path) as archive:
            # Read each table, including mission language files; no extraction.
            tables = {}
            all_table_metadata = {}
            for member in archive.namelist():
                if member.endswith('.tble'):
                    rows, metadata = table(archive.read(member), Path(member).name in {'ItemInfo.tble','WeaponInfo.tble','MissionList.tble','PartsList.tble'})
                    name = Path(member).name
                    if name in tables:
                        raise ValueError('Ambiguous basename')
                    tables[name] = rows
                    all_table_metadata[name] = metadata
            def read_named(name):
                matches = [n for n in archive.namelist() if n.endswith('/'+name)]
                if len(matches) != 1:
                    raise ValueError(f'Expected unique member: {name}')
                return archive.read(matches[0])
            item_rows = tables['ItemInfo.tble']
            weapon_rows = tables['WeaponInfo.tble']
            missions = tables['MissionList.tble']
            mission_key = all_table_metadata['MissionList.tble']['key_column']
            mission_ids = sorted(row[mission_key] for row in missions)
            language_ids = {lang: sorted({re.fullmatch(r'M(\d{6})_'+lang+r'\.tble', n).group(1)
                for n in tables if re.fullmatch(r'M(\d{6})_'+lang+r'\.tble', n)}) for lang in ['kr','us']}
            mcb = read_named('PartsInfo.mcb')
            parts = parts_records(mcb)
            if 'PartsList.tble' in tables:
                if set(parts) != {r['PartsName'] for r in tables['PartsList.tble']}:
                    raise ValueError('PartsList vs MCB differs')
            types = dict(sorted(collections.Counter(row['Type'] for row in item_rows).items()))
            equipment = [r for r in item_rows if r['Type'].startswith('Pt')]
            weapons = [r for r in equipment if r['Type']=='PtWpn']
            v = {'source_index':index,'file':original['relative_path'],'ipa_sha256':digest,
                 'decoded_tble_count':len(tables),'tables':all_table_metadata,
                 'item_type_counts':types,'equipment_items':len(equipment),
                 'weapon_equipment_items':len(weapons),
                 'nonweapon_equipment_items':len(equipment)-len(weapons),
                 'weapon_definition_rows':len(weapon_rows),
                 'weapon_definition_types':dict(sorted(collections.Counter(r['Type'] for r in weapon_rows).items())),
                 'parts_mcb_records_including_dummy_enemy_objects':len(parts),'parts_mcb_sha256':sha(mcb),
                 'parts_list_matches_mcb':True if 'PartsList.tble' in tables else None,
                 'mission_count':len(mission_ids),'mission_ids':mission_ids,
                 'mission_language_file_counts':{k:len(v) for k,v in language_ids.items()},
                 'mission_missing_language_files':{k:sorted(set(mission_ids)-{'M'+s for s in v}) for k,v in language_ids.items()},
                 'weapon_item_missing_definition':[r['Code'] for r in weapons if r['strParam'] not in {w['Parts'] for w in weapon_rows}],
                 'equipment_unresolved_part_reference':[r['Code'] for r in equipment if r['strParam'] not in ['none','-'] and r['strParam'] not in set(parts)],
                 'equipment_identifiers':[{'code':r['Code'],'type':r['Type'],'part':r['strParam'],'name':r.get('Name','')} for r in equipment]}
            versions.append(v)
        if sha(path.read_bytes()) != digest:
            raise ValueError('Original changed during read')
    for v in versions:
        baseline = versions[3]
        codes = {r['code'] for r in baseline['equipment_identifiers']}
        v['equipment_additions_vs_phone201'] = [r for r in v['equipment_identifiers'] if r['code'] not in codes]
    return {'scope':'All six locally inventoried HM2 IPAs; static declared content, not proven in-game obtainability',
            'count_rules':{'equipment':'ItemInfo active unique Code, Type starts Pt; armor included',
                           'weapons':'ItemInfo active unique Code, Type PtWpn',
                           'parts_mcb':'All named top-level chunks; includes dummy, enemy and object parts',
                           'missions':'MissionList unique active IDs; Korean/English files not added together'},
            'versions':versions}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,default=ROOT/'.local/evidence/ipa-version-audit/content-audit.json')
    args = parser.parse_args()
    result = audit(json.loads((ROOT/'docs/source-provenance.json').read_text()))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    for v in result['versions']:
        print(v['source_index'], 'weapons',v['weapon_equipment_items'],'other equipment',v['nonweapon_equipment_items'],
              'all equipment',v['equipment_items'],'MCB parts',v['parts_mcb_records_including_dummy_enemy_objects'],
              'missions',v['mission_count'],'decoded tables',v['decoded_tble_count'])
