#!/usr/bin/env python3
"""Apply authored campaign data to a separate copy of prepared HD resources."""

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
import zipfile

from audit_ipa_versions import decode, table
from prepare_hd_assets import prepare as prepare_original_hd
from prepare_phone_menu_assets import prepare as prepare_original_phone

ROOT = Path(__file__).resolve().parents[1]
AUTHORING = ROOT / 'Resources/Campaign'
OUTPUT = ROOT / '.local/prepared/Campaign/HD'


@dataclass
class Chunk:
    kind: int
    data: bytes
    children: list
    declared: int = 0

    def encode(self):
        children = b''.join(child.encode() for child in self.children)
        size = self.declared or (16 + len(self.data) + len(children))
        return struct.pack('<III', size, self.kind, len(self.data)) + self.data + struct.pack('<I', len(self.children)) + children


def parse_chunk(data):
    def read(at):
        declared, kind, length = struct.unpack_from('<III', data, at)
        start = at + 12
        end = start + length
        count, = struct.unpack_from('<I', data, end)
        at = end + 4
        children = []
        for _ in range(count):
            child, at = read(at)
            children.append(child)
        return Chunk(kind, data[start:end], children, declared), at

    root, end = read(0)
    if end != len(data) or root.kind != 0 or root.data:
        raise ValueError('Invalid original MCB root')
    return root


def fixed_text(text, width):
    data = text.encode('utf-8')
    if len(data) >= width or b'\0' in data:
        raise ValueError(f'Invalid {width}-byte field: {text}')
    return data.ljust(width, b'\0')


def text(data):
    return data.split(b'\0', 1)[0].decode('utf-8')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def append_rows(path, rows):
    content = decode(path.read_bytes()).decode('utf-8-sig')
    addition = '\n'.join('\t'.join(map(str, row)) for row in rows)
    path.write_text(content.rstrip('\r\n') + '\n' + addition + '\n', encoding='utf-8')


def coordinate(x, y):
    if type(x) is not int or type(y) is not int or abs(x) > 490 or abs(y) > 490:
        raise ValueError(f'Point outside campaign world: {(x, y)}')
    return x, y


def polygon(points):
    if len(points) < 3 or len(set(map(tuple, points))) != len(points):
        raise ValueError('Invalid campaign polygon')
    area = sum(x1*y2-x2*y1 for (x1,y1),(x2,y2) in zip(points, points[1:]+points[:1]))
    if area >= 0:
        raise ValueError('Campaign obstacles require clockwise vertices')
    payload = struct.pack('<h', len(points))
    for x, y in points:
        payload += struct.pack('<ff', *coordinate(x, y))
    return Chunk(3, payload, [])


def make_gate(number, x, y, target, target_number):
    if not 0 <= number <= 127 or not 0 <= target_number <= 127:
        raise ValueError('Campaign gate number exceeds the base exit limit')
    return Chunk(2, struct.pack('<hhh16shB', number, *coordinate(x,y),
                               fixed_text(target, 16), target_number, 0), [])


def authored_map(name, record, known_machines):
    if not re.fullmatch(r'[A-Za-z0-9_]+', name):
        raise ValueError(f'Invalid campaign map ID: {name}')
    payload = fixed_text(name, 24) + fixed_text(record['name'], 24) + bytes(8)
    children = [polygon(points) for points in record.get('collision', [])]
    for model, x, y in record.get('enemies', []):
        if model not in known_machines:
            raise ValueError(f'Unresolved campaign enemy: {model}')
        children.append(Chunk(4, struct.pack('<h16shh', 0, fixed_text(model, 16), *coordinate(x,y)), []))
    return Chunk(1, payload, children)


def extend_maps(directory, definition):
    path = directory / 'MapInfo.mcb'
    root = parse_chunk(path.read_bytes())
    maps = {text(record.data[:24]): record for record in root.children if record.kind == 1}
    known_machines = {r['MachName'] for r in table((directory/'MachList.tble').read_bytes())[0]}
    for name, record in definition['maps'].items():
        if name in maps:
            raise ValueError(f'Duplicate campaign map: {name}')
        node = authored_map(name, record, known_machines)
        maps[name] = node
        root.children.append(node)
    initial_numbers = {
        (name, struct.unpack_from('<h', gate.data)[0])
        for name, node in maps.items() for gate in node.children if gate.kind == 2
    }
    added = set()
    for point_a, point_b in definition['links']:
        for (name, number, x, y), (destination, other_number, _, _) in ((point_a, point_b), (point_b, point_a)):
            key = (name, number)
            if key in initial_numbers or key in added or destination == name:
                raise ValueError(f'Duplicate or invalid campaign gate: {key}')
            node = maps[name]
            gate = make_gate(number, x, y, destination, other_number)
            # The normal base UI resolves exits by raw child index.
            if name.startswith('Base') and number != len(node.children):
                raise ValueError(f'Base campaign exits must be consecutive: {key}')
            node.children.append(gate)
            node.declared = 0
            added.add(key)
    root.declared = 0
    path.write_bytes(root.encode())
    records = [
        (name, name + '.png', record['name']) for name, record in definition['maps'].items()
    ]
    append_rows(directory/'MapList.tble', records)
    return len(added)


