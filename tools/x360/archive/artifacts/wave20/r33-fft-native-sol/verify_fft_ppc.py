
import argparse
from collections import Counter
import ctypes
import json
import math
from pathlib import Path
import random
import re
import struct

from capstone import CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN, Cs

ARTIST = {
    'ForwardComplex': 0x82B689C8, 'ForwardReal': 0x82B68F98,
    'InverseComplex': 0x82B69098, 'InverseReal': 0x82B69698,
}
STATE, BUFFER, STACK = 0x10000000, 0x20000000, 0x30000000
MASK = (1 << 64) - 1
fma = ctypes.CDLL('ucrtbase').fmaf
fma.restype = ctypes.c_float
fma.argtypes = [ctypes.c_float] * 3
decoder = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)


def signed(value, bits=32):
    value &= (1 << bits) - 1
    return (value ^ (1 << (bits - 1))) - (1 << (bits - 1))


def f32(value):
    return ctypes.c_float(value).value


def ppc_float_bits(value):
    return struct.pack('>f', value)


class Program:
    def __init__(self):
        self.entries, self.code, self.data, self.relocations = {}, {}, {}, {}
        self.helpers = {}

    def add_code(self, address, data):
        self.code.update((op.address, op) for op in decoder.disasm(data, address))

    def add_data(self, address, data):
        self.data.update((address + i, byte) for i, byte in enumerate(data))


def load_artist(path):
    payload = json.loads(path.read_text())
    program = Program()
    for name, address in ARTIST.items():
        program.entries[name] = address
        program.add_code(address, bytes.fromhex(payload[hex(address)]))
    for address in (0x8214B1D4, 0x82001DA0, 0x82001C98):
        program.add_data(address, bytes.fromhex(payload[hex(address)]))
    for address, op in program.code.items():
        if op.mnemonic not in ('b', 'bl'):
            continue
        target = int(op.op_str, 0)
        if target not in program.code:
            program.helpers[target] = 'save' if op.mnemonic == 'bl' else 'restore'
    assert len(program.helpers) == 4, program.helpers
    return program


def load_candidate(path):
    data = path.read_bytes()
    machine, sections, _, syms, nsym, opt, _ = struct.unpack_from('<HHIIIHH', data)
    assert machine == 0x1F2 and opt == 0
    strings = syms + nsym * 18
    symbol = {}
    i = 0
    while i < nsym:
        name, offset, section, _, _, aux = struct.unpack_from('<8sIhHBB', data, syms + i * 18)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from('<I', name, 4)[0]
            name = data[start:data.index(b'\0', start)]
        symbol[i] = (name.rstrip(b'\0').decode('ascii'), section, offset)
        i += 1 + aux
    program = Program()
    base = lambda section: 0x40000000 + 0x10000 * section
    for i in range(sections):
        _, _, _, size, ptr, rel, _, nrel, _, flags = struct.unpack_from(
            '<8sIIIIIIHHI', data, 20 + i * 40)
        address = base(i + 1)
        if ptr and flags & 0x20:
            program.add_code(address, data[ptr:ptr + size])
        if ptr and flags & 0x40:
            program.add_data(address, data[ptr:ptr + size])
        for j in range(nrel):
            offset, si, kind = struct.unpack_from('<IIH', data, rel + 10 * j)
            if address + offset in program.code and kind != 18:
                name, section, value = symbol[si]
                program.relocations[address + offset] = (kind, name, base(section) + value if section > 0 else None)
    for name, section, offset in symbol.values():
        if section > 0:
            for key in ARTIST:
                if name.startswith('?FFT_' + key + '@'):
                    program.entries[key] = base(section) + offset
    assert program.entries.keys() == ARTIST.keys()
    return program


