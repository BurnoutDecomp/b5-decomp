#!/usr/bin/env python3
"""Extract the four ARTIST loading-screen arrays into a private compiler include.

Supply the workflow's tools/re/x360rd.py and an output directory. The reader
must resolve the original ARTIST image, not a different build. The generated
include is a build input and should not be committed.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ARRAYS = (
    ('gpLoadingScreenTextureArrow', 0x82CDC078, 0x4000, 'affee0686027b6428434336ac8f19d1fc5745fc6f2c26dc0d2f9422ec94efe57'),
    ('gpLoadingScreenTextureCar', 0x82CE0078, 0x200000, '90512933f117408cac32d23b4ef426d0a6590b420c2a314c128ad54ded7a7338'),
    ('gpLoadingScreenTextureText', 0x82EE0078, 0x40000, '4333d59da3d710079f208cf18f6f640483caf3633bc1b6c2abb2f900c7ec47dc'),
    ('gpLoadingScreenTextureBox', 0x82F20078, 0x4000, 'b3fdb4214c53995d75952f09b80eab93cc40389b29ea1876482e3919c0635ff2'),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('x360rd', args.reader)
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    reader._init()
    word = lambda address: struct.unpack('>I', reader.rd(address, 4))[0]
    # Verify the named constructor instruction and both independently observed
    # initial payload words before accepting this image.
    assert word(0x823CE2EC) == 0x3D6082CE
    assert word(0x82CDC078) == 0x05000000
    assert word(0x82CE0078) == 0x0500FFFF
    for index in range(len(ARRAYS) - 1):
        assert ARRAYS[index][1] + ARRAYS[index][2] == ARRAYS[index + 1][1]

    def read_range(address, size):
        for start, end, cumulative in reader._segs:
            if start <= address and address + size <= end:
                offset = cumulative + address - start
                if reader._base is None:
                    data = bytes(reader._m[offset:offset + size])
                else:
                    offset = reader._base + offset * 4
                    data = bytes(reader._m[offset:offset + size * 4:4])
                assert len(data) == size
                assert data[:32] == reader.rd(address, 32)
                assert data[-32:] == reader.rd(address + size - 32, 32)
                return data
        raise ValueError('Array crosses an unmapped image segment')

    entries = []
    for name, address, size, expected in ARRAYS:
        data = read_range(address, size)
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected:
            raise ValueError('%s: unexpected ARTIST image data: %s' % (name, digest))
        entries.append((name, address, size, digest, data))
    args.out.mkdir(parents=True, exist_ok=True)
    include = args.out / 'BrnLoadingScreenTextures.generated.inc'
    temporary = include.with_suffix('.tmp')
    with temporary.open('w', encoding='ascii', newline='\n') as output:
        for name, _, size, _, data in entries:
            output.write('__declspec(align(8)) unsigned char %s[%d] = {\n' % (name, size))
            for offset in range(0, size, 32):
                output.write(','.join('0x%02x' % value for value in data[offset:offset + 32]) + ',\n')
            output.write('};\n')
    temporary.replace(include)
    manifest = {name: {'address': '0x%08X' % address, 'size': size, 'sha256': digest}
                for name, address, size, digest, _ in entries}
    (args.out / 'BrnLoadingScreenTextures.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(include)


if __name__ == '__main__':
    main()
