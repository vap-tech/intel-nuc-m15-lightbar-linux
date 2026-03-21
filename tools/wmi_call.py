#!/usr/bin/env python3
import errno
import os
import sys
from pathlib import Path

guid_dir = Path(os.environ.get('GUID_DIR', '/sys/kernel/debug/intel_lightbar_wmi/8C5DA44C-CDC3-46B3-8619-4E26D34390B7-5'))
call = guid_dir / 'call'
last = guid_dir / 'last'

if len(sys.argv) < 3:
    print(f'usage: {sys.argv[0]} <instance> <method> [hexbyte ...]', file=sys.stderr)
    sys.exit(2)

line = ' '.join(sys.argv[1:]) + '\n'
fd = os.open(call, os.O_WRONLY)
write_ok = False
try:
    data = line.encode('ascii')
    off = 0
    while off < len(data):
        try:
            n = os.write(fd, data[off:])
        except OSError as e:
            print(f'WRITE_ERROR errno={e.errno} name={errno.errorcode.get(e.errno, "?")} msg={e.strerror}', file=sys.stderr)
            break
        if n <= 0:
            print('WRITE_ERROR errno=0 name=SHORTWRITE msg=zero-byte write', file=sys.stderr)
            break
        off += n
    write_ok = (off == len(data))
finally:
    os.close(fd)

print(f'WRITE_OK={1 if write_ok else 0}')
if last.exists():
    print(last.read_text(errors='replace').rstrip())
