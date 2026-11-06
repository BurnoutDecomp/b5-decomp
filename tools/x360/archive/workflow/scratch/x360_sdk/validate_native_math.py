from pathlib import Path
import json
import math
import random
import re
import struct
import sys
import warnings

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'b5-decomp/build/x360/native-math-validation'
sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fixes/FX-GATE.sincos')
import sincos_emu as S
import vmxemu as V
sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fixes/FX-GATE.slerp')
from atc_update_emu import UpdateCPU
warnings.filterwarnings('ignore', category=RuntimeWarning)


class CPU(UpdateCPU):
    def exec_vector(self, address, word, mnemonic):
        if word >> 26 == 4 and (word & 0x7ff) == 970:
            dst, src, scale = (word >> 21) & 31, (word >> 11) & 31, (word >> 16) & 31
            values = []
            for x in self.vf(src):
                x = float(x) * (2 ** scale)
                y = 0 if math.isnan(x) else (0x7fffffff if x >= 2147483648.0 else
                    (-2147483648 if x < -2147483648.0 else int(x)))
                values.append(y & 0xffffffff)
            self.vr[dst] = tuple(values)
            return True
        if word >> 26 == 4 and (word & 0x3ff) == 966:
            # vcmpbfp: unordered or above +bound sets bit 31; unordered or
            # below -bound sets bit 30. ARTIST XMVectorTan uses the bit result.
            dst, lhs, rhs = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            a, b = self.vf(lhs), self.vf(rhs)
            self.vr[dst] = tuple((0x80000000 if not (x <= bound) else 0) |
                                 (0x40000000 if not (x >= -bound) else 0)
                                 for x, bound in zip(a, b))
            if word & 0x400:
                self.setcr(6, False, False, all(x == 0 for x in self.vr[dst]))
            return True
        return super().exec_vector(address, word, mnemonic)

assert V.x360rd.u32(0x82815570) == 0x7c8a2378
DLL = (OUT / 'native-math.dll').read_bytes()
PE = struct.unpack_from('<I', DLL, 0x3c)[0]
assert DLL[PE:PE+4] == b'PE\0\0'
assert struct.unpack_from('<H', DLL, PE + 4)[0] == 0x1f2
COUNT = struct.unpack_from('<H', DLL, PE + 6)[0]
OPTIONAL = PE + 24
IMAGE_BASE = struct.unpack_from('<I', DLL, OPTIONAL + 28)[0]
SECTION_TABLE = OPTIONAL + struct.unpack_from('<H', DLL, PE + 20)[0]
SECTIONS = []
for i in range(COUNT):
    at = SECTION_TABLE + i * 40
    virtual_size, rva, raw_size, raw_offset = struct.unpack_from('<IIII', DLL, at + 8)
    data = bytearray(max(virtual_size, raw_size))
    data[:raw_size] = DLL[raw_offset:raw_offset + raw_size]
    SECTIONS.append((IMAGE_BASE + rva, data))
SYMBOLS = {}
for line in (OUT / 'native-math.map').read_text().splitlines():
    m = re.match(r'\s+\d{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s', line)
    if m:
        SYMBOLS[m[1]] = int(m[2], 16)


def new_cpu():
    mem = V.Mem()
    mem.add(S.STACK, 0x20000)
    mem.add(S.SELF, 0x20000)
    mem.add(S.AUX, 0x20000)
    mem.add(0x8327F000, 0x200)
    for address, data in SECTIONS:
        mem.add(address, len(data), data)
    cpu = CPU(mem)
    cpu.setg(1, S.STACK + 0x10000)
    for init in (0x82c74000, 0x82c74030, 0x82c74060):
        cpu.run(init)
    return cpu


def bits(value):
    return struct.unpack('>I', struct.pack('>f', value))[0]


def same(a, b):
    return (math.isnan(a) and math.isnan(b)) or bits(a) == bits(b)


def value(word):
    return struct.unpack('>f', struct.pack('>I', word))[0]


def run_scalar(symbol, address, x):
    original, compiled = new_cpu(), new_cpu()
    original.setvf(1, [x] * 4)
    original.run(address)
    compiled.setf(1, x)
    compiled.run(SYMBOLS[symbol])
    return float(original.vf(1)[0]), compiled.f(1)


