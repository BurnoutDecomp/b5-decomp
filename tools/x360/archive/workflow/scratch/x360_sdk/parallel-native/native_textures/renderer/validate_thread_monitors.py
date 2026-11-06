from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
import native_atomic_fixture as F

out = parent / 'b5-decomp/build/x360/parallel/native_textures/renderer-thread-monitors3'
obj = Path(json.loads((out / 'compile-results.json').read_text())['results'][0]['object'])
bundle = F.Bundle([F.Coff(obj, '?RenderThreeThreadMonitors@')])
bundle.select('?RenderThreeThreadMonitors@')
SELF, AUX = F.M.S.SELF, F.M.S.AUX
RENDERER, ACTIVE = SELF + 0xf70, 0x83010f9c
LIBRARY, DEFAULT = 0x83010f20, 0x830112d0
addresses = {'?mgpActiveRenderer@': ACTIVE, '?mgStateLibrary@': LIBRARY, '?mgDefault@': DEFAULT}
for (oi, sec), base in bundle.bases.items():
    source = bundle.objects[oi]
    body = bundle.images[base]
    for at, index, kind in source.relocations(sec):
        name = source.symbols[index][0]
        matches = [v for p, v in addresses.items() if name.startswith(p)]
        if not matches: continue
        value = matches[0]
        word = struct.unpack_from('>I', body, at)[0]
        raw = struct.unpack_from('>I', source.data, source.sections[sec][4] + at)[0]
        if kind == 0x10: word = (word & 0xffff0000) | (((value + 0x8000) >> 16) & 0xffff)
        elif kind == 0x11: word = (word & 0xffff0000) | ((value + (raw & 0x7fff) - (raw & 0x8000)) & 0xffff)
        elif kind == 2: word = value + raw
        else: raise AssertionError((name, kind))
        struct.pack_into('>I', body, at, word)

class ThreadCPU(F.AtomicCPU):
    def step(self, pc, word, nxt, depth):
        if word >> 26 == 8:  # subfic, including XER carry for compiler bool masks
            dst, src = (word >> 21) & 31, (word >> 16) & 31
            imm = word & 0xffff
            if imm & 0x8000: imm -= 0x10000
            mask = 0xffffffffffffffff
            total = (imm & mask) + ((~self.g(src)) & mask) + 1
            self.xer_ca = int(total > mask)
            self.setg(dst, total & mask)
            return nxt
        if word >> 26 == 31 and ((word >> 1) & 1023) == 136:  # subfe
            dst, lhs, rhs = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            assert hasattr(self, 'xer_ca'), 'Missing carry producer in fixture'
            mask = 0xffffffffffffffff
            total = ((~self.g(lhs)) & mask) + self.g(rhs) + self.xer_ca
            self.xer_ca = int(total > mask)
            self.setg(dst, total & mask)
            assert not (word & 1), 'Record-form subfe needs explicit CR modeling'
            return nxt
        return super().step(pc, word, nxt, depth)

