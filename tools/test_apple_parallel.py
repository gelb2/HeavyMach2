#!/usr/bin/env python3
"""Run the same built Apple tests concurrently in two distinct simulators.

This is offline test orchestration, not a game runtime. Shared built products
are reused by test-without-building; each runner owns its result/log directory.
"""
import argparse
import concurrent.futures
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]

def test_interval(text):
    values = []
    for label in ['started', 'passed']:
        match = re.search(r"Test Suite 'All tests' " + label + r" at ([0-9 :.-]+)\.", text)
        if not match:
            return None
        values.append(datetime.strptime(match[1], '%Y-%m-%d %H:%M:%S.%f').timestamp())
    return values

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--xctestrun', type=Path, required=True)
    parser.add_argument('--iphone', required=True)
    parser.add_argument('--ipad', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.iphone == args.ipad:
        parser.error('Each parallel runner needs a different simulator')
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / '.local/evidence') or output.exists():
        parser.error('Use a new directory inside this repository .local/evidence')
    run = args.xctestrun.resolve()
    if not run.is_file():
        parser.error('Build-for-testing output .xctestrun is missing')
    available = json.loads(subprocess.check_output(['xcrun', 'simctl', 'list', 'devices', 'available', '--json'], text=True))
    devices = {device['udid']: device for group in available['devices'].values() for device in group}
    for kind, identifier in [('iPhone', args.iphone), ('iPad', args.ipad)]:
        if identifier not in devices or kind not in devices[identifier]['name']:
            parser.error('Expected an available ' + kind + ' simulator: ' + identifier)
    output.mkdir(parents=True)

    def execute(kind, identifier):
        directory = output / kind
        directory.mkdir()
        command = ['xcodebuild', 'test-without-building', '-xctestrun', str(run),
                   '-destination', 'platform=iOS Simulator,id=' + identifier,
                   '-parallel-testing-enabled', 'NO', '-resultBundlePath', str(directory / 'tests.xcresult')]
        start = time.time()
        with (directory / 'tests.log').open('w') as log:
            result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        end = time.time()
        text = (directory / 'tests.log').read_text()
        return {'kind': kind, 'device': devices[identifier]['name'], 'udid': identifier,
                'start_epoch': start, 'end_epoch': end, 'returncode': result.returncode,
                'test_interval_epoch': test_interval(text),
                'success': result.returncode == 0 and '** TEST EXECUTE SUCCEEDED **' in text,
                'metrics': re.findall(r'HM2_ASSEMBLY_GPU .*', text), 'command': command}

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        futures = [pool.submit(execute, 'iphone', args.iphone), pool.submit(execute, 'ipad', args.ipad)]
        results = [future.result() for future in futures]
    intervals = [r['test_interval_epoch'] for r in results]
    overlap = max(0., min(v[1] for v in intervals) - max(v[0] for v in intervals)) if all(intervals) else None
    summary = {'recorded_utc': datetime.now(timezone.utc).isoformat(), 'scope': 'first reader/static assembly/GPU component only; not complete game cheat coverage',
               'shared_build': str(run), 'runs': results, 'observed_test_overlap_seconds': overlap,
               'success': all(r['success'] for r in results) and overlap is not None and overlap > 0}
    (output / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(output), 'success': summary['success'], 'observed_test_overlap_seconds': overlap}, ensure_ascii=False))
    return 0 if summary['success'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
