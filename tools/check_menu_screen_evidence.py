#!/usr/bin/env python3
"""Independent simulator-picture review: black filler and edge stretching only."""
import argparse, hashlib, json, struct, sys, time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
HOME = ROOT / '.local/evidence/menu-screen-check'
SCOPE = ('HM2SourceView.mm', 'HM2Application.mm', 'Screens.cpp', 'DisplayLayout.cpp', 'DisplayLayout.hpp', 'HM2Renderer.mm', 'check_menu_screen_evidence.py')
INPUTS = ('Sources/Core/UI/Screens.cpp', 'Sources/Core/Presentation/DisplayLayout.cpp', 'Sources/Core/Presentation/DisplayLayout.hpp', 'Sources/Platform/Apple/HM2Renderer.mm')
def sha(data): return hashlib.sha256(data).hexdigest()
def verify(data):
    if data.get('sources') != {p: sha((ROOT / p).read_bytes()) for p in INPUTS}: raise ValueError('Captured source candidate is stale')
    binary = ROOT / 'build/apple/Build/Products/Debug-iphonesimulator/HeavyMach2Source.app/HeavyMach2Source'
    if data.get('binarySHA256') != sha(binary.read_bytes()): raise ValueError('Captured executable is stale')
    if data.get('gameSHA256') != sha(binary.with_name('HeavyMach2Source.debug.dylib').read_bytes()): raise ValueError('Captured game library is stale')
    expected = {(d, 'Base00' + str(b), s) for d in ('iphone', 'ipad') for b in range(1, 7) for s in ('hub', 'shop', 'command')}
    seen, failures = set(), []
    for row in data['rows']:
        key = row['device'], row['base'], row['screen']
        if key not in expected or key in seen: raise ValueError('Unexpected/duplicate screen: ' + str(key))
        seen.add(key); path = Path(row['png'])
        if path.is_symlink() or not path.resolve().is_relative_to((ROOT / '.local/evidence').resolve()): raise ValueError('Invalid capture path')
        png = path.read_bytes(); identity = sha(png)
        if png[:8] != b'\x89PNG\r\n\x1a\n' or min(struct.unpack('>II', png[16:24])) <= 0: raise ValueError('Missing actual PNG')
        if identity != row['sha256']: raise ValueError('Capture changed after collection')
        review = row.get('review', {})
        if review.get('reviewedSHA256') != identity or not review.get('evidence'): failures.append(str(key) + ': NOT_REVIEWED'); continue
        if review.get('blackFiller') is not False: failures.append(str(key) + ': black filler/unverified')
        if review.get('edgeStretch') is not False: failures.append(str(key) + ': edge stretching/unverified')
    if seen != expected: raise ValueError('Missing screens: ' + str(sorted(expected - seen)))
    return {'status': 'FAIL' if failures else 'PASS', 'contexts': len(seen), 'failures': failures}
def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--hook', action='store_true'); parser.add_argument('--input', type=Path, default=HOME / 'latest-input.json'); args = parser.parse_args()
    event = json.load(sys.stdin) if args.hook else {}
    if args.hook:
        payload = json.dumps(event.get('tool_input', {})); cwd = str(event.get('cwd', ''))
        if not (cwd.startswith(str(ROOT)) or str(ROOT) in payload) or not any(x in payload for x in SCOPE): print('{}'); return 0
    try: result = verify(json.loads(args.input.read_text())) if args.input.exists() else {'status': 'NOT_RUN', 'reason': 'Run simulator captures and review black filler/edge stretching'}
    except Exception as error: result = {'status': 'ERROR', 'reason': str(error)}
    fingerprint = sha(args.input.read_bytes()) if args.input.exists() else None
    old = sorted(p for p in HOME.glob('*.json') if p.stem.isdigit())[-1:] if HOME.exists() else []
    previous = json.loads(old[0].read_text()) if old else {}
    result['repeatedFailure'] = result['status'] in ('FAIL', 'ERROR') and previous.get('status') == result['status'] and previous.get('failures') == result.get('failures') and previous.get('reason') == result.get('reason')
    if result['repeatedFailure']: result['nextAction'] = 'Stop this failed approach; inspect original-sized pictures and correct the cause before rerunning.'
    result.update(time_ns=time.time_ns(), input=str(args.input), inputSHA256=fingerprint, hook_event=event.get('hook_event_name'), tool_id=event.get('tool_use_id'))
    HOME.mkdir(parents=True, exist_ok=True); output = HOME / (str(result['time_ns']) + '.json'); output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    if args.input.exists(): output.with_suffix('.input.json').write_bytes(args.input.read_bytes())
    if args.hook:
        print(json.dumps({'hookSpecificOutput': {'hookEventName': event['hook_event_name'], 'additionalContext': 'HM2 screen pictures only: no black filler, no edge stretching. ' + result['status'] + '; ' + str(output) + '. Repair tools remain allowed.'}})); return 0
    print(json.dumps(result, ensure_ascii=False)); return 0 if result['status'] == 'PASS' else 1
if __name__ == '__main__':
    try: code = main()
    except Exception as error:
        print(json.dumps({'systemMessage': 'Screen check ERROR: ' + str(error)} if '--hook' in sys.argv else {'status': 'ERROR', 'reason': str(error)})); code = 0 if '--hook' in sys.argv else 1
    sys.exit(code)