rng = random.Random(0x82405a30)
errors = []
for i in range(256):
    module = rng.randbytes(0xca80)
    library = rng.randbytes(0x50)
    transform = rng.randbytes(64)
    both = []
    for compiled in [False, True]:
        c = F.cpu(bundle)
        c.__class__ = ThreadCPU
        c.mem.add(0x83010000, 0x2000)
        c.mem.wr(SELF, module)
        c.mem.wr(LIBRARY, library)
        c.mem.wr(DEFAULT, transform)
        F.put(c, ACTIVE, 0)
        F.put(c, RENDERER, AUX + 0x4000)
        F.put(c, AUX + 0x4000, 0x84803000)
        F.put(c, AUX + 0x4100, 0x84803010)
        c.setg(3, SELF)
        for reg in range(4, 7): c.setg(reg, (i >> (reg - 4)) & 1)
        sp = c.g(1) & 0xffffffff
        c.mem.wr(sp - 0x2000, bytes([i]) * 0x2000)
        trace = []
        count = [0]

        def context():
            return (hashlib.sha256(c.mem.rd(SELF, 0xca80)).hexdigest(),
                    c.mem.rd(LIBRARY, 0x50).hex(), c.mem.rd(ACTIVE, 4).hex())
        def begin(x):
            trace.append(('begin', x.g(3) & 0xffffffff, context()))
            F.put(x, ACTIVE, RENDERER + 4)
            if i % 5 == 0:
                F.put(x, LIBRARY, AUX + 0x5000 + i)
                x.mem.wr(DEFAULT, bytes([0x5a]) * 64)
        def state(x, raster=False):
            trace.append(('raster' if raster else 'blend', x.g(3) & 0xffffffff,
                          x.g(4) & 0xffffffff, context()))
            if i % 5 == 1 and not raster: F.put(x, LIBRARY + 0x1c, AUX + 0x6000 + i)
        def set_transform(x):
            trace.append(('transform', x.g(3) & 0xffffffff, x.g(4) & 0xffffffff,
                          x.mem.rd(x.g(4) & 0xffffffff, 64).hex(), context()))
            if i % 5 == 2: F.put(x, RENDERER, AUX + 0x4100)
        def program(x, alternate=False):
            trace.append(('program_alt' if alternate else 'program', x.g(3) & 0xffffffff,
                          x.g(4) & 0xffffffff, context()))
            x.mem.wr(RENDERER + 0x54, b'\0')
            x.setg(3, i % 2)
        def draw(x):
            n = count[0]
            count[0] += 1
            vertices = x.g(5) & 0xffffffff
            vertex_count = x.g(6) & 0xffffffff
            trace.append(('draw', n, x.g(3) & 0xffffffff, x.g(4) & 0xffffffff,
                          vertex_count, x.mem.rd(vertices, vertex_count * 12).hex(), context()))
            if i % 5 == 3 and n == 2: F.put(x, ACTIVE, AUX + 0x7000)
        def assert_begin(x): trace.append(('assert_begin', context()))
        def fire(x):
            trace.append(('assert_fire', F.cstring(x, x.g(3) & 0xffffffff),
                          F.cstring(x, x.g(4) & 0xffffffff), x.g(5) & 0xffffffff, context()))
        def assert_end(x): trace.append(('assert_end', context()))

        c.hooks.update({0x823f9640: begin, 0x82276d08: state,
                        0x82276e48: lambda x: state(x, True), 0x823f9740: set_transform,
                        0x82404858: draw, 0x84803000: program,
                        0x84803010: lambda x: program(x, True),
                        0x82817548: assert_begin, 0x82820810: fire, 0x82817558: assert_end})
        if compiled:
            for at, name in bundle.unresolved.items():
                if name.startswith('?BeginRendering@'): c.hooks[at] = begin
                elif name.startswith('?SetState@ImRendererBase@'):
                    c.hooks[at] = (lambda x: state(x, True)) if 'RasterizerState@' in name else state
                elif name.startswith('?SetTransform@'): c.hooks[at] = set_transform
                elif name.startswith('?Render@'): c.hooks[at] = draw
                elif name.startswith('?BeginAssert@'): c.hooks[at] = assert_begin
                elif name.startswith('?FireAssert@'): c.hooks[at] = fire
                elif name.startswith('?EndAssert@'): c.hooks[at] = assert_end
        c.run(bundle.entry if compiled else 0x82405a30)
        assert count[0] == 3, count
        both.append((c.mem.rd(SELF, 0xca80), c.mem.rd(LIBRARY, 0x50), c.mem.rd(ACTIVE, 4), trace))
    if both[0] != both[1]:
        errors.append(dict(case=i, module_differences=[hex(n) for n, (a, b) in enumerate(zip(both[0][0], both[1][0])) if a != b],
                           traces=[v[3] for v in both]))

report = out / 'thread-monitors-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed evidence'
report.write_text(json.dumps(dict(cases=256, errors=errors, object=str(obj), object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
                                  scope='Whole actual RenderThreeThreadMonitors versus ARTIST82405A30; all eight bool combinations, three complete48byte draw payloads, exact colors/coordinates, source library freshness, actual virtual slot0 lookup after transform, final assertion metadata and active-renderer clear compare with full Renderer/library outputs.',
                                  limits='Im2d rendering/state/transform, virtual SetProgram and assertion services are controlled. No real GPU, complete frame or helper body claim.'), indent=2) + '\n')
print('256 whole thread monitor cases; errors', len(errors))
print(errors[:1])
raise SystemExit(bool(errors))
