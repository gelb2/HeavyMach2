#!/usr/bin/env python3
"""Recheck local HM2 native follow-up metadata without running the game.

Uses retained REA Evidence and local original identities. Does not install a
provider, mutate a binary, rebuild an app, or copy function bodies into the repo.
It verifies evidence availability and addresses, not semantic equivalence.
"""
import argparse
import collections
import hashlib
import json
import pathlib
import struct

from audit_universal_native_coverage import digest, macho_index, read_json, write_json


PHONE_SHA = '0fffcfc09cb797c04c70aa0a7b6d748adf7888d7b6583954b9a71fe1f448512a'
HD_SHA = '5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090'
PHONE_PATHS = '4ef90 4ee34 4bebc 4e07c 5a418 57124 5af50 5a1dc 4e0e4 64e8 7004 40fc 3ce8 4bcfc 4c5e4 10f08 10230 54fb4 168a8 13d28 efe8 23d70 8c64 8584 8910 550d0 5504c 58038 57ac8 57bb8 58738 a240 aab8 a6ec a9ac ad54 a3f0 4c2cc e4f8 bd7c 10ce4 106e4 10a58'.split()
HD_PATHS = '11cac 37870 37958 387c0 388b8 38968 3a39c 2730 12698 10f84 36d98 37750 3689c 36f04 38e50 cfa4 d6d8 6464 71d0 10ddc 10dac 5fd0 5f94 5f84 1689c 19d28 ff18 10a00 7e20 80ac 982c a998 94f8 8f04 8e28 110d8 c7d0 37218'.split()


def sections(blob):
    if struct.unpack_from('<I', blob)[0] != 0xfeedface:
        raise ValueError('Expected ARMv7 little-endian Mach-O')
    result, cursor = [], 28
    for _ in range(struct.unpack_from('<I', blob, 16)[0]):
        cmd, size = struct.unpack_from('<II', blob, cursor)
        if cmd == 1:
            for i in range(struct.unpack_from('<I', blob, cursor + 48)[0]):
                name, seg, va, length, off = struct.unpack_from('<16s16sIII', blob, cursor + 56 + 68 * i)
                result.append({'section': name.rstrip(b'\0').decode(), 'segment': seg.rstrip(b'\0').decode(),
                               'address': va, 'size': length, 'file_offset': off})
        cursor += size
    return result


