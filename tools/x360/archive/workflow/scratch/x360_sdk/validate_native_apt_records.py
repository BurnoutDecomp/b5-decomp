"""Compare compiled candidate record consumers against original ARTIST instructions."""
import json
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0, str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff
from validate_resource_pointer import Bundle

ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
reports = ['native-apt-record-candidate-build', 'native-apt-record-candidate-followup',
           'native-apt-record-export-probe']
paths = {}
for report in reports:
    for row in json.loads((ROOT/'build/x360'/report/'compile-results.json').read_text())['results']:
        if row['passed']:
            paths[Path(row['source']).name] = Path(row['object'])
objects = [Coff(paths['AptFile.cpp'], '?FindExport@AptFile@@'),
           Coff(paths['AptCharacterAnimationInst.cpp'], '?AptGetMovieCharacterAnimation@@'),
           Coff(paths['apt-native-export.cpp'], '?GetIDFromImportFile@AptCharacterAnimation@@')]
bundle = Bundle(objects)
rng = random.Random(360_2810)
cases, errors = 0, []
for kind, prefix, original in [('find', '?FindExport@AptFile@@', 0x82AD9DF0),
                               ('id', '?GetIDFromImportFile@AptCharacterAnimation@@', 0x82ADE998)]:
    bundle.select(prefix)
    for i in range(256):
        a, b = M.S.SELF, M.S.AUX
        own = bytearray(rng.randbytes(0x1000)); data = bytearray(rng.randbytes(0x2000))
        file, root, exports, characters = a, b, b+0x100, b+0x200
        imports, definition, search = b+0x300, a+0x100, b+0x800
        count = [-1, 0, 1, 2, 4, 8][i % 6]
        names = [b'', b'a', b'ab', b'abc', b'MovieClip', b'__Packages.A', b'\x80\xff']
        wanted = names[i % len(names)] if i % 11 else b'absent'
        struct.pack_into('>I', own, 0x14, root)
        struct.pack_into('>IiI', data, 0x34, imports, count, exports)
        struct.pack_into('>I', data, 0x20, characters)
        for slot in range(16):
            struct.pack_into('>I', data, 0x200+4*slot, rng.getrandbits(32))
        for slot in range(max(count, 0)):
            label = names[rng.randrange(len(names))]
            address = b+0x1000+0x40*slot
            data[address-b:address-b+len(label)+1] = label+b'\0'
            struct.pack_into('>II', data, 0x100+slot*8, address, rng.randrange(16))
        data[0x800:0x800+len(wanted)+1] = wanted+b'\0'
        index = i % 8
        struct.pack_into('>I', own, 0x100+0x24, imports)
        struct.pack_into('>II', data, 0x300+index*16+4, search, rng.getrandbits(32))
        struct.pack_into('>I', data, 0x300+index*16+12, file)
        results = []
        for compiled in (False, True):
            cpu = bundle.cpu();cpu.mem.wr(a, own);cpu.mem.wr(b, data)
            cpu.setg(3, file if kind == 'find' else definition)
            cpu.setg(4, search if kind == 'find' else index)
            cpu.run(bundle.entry if compiled else original)
            results.append((cpu.g(3)&0xFFFFFFFF, cpu.mem.rd(a, 0x1000), cpu.mem.rd(b, 0x2000)))
        cases += 1
        if results[0] != results[1]:
            errors.append({'kind':kind, 'case':i, 'original_return':results[0][0],
                           'compiled_return':results[1][0]})
report = {'cases':cases, 'mismatches':errors,
          'objects':{n:str(p) for n,p in paths.items() if n in ['AptFile.cpp', 'AptCharacterAnimationInst.cpp', 'apt-native-export.cpp']},
          'scope':'actual candidate AptFile.cpp and embedded-movie accessor; isolated unchanged GetIDFromImportFile body; signed/empty counts, first-match/absent/empty/high-bit names, native8-byte entries; memory unchanged',
          'limits':'not production yet; isolated import-id body does not prove full AptCharacterAnimation.cpp compiles or native loading/unloading works'}
(ROOT/'build/x360/native-apt-record-export-probe/comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(cases,'compiled PPC export comparisons;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:4],indent=2))
raise SystemExit(bool(errors))