class Machine:
    def __init__(self, program, table, samples):
        self.program = program
        self.r = [0] * 32
        self.f = [0.0] * 32
        self.cr = [0] * 8
        self.carry = 0
        self.r[1], self.r[3], self.r[4] = STACK, STATE, BUFFER
        self.memory = dict(program.data)
        self.store(STATE, table)
        self.store(BUFFER, samples)
        self.buffer_reads = set()
        self.buffer_writes = set()

    def store(self, address, data):
        self.memory.update(((address + i) & 0xffffffff, byte) for i, byte in enumerate(data))

    def read(self, address, count):
        if BUFFER <= address < BUFFER + 0x100000:
            self.buffer_reads.update(range(address - BUFFER, address - BUFFER + count))
        return bytes(self.memory.get((address + i) & 0xffffffff, 0) for i in range(count))

    def run(self, entry):
        saved = self.r[14:].copy()
        saved_sp = self.r[1]
        self.execute(entry)
        self.r[14:] = saved
        self.r[1] = saved_sp

    def execute(self, pc):
        r, f, cr = self.r, self.f, self.cr
        program = self.program
        link = 0
        while True:
            op = program.code[pc]
            name, args = op.mnemonic, op.op_str.split(', ')
            next_pc = pc + 4
            rel = program.relocations.get(pc)

            def reg(arg):
                return int(arg[1:])

            def imm(arg):
                return int(arg, 0)

            def address(arg):
                offset, base = re.fullmatch(r'(.*?)\(r(\d+)\)', arg).groups()
                displacement = signed(rel[2], 16) if rel else int(offset, 0)
                return (r[int(base)] + displacement) & MASK

            if name in ('b', 'bl'):
                if rel:
                    assert rel[0] == 6, rel
                    kind = 'save' if rel[1].startswith('__savegprlr') else 'restore' if rel[1].startswith('__restgprlr') else None
                    target = rel[2]
                else:
                    target = imm(args[0])
                    kind = program.helpers.get(target)
                if kind == 'save':
                    pass
                elif kind == 'restore':
                    return
                elif name == 'bl':
                    link = next_pc
                    self.run(target)
                elif target in program.entries.values() or rel:
                    self.run(target)
                    return
                else:
                    next_pc = target
            elif name in ('bge', 'bgt', 'ble', 'blt', 'bne', 'beq'):
                field = int(args[0][2:]) if len(args) == 2 else 0
                c = cr[field]
                take = {'bge': c >= 0, 'bgt': c > 0, 'ble': c <= 0, 'blt': c < 0,
                        'bne': c != 0, 'beq': c == 0}
                if take[name]:
                    next_pc = imm(args[-1])
            elif name == 'blr':
                return
            elif name in ('cmpw', 'cmpwi', 'cmplw', 'cmplwi'):
                field = int(args.pop(0)[2:]) if len(args) == 3 else 0
                left = r[reg(args[0])]
                right = imm(args[1]) if name.endswith('i') else r[reg(args[1])]
                a, b = ((left & 0xffffffff, right & 0xffffffff) if 'l' in name else
                        (signed(left), signed(right)))
                cr[field] = (a > b) - (a < b)
            elif name in ('lwz', 'lfs', 'lfd', 'lfsx'):
                a = (r[reg(args[1])] + r[reg(args[2])]) & MASK if name.endswith('x') else address(args[1])
                if name == 'lwz':
                    r[reg(args[0])] = struct.unpack('>I', self.read(a, 4))[0]
                elif name == 'lfd':
                    f[reg(args[0])] = struct.unpack('>d', self.read(a, 8))[0]
                else:
                    f[reg(args[0])] = struct.unpack('>f', self.read(a, 4))[0]
            elif name in ('stw', 'stwu', 'std', 'stfs', 'stfsx', 'stfd'):
                a = (r[reg(args[1])] + r[reg(args[2])]) & MASK if name.endswith('x') else address(args[1])
                if name.startswith('stf'):
                    value = f[reg(args[0])]
                    data = struct.pack('>d' if name == 'stfd' else '>f', value)
                else:
                    count = 8 if name == 'std' else 4
                    data = (r[reg(args[0])] & ((1 << (count * 8)) - 1)).to_bytes(count, 'big')
                self.store(a, data)
                if name == 'stwu':
                    r[int(args[1].split('(r')[1][:-1])] = a
                if BUFFER <= a < BUFFER + 0x100000:
                    self.buffer_writes.add(a - BUFFER)
            elif name.startswith('f'):
                d = reg(args[0])
                if name == 'fcfid':
                    f[d] = float(struct.unpack('>q', struct.pack('>d', f[reg(args[1])]))[0])
                else:
                    values = [f[reg(arg)] for arg in args[1:]]
                    a = values[0]
                    if name == 'fneg':
                        f[d] = -a
                    elif name == 'frsp':
                        f[d] = f32(a)
                    else:
                        b = values[1]
                        if name in ('fmadds', 'fmsubs', 'fnmadds', 'fnmsubs'):
                            c = values[2]
                            value = fma(a, b, -c if 'sub' in name else c)
                            f[d] = -value if name.startswith('fn') else value
                        else:
                            f[d] = f32({'fadds': lambda: a + b, 'fsubs': lambda: a - b,
                                        'fmuls': lambda: a * b, 'fdivs': lambda: a / b}[name]())
            else:
                d = reg(args[0])
                if name == 'mflr':
                    value = link
                elif name in ('lis', 'li'):
                    if rel:
                        assert rel[0] in (16, 17), (hex(pc), op.op_str, rel)
                        value = (rel[2] + 0x8000) & 0xffff0000 if name == 'lis' else signed(rel[2], 16)
                    else:
                        value = imm(args[1]) << (16 if name == 'lis' else 0)
                else:
                    a = r[reg(args[1])]
                    if name == 'mr':
                        value = a
                    elif name == 'extsw':
                        value = signed(a)
                    elif name == 'addze':
                        value = a + self.carry
                    else:
                        b = (signed(rel[2], 16) if rel else
                             r[reg(args[2])] if args[2].startswith('r') else imm(args[2]))
                        if name in ('add', 'addi', 'addic.'):
                            value = a + b
                        elif name in ('subf', 'subfic'):
                            value = b - a
                        elif name in ('slw', 'slw.', 'slwi'):
                            value = (a << b) & 0xffffffff if b < 32 else 0
                        elif name == 'srwi':
                            value = (a & 0xffffffff) >> b
                        elif name in ('srawi', 'srawi.'):
                            value = signed(a) >> b
                            self.carry = int(signed(a) < 0 and (a & ((1 << b) - 1)) != 0)
                        elif name in ('rlwinm', 'rlwinm.'):
                            a &= 0xffffffff
                            bits = {i % 32 for i in range(imm(args[3]), imm(args[4]) + 1)}
                            value = ((a << b) | (a >> (32 - b))) & sum(1 << (31 - i) for i in bits)
                        else:
                            raise ValueError(f'{pc:x} {name} {op.op_str}')
                r[d] = value & MASK
                if name.endswith('.'):
                    v = signed(value)
                    cr[0] = (v > 0) - (v < 0)
            pc = next_pc


