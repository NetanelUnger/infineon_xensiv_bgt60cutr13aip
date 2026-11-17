"""Read Infineon CSK info/settings and sample JSON events without changing configuration."""
import argparse
import binascii
import json
import struct
import time
from pathlib import Path

import serial


def command(command_id):
    header = struct.pack('<BBH', 0xD9, command_id, 0)
    return header + struct.pack('<H', binascii.crc_hqx(header, 0xFFFF))


def probe(port, seconds):
    results = []
    with serial.Serial(port, 2_000_000, timeout=0.2) as uart:
        uart.reset_input_buffer()
        uart.write(command(1))
        uart.write(command(7))
        deadline = time.monotonic() + seconds
        pending = bytearray()
        while time.monotonic() < deadline:
            pending.extend(uart.read(uart.in_waiting or 1))
            while b'\n' in pending:
                line, _, remainder = pending.partition(b'\n')
                pending = bytearray(remainder)
                try:
                    results.append(json.loads(line))
                except (ValueError, UnicodeDecodeError):
                    pass
    return results


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM11')
    parser.add_argument('--seconds', type=float, default=8)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    result = probe(args.port, args.seconds)
    Path(args.output).write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f'Received {len(result)} JSON messages')
    print('Message keys:', sorted({k for obj in result if isinstance(obj, dict) for k in obj}))
    for obj in result:
        if isinstance(obj, dict) and any(k in obj for k in ('info', 'version', 'settings')):
            print(json.dumps(obj)[:1800])