def run_sincos(symbol, x):
    results = []
    for address in (0x821f0c08, SYMBOLS[symbol]):
        cpu = new_cpu()
        cpu.setg(3, S.SELF)
        cpu.setg(4, S.SELF + 4)
        cpu.setf(1, x)
        cpu.run(address)
        results.append((value(cpu.rd32(S.SELF)), value(cpu.rd32(S.SELF + 4))))
    return results


def run_quaternion(symbol, original_address, matrix, offset):
    results = []
    for address in (original_address, SYMBOLS[symbol]):
        cpu = new_cpu()
        cpu.mem.wr(S.AUX, struct.pack('>12f', *matrix))
        cpu.setg(3, S.SELF)
        cpu.setg(4, S.AUX)
        cpu.run(address)
        results.append(struct.unpack('>4f', cpu.mem.rd(S.SELF + offset, 16)))
    return results


def main():
    random.seed(360)
    special = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, math.pi, -math.pi,
               value(1), value(0x80000001), value(0x007fffff),
               value(0x00800000), value(0x7f7fffff), math.inf, -math.inf, math.nan]
    checks = []
    mismatches = []
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 32
    for name, address, bound in [('ACos', 0x821f0980, 2), ('ATan', 0x821f0a70, 100),
                                 ('Sin', 0x821f05e0, 100), ('Cos', 0x821f06b0, 100),
                                 ('Tan', 0x821f0788, 10)]:
        cases = special + [value(bits(random.uniform(-bound, bound))) for _ in range(count)]
        for x in cases:
            a, b = run_scalar('Native' + name, address, x)
            if not same(a, b):
                mismatches.append({'routine': name, 'input': hex(bits(x)),
                                   'original': hex(bits(a)), 'compiled': hex(bits(b))})
        checks.append({'routine': name, 'cases': len(cases)})
        print(name, len(cases), 'checked', flush=True)
    cases = special + [value(bits(random.uniform(-100, 100))) for _ in range(count)]
    for x in cases:
        a, b = run_sincos('NativeScalarSinCos', x)
        if not all(same(y, z) for y, z in zip(a, b)):
            mismatches.append({'routine': 'ScalarSinCos', 'input': hex(bits(x)),
                               'original': [hex(bits(y)) for y in a],
                               'compiled': [hex(bits(y)) for y in b]})
    checks.append({'routine': 'ScalarSinCos', 'cases': len(cases)})
    matrices = [[1, 0, 0, 99, 0, 1, 0, 98, 0, 0, 1, 97],
                [1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0],
                [-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0],
                [-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0]]
    matrices += [[value(bits(random.uniform(-2, 2))) for _ in range(12)] for _ in range(count)]
    matrices += [[x] * 12 for x in special]
    for name, address, offset in [('SetChildAngularFrame', 0x8259b580, 0),
                                   ('SetParentAngularFrame', 0x8259b728, 32),
                                   ('SetParentLinearFrame', 0x8259b8d0, 64)]:
        symbol = next(s for s in SYMBOLS if s.startswith('?' + name + '@JointFrames'))
        for i, m in enumerate(matrices):
            a, b = run_quaternion(symbol, address, m, offset)
            if not all(same(y, z) for y, z in zip(a, b)):
                mismatches.append({'routine': name, 'case': i,
                                   'original': [hex(bits(y)) for y in a],
                                   'compiled': [hex(bits(y)) for y in b]})
        checks.append({'routine': name, 'cases': len(matrices)})
        print(name, len(matrices), 'checked', flush=True)
    report = {'kind': 'PPC interpreter comparison with original ARTIST instructions',
              'hardware_estimates': 'interpreter models, same on both sides',
              'nan_payloads_compared': False, 'checks': checks,
              'mismatches': mismatches, 'game_booted': False}
    (OUT / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Total cases', sum(r['cases'] for r in checks), 'mismatches', len(mismatches))
    if mismatches:
        print(json.dumps(mismatches[:8], indent=2))
    return bool(mismatches)


if __name__ == '__main__':
    raise SystemExit(main())
