#!/usr/bin/env python3
"""Index existing local REA evidence without copying original function bodies.

Analysis tooling only. Reads the two shipped binaries and retained REA exports.
Never writes to those artifacts and never builds or runs the game.
"""
import argparse
import hashlib
import json
import pathlib
import re
import struct


def read_json(path):
    return json.loads(path.read_text())


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_followup_reviews(root, out):
    """Recheck referenced local exports; never copy their function bodies."""
    results = []
    manifests = {}
    for kind, relative in [('runtime', 'universal-runtime-native-review-20261113'),
                           ('ui', 'universal-original-ui-review-20261113')]:
        path = root / '.local/evidence' / relative / 'findings.json'
        if not path.is_file():
            results.append({'kind': kind, 'status': 'input-not-present'})
            continue
        findings = read_json(path)
        if kind == 'runtime':
            entries = [e for f in findings['findings'] for e in f['evidence']]
            references = [(e['address'], f) for e in entries for f in e['files']]
        else:
            entries = findings['evidence']
            references = [(e['address'], f) for e in entries for f in e['exports']]
        verified = []
        for address, ref in references:
            f = pathlib.Path(ref['path'])
            if not f.is_file() or digest(f) != ref['sha256']:
                raise ValueError(f'{kind}: changed or missing cached Evidence: {f}')
            if f.suffix == '.asm' and not f.read_text().lstrip().startswith(address + ':'):
                raise ValueError(f'{kind}: unexpected assembly address: {f}')
            manifest_path = f.parent.parent / 'export-manifest.json'
            if manifest_path not in manifests:
                manifests[manifest_path] = {m['address']: m for m in read_json(manifest_path)['methods']}
            primitive = 'assembly' if f.suffix == '.asm' else 'pseudo'
            canonical = manifests[manifest_path][address]['exports'][primitive]
            if canonical['evidence_id'] != ref['evidence_id']:
                raise ValueError(f'{kind}: Evidence ID differs from canonical manifest: {f}')
            if 'sha256' in canonical and canonical['sha256'] != ref['sha256']:
                raise ValueError(f'{kind}: SHA differs from canonical manifest: {f}')
            verified.append({'address': address, 'path': str(f), 'sha256': ref['sha256'], 'evidence_id': ref['evidence_id']})
        result = {'kind': kind, 'status': 'cached-references-reverified',
                  'findings_path': str(path.relative_to(root)), 'findings_sha256': digest(path),
                  'finding_count': len(findings['findings']), 'function_references': len(entries),
                  'unique_functions': len({(r['address'], str(pathlib.Path(r['path']).parent)) for r in verified}),
                  'file_references': len(verified), 'unique_files': len({r['path'] for r in verified}),
                  'new_runtime_execution': False, 'all_hashes_match': True}
        verification = path.with_name('verification.json')
        if kind == 'runtime' and verification.exists():
            prior = read_json(verification)
            if prior['finding_sha256'] != result['findings_sha256']:
                raise ValueError('Runtime findings do not match the prior review verification')
            result['prior_verification_sha256'] = digest(verification)
        write_json(out / (kind + '-followup-reference-index.json'), verified)
        results.append(result)
    return results


def procedure_inventory(snapshot):
    records = snapshot['evidence_bundle']['records']
    evidence = next(r for r in records if r['operation'] == 'list_procedures')
    rows = []
    for p in evidence['normalized_result']:
        kind = 'external' if p['procedure']['external'] else 'thunk' if p['procedure']['thunk'] else 'body'
        rows.append({'address': p['address'], 'name': p['value'], 'kind': kind})
    return rows, evidence['evidence_id']


