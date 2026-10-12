#!/usr/bin/env python3
"""Check generated 3DS binaries beyond mere 'file exists' tests.

These are format sanity checks, NOT a full install or hardware compatibility test.
"""
from pathlib import Path
import struct

root = Path(__file__).resolve().parent.parent

def require(path: Path, minimum: int) -> bytes:
    assert path.is_file(), f"Missing: {path}"
    data = path.read_bytes()
    assert len(data) >= minimum, f"Truncated or empty: {path} ({len(data)} bytes)"
    return data

elf = require(root / 'gerald.elf', 256)
assert elf[:4] == b'\x7fELF', 'gerald.elf is not an ELF binary'
assert elf[4] == 1, 'Gerald is expected to be a 32-bit ARM ELF'
assert struct.unpack_from('<H', elf, 18)[0] == 40, 'ELF is not for ARM'

homebrew = require(root / 'dist/gerald.3dsx', 256)
assert homebrew[:4] == b'3DSX', 'gerald.3dsx is missing the 3DSX signature'

cia = require(root / 'dist/gerald.cia', 8192)
header_size = struct.unpack_from('<I', cia, 0)[0]
certificate_size = struct.unpack_from('<I', cia, 8)[0]
ticket_size = struct.unpack_from('<I', cia, 12)[0]
tmd_size = struct.unpack_from('<I', cia, 16)[0]
content_size = struct.unpack_from('<Q', cia, 24)[0]
assert 0x2020 <= header_size <= 0x10000 and header_size % 0x20 == 0, 'CIA header size invalid'
assert certificate_size and ticket_size and tmd_size and content_size, 'CIA metadata or content is missing'
assert content_size < len(cia), 'CIA content size is implausible'

for filename in ('banner.png', 'icon.png'):
    image = require(root / 'dist' / filename, 24)
    assert image.startswith(b'\x89PNG\r\n\x1a\n'), f'{filename} is not PNG'

print(f'PASS: ARM ELF, valid 3DSX signature and plausible CIA header/content ({len(cia):,} bytes)')
print('Note: format checks cannot replace installing and testing on a real 3DS.')
