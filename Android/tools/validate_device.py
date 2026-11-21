#!/usr/bin/env python3
"""Run an installed HM2 cheat APK on one explicitly selected Android device."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid

APP = 'com.gelb2.heavymach2.validation'
ACTIVITY = APP + '/com.gelb2.heavymach2.GameActivity'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('serial')
    parser.add_argument('command', help='status, field:SnowPass, campaign or concurrent')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--timeout', type=int, default=1200)
    args = parser.parse_args()
    sdk = Path(os.environ.get('ANDROID_HOME', Path.home() / 'Library/Android/sdk'))
    adb = shutil.which('adb') or str(sdk / 'platform-tools/adb')
    prefix = [adb, '-s', args.serial]

    def shell(*words, check=True):
        return subprocess.run(prefix + ['shell', *words], capture_output=True, text=True, check=check)

    def send(command, timeout=120):
        # Explicit intents carry one small allowlisted command and a request id.
        if not all(c.isascii() and (c.isalnum() or c in ':_-') for c in command):
            raise ValueError('Unexpected command characters')
        request = uuid.uuid4().hex
        shell('am', 'start', '-n', ACTIVITY, '--es', 'hm2.validation', command,
              '--es', 'hm2.request', request)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            response = shell('run-as', APP, 'cat', 'files/validation.json', check=False)
            if response.returncode == 0:
                value = json.loads(response.stdout)
                if value.get('request') == request:
                    if 'error' in value:
                        raise RuntimeError(value['error'])
                    return value['result']
            time.sleep(0.6)
        raise TimeoutError(f'{args.serial}: no response to {command}')

    def record(result):
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')

    if args.command not in ('campaign', 'concurrent'):
        result = send(args.command)
    else:
        suite = args.command
        result = send(suite + ':start')
        deadline = time.monotonic() + args.timeout
        # Starting a foreground Activity again can invoke onPause/onResume
        # and open the real field pause menu. The game writes an atomic report
        # from its own clock so observers leave the running scenario alone.
        report_path = f'files/{suite}/report.json'
        while not result.get('done') and time.monotonic() < deadline:
            time.sleep(4)
            raw = shell('run-as', APP, 'cat', report_path)
            result = json.loads(raw.stdout)
            record(result)
            print(suite, 'frames', result.get('frames'), 'cases', result.get('cases'), flush=True)
        record(result)
        if not result.get('pass'):
            raise SystemExit(json.dumps(result, ensure_ascii=False))
        send(suite + ':stop')
    record(result)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
