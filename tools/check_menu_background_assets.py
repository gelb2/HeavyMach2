#!/usr/bin/env python3
"""Read-only original-background check; native hooks never deny a tool."""
import argparse, hashlib, json, sys, time, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
HOME = ROOT / '.local/evidence/menu-asset-check'
PACKAGE = ROOT / 'build/apple/Build/Products/Debug-iphonesimulator/HeavyMach2Source.app'
SCOPE = ('GameSession.cpp', 'MenuArtwork.hpp', 'Screens.cpp', 'DisplayLayout.hpp', 'HM2SourceView.mm', 'HM2Application.mm', 'prepare_phone_menu_assets.py', 'prepare_hd_assets.py', 'check_menu_background_assets.py')
SHA = {'iphone': '23ff9c74c7d890cc6f28952b4845f6c616bae4bb7207a393f80ed9ed76062d32', 'ipad': '7eeb36590b3c19a28741e21671379029d9023864ceb241a5b8560eef045112d2'}
INPUTS = ('Sources/Core/Presentation/MenuArtwork.hpp', 'Sources/Core/Presentation/DisplayLayout.hpp', 'Sources/Core/UI/Screens.cpp', 'Sources/Platform/Apple/HM2SourceView.mm', 'Sources/Platform/Apple/HM2Application.mm')
def sha(data): return hashlib.sha256(data).hexdigest()
def verify(data, package=PACKAGE):
    if data.get('sources') != {p: sha((ROOT / p).read_bytes()) for p in INPUTS}: raise ValueError('Captured source candidate is stale')
    provenance = json.loads((ROOT / 'docs/source-provenance.json').read_text())
    census = json.loads((ROOT / 'docs/evidence/hub-artwork-census-20261115.json').read_text())
    expected = {(p, b, s) for p in SHA for b in census['baseIds'] for s in ('hub', 'shop', 'command')}
    originals = {}
    for device, identity in SHA.items():
        entry = next(x for x in provenance['files'] if x['sha256'] == identity)
        path = Path(provenance['source_root']) / entry['relative_path']
        if sha(path.read_bytes()) != identity: raise ValueError('Original IPA changed: ' + device)
        with zipfile.ZipFile(path) as archive:
            originals[device] = {Path(n).name: sha(archive.read(n)) for n in archive.namelist() if len(Path(n).parts) == 3 and Path(n).suffix.lower() in ('.jpg', '.png')}
    seen, failures = set(), []
    if data['binarySHA256'] != sha((package / 'HeavyMach2Source').read_bytes()): raise ValueError('Captured executable is stale')
    if data.get('gameSHA256') != sha((package / 'HeavyMach2Source.debug.dylib').read_bytes()): raise ValueError('Captured game library is stale')
    for row in data['rows']:
        key = row['device'], row['base'], row['screen']
        if key not in expected or key in seen: raise ValueError('Unexpected/duplicate context: ' + str(key))
        seen.add(key); device, base, screen = key
        hub = next(x['background'] for x in census['entries'] if x['profile'] == device and x['base'] == base)
        wanted = hub if screen == 'hub' else (('PhoneMenu/' if device == 'iphone' else '') + {'shop': 'ShopBG', 'command': 'CmdCenterBG'}[screen] + ('.jpg' if device == 'iphone' else '.png'))
        if wanted not in [x['image'] for x in row['backgrounds']]: failures.append(str(key) + ': missing ' + wanted)
        for image in row['backgrounds']:
            name = image['image']; path = package / 'HD' / name
            if not path.resolve().is_relative_to((package / 'HD').resolve()) or path.is_symlink(): raise ValueError('Invalid background path')
            actual = sha(path.read_bytes()); original = originals[device].get(Path(name).name)
            if actual != image['sha256'] or actual != original or name.startswith('PhoneMenu/') != (device == 'iphone'): failures.append(str(key) + ': wrong source ' + name)
    if seen != expected: raise ValueError('Missing contexts: ' + str(sorted(expected - seen)))
    return {'status': 'FAIL' if failures else 'PASS', 'contexts': len(seen), 'failures': failures}
def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--hook', action='store_true'); parser.add_argument('--input', type=Path, default=HOME / 'latest-input.json'); args = parser.parse_args()
    event = json.load(sys.stdin) if args.hook else {}
    if args.hook:
        payload = json.dumps(event.get('tool_input', {})); cwd = str(event.get('cwd', ''))
        if not (cwd.startswith(str(ROOT)) or str(ROOT) in payload) or not any(x in payload for x in SCOPE): print('{}'); return 0
    try: result = verify(json.loads(args.input.read_text())) if args.input.exists() else {'status': 'NOT_RUN', 'reason': 'Capture current device backgrounds before acceptance'}
    except Exception as error: result = {'status': 'ERROR', 'reason': str(error)}
    fingerprint = sha(args.input.read_bytes()) if args.input.exists() else None
    old = sorted(p for p in HOME.glob('*.json') if p.stem.isdigit())[-1:] if HOME.exists() else []
    previous = json.loads(old[0].read_text()) if old else {}
    result['repeatedFailure'] = result['status'] in ('FAIL', 'ERROR') and previous.get('status') == result['status'] and previous.get('failures') == result.get('failures') and previous.get('reason') == result.get('reason')
    if result['repeatedFailure']: result['nextAction'] = 'Stop this failed approach; identify the first mismatch before changing the candidate or rerunning.'
    result.update(time_ns=time.time_ns(), input=str(args.input), inputSHA256=fingerprint, hook_event=event.get('hook_event_name'), tool_id=event.get('tool_use_id'))
    HOME.mkdir(parents=True, exist_ok=True); output = HOME / (str(result['time_ns']) + '.json'); output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    if args.input.exists(): output.with_suffix('.input.json').write_bytes(args.input.read_bytes())
    if args.hook:
        print(json.dumps({'hookSpecificOutput': {'hookEventName': event['hook_event_name'], 'additionalContext': 'HM2 background assets only: iPhone original / iPad original. ' + result['status'] + '; ' + str(output) + '. Never block editing, stash, recovery or uninstall.'}})); return 0
    print(json.dumps(result, ensure_ascii=False)); return 0 if result['status'] == 'PASS' else 1
if __name__ == '__main__':
    try: code = main()
    except Exception as error:
        print(json.dumps({'systemMessage': 'Asset check ERROR: ' + str(error)} if '--hook' in sys.argv else {'status': 'ERROR', 'reason': str(error)})); code = 0 if '--hook' in sys.argv else 1
    sys.exit(code)
