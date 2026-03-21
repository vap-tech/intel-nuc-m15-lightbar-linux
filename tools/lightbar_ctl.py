#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
run = repo / 'tools' / 'run_f351_packet.py'
mid = 0x0603

states = {
    'off': 1,
    'blank': 1,
    'waking': 2,
    'listening': 2,
    'direction': 3,
    'active_direction': 3,
    'ending': 4,
    'active_ending': 4,
    'thinking': 5,
    'talking': 6,
    'speaking': 6,
    'mics_off_start': 7,
    'mics_off': 8,
    'mics_off_end': 9,
    'alert': 10,
    'alert_short': 11,
    'notification_incoming': 12,
    'notification_unheard': 13,
    'do_not_disturb': 14,
    'bluetooth_connect': 15,
    'bluetooth_disconnect': 16,
    'error': 17,
}

verified = {
    1: 'verified-off',
    2: 'verified',
    3: 'context-dependent',
    4: 'verified',
    5: 'verified',
    6: 'verified-on',
    7: 'verified',
    8: 'verified',
    9: 'no-visible-effect',
    10: 'verified',
    11: 'verified',
    12: 'verified',
    13: 'verified',
    14: 'verified',
    15: 'verified',
    16: 'verified',
    17: 'verified',
}


def build_packet(state: int, speed: int, brightness: int, direction: int) -> bytes:
    buf = bytearray(256)
    buf[0] = (mid >> 8) & 0xFF
    buf[1] = mid & 0xFF
    buf[2] = 1
    buf[3] = state & 0xFF
    buf[4] = speed & 0xFF
    buf[5] = brightness & 0xFF
    buf[6] = direction & 0xFF
    return bytes(buf)


def run_packet(packet: bytes) -> int:
    with tempfile.NamedTemporaryFile(prefix='lightbar-', suffix='.bin', delete=False) as f:
        f.write(packet)
        tmp = f.name
    try:
        proc = subprocess.run(['python', str(run), tmp])
        return proc.returncode
    finally:
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description='Control Intel/Uniwill front light bar via raw F351 -> 0x0603 path')
    p.add_argument('state', nargs='?', default='talking', help='state name or numeric state id')
    p.add_argument('--speed', type=int, default=5, help='1..10, default 5')
    p.add_argument('--brightness', type=int, default=100, help='1..100, default 100')
    p.add_argument('--direction', type=int, default=3, help='1=left 2=right 3=center, default 3')
    p.add_argument('--list-states', action='store_true', help='list known states and exit')
    return p.parse_args()


def main() -> int:
    args = parse_args()
    if args.list_states:
        for name, value in sorted(states.items(), key=lambda kv: (kv[1], kv[0])):
            note = verified.get(value, 'unverified')
            print(f'{name:24} {value:2d}  {note}')
        return 0

    if os.geteuid() != 0:
        print('run as root', file=sys.stderr)
        return 1

    try:
        state = int(args.state, 0)
    except ValueError:
        key = args.state.lower().replace('-', '_')
        if key not in states:
            print(f'unknown state: {args.state}', file=sys.stderr)
            return 2
        state = states[key]

    if not (1 <= args.speed <= 10):
        print('speed must be 1..10', file=sys.stderr)
        return 2
    if not (1 <= args.brightness <= 100):
        print('brightness must be 1..100', file=sys.stderr)
        return 2
    if args.direction not in (1, 2, 3):
        print('direction must be 1, 2, or 3', file=sys.stderr)
        return 2

    return run_packet(build_packet(state, args.speed, args.brightness, args.direction))


if __name__ == '__main__':
    raise SystemExit(main())
