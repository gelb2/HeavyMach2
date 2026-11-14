#!/usr/bin/env python3
"""Attach Xcode Instruments to an already running, explicitly selected HM2 app.

Does not build, install, launch, drive input, change saves, or change app timing.
Use after functional implementation/validation is ready. Raw trace stays local.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--device', required=True)
parser.add_argument('--kind', choices=('source', 'validation'), required=True)
parser.add_argument('--template', choices=('Time Profiler', 'Allocations', 'Game Memory',
                    'Game Performance', 'Animation Hitches', 'Metal System Trace',
                    'Power Profiler'), required=True)
parser.add_argument('--seconds', type=int, default=120)
parser.add_argument('--binary', type=Path, required=True, help='The exact installed app binary, retained locally')
parser.add_argument('--dsym', type=Path, help='Matching dSYM bundle; UUID mismatch stops before recording')
parser.add_argument('--output', type=Path, required=True, help='New directory under this repository .local/')
args = parser.parse_args()
if not 1 <= args.seconds <= 1800:
    parser.error('--seconds must be 1..1800')
out = args.output.resolve()
if not out.is_relative_to(ROOT / '.local'):
    parser.error('Raw recording must remain in the HM2 .local directory')
binary = args.binary.resolve()
if not binary.is_file():
    parser.error('Exact app binary is missing')
expected = 'HeavyMach2Source' if args.kind == 'source' else 'HeavyMach2Validation'
if binary.name != expected:
    parser.error('Binary name does not match the requested normal/validation target')

def read_command(command):
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT).strip()

def uuids(text):
    return set(re.findall(r'UUID: ([0-9A-Fa-f-]+)', text))

binary_uuid = read_command(['xcrun', 'dwarfdump', '--uuid', str(binary)])
dsym_uuid = None
if args.dsym:
    dsym_uuid = read_command(['xcrun', 'dwarfdump', '--uuid', str(args.dsym.resolve())])
    if not uuids(binary_uuid) or uuids(binary_uuid) != uuids(dsym_uuid):
        parser.error('Binary and dSYM UUIDs differ')
out.mkdir(parents=True, exist_ok=False)
command = ['xcrun', 'xctrace', 'record', '--device', args.device,
           '--template', args.template, '--attach', expected,
           '--time-limit', f'{args.seconds}s', '--output', str(out / 'recording.trace')]
if args.template == 'Allocations':
    command += ['--instrument', 'Leaks']
manifest = {
    'kind': args.kind, 'target_process_requested': expected, 'device_requested': args.device,
    'template': args.template, 'seconds_requested': args.seconds,
    'source_head': read_command(['git', '-C', str(ROOT), 'rev-parse', 'HEAD']),
    'binary': str(binary), 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
    'binary_uuid': binary_uuid, 'dsym_uuid': dsym_uuid,
    'xcode': read_command(['xcodebuild', '-version']), 'record_command': command,
    'scope': 'Instruments attach only; actual target/sample coverage must be checked in trace export',
    'started_unix': time.time(),
}
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Attaching {args.template} to {expected}; output remains local: {out}', flush=True)
try:
    with (out / 'record.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                timeout=args.seconds + 180)
    manifest['record_exit'] = result.returncode
except subprocess.TimeoutExpired:
    manifest['record_timeout'] = True
    manifest['record_exit'] = None
manifest['finished_unix'] = time.time()
trace = out / 'recording.trace'
if trace.exists():
    with (out / 'export.log').open('w') as log:
        export = subprocess.run(['xcrun', 'xctrace', 'export', '--input', str(trace), '--toc',
                                 '--output', str(out / 'toc.xml')],
                                stdout=log, stderr=subprocess.STDOUT, timeout=90)
    manifest['export_exit'] = export.returncode
    manifest['toc_created'] = (out / 'toc.xml').is_file()
else:
    manifest['toc_created'] = False
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
success = manifest['record_exit'] == 0 and manifest.get('export_exit') == 0 and manifest['toc_created']
print(json.dumps({'record_exit': manifest['record_exit'], 'export_exit': manifest.get('export_exit'),
                  'trace_saved': trace.exists(), 'toc_created': manifest['toc_created'],
                  'target_and_sample_analysis_complete': False}), flush=True)
sys.exit(0 if success else 1)
