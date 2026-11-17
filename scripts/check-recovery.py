"""Validate the original Intel HEX against read-back and compare serial identity/settings."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
directory = ROOT / 'recovery'
firmware = directory / 'presence_detection_and_tracking_csk_v1_4_1.hex'
snapshot = (directory / 'main-flash-before.bin').read_bytes()
assert len(snapshot) == 0x200000
base = count = mismatches = 0
for line in firmware.read_text().splitlines():
    assert line.startswith(':')
    record = bytes.fromhex(line[1:])
    size, kind = record[0], record[3]
    assert len(record) == size + 5 and sum(record) % 256 == 0
    data = record[4:4 + size]
    if kind == 4:
        base = int.from_bytes(data, 'big') << 16
    elif kind == 0:
        address = base + int.from_bytes(record[1:3], 'big')
        offset = address - 0x10000000
        assert 0 <= offset and offset + size <= len(snapshot), 'Unexpected flash region'
        mismatches += sum(a != b for a, b in zip(data, snapshot[offset:offset + size]))
        count += size
assert count > 0 and mismatches == 0
before = json.loads((directory / 'serial-before.json').read_text())
after = json.loads((directory / 'serial-after-restore.json').read_text())
info_before = next(x for x in before if 'version' in x)
info_after = next(x for x in after if 'version' in x)
assert info_before == info_after, 'Firmware identity changed'
settings_before = next(x['settings'] for x in before if 'settings' in x)
settings_after = next(x['settings'] for x in after if 'settings' in x)
assert settings_before == settings_after, 'Algorithm settings changed'
assert any('tracks' in x for x in after), 'No tracking frames after restore'
(directory / 'original-settings.json').write_text(json.dumps(settings_before, indent=2))
result = {
    'original_sha256': hashlib.sha256(firmware.read_bytes()).hexdigest(),
    'main_flash_backup_sha256': hashlib.sha256(snapshot).hexdigest(),
    'verified_original_bytes': count,
    'mismatched_bytes': mismatches,
    'name': info_after['name'], 'version': info_after['version'],
    'build': info_after['build'], 'algorithm': info_after['algo_info']['version'],
    'serial_identity_equal': True, 'settings_equal': True,
    'messages_after_restore': len(after),
    'tracking_frames_after_restore': sum('tracks' in x for x in after),
    'openocd_program_verified': '** Verified OK **' in (directory / 'restore-test.log').read_text(encoding='utf-8-sig'),
}
assert result['openocd_program_verified']
(directory / 'verification.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
