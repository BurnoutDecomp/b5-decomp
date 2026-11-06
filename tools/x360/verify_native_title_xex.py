"""Verify a native PE against an unencrypted BASIC XEX. Never writes an image."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import pefile

TITLE_ID = 0x45410806
MAX_IMAGE = 512 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def u32(data, off):
    require(0 <= off <= len(data) - 4, 'truncated u32')
    return struct.unpack_from('>I', data, off)[0]


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--pe', type=Path, required=True)
    cli.add_argument('--map', type=Path, required=True)
    cli.add_argument('--xex', type=Path, required=True)
    cli.add_argument('--output', type=Path, required=True)
    args = cli.parse_args()
    require(not args.output.exists(), 'output already exists')
    require(args.output.resolve() not in {p.resolve() for p in (args.pe, args.map, args.xex)}, 'output is input')
    pe_data, map_data, xex = (p.read_bytes() for p in (args.pe, args.map, args.xex))
    pe = pefile.PE(data=pe_data, fast_load=True)
    require(xex[:4] == b'XEX2', 'not XEX2')
    image_offset, sec, count = u32(xex, 8), u32(xex, 16), u32(xex, 20)
    require(count <= 1024 and 24 + count * 8 <= len(xex), 'invalid optional-header count')
    pairs = list(struct.iter_unpack('>II', xex[24:24 + count * 8]))
    headers = dict(pairs)
    require(len(headers) == len(pairs), 'duplicate optional headers')
    require(all(k in headers for k in (0x3ff, 0x10100, 0x10201, 0x103ff, 0x40006)), 'missing required header')
    base = pe.OPTIONAL_HEADER.ImageBase
    entry = base + pe.OPTIONAL_HEADER.AddressOfEntryPoint
    require(headers[0x10201] == base and headers[0x10100] == entry, 'base/entry mismatch')
    eid = headers[0x40006]
    require(eid + 24 <= len(xex) and u32(xex, eid + 12) == TITLE_ID, 'execution title ID mismatch')
    image_size = u32(xex, sec + 4)
    require(0 < image_size <= MAX_IMAGE and 0 < image_offset <= len(xex), 'invalid XEX image size')
    f = headers[0x3ff]
    format_size = u32(xex, f)
    require(format_size >= 8 and (format_size - 8) % 8 == 0 and f + format_size <= len(xex), 'invalid format info')
    enc, comp = struct.unpack_from('>HH', xex, f + 4)
    require((enc, comp) == (0, 1), 'requires unencrypted BASIC format')
    blocks = list(struct.iter_unpack('>II', xex[f + 8:f + format_size]))
    image = bytearray()
    pos = image_offset
    for data_size, zero_size in blocks:
        require(pos + data_size <= len(xex) and len(image) + data_size + zero_size <= image_size, 'block out of bounds')
        image.extend(xex[pos:pos + data_size])
        image.extend(bytes(zero_size))
        pos += data_size
    require(pos == len(xex), 'unconsumed XEX data')
    reconstructed = len(image)
    image.extend(bytes(image_size - len(image)))

    expected = bytearray(image_size)
    header_size = pe.OPTIONAL_HEADER.SizeOfHeaders
    require(0 < header_size <= min(image_size, len(pe_data)), 'invalid PE headers size')
    expected[:header_size] = pe_data[:header_size]
    occupied = [(0, header_size, 'headers')]
    sections = []
    excluded = []
    for s in pe.sections:
        name = s.Name.rstrip(b'\0').decode('ascii', 'replace')
        va, size = s.VirtualAddress, s.Misc_VirtualSize
        if va >= image_size:
            excluded.append(name)
            continue
        require(size <= image_size - va, 'PE section exceeds XEX image')
        require(all(va + size <= a or va >= b for a, b, _ in occupied), 'overlapping PE sections')
        rawsize = min(size, s.SizeOfRawData)
        require(s.PointerToRawData + rawsize <= len(pe_data), 'truncated PE section')
        expected[va:va + rawsize] = pe_data[s.PointerToRawData:s.PointerToRawData + rawsize]
        occupied.append((va, va + size, name))
        sections.append((name, va, size))

    rx = re.compile(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+([0-9a-fA-F]+)H\s+(\S+)\s+(DATA|CODE)\s*$')
    rows = {}
    for line in map_data.decode(errors='replace').splitlines():
        m = rx.match(line)
        if m and m[4] in ('.idata$5', '.xidata'):
            require(m[4] not in rows, 'duplicate map range')
            rows[m[4]] = (int(m[1], 16), int(m[2], 16), int(m[3], 16), m[5])
    require(set(rows) == {'.idata$5', '.xidata'}, 'missing import ranges')
    data_sec, text_sec = (next((s for s in sections if s[0] == name), None) for name in ('.rdata', '.text'))
    require(data_sec is not None and text_sec is not None, 'missing PE rdata/text')
    di, do, dl, dc = rows['.idata$5']
    ti, to, tl, tc = rows['.xidata']
    require(di == 1 and ti == 3 and dc == 'DATA' and tc == 'CODE', 'unexpected map section index')
    require(do + dl <= data_sec[2] and to + tl <= text_sec[2] and dl % 4 == 0 and tl % 16 == 0, 'invalid import ranges')
    ds, ts = data_sec[1] + do, text_sec[1] + to
    require(dl > 0 and tl > 0, 'empty import range')
    mask = bytearray(image_size)
    mask[ds:ds + dl] = b'\1' * dl
    mask[ts:ts + tl] = b'\1' * tl
    differences = [0, 0, 0]
    for i, (a, b) in enumerate(zip(expected, image)):
        if a != b:
            differences[0 if ds <= i < ds + dl else 1 if ts <= i < ts + tl else 2] += 1
    require(differences[2] == 0, 'mapped bytes differ outside import ranges')
    imp = headers[0x103ff]
    import_size, strings_size, libraries = u32(xex, imp), u32(xex, imp + 4), u32(xex, imp + 8)
    require(imp + import_size <= len(xex) and strings_size <= import_size - 12 and libraries > 0, 'bad import metadata')
    for off in range(0, dl, 4):
        old = struct.unpack_from('<I', expected, ds + off)[0]
        new = u32(image, ds + off)
        require((old == new == 0) or
                (old & 0x80000000 and old & 0xffff == new & 0xffff and new >> 16 < libraries),
                'import slot ordinal mismatch')
    for off in range(0, tl, 16):
        old = struct.unpack_from('>IIII', expected, ts + off)
        new = struct.unpack_from('>IIII', image, ts + off)
        require(old[0] >> 16 == 0x3d60 and old[1] >> 16 == 0x816b, 'unexpected original thunk')
        low = struct.unpack('>h', struct.pack('>H', old[1] & 0xffff))[0]
        target = ((old[0] & 0xffff) << 16) + low - base
        require(ds <= target <= ds + dl - 4 and (target - ds) % 4 == 0, 'thunk target outside IAT')
        require(new[0] >> 24 == 1 and new[1] >> 24 == 2 and
                new[0] & 0xffffff == new[1] & 0xffffff == u32(image, target) and
                old[2:] == new[2:], 'import thunk marker mismatch')
    report = {'status': 'pass', 'pe_sha256': sha(pe_data), 'map_sha256': sha(map_data), 'xex_sha256': sha(xex),
              'image_base': hex(base), 'entrypoint': hex(entry), 'title_id': hex(TITLE_ID),
              'image_size': image_size, 'reconstructed_size_before_zero_extension': reconstructed,
              'basic_blocks': [{'data': a, 'zero_fill': b} for a, b in blocks],
              'import_slot_bytes': dl, 'import_thunks_verified': tl // 16, 'import_library_count': libraries,
              'different_bytes': {'.idata$5': differences[0], '.xidata': differences[1], 'outside_import_ranges': differences[2]},
              'sections_outside_image': excluded,
              'scope': 'Static mapped image with validated import metadata; no loader execution'}
    with args.output.open('x', encoding='utf-8') as out:
        json.dump(report, out, indent=2)
        out.write('\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