def selector_index(blob):
    sec = sections(blob)
    result = []
    for name in ['setCollideCheck:', 'collideCheck', '_collideCheck']:
        offset = blob.find(b'\0' + name.encode() + b'\0') + 1
        if not offset:
            raise ValueError('Missing selector/ivar name: ' + name)
        source = next(s for s in sec if s['file_offset'] <= offset < s['file_offset'] + s['size'])
        va = offset - source['file_offset'] + source['address']
        refs, cursor = [], 0
        while True:
            at = blob.find(struct.pack('<I', va), cursor)
            if at < 0:
                break
            cursor = at + 1
            where = next((s for s in sec if s['file_offset'] <= at < s['file_offset'] + s['size']), None)
            ref = {'file_offset': at, 'address': hex(at - where['file_offset'] + where['address']) if where else None,
                   'section': where['section'] if where else None}
            if name == 'setCollideCheck:' and where and where['section'] == '__objc_const':
                raw = struct.unpack_from('<I', blob, at + 8)[0]
                ref.update(method_implementation_raw=hex(raw), method_implementation_thumb_masked=hex(raw & ~1),
                           matches_known_setter=(raw & ~1) == 0x5f94)
            refs.append(ref)
        result.append({'name': name, 'address': hex(va), 'section': source['section'], 'pointer_occurrences': refs})
    return {'source_sha256': HD_SHA, 'names': result,
            'limits': ['Raw32 pointer occurrence search is not a complete dynamic/alias graph.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analysis-root', type=pathlib.Path, default=pathlib.Path('/Users/sokol/Documents/Codex/HeavyMach2/Analysis'))
    parser.add_argument('--reference-root', type=pathlib.Path, default=pathlib.Path('/Users/sokol/Documents/Codex/HeavyMach2/ReferenceBuilds/inspection'))
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    out = root / '.local/evidence/iphone-universal-native-followup-20261113'
    out.mkdir(parents=True, exist_ok=True)
    phone_bin, hd_bin = args.reference_root / 'phone/armv7', args.reference_root / 'hd/original-executable'
    if digest(phone_bin) != PHONE_SHA or digest(hd_bin) != HD_SHA:
        raise ValueError('Original identity changed')
    base = args.analysis_root / 'Phone201/universal-audit-20261113/full-export'
    supplement = args.analysis_root / 'Phone201/iphone-universal-followup-20261113'
    bundle_path = supplement / 'rea-evidence-final.json'
    bundle = read_json(bundle_path)
    procedures = next(r['normalized_result'] for r in bundle['records'] if r['operation'] == 'list_procedures')
    bodies = {int(r['address'], 16) for r in procedures if not r['procedure']['external'] and not r['procedure']['thunk']}
    phone_meta = macho_index(phone_bin.read_bytes(), bodies)
    missing = sorted({s['address'] for c in phone_meta['rtti_classes'] for t in c['vtables'] for s in t['slots'] if not s['in_ghidra_body_inventory']}, key=lambda a: int(a, 16))
    exact = {r['normalized_result']['address']: r for r in bundle['records'] if r['operation'] == 'inspect_native_instruction'}
    entries = []
    for addr in missing:
        e = exact.get(addr)
        if e is None or e['subject']['digest']['sha256'] != PHONE_SHA:
            raise ValueError('Missing exact-entry Evidence: ' + addr)
        n = e['normalized_result']
        entries.append({'address': addr, 'status': n['status'], 'entry_evidence_id': e['evidence_id'],
                        'entry_flow': n['flow']['kind'], 'length': n['length'],
                        'owners': [{'class': c['name'], 'slot': s['slot'], 'vtable': t['address'], 'raw_pointer': s['raw']} for c in phone_meta['rtti_classes'] for t in c['vtables'] for s in t['slots'] if s['address'] == addr]})
    walk_path = out / 'vtable-entry-walk-index.json'
    walks = []
    for addr in missing:
        queue, seen, observations, unknowns, tails = [addr], set(), [], [], []
        while queue and len(seen) < 1024:
            at = queue.pop(0)
            if at in seen:
                continue
            seen.add(at)
            e = exact.get(at)
            if e is None:
                unknowns.append(at + ':not-in-retained-exact-instruction-cache')
                continue
            if e['subject']['digest']['sha256'] != PHONE_SHA:
                raise ValueError('Instruction target identity changed')
            n = e['normalized_result']
            observations.append({'address': at, 'evidence_id': e['evidence_id'], 'status': n['status'], 'length': n['length'], 'flow': n['flow']})
            if n['status'] != 'decoded':
                unknowns.append(at + ':' + n['status'])
                continue
            flow = n['flow']
            if flow['kind'] in ['fallthrough', 'call'] or flow['conditional']:
                queue.append(hex(int(at, 16) + n['length']))
            if flow['kind'] == 'jump':
                for dest in flow['direct_destinations']:
                    dest = hex(int(dest, 16))
                    if int(dest, 16) in bodies or not int(addr, 16) <= int(dest, 16) < int(addr, 16) + 0x1000:
                        tails.append({'from': at, 'to': dest})
                    else:
                        queue.append(dest)
        walks.append({'entry': addr, 'observed_instruction_count': len(observations), 'instructions': observations,
                      'frontier': queue, 'unknowns': unknowns, 'tail_edges': tails})
    write_json(walk_path, {'source_sha256': PHONE_SHA,
                         'scope': 'Replayed bounded graph over retained exact instruction Evidence; not a certified complete CFG.',
                         'entries': walks})
    write_json(out / 'vtable-entry-verification.json', entries)
    phone_manifest = read_json(base / 'export-manifest.json')
    if phone_manifest['source_sha256'] != PHONE_SHA:
        raise ValueError('Phone manifest target changed')
    manifests = {'phone': {m['address']: (base, m) for m in phone_manifest['methods']}, 'hd': {}}
    for folder in ['method-evidence', 'native-evidence']:
        directory = args.analysis_root / 'HD103' / folder
        manifest = read_json(directory / 'export-manifest.json')
        if manifest['source_sha256'] != HD_SHA:
            raise ValueError('HD manifest target changed')
        manifests['hd'].update({m['address']: (directory, m) for m in manifest['methods']})
    refs = []
    for domain, addresses in [('phone', PHONE_PATHS), ('hd', HD_PATHS)]:
        for short in addresses:
            addr = '0x' + short
            directory, m = manifests[domain][addr]
            exports = []
            for kind, suffix in [('pseudo', '.pseudo.c'), ('assembly', '.asm')]:
                f = directory / 'functions' / (addr + suffix)
                h = digest(f)
                exp = m['exports'][kind]
                if 'sha256' in exp and exp['sha256'] != h:
                    raise ValueError('Original cached export changed: ' + str(f))
                if kind == 'assembly' and not f.read_text().lstrip().startswith(addr + ':'):
                    raise ValueError('Unexpected assembly start')
                exports.append({'kind': kind, 'path': str(f), 'sha256': h, 'bytes': f.stat().st_size, 'evidence_id': exp['evidence_id']})
            refs.append({'domain': domain, 'address': addr, 'source_sha256': PHONE_SHA if domain == 'phone' else HD_SHA, 'exports': exports})
    write_json(out / 'cached-function-reference-index.json', refs)
    spans = read_json(supplement / 'llvm-span-manifest.json')
    for span in spans:
        f = supplement / (span['start'] + '.llvm-thumb-span.asm')
        if span['source_sha256'] != PHONE_SHA or digest(f) != span['sha256']:
            raise ValueError('LLVM supplement identity changed')
    supplemental_bytes = []
    phone_bytes, phone_sections = phone_bin.read_bytes(), sections(phone_bin.read_bytes())
    for e in bundle['records']:
        if e['operation'] != 'read_bytes' or e['parameters'].get('address') not in ['0xb318', '0xb988']:
            continue
        n = e['normalized_result']
        address = int(n['address'], 16)
        sec = next(s for s in phone_sections if s['address'] <= address < s['address'] + s['size'])
        off = address - sec['address'] + sec['file_offset']
        payload = bytes.fromhex(n['bytes_hex'])
        if payload != phone_bytes[off:off + len(payload)] or not n['complete']:
            raise ValueError('REA missing-entry bytes differ from original')
        supplemental_bytes.append({'address': n['address'], 'evidence_id': e['evidence_id'], 'length': len(payload),
                                   'bytes_sha256': hashlib.sha256(payload).hexdigest(), 'matches_original': True})
    hd_matches = {'isInScreen': [], '_glScalef': [], 'setCollideCheck': []}
    for addr, (directory, m) in manifests['hd'].items():
        text = (directory / 'functions' / (addr + '.pseudo.c')).read_text()
        for query in hd_matches:
            if query in text:
                hd_matches[query].append({'address': addr, 'owner': m['owner'], 'selector': m['selector']})
    write_json(out / 'hd-collidecheck-selector-static-index.json', selector_index(hd_bin.read_bytes()))
    summary = {'schema': 1, 'date': '2026-11-13', 'originals': {'phone': PHONE_SHA, 'hd': HD_SHA},
               'cached_functions_reverified': {'phone': len(PHONE_PATHS), 'hd': len(HD_PATHS), 'file_references': len(refs) * 2},
               'ghidra_body_inventory': {'phone': len(bodies), 'hd': len(manifests['hd'])},
               'supplemental_vtable_entries': len(entries), 'entry_status_counts': dict(collections.Counter(e['status'] for e in entries)),
               'exact_instruction_records_in_final_bundle': len(exact), 'final_bundle_records_including_reused': len(bundle['records']),
               'final_bundle_sha256': digest(bundle_path), 'llvm_bounded_spans': spans,
               'undecodable_entry_read_bytes': supplemental_bytes,
               'hd_complete_cached_body_text_queries': hd_matches,
               'runtime_or_app_execution': False, 'source_or_original_mutation': False,
               'limits': ['Code/text query and file verification are not all-body semantic equivalence.',
                          'LLVM spans do not certify complete CFG/type recovery.',
                          'Exact entry verification does not resolve all indirect/alias effects.']}
    summary['instruction_walk_index_sha256'] = digest(walk_path)
    write_json(out / 'verification.json', summary)
    print(json.dumps({'files_verified': len(refs) * 2, 'vtable_entries': len(entries), 'entry_status': summary['entry_status_counts'],
                      'exact_instruction_records': len(exact), 'runtime_execution': False}, ensure_ascii=False))


if __name__ == '__main__':
    main()
