#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
run = repo / 'tools' / 'run_f351_packet.py'

if len(sys.argv) < 3:
    print(f'usage: {sys.argv[0]} <mid-hex> <payload-byte ...>', file=sys.stderr)
    sys.exit(2)

mid = int(sys.argv[1], 0)
payload = bytes(int(x, 0) & 0xFF for x in sys.argv[2:])
buf = bytearray(256)
buf[0] = (mid >> 8) & 0xFF
buf[1] = mid & 0xFF
buf[2:2 + len(payload)] = payload

with tempfile.NamedTemporaryFile(prefix='rawf351-', suffix='.bin', delete=False) as f:
    f.write(buf)
    tmp = f.name
try:
    p = subprocess.run(['python', str(run), tmp], text=True, capture_output=True, check=True)
    print(p.stdout, end='')
finally:
    try:
        os.unlink(tmp)
    except FileNotFoundError:
        pass