def make_state(log2):
    count = 1 << log2
    twiddles = count // 2 + 1
    cos_offset = 24
    sin_offset = cos_offset + 4 * twiddles
    perm_offset = sin_offset + 4 * twiddles
    table = struct.pack('>6I', log2, twiddles, count, cos_offset, sin_offset, perm_offset)
    angles = [f32(f32(i * f32(math.pi)) / count) for i in range(twiddles)]
    for fn in (math.cos, math.sin):
        table += b''.join(ppc_float_bits(fn(angle)) for angle in angles)
    return table + b''.join(struct.pack('>I', int(f'{i:0{log2}b}'[::-1], 2)) for i in range(count))


def verify(artist, candidate, log2s, random_cases):
    rng = random.Random(0x82B689C8)
    totals = Counter(cases=0, mismatching_cases=0, mismatching_words=0)
    for log2 in log2s:
        count = 1 << log2
        table = make_state(log2)
        length = max(16, 2 * count + 2)
        cases = [
            [0.0] * length,
            [1.0] + [0.0] * (length - 1),
            [float((-1) ** i) for i in range(length)],
            *([rng.uniform(-2, 2) for _ in range(length)] for _ in range(random_cases)),
        ]
        for name in ARTIST:
            expected_max = max(2 * count, 16) - 1 if name.endswith('Complex') else length - 1
            for case_index, case in enumerate(cases):
                guard = bytes.fromhex('deadc0de' * 4)
                data = b''.join(ppc_float_bits(v) for v in case) + guard
                machines = [Machine(artist, table, data), Machine(candidate, table, data)]
                for machine in machines:
                    machine.run(machine.program.entries[name])
                    assert not machine.buffer_writes or max(machine.buffer_writes) // 4 == expected_max, (
                        name, log2, case_index, max(machine.buffer_writes) // 4, expected_max)
                    assert max(machine.buffer_reads) // 4 == expected_max
                    assert machine.read(BUFFER + 4 * length, len(guard)) == guard
                expected, actual = [machine.read(BUFFER, 4 * length) for machine in machines]
                differences = [i for i in range(length)
                               if expected[4*i:4*i+4] != actual[4*i:4*i+4]]
                if differences:
                    for index in differences[:8]:
                        print(name, 'N', count, 'case', case_index, 'index', index,
                              expected[4*index:4*index+4].hex(), actual[4*index:4*index+4].hex())
                    totals['mismatching_cases'] += 1
                    totals['mismatching_words'] += len(differences)
                totals['cases'] += 1
    return dict(totals)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('artist', type=Path)
    parser.add_argument('object', type=Path)
    parser.add_argument('--log2', type=int, nargs='+', default=[1, 2, 3, 4, 5, 6, 8, 10])
    parser.add_argument('--random', type=int, default=2)
    args = parser.parse_args()
    result = verify(load_artist(args.artist), load_candidate(args.object), args.log2, args.random)
    print(json.dumps(result))
    return bool(result.get('mismatching_cases'))


if __name__ == '__main__':
    raise SystemExit(main())

