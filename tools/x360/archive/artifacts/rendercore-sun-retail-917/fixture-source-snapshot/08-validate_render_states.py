import json, random, re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import validate_native_math as M

ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
OUT = ROOT / 'build/x360/native-render-validation'
OUT.mkdir(exist_ok=True)
REPORT = ROOT / 'build/x360/native-render-final/compile-results.json'


class CPU(M.CPU):
    def step(self, pc, word, nxt, depth):
        if word >> 26 == 8:  # subfic: signed immediate minus GPR, without CR update
            dst, src = (word >> 21) & 31, (word >> 16) & 31
            imm = word & 0xffff
            if imm & 0x8000:
                imm -= 0x10000
            self.setg(dst, imm - self.g(src))
            return nxt
        return super().step(pc, word, nxt, depth)


class Coff:
    def __init__(self, path, prefix):
        self.path = path
        self.data = path.read_bytes()
        machine, nsec, _, symoff, nsyms, optsize, _ = struct.unpack_from('<HHIIIHH', self.data)
        assert machine == 0x1f2
        strings = self.data[symoff + nsyms * 18:]
        def name(raw):
            if raw[:4] == b'\0' * 4:
                at = struct.unpack_from('<I', raw, 4)[0]
                return strings[at:strings.index(b'\0', at)].decode()
            return raw.rstrip(b'\0').decode()
        self.sections = {i+1: struct.unpack_from('<8sIIIIIIHHI', self.data, 20+optsize+i*40) for i in range(nsec)}
        self.symbols = {}
        i = 0
        while i < nsyms:
            raw, value, sec, kind, storage, aux = struct.unpack_from('<8sIhHBB', self.data, symoff+i*18)
            self.symbols[i] = (name(raw), value, sec)
            i += 1 + aux
        matches = [v for v in self.symbols.values() if v[0].startswith(prefix) and v[2] > 0]
        exact = [v for v in matches if v[0] == prefix]
        matches = exact or matches
        assert len(matches) == 1, (prefix, matches)
        self.fn = matches[0]
        used, pending = set(), [self.fn[2]]
        while pending:
            sec = pending.pop()
            if sec in used:
                continue
            used.add(sec)
            for at, index, kind in self.relocations(sec):
                if kind == 0x12:
                    assert index == 0
                    continue
                symbol, value, target = self.symbols[index]
                if target > 0:
                    pending.append(target)
        bases = {sec: 0x83800000 + i*0x10000 for i, sec in enumerate(sorted(used))}
        self.images, self.unresolved = {}, {}
        for sec in used:
            s = self.sections[sec]
            assert s[3] < 0x10000
            body = bytearray(self.data[s[4]:s[4]+s[3]]) if s[4] else bytearray(s[3])
            for at, index, kind in self.relocations(sec):
                if kind == 0x12:
                    continue
                symbol, value, target = self.symbols[index]
                crt = re.fullmatch(r'__(save|rest)gprlr_(\d+)', symbol)
                if target > 0:
                    address = bases[target] + value
                elif crt:
                    address = (0x82c08eb0 if crt[1] == 'save' else 0x82c08f00) + 4*(int(crt[2])-14)
                else:
                    address = 0x84800000 + len(self.unresolved)*4
                    self.unresolved[address] = symbol
                word = struct.unpack_from('>I', body, at)[0]
                if kind in [0x10, 0x11]:
                    assert word & 0xffff == 0
                    immediate = (address+0x8000) >> 16 if kind == 0x10 else address
                    word = (word & 0xffff0000) | (immediate & 0xffff)
                elif kind == 6:
                    delta = address - (bases[sec]+at)
                    assert -(1 << 25) <= delta < (1 << 25)
                    word = (word & 0xfc000003) | (delta & 0x03fffffc)
                elif kind == 2:
                    word = (word+address) & 0xffffffff
                else:
                    raise AssertionError(('Unsupported PPC relocation', kind, symbol))
                struct.pack_into('>I', body, at, word)
            self.images[bases[sec]] = body
        self.entry = bases[self.fn[2]] + self.fn[1]

    def relocations(self, sec):
        s = self.sections[sec]
        return [struct.unpack_from('<IIH', self.data, s[5]+j*10) for j in range(s[7])]

    def cpu(self):
        c = M.new_cpu()
        c.__class__ = CPU
        for at, body in self.images.items():
            c.mem.add(at, len(body), body)
        def reject(symbol):
            def called(cpu):
                raise AssertionError(('Unresolved function executed', symbol))
            return called
        for at, symbol in self.unresolved.items():
            c.hooks[at] = reject(symbol)
        return c


