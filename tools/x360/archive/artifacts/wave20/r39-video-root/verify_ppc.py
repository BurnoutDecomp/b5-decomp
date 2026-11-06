import collections
import hashlib
import json
import pathlib
import random
import re
import struct
import sys

from capstone import CS_ARCH_PPC, CS_MODE_64, CS_MODE_BIG_ENDIAN, Cs

here = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import x360rd

decoder = Cs(CS_ARCH_PPC, CS_MODE_64 | CS_MODE_BIG_ENDIAN)
mask = 0xffffffff
stack = 0x30000000
frame, source = 0x10000000, 0x11000000
texture, blocks, difference = 0x12000000, 0x13000000, 0x14000000
routine, table = 0x21001000, 0x21002000
texture_name = "?computeFrameTextureMap@CVideoFrameAnalyst@@QAAXPAE@Z"

def signed(value):
    value &= mask
    return (value ^ 0x80000000) - 0x80000000

class Program:
    def __init__(self):
        self.code, self.relocations, self.entries, self.hooks = {}, {}, {}, {}
        self.global_addr = self.scratch_offset = self.start = 0

    def add_code(self, address, payload):
        self.code.update((op.address, op) for op in decoder.disasm(payload, address))

def artist():
    program = Program()
    spans = [(0x82A47640, 0x82A486E0), (0x82A47050, 0x82A47238),
             (0x82A46610, 0x82A46710), (0x82A46710, 0x82A46AE8)]
    for start, end in spans:
        program.add_code(start, x360rd.rd(start, end - start))
    program.start = spans[0][0]
    program.entries = {"mean1": spans[2][0], "mean4": spans[3][0], "gradient": spans[1][0]}
    program.global_addr = 0x8322634C
    program.scratch_offset = 0x80
    for address in (0x82A47678, 0x82A47748, 0x82A47758, 0x82A47780, 0x82A477B8,
                    0x82A48604, 0x82A486C8, 0x82A486D0):
        op = program.code[address]
        target = int(op.op_str, 0)
        names = {0x82A47678: "malloc", 0x82A47780: "malloc", 0x82A47748: "memcpy",
                 0x82A47758: "memcpy", 0x82A477B8: "memset", 0x82A48604: "memset",
                 0x82A486C8: "free", 0x82A486D0: "free"}
        program.hooks[target] = names[address]
    for op in program.code.values():
        if op.mnemonic not in {"b", "bl"}:
            continue
        target = int(op.op_str, 0)
        if target not in program.code and target not in program.hooks:
            program.hooks[target] = "save" if op.mnemonic == "bl" else "restore"
    return program

def candidate():
    report = json.loads((here / "native/compile-results.json").read_text())
    program = Program()
    assert report["passed"] == 3 and report["failed"] == 0
    defs, pending = {}, {}
    for file_id, row in enumerate(report["results"]):
        if pathlib.Path(row["source"]).stem == "CVideoAnalyst":
            continue
        raw = pathlib.Path(row["object"]).read_bytes()
        machine, sections, _, syms, nsym, opt, _ = struct.unpack_from("<HHIIIHH", raw)
        assert machine == 0x1f2 and not opt
        strings = syms + nsym * 18
        base = lambda section: 0x40000000 + 0x01000000 * file_id + 0x10000 * section
        symbols, i = {}, 0
        while i < nsym:
            name, offset, section, _, _, aux = struct.unpack_from("<8sIhHBB", raw, syms + i * 18)
            if name[:4] == bytes(4):
                start = strings + struct.unpack_from("<I", name, 4)[0]
                name = raw[start:raw.index(b"\0", start)]
            name = name.rstrip(b"\0").decode("ascii")
            loc = base(section) + offset if section > 0 else None
            symbols[i] = (name, loc)
            if loc and name and not name.startswith("."):
                defs[name] = loc
            i += 1 + aux
        for section in range(sections):
            _, _, _, size, ptr, rel, _, nrel, _, flags = struct.unpack_from(
                "<8sIIIIIIHHI", raw, 20 + section * 40)
            if ptr and flags & 0x20:
                address = base(section + 1)
                program.add_code(address, raw[ptr:ptr + size])
                for j in range(nrel):
                    offset, si, kind = struct.unpack_from("<IIH", raw, rel + 10 * j)
                    if kind != 18:
                        pending[address + offset] = (kind, *symbols[si])
    program.start = defs[texture_name]
    program.entries = {key: next(a for n, a in defs.items() if n.startswith("?" + stem + "@"))
                       for key, stem in (("mean1", "calcMean4x4"), ("mean4", "calcMean4x4x4"),
                                         ("gradient", "calcGradient8x1B"))}
    program.global_addr = 0x21000000
    program.scratch_offset = 0x70
    for pc, (kind, name, target) in pending.items():
        target = target or defs.get(name)
        if not target:
            if name.startswith("?gpWMVFARoutines"):
                target = program.global_addr
            else:
                target = 0x70000000 + 4 * len(program.hooks)
                program.hooks[target] = ("save" if name.startswith("__savegprlr") else
                                         "restore" if name.startswith("__restgprlr") else name)
        program.relocations[pc] = (kind, name, target)
    return program

