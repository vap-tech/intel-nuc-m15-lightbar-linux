#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path

if len(sys.argv) != 2:
    print(f'usage: {sys.argv[0]} <packet.bin>', file=sys.stderr)
    sys.exit(2)

repo = Path(__file__).resolve().parents[1]
base = Path('/sys/kernel/debug/intel_lightbar_wmi')
choices = [p for p in base.iterdir() if 'F3517D45-0E66-41EF-8472-FCB7C98AE932' in p.name]
if not choices:
    print('no F351 guid dir', file=sys.stderr)
    sys.exit(1)

guid = str(choices[0])
pkt = Path(sys.argv[1])
args = [f'{b:02x}' for b in pkt.read_bytes()]
env = os.environ.copy()
env['GUID_DIR'] = guid
cmd = ['python', str(repo / 'tools' / 'wmi_call.py'), '0', '1'] + args
print('GUID_DIR=' + guid)
print('PKT=' + str(pkt))
print('ARGS=' + str(len(args)))
subprocess.run(cmd, env=env, check=False)