def main():
    d = json.loads(REPORT.read_text(encoding='utf-8'))
    objects = {Path(x['source']).name: Path(x['object']) for x in d['results'] if x['passed']}
    specs = [
        ('depthstencilstate.cpp', '?GetResourceDescriptor@DepthStencilState@', 0x82b636f8, 'descriptor'),
        ('depthstencilstate.cpp', '?Initialize@DepthStencilState@', 0x82b62890, 'initialize'),
        ('rasterizerstate.cpp', '?GetResourceDescriptor@RasterizerState@', 0x82b63738, 'descriptor'),
        ('rasterizerstate.cpp', '?Initialize@RasterizerState@', 0x82b62958, 'initialize'),
        ('vertexdescriptorparameters.cpp', '??0Parameters@VertexDescriptor@', 0x82276870, 'constructor'),
        ('CgsRwVertexDescResourceType.cpp', '?GetParameters@VertexDescriptor@', 0x82b61260, 'parameters'),
        ('CgsRwVertexDescResourceType.cpp', '?GetResourceDescriptor@VertexDescriptor@', 0x82b637d0, 'vertexdescriptor'),
        ('programbuffer.cpp', '?GetResourceDescriptor@ProgramBuffer@', 0x82b634b0, 'programdescriptor'),
        ('CgsNetworkTexture.cpp', '?Construct@NetworkTexture@', 0x8287e5a0, 'constructor'),
        ('CgsNetworkTexture.cpp', '?Destruct@NetworkTexture@', 0x8287e678, 'destructor'),
        ('CgsNetworkTexture.cpp', '?GetBitsPerPixel@NetworkTexture@', 0x8287e6a8, 'bits'),
    ]
    rng = random.Random(360)
    errors, checked, details = [], 0, []
    for source, prefix, original, kind in specs:
        coff = Coff(objects[source], prefix)
        cases = 128 if kind != 'bits' else 6
        for index in range(cases):
            dest, params = bytearray(rng.randbytes(0x400)), bytearray(rng.randbytes(0x400))
            if kind == 'parameters':
                struct.pack_into('>H', dest, 8, index % 17)
            elif kind == 'vertexdescriptor':
                for element in range(16):
                    struct.pack_into('>I', params, 16*element+4, 0xffffffff if element >= index % 17 else rng.getrandbits(31))
            elif kind == 'programdescriptor':
                struct.pack_into('>I', params, 0, 0)
            outputs = []
            for compiled in [False, True]:
                c = coff.cpu()
                c.mem.wr(M.S.SELF, dest)
                c.mem.wr(M.S.AUX, params)
                c.mem.wr(M.S.AUX+0x1000, struct.pack('>I', M.S.SELF))
                c.setg(3, M.S.AUX+0x1000 if kind == 'initialize' else M.S.SELF)
                c.setg(4, M.S.AUX)
                if kind == 'bits':
                    c.setg(4, [0x1a200052,0x18280043,0x1a20000b,0x28280044,0x18280086,0x28280086][index])
                c.run(coff.entry if compiled else original)
                # Return values are compared only where the recovered C++ API returns one.
                ret = c.g(3) & 0xffffffff if kind in ['bits','initialize','descriptor','programdescriptor'] else None
                outputs.append((ret, c.mem.rd(M.S.SELF, 0x400), c.mem.rd(M.S.AUX, 0x400)))
            checked += 1
            if outputs[0] != outputs[1]:
                errors.append({'function': prefix, 'case': index, 'original_return': outputs[0][0], 'compiled_return': outputs[1][0],
                               'different_dest_bytes': [i for i,(a,b) in enumerate(zip(outputs[0][1],outputs[1][1])) if a != b],
                               'different_parameter_bytes': [i for i,(a,b) in enumerate(zip(outputs[0][2],outputs[1][2])) if a != b]})
        details.append({'source': source, 'symbol': coff.fn[0], 'original': hex(original), 'cases': cases,
                        'object': str(objects[source]), 'loaded_sections': len(coff.images)})
        print(prefix, cases, 'checked', flush=True)
    result = {'cases': checked, 'mismatches': errors, 'functions': details,
              'target': 'actual PPC COFF code vs ARTIST instructions',
              'limits': 'Program descriptor inline-data path only; six valid pixel formats; unresolved SDK/assert calls reject execution; no hardware or boot claim',
              'game_booted': False}
    (OUT/'comparison.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('Native render:', checked, 'cases;', len(errors), 'mismatches')
    if errors:
        print(json.dumps(errors[:6], indent=2))
    raise SystemExit(bool(errors))


if __name__ == '__main__':
    main()