class Machine:
    def __init__(self, program, width, height, pixels, scratch, fail):
        self.program, self.fail = program, fail
        self.memory, self.calls = {}, collections.Counter()
        self.allocations, self.freed, self.heap = [], [], 0x50000000
        self.r, self.cr, self.ctr, self.carry = [0] * 32, [0] * 8, 0, 0
        self.r[1], self.r[3], self.r[4] = stack, frame, source
        self.put(frame + 0, texture)
        self.put(frame + 4, blocks)
        self.put(frame + 12, difference)
        self.put(frame + 0x4020, width)
        self.put(frame + 0x4024, height)
        self.put(program.global_addr, routine)
        self.put(routine, table)
        self.put(table, program.entries["mean4"])
        self.put(table + 12, program.entries["gradient"])
        self.store(source, pixels)
        self.store(frame + 24, bytes([0xc7]) * 0x4000)
        for address in (texture, blocks, difference):
            self.store(address, bytes([0xa5]) * 4096)
        adjust = next(op for pc in range(program.start, program.start + 40, 4)
                      if (op := program.code[pc]).mnemonic == "stwu")
        displacement = int(adjust.op_str.split(", ")[1].split("(")[0], 0)
        for i, word in enumerate(scratch):
            self.put(stack + displacement + program.scratch_offset + 4 * i, word)

    def put(self, address, word):
        self.store(address, (word & mask).to_bytes(4, "big"))

    def store(self, address, payload):
        self.memory.update(((address + i) & mask, value) for i, value in enumerate(payload))

    def read(self, address, count):
        return bytes(self.memory.get((address + i) & mask, 0) for i in range(count))

    def hook(self, name):
        r = self.r
        self.calls[name] += 1
        if name == "malloc":
            if self.calls[name] == self.fail:
                r[3] = 0
            else:
                size = r[3]
                r[3] = self.heap
                self.allocations.append((r[3], size))
                self.store(r[3], bytes([0x6d]) * size)
                self.heap += (size + 255) & ~255
        elif name == "free":
            self.freed.append(r[3])
        elif name == "memcpy":
            self.store(r[3], self.read(r[4], r[5]))
        elif name == "memset":
            self.store(r[3], bytes([r[4] & 255]) * r[5])
        else:
            raise ValueError(name)

    def run(self, pc):
        r, cr, program = self.r, self.cr, self.program
        saved = (r[1], r[14:])
        link = 0
        count = 0
        reg = lambda arg: int(arg[1:])
        imm = lambda arg: int(arg, 0)
        def ra(arg):
            i = reg(arg)
            return r[i] if i else 0
        try:
            while True:
                count += 1
                if count > 1500000:
                    raise ValueError(f"operation limit {pc:x}")
                op = program.code[pc]
                name, args = op.mnemonic, op.op_str.split(", ")
                rel = program.relocations.get(pc)
                nxt = pc + 4
                if name in {"b", "bl", "bctrl"}:
                    target = rel[2] if rel else self.ctr if name == "bctrl" else imm(args[0])
                    hook = program.hooks.get(target)
                    if hook == "restore":
                        return
                    if hook == "save":
                        pass
                    elif hook:
                        self.hook(hook)
                        if name == "b":
                            return
                    elif name == "b":
                        nxt = target
                    else:
                        link = nxt
                        self.run(target)
                elif name in {"beq", "bne", "bge", "blt", "bgt", "ble"}:
                    field = int(args[0][2:]) if len(args) > 1 else 0
                    c = cr[field]
                    take = {"beq": c == 0, "bne": c != 0, "bge": c >= 0,
                            "blt": c < 0, "bgt": c > 0, "ble": c <= 0}
                    if take[name]:
                        nxt = imm(args[-1])
                elif name == "blr":
                    return
                elif name in {"cmpw", "cmpwi", "cmplw", "cmplwi"}:
                    field = int(args.pop(0)[2:]) if len(args) == 3 else 0
                    a, b = r[reg(args[0])], imm(args[1]) if name.endswith("i") else r[reg(args[1])]
                    if "l" not in name:
                        a, b = signed(a), signed(b)
                    cr[field] = (a > b) - (a < b)
                elif name in {"lwz", "lwzx", "lbz", "lbzx", "stw", "stwx", "stwu", "stb", "stbx", "std"}:
                    d = reg(args[0])
                    if name.endswith("x"):
                        address = ra(args[1]) + r[reg(args[2])]
                    else:
                        disp, base_reg = re.fullmatch(r"(.*?)\((r\d+)\)", args[1]).groups()
                        if rel:
                            val = rel[2] & 0xffff
                            disp = val - 0x10000 if val > 0x7fff else val
                        else:
                            disp = imm(disp)
                        address = ra(base_reg) + disp
                    n = 1 if name[1] == "b" or name.startswith("stb") else 8 if name == "std" else 4
                    if name.startswith("l"):
                        r[d] = int.from_bytes(self.read(address, n), "big")
                    else:
                        self.store(address, (r[d] & ((1 << (n * 8)) - 1)).to_bytes(n, "big"))
                        if name == "stwu":
                            r[reg(base_reg)] = address
                elif name == "mtctr":
                    self.ctr = r[reg(args[0])]
                else:
                    d = reg(args[0])
                    if name == "mflr":
                        value = link
                    elif name in {"li", "lis"}:
                        value = imm(args[1]) << (16 if name == "lis" else 0)
                        if rel:
                            target = rel[2]
                            value = ((target + 0x8000) & 0xffff0000) if name == "lis" else target & 0xffff
                    elif name == "mr":
                        value = r[reg(args[1])]
                    else:
                        a = r[reg(args[1])]
                        b = (r[reg(args[2])] if args[2].startswith("r") else imm(args[2])) if len(args) > 2 else 0
                        if rel:
                            b = rel[2] & 0xffff
                            if b > 0x7fff:
                                b -= 0x10000
                        plain = name.rstrip(".")
                        if plain in {"add", "addi", "addic"}:
                            value = a + b
                        elif plain in {"subf", "subfc", "subfe", "subfic"}:
                            value = (~a & mask) + (b & mask) + (self.carry if plain == "subfe" else 1)
                            if plain != "subf":
                                self.carry = int(value > mask)
                        elif plain in {"mullw", "mulli"}:
                            value = a * b
                        elif plain in {"srawi", "srwi"}:
                            value = (signed(a) if plain == "srawi" else a) >> b
                        elif plain in {"slwi", "slw"}:
                            value = a << b
                        elif plain == "clrlwi":
                            value = a & (mask >> b)
                        elif plain == "clrrwi":
                            value = a & (mask << b)
                        elif plain == "rlwinm":
                            low, high = imm(args[3]), imm(args[4])
                            bits = [i for i in range(32) if (low <= i <= high if low <= high else i <= high or i >= low)]
                            value = ((a << b) | (a >> (32 - b))) & sum(1 << (31 - i) for i in bits)
                        elif plain == "xor":
                            value = a ^ b
                        elif plain in {"and", "andi"}:
                            value = a & b
                        elif plain == "or":
                            value = a | b
                        elif plain == "neg":
                            value = -a
                        else:
                            raise ValueError(f"{pc:08x} {name} {op.op_str}")
                    r[d] = value & mask
                    if name.endswith("."):
                        v = signed(value)
                        cr[0] = (v > 0) - (v < 0)
                pc = nxt
        finally:
            r[1], r[14:] = saved