def macho_index(blob, bodies):
    """Parse ARMv7 Mach-O metadata and validate length-prefixed RTTI names."""
    if struct.unpack_from('<I', blob)[0] != 0xfeedface:
        raise ValueError('Expected little-endian 32-bit Mach-O')
    sections = []
    cursor = 28
    for _ in range(struct.unpack_from('<I', blob, 16)[0]):
        cmd, size = struct.unpack_from('<II', blob, cursor)
        if cmd == 1:
            for i in range(struct.unpack_from('<I', blob, cursor + 48)[0]):
                off = cursor + 56 + 68 * i
                sect, seg, addr, length, fileoff = struct.unpack_from('<16s16sIII', blob, off)
                sections.append({'section': sect.rstrip(b'\0').decode(), 'segment': seg.rstrip(b'\0').decode(),
                                 'address': addr, 'size': length, 'file_offset': fileoff})
        cursor += size

    def section(seg, name):
        return next(s for s in sections if s['segment'] == seg and s['section'] == name)

    def offset(va):
        for s in sections:
            if s['address'] <= va < s['address'] + s['size'] and s['file_offset']:
                return va - s['address'] + s['file_offset']
        raise ValueError(hex(va))

    def u32(va):
        return struct.unpack_from('<I', blob, offset(va))[0]

    def cstring(va):
        off = offset(va)
        return blob[off:blob.index(b'\0', off)].decode('utf-8', errors='replace')

    def method_list(va):
        if not va:
            return []
        stride, count = u32(va) & 0xffff, u32(va + 4)
        if stride < 12 or count > 10000:
            raise ValueError('Unexpected Objective-C method list')
        return [{'selector': cstring(u32(va + 8 + i * stride)),
                 'type_encoding': cstring(u32(va + 12 + i * stride)),
                 'address': hex(u32(va + 16 + i * stride) & ~1)} for i in range(count)]

    cstr, const = section('__TEXT', '__cstring'), section('__DATA', '__const')
    strings = blob[cstr['file_offset']:cstr['file_offset'] + cstr['size']]
    classes = []
    cursor = 0
    for chunk in strings.split(b'\0'):
        match = re.fullmatch(rb'(\d+)([A-Za-z_]\w*)', chunk)
        if match and int(match[1]) == len(match[2]):
            name_address = cstr['address'] + cursor
            refs = [va for va in range(const['address'], const['address'] + const['size'] - 3, 4)
                    if u32(va) == name_address]
            for ref in refs:
                ti = ref - 4
                tables = []
                for va in range(const['address'] + 4, const['address'] + const['size'] - 3, 4):
                    if u32(va) == ti and u32(va - 4) == 0:
                        slots = []
                        pos = va + 4
                        while pos < const['address'] + const['size']:
                            raw = u32(pos)
                            target = raw & ~1
                            text = section('__TEXT', '__text')
                            if not text['address'] <= target < text['address'] + text['size']:
                                break
                            slots.append({'slot': len(slots), 'address': hex(target), 'raw': hex(raw),
                                          'in_ghidra_body_inventory': target in bodies})
                            pos += 4
                        tables.append({'address': hex(va + 4), 'slots': slots})
                classes.append({'name': match[2].decode(), 'name_address': hex(name_address),
                                'typeinfo_address': hex(ti), 'possible_base_pointer': hex(u32(ti + 8)),
                                'vtables': tables})
        cursor += len(chunk) + 1
    types = {int(c['typeinfo_address'], 16): c['name'] for c in classes}
    for c in classes:
        c['base_type_if_known'] = types.get(int(c['possible_base_pointer'], 16))
    objc = section('__DATA', '__objc_classlist')
    pointers = [hex(u32(a)) for a in range(objc['address'], objc['address'] + objc['size'], 4)]
    objc_classes = []
    for pointer in pointers:
        va = int(pointer, 16)
        ro, meta_ro = u32(va + 16) & ~3, u32(u32(va) + 16) & ~3
        objc_classes.append({'name': cstring(u32(ro + 16)), 'address': pointer,
                             'instance_size': u32(ro + 8),
                             'methods': method_list(u32(ro + 20)),
                             'class_methods': method_list(u32(meta_ro + 20))})
    return {'sections': sections, 'rtti_classes': classes, 'objc_classlist_pointers': pointers, 'objc_classes': objc_classes,
            'limitations': ['RTTI and vtables are metadata, not proof of method equivalence.',
                           'Only length-validated simple Itanium RTTI names are recovered.',
                           'Recognized single-inheritance base pointer is a candidate; no complete C++ member/type layout exists.',
                           'Valid tiny virtual stubs can be missing from the Ghidra procedure inventory.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analysis-root', type=pathlib.Path, default=pathlib.Path('/Users/sokol/Documents/Codex/HeavyMach2/Analysis'))
    parser.add_argument('--reference-root', type=pathlib.Path, default=pathlib.Path('/Users/sokol/Documents/Codex/HeavyMach2/ReferenceBuilds/inspection'))
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    out = root / '.local/evidence/universal-native-audit-20261113'
    out.mkdir(parents=True, exist_ok=True)
    old = root / '.local/evidence/reference-comparison-20261110'
    native = args.analysis_root / 'Phone201/universal-audit-20261113/rea-evidence-bundle.json'
    bundle = read_json(native)
    hd = args.analysis_root / 'HD103'
    layouts = read_json(hd / 'objc-layouts.json')
    summary = {'schema': 1, 'date': '2026-11-13', 'scope': 'static metadata and selected native paths; not whole-game semantic equivalence', 'targets': {}}
    inventories = {}
    for name, snapname, binary, expected in [
        ('phone201', 'phone-analysis-snapshot.json', args.reference_root / 'phone/armv7', '0fffcfc09cb797c04c70aa0a7b6d748adf7888d7b6583954b9a71fe1f448512a'),
        ('hd103', 'hd-analysis-snapshot-expanded.json', args.reference_root / 'hd/original-executable', '5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090')]:
        inventory, eid = procedure_inventory(read_json(old / snapname))
        inventories[name] = inventory
        actual = digest(binary)
        if actual != expected:
            raise ValueError(f'{name}: original hash changed')
        counts = {kind: sum(r['kind'] == kind for r in inventory) for kind in ['body', 'external', 'thunk']}
        (out / (name + '-procedures.tsv')).write_text('address\tkind\tname\n' + ''.join(f"{r['address']}\t{r['kind']}\t{r['name']}\n" for r in inventory))
        summary['targets'][name] = {'sha256': actual, 'bytes': binary.stat().st_size, 'counts': counts,
                                   'total': len(inventory), 'list_evidence_id': eid,
                                   'anonymous_bodies': sum(r['kind'] == 'body' and r['name'].startswith('FUN_') for r in inventory)}
    bodies = {int(r['address'], 16) for r in inventories['phone201'] if r['kind'] == 'body'}
    phone = macho_index((args.reference_root / 'phone/armv7').read_bytes(), bodies)
    write_json(out / 'phone-rtti-vtables.json', phone)
    write_json(out / 'hd-objc-inventory.json', {'classes': [{'name': c['name'], 'superclass': c.get('superclass'), 'methods': [{'selector': m['selector'], 'address': m['address'], 'type_encoding': m['type_encoding'], 'class_method': cm} for cm, key in [(False, 'methods'), (True, 'class_methods')] for m in c[key]], 'ivars': c['ivars']} for c in layouts['classes']]})
    summary['targets']['phone201'].update(rtti_classes=len(phone['rtti_classes']), objc_classlist_entries=len(phone['objc_classlist_pointers']))
    summary['targets']['phone201']['objc_methods'] = sum(len(c['methods']) + len(c['class_methods']) for c in phone['objc_classes'])
    summary['targets']['hd103'].update(objc_classes=len(layouts['classes']), objc_methods=sum(len(c['methods']) + len(c['class_methods']) for c in layouts['classes']), objc_ivars=sum(len(c['ivars']) for c in layouts['classes']))
    exports = []
    for folder in ['method-evidence', 'native-evidence']:
        manifest = read_json(hd / folder / 'export-manifest.json')
        counts = {'selected': manifest['selected_methods'], 'completed': manifest['completed_methods'], 'pseudo_files': 0, 'asm_files': 0}
        for m in manifest['methods']:
            for kind, suffix in [('pseudo', '.pseudo.c'), ('assembly', '.asm')]:
                path = hd / folder / 'functions' / (m['address'] + suffix)
                if not path.is_file():
                    raise FileNotFoundError(path)
                counts['pseudo_files' if kind == 'pseudo' else 'asm_files'] += 1
                exports.append({'address': m['address'], 'owner': m['owner'], 'selector': m['selector'], 'kind': kind, 'sha256': digest(path), 'evidence_id': m['exports'][kind]['evidence_id']})
        summary['targets']['hd103'][folder] = counts
    write_json(out / 'hd-export-index.json', exports)
    records = []
    for r in bundle['records']:
        if r['operation'] in ['procedure_pseudo_code', 'procedure_assembly']:
            requested = r['parameters'].get('procedure')
            # Ghidra may fallback from a non-procedure address to a file offset.
            rejected = requested == '0x23ba4'
            records.append({'operation': r['operation'], 'requested': requested, 'evidence_id': r['evidence_id'], 'accepted_address_binding': not rejected,
                            'reason': 'No procedure at 0x23ba4; numeric fallback resolves unrelated file offset. Raw stub is bx lr.' if rejected else None})
    write_json(out / 'phone-query-index.json', records)
    unique = {int(r['requested'], 16) for r in records if r['operation'] == 'procedure_pseudo_code' and r['accepted_address_binding'] and r['requested'].startswith('0x')}
    summary['targets']['phone201']['new_evidence_records'] = len(bundle['records'])
    summary['targets']['phone201']['initial_targeted_pseudo_body_count'] = len(unique & bodies)
    summary['targets']['phone201']['initial_not_targeted_pseudo_body_count'] = len(bodies - unique)
    summary['targets']['phone201']['rejected_numeric_address_bindings'] = sum(not r['accepted_address_binding'] for r in records)
    stub_slots = {(s['address']) for c in phone['rtti_classes'] for t in c['vtables'] for s in t['slots'] if not s['in_ghidra_body_inventory']}
    summary['targets']['phone201']['vtable_entrypoints_outside_body_inventory'] = sorted(stub_slots)
    full_bundle_path = native.with_name('rea-full-export-bundle.json')
    if full_bundle_path.exists():
        full_bundle = read_json(full_bundle_path)
        exports_root = native.parent / 'full-export'
        functions = exports_root / 'functions'
        functions.mkdir(parents=True, exist_ok=True)
        body_by_address = {r['address']: r for r in inventories['phone201'] if r['kind'] == 'body'}
        observed = {}
        owners = {}
        for c in phone['objc_classes']:
            for m in c['methods'] + c['class_methods']:
                owners.setdefault(m['address'], []).append('objc:' + c['name'] + ':' + m['selector'])
        for c in phone['rtti_classes']:
            for table in c['vtables']:
                for slot in table['slots']:
                    owners.setdefault(slot['address'], []).append('rtti:' + c['name'] + ':slot' + str(slot['slot']))
        for r in full_bundle['records']:
            operation = r['operation']
            addr = r['parameters'].get('procedure')
            if operation not in ['procedure_pseudo_code', 'procedure_assembly'] or addr not in body_by_address:
                continue
            if r['subject']['digest']['sha256'] != summary['targets']['phone201']['sha256']:
                raise ValueError('Full export Evidence belongs to another target')
            observed[(addr, operation)] = r
        rows = []
        for addr, row in body_by_address.items():
            entry = {'address': addr, 'name': row['name'], 'owners_from_metadata': sorted(set(owners.get(addr, []))), 'exports': {}}
            entry['classification'] = 'objc_metadata' if any(o.startswith('objc:') for o in entry['owners_from_metadata']) else 'cpp_rtti_virtual_entry' if entry['owners_from_metadata'] else 'unclassified'
            for operation, kind, suffix in [('procedure_pseudo_code', 'pseudo', '.pseudo.c'), ('procedure_assembly', 'assembly', '.asm')]:
                r = observed.get((addr, operation))
                if r is None:
                    entry['exports'][kind] = {'status': 'missing'}
                    continue
                text = r['normalized_result']
                if not isinstance(text, str) or not text.strip():
                    entry['exports'][kind] = {'status': 'empty', 'evidence_id': r['evidence_id']}
                    continue
                if kind == 'assembly' and not text.lstrip().startswith(addr + ':'):
                    raise ValueError('Assembly does not start at requested body: ' + addr)
                path = functions / (addr + suffix)
                path.write_text(text)
                entry['exports'][kind] = {'status': 'exported', 'evidence_id': r['evidence_id'], 'sha256': digest(path), 'bytes': path.stat().st_size}
            rows.append(entry)
        manifest = {'source_sha256': summary['targets']['phone201']['sha256'], 'body_denominator': len(body_by_address),
                    'selected_methods': len(rows), 'completed_methods': sum(all(e['status'] == 'exported' for e in r['exports'].values()) for r in rows), 'methods': rows,
                    'classification_note': 'Owner metadata only; unclassified bodies are not assigned to a library or game subsystem by name guessing.'}
        write_json(exports_root / 'export-manifest.json', manifest)
        write_json(out / 'phone-full-export-index.json', manifest)
        summary['targets']['phone201']['full_export'] = {
            'bundle_sha256': digest(full_bundle_path), 'evidence_records': len(full_bundle['records']),
            'selected': len(rows), 'completed': manifest['completed_methods'],
            'pseudo_exported': sum(r['exports']['pseudo']['status'] == 'exported' for r in rows),
            'assembly_exported': sum(r['exports']['assembly']['status'] == 'exported' for r in rows),
            'classification': {kind: sum(r['classification'] == kind for r in rows) for kind in ['objc_metadata', 'cpp_rtti_virtual_entry', 'unclassified']},
            'raw_output_directory': str(exports_root),
            'note': 'Full export is evidence availability, not semantic equivalence or completed function correspondence.'}
        writes, reads, callers, instructions = [], [], [], set()
        for r in rows:
            asm = (functions / (r['address'] + '.asm')).read_text().splitlines()
            for line in asm:
                address = line.split(':', 1)[0]
                instructions.add(address)
                item = {'function': r['address'], 'instruction_address': address,
                        'assembly_sha256': r['exports']['assembly']['sha256'],
                        'evidence_id': r['exports']['assembly']['evidence_id']}
                if re.search(r'\bstrb(?:\.w)?\s+[^\n]*\[[^\n]*,#0x5\]', line):
                    writes.append(item)
                if re.search(r'\bldrb(?:\.w)?\s+[^\n]*\[[^\n]*,#0x5\]', line):
                    reads.append(item)
                if re.search(r'\bbl(?:\.w)? 0x0004bcf8\b', line):
                    callers.append(item)
        activation = {'scope': 'Exact immediate-offset byte access and direct BL only across cached1077 assembly bodies.',
                      'literal_offset5_writes': writes, 'literal_offset5_reads': reads,
                      'show_setter_direct_calls': callers,
                      'counts': {'write_instructions': len(writes), 'write_functions': len({r['function'] for r in writes}),
                                 'read_instructions': len(reads), 'read_functions': len({r['function'] for r in reads}),
                                 'setter_direct_calls': len(callers), 'setter_direct_caller_functions': len({r['function'] for r in callers})},
                      'vtable_targets_without_cached_instruction': sorted(a for a in stub_slots if a not in instructions),
                      'limits': ['Literal offset5 is not proof of object identity at every access.',
                                 'Register-offset accesses, aliases, wider stores, indirect calls and excluded blocks are not resolved by this scan.',
                                 'The150 setter call targets/conditions are not all semantically classified. No global absence of screen gating is inferred.']}
        write_json(out / 'phone-activation-flag-index.json', activation)
        summary['phone_activation_scan'] = {k: v for k, v in activation.items() if k not in ['literal_offset5_writes', 'literal_offset5_reads', 'show_setter_direct_calls']}
    summary['limits'] = ['Body inventories include platform glue and helpers, not only gameplay.', 'HD exports cover every indexed body but export existence does not mean all functions were semantically compared.', 'Phone queried-body count means retained decompilation, not fully understood or semantically matched.', 'Full class/member/type correspondence is not established.', 'Runtime behavior was not executed in this audit.']
    summary['followup_reviews'] = verify_followup_reviews(root, out)
    report = root / 'docs/standard-hd-logic-differences-20261113.md'
    contract = root / 'docs/evidence/standard-hd-logic-differences-20261113.json'
    if report.exists() and contract.exists():
        rows = [line for line in report.read_text().splitlines() if line.startswith('| ') and
                ('역할 대응 확인.' in line or '| 후보.' in line or '| 미대응.' in line)]
        counts = {key: sum(marker in line for line in rows) for key, marker in
                  [('role-confirmed', '역할 대응 확인.'), ('candidate', '| 후보.'), ('unmapped', '| 미대응.')]}
        stated = read_json(contract)['correspondence_scope']
        if stated['rows'] != len(rows) or any(stated['counts'].get(k, 0) != v for k, v in counts.items()):
            raise ValueError('Native report and JSON correspondence counts disagree')
        summary['path_correspondence'] = {'rows': len(rows), 'counts': counts,
                                          'note': 'Selected broad paths only; zero unmapped rows does not mean zero semantic unknowns.'}
    write_json(out / 'coverage-summary.json', summary)
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
