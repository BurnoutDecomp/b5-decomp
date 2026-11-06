from pathlib import Path
import hashlib, json, struct, sys

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
SDK = PARENT / 'b5-decomp/build/x360/sdk-6534.4/portable/XDK'
EXPORT = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
sys.path.insert(0, str(PARENT / 'tools/re'))
import x360rd

def sha(data):
    return hashlib.sha256(data).hexdigest()

assert x360rd.u32(0x82815570) == 0x7C8A2378
assert x360rd.u32(0x82F264A8) == 4
libpath = SDK / 'lib/xbox/libcMT.lib'
lib = libpath.read_bytes()
assert lib[:8] == b'!<arch>\n'
off, found = 8, None
while off + 60 <= len(lib):
    header = lib[off:off + 60]
    size = int(header[48:58])
    data = lib[off + 60:off + 60 + size]
    off += 60 + size + (size & 1)
    if len(data) < 20 or struct.unpack_from('<H', data)[0] != 0x1F2:
        continue
    _, ns, _, symoff, nsyms, optsize, _ = struct.unpack_from('<HHIIIHH', data)
    if not (0 < ns < 100 and symoff + nsyms * 18 + 4 <= len(data)):
        continue
    strings = data[symoff + nsyms * 18:]
    symbols, i = {}, 0
    while i < nsyms:
        sym = data[symoff + i * 18:symoff + (i + 1) * 18]
        name = (strings[struct.unpack_from('<I', sym, 4)[0]:].split(b'\0', 1)[0]
                if sym[:4] == b'\0' * 4 else sym[:8].split(b'\0', 1)[0])
        value, section, typ, storage, aux = struct.unpack_from('<IhHBB', sym, 8)
        symbols[i] = (name.decode(errors='replace'), value, section, storage)
        i += 1 + aux
    for name, value, section, storage in symbols.values():
        if name != '_snprintf' or section <= 0 or storage != 2:
            continue
        sh = 20 + optsize + (section - 1) * 40
        sz, ptr, relptr = struct.unpack_from('<III', data, sh + 16)
        nrel = struct.unpack_from('<H', data, sh + 32)[0]
        assert value == 0 and sz == 0x114
        code = bytearray(data[ptr:ptr + sz])
        original = bytearray(x360rd.rd(0x82C10930, sz))
        relocs = []
        for j in range(nrel):
            at, symindex, kind = struct.unpack_from('<IIH', data, relptr + j * 10)
            relocs.append({'offset': at, 'type': kind, 'symbol': symbols[symindex][0]})
            assert kind == 6  # IMAGE_REL_PPC_REL24, genuine external branch relocation.
            for stream in (code, original):
                word = struct.unpack_from('>I', stream, at)[0]
                struct.pack_into('>I', stream, at, word & 0xFC000003)
        assert code == original, 'SDK _snprintf does not match original helper'
        found = {'symbol': name, 'artist_address': '0x82C10930', 'size': sz,
                 'relocations': relocs, 'normalized_sha256': sha(code),
                 'original_sha256': sha(x360rd.rd(0x82C10930, sz)),
                 'library_member_sha256': sha(data)}
assert found
result = {'calibration_words': ['0x82815570=7C8A2378', '0x82F264A8=00000004'],
          'library': str(libpath), 'library_sha256': sha(lib), 'snprintf': found,
          'format_export': str(EXPORT / '0x82BBC118.json'),
          'format_export_sha256': sha((EXPORT / '0x82BBC118.json').read_bytes()),
          'helper_export_sha256': sha((EXPORT / '0x82C10930.json').read_bytes()),
          'format_string_hex': x360rd.rd(0x82180F2C, 12).hex(),
          'verifier_sha256': sha(Path(__file__).read_bytes()),
          'scope': 'Complete 276-byte original CRT helper equals SDK _snprintf after four genuine external branch relocations. Formatter method itself is not claimed byte-matched.'}
with (HERE / 'crt-proof.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps(result, indent=2))