def main():
    old, new = artist(), candidate()
    results = []
    for width, height in ((16, 16), (32, 32), (48, 32), (64, 48), (80, 48), (38, 26), (31, 25)):
        for seed in (1, 2):
            rng = random.Random(seed)
            pixels = rng.randbytes(4 * width * height + 1024)
            scratch = [rng.getrandbits(32) for _ in range(8)]
            for failure in ((0, 1, 2) if (width, height, seed) == (32, 32, 1) else (0,)):
                cases = [Machine(program, width, height, pixels, scratch, failure) for program in (old, new)]
                for m in cases:
                    m.run(m.program.start)
                output = [(m.read(texture, 4096), m.read(blocks, 4096), m.read(difference, 4096),
                           m.read(frame + 24, 0x4000), m.allocations, m.freed) for m in cases]
                labels = ("texture", "activity", "gradient", "histogram", "allocation sizes", "free order")
                mismatches = [name for name, a, b in zip(labels, *output) if a != b]
                if mismatches:
                    for name, a, b in zip(labels, *output):
                        if a != b and isinstance(a, bytes):
                            print(name, [(i, a[i:i+4].hex(), b[i:i+4].hex()) for i in range(0, len(a), 4)
                                         if a[i:i+4] != b[i:i+4]][:16])
                    raise ValueError((width, height, seed, failure, mismatches))
                results.append({"width": width, "height": height, "seed": seed, "malloc_failure": failure,
                                "matched_outputs": labels, "output_sha256": hashlib.sha256(b"".join(output[0][:4])).hexdigest()})
                print("match", width, height, seed, failure, flush=True)
    (here / "differential.json").write_text(json.dumps(results, indent=2) + "\n")

if __name__ == "__main__":
    main()
