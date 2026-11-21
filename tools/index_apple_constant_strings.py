#!/usr/bin/env python3
"""Read original 32-bit Mach-O CFString constants into ignored local evidence only."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / '.local'):
        parser.error('Original constants must stay in this repository .local space')
    blob = args.binary.read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    if digest != args.sha256:
        raise ValueError('Original SHA-256 differs from selected artifact')
    if len(blob) < 28 or struct.unpack_from('<I', blob)[0] != 0xfeedface:
        raise ValueError('Expected thin little-endian 32-bit Mach-O')
    sections = []
    cursor = 28
    command_end = cursor + struct.unpack_from('<I', blob, 20)[0]
    if command_end > len(blob):
        raise ValueError('Truncated load commands')
    for _ in range(struct.unpack_from('<I', blob, 16)[0]):
        if cursor + 8 > command_end:
            raise ValueError('Truncated command header')
        command, size = struct.unpack_from('<II', blob, cursor)
        if size < 8 or cursor + size > command_end:
            raise ValueError('Invalid command bounds')
        if command == 1:
            if size < 56:
                raise ValueError('Truncated segment')
            count = struct.unpack_from('<I', blob, cursor + 48)[0]
            if 56 + count * 68 > size:
                raise ValueError('Truncated section array')
            for i in range(count):
                off = cursor + 56 + i * 68
                name, segment, address, length, fileoff = struct.unpack_from('<16s16sIII', blob, off)
                sections.append({'name': name.split(b'\0')[0].decode('ascii'),
                                 'address': address, 'length': length, 'fileoff': fileoff})
        cursor += size

    def file_offset(address, length):
        for section in sections:
            if section['address'] <= address and address + length <= section['address'] + section['length']:
                off = section['fileoff'] + address - section['address']
                if off + length <= len(blob):
                    return off
        raise ValueError('Constant points outside file-backed sections')

    constants = [s for s in sections if s['name'] == '__cfstring']
    if len(constants) != 1 or constants[0]['length'] % 16:
        raise ValueError('Expected exactly one aligned CFString section')
    section = constants[0]
    rows = []
    for address in range(section['address'], section['address'] + section['length'], 16):
        off = file_offset(address, 16)
        isa, flags, pointer, length = struct.unpack_from('<IIII', blob, off)
        if flags not in {0x7c8, 0x7d0}:
            raise ValueError(f'Unsupported constant string flags: {flags:#x}')
        encoding = 'utf-16-le' if flags == 0x7d0 else 'utf-8'
        size = length * (2 if flags == 0x7d0 else 1)
        start = file_offset(pointer, size)
        text = blob[start:start + size].decode(encoding)
        rows.append({'address': hex(address), 'file_offset': off, 'flags': hex(flags),
                     'data_address': hex(pointer), 'length': length, 'text': text})
    if hashlib.sha256(args.binary.read_bytes()).hexdigest() != digest:
        raise ValueError('Original changed during indexing')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({'source_sha256': digest, 'source_bytes': len(blob),
                                 'scope': 'local constant strings, not runtime observation',
                                 'section': section, 'constants': rows}, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'constants': len(rows), 'source_sha256': digest, 'output': str(output)}))


if __name__ == '__main__':
    main()