def import_snow_images(directory, definition):
    source = json.loads((ROOT/'docs/source-provenance.json').read_text(encoding='utf-8'))
    candidates = [entry for entry in source['files']
                  if entry['sha256'] == '31ebed81043fe9a7c5ab25a7adaf13eaa3629f885bce18c3793f5d78f571cc56']
    if len(candidates) != 1:
        raise ValueError('Expected the documented HD 1.0.0 archive')
    entry = candidates[0]
    original = Path(source['source_root'])/entry['relative_path']
    with original.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != entry['sha256']:
            raise ValueError('HD 1.0.0 archive has a different SHA-256')
    expected = json.loads((ROOT/'docs/evidence/white-mountain-feasibility-20261119.json').read_text())['heavyMach2']['snowInHD100']
    copied = {}
    with zipfile.ZipFile(original) as archive:
        for name, record in definition['maps'].items():
            if not record.get('image'):
                continue
            image = record['image']
            members = [n for n in archive.namelist() if n.startswith('Payload/') and Path(n).name == image]
            if len(members) != 1:
                raise ValueError(f'Missing unambiguous HD 1.0.0 terrain: {image}')
            pixels = archive.read(members[0])
            if digest(pixels) != expected[image]['sha256']:
                raise ValueError(f'HD 1.0.0 terrain fingerprint differs: {image}')
            target = directory/(name+'.png')
            if target.exists():
                raise ValueError(f'Campaign artwork already exists: {target.name}')
            target.write_bytes(pixels)
            copied[target.name] = dict(original=image, sha256=digest(pixels))
    return copied


def base_exits(directory):
    for location, y, x in ((directory, -190, -233), (directory/'PhoneMenu', -72, -148)):
        table_path = location/'Base004.tble'
        if not table_path.exists():
            raise ValueError('Prepare the original Phone menu and HD resources first')
        rows = []
        for index, destination in enumerate(('SnowPass', 'SnowBasin')):
            button = 'Btn_Go'+destination+'.png'
            shutil.copyfile(AUTHORING/button, location/button)
            offset = 68 if location == directory else 42
            rows.append(('Button', button, x, y-offset*index, 1, 5+index))
        append_rows(table_path, rows)


def prepare(output=OUTPUT):
    output = output.resolve()
    prepared = ROOT/'.local/prepared'
    campaign_root = prepared/'Campaign'
    if not output.is_relative_to(campaign_root) or output == campaign_root:
        raise ValueError('Prepare expansion resources under .local/prepared/Campaign')
    definition = json.loads((AUTHORING/'white-mountain.json').read_text())
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='campaign-', dir=output.parent) as temporary:
        pending = Path(temporary)/'HD'
        prepare_original_hd(pending)
        prepare_original_phone(pending/'PhoneMenu')
        source_manifest = digest((pending/'manifest.json').read_bytes())
        gate_count = extend_maps(pending, definition)
        images = import_snow_images(pending, definition)
        phone_manifest = pending/'PhoneMenu/manifest.json'
        phone_source = digest(phone_manifest.read_bytes())
        base_exits(pending)
        phone_manifest.unlink()
        patched = ['MapInfo.mcb', 'MapList.tble', 'Base004.tble', 'PhoneMenu/Base004.tble',
                   'Btn_GoSnowPass.png', 'Btn_GoSnowBasin.png',
                   'PhoneMenu/Btn_GoSnowPass.png', 'PhoneMenu/Btn_GoSnowBasin.png']
        manifest = dict(profile='white-mountain', originalManifestSha256=source_manifest,
                        originalPhoneMenuManifestSha256=phone_source,
                        newMaps=list(definition['maps']), newGates=gate_count,
                        hd100Terrain=images, patches={name:digest((pending/name).read_bytes()) for name in patched})
        (pending/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        previous = Path(temporary)/'previous'
        if output.exists():
            output.rename(previous)
        try:
            pending.rename(output)
        except OSError:
            if previous.exists():
                previous.rename(output)
            raise
    print(json.dumps(dict(output=str(output), **manifest)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=OUTPUT)
    prepare(**vars(parser.parse_args()))
