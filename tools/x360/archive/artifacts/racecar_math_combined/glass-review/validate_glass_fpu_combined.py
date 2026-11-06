"""Whole actual GlassFracture native function vs ARTIST, including true vector ABI.

Shader services are observed at their real ABI boundary and clobber volatile registers.
"""
from pathlib import Path
import ast
import hashlib
import json
import math
import random
import re
import struct
import sys
import warnings

PARENT = Path(r"C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow")
NOTES = Path(r"C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/parallel-native/vector_abi_sol")
OUT = PARENT / "b5-decomp/build/x360/parallel/vector_abi_sol"
SELF, STACK = 0x40000000, 0x44000000
sys.path.insert(0, r"D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu")
import emu64 as E
import vmxemu as V

warnings.filterwarnings("ignore", category=RuntimeWarning)
assert V.x360rd.u32(0x82815570) == 0x7C8A2378
tree = ast.parse((NOTES / "validate_abi_candidate.py").read_text(encoding="utf-8"))
nodes = [node for node in tree.body if
         isinstance(node, ast.ClassDef) and node.name in ("CPU", "M") or
         isinstance(node, ast.FunctionDef) and node.name == "new_cpu"]
exec(compile(ast.Module(body=nodes, type_ignores=[]), "abi-fixture", "exec"), globals())
loader = PARENT / "scratch/x360_sdk/validate_render_states.py"
tree = ast.parse(loader.read_text(encoding="utf-8"))
coff_class = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == "Coff")
coff_source = ast.get_source_segment(loader.read_text(encoding="utf-8"), coff_class)
old_address = "address = 0x84800000 + len(self.unresolved)*4"
assert coff_source.count(old_address) == 1
# The historical loader allocates each external relocation separately. Reuse the
# same external symbol address, as a real linker does (three shader calls + table HI/LO).
coff_source = coff_source.replace(old_address, "address = next((existing_address for existing_address, existing_symbol in self.unresolved.items() if existing_symbol == symbol), 0x84800000 + len(self.unresolved)*4)")
exec(compile(coff_source, "coff-with-real-external-symbol-aliasing", "exec"), globals())

PROOF = Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined/glass-review")
obj = PROOF / "provider-5cee0a27.obj"
assert hashlib.sha256(obj.read_bytes()).hexdigest() == "5cee0a272621525a7564f342116c15ce85d181770db24e578c7fdfbd10f619c0"
coff = Coff(obj, "?SetGlassFractureConstants@BrnWorld@@")
assert "Vector2Template" in coff.fn[0] and "@fpu@math@rw@@" in coff.fn[0], coff.fn[0]
oldobj = PARENT / "b5-decomp/build/x360/parallel/racecar_layout_sol/racecar_aggregate_clean/BrnRaceCarEntityModule_GlassFracture-607b8315.obj"
assert hashlib.sha256(oldobj.read_bytes()).hexdigest() == "4c9ef1623d668ae8916029dde2c1f4f64cdea0d5c1d9e61773907f97b83f4d7a"
oldcoff = Coff(oldobj, "?SetGlassFractureConstants@BrnWorld@@")
START, END = 0x822BD280, 0x822BD36C

def branch(pc):
    word = V.x360rd.u32(pc)
    delta = word & 0x03FFFFFC
    if delta & 0x02000000:
        delta -= 0x04000000
    return (pc + delta) & 0xFFFFFFFF

shader_target = branch(0x822BD300)
assert shader_target == branch(0x822BD310) == branch(0x822BD344)
table_hi, table_lo = V.x360rd.u32(0x822BD2D4) & 0xFFFF, V.x360rd.u32(0x822BD2E4) & 0xFFFF
table_address = ((table_hi << 16) + (table_lo - 0x10000 if table_lo & 0x8000 else table_lo)) & 0xFFFFFFFF
symbol_addresses = {"__real@00000000": 0x82001CC0, "__real@3f800000": 0x82001C98,
                    "?mShaderConstantTable@CgsGraphics@@3UShaderConstantTable@@A": table_address,
                    "?SetShaderConstantData@ShaderConstantTable@@QAAXIUVector4@vpu@math@rw@@@Z": shader_target,
                    "?SetShaderConstantData@ShaderConstantTable@@QAAXIVVector4@vpu@math@rw@@@Z": shader_target}
assert V.x360rd.u32(symbol_addresses["__real@00000000"]) == 0
assert V.x360rd.u32(symbol_addresses["__real@3f800000"]) == 0x3F800000
section = coff.fn[2]
s = coff.sections[section]
candidate_bytes = bytearray(coff.data[s[4]:s[4] + s[3]])
assert len(candidate_bytes) == END - START
relocations = []
for at, index, kind in coff.relocations(section):
    if kind == 0x12:
        continue
    symbol = coff.symbols[index][0]
    address = symbol_addresses[symbol]
    word = struct.unpack_from(">I", candidate_bytes, at)[0]
    if kind in [0x10, 0x11]:
        immediate = (address + 0x8000) >> 16 if kind == 0x10 else address
        word = (word & 0xFFFF0000) | (immediate & 0xFFFF)
    elif kind == 6:
        word = (word & 0xFC000003) | ((address - (START + at)) & 0x03FFFFFC)
    else:
        raise AssertionError((kind, symbol))
    struct.pack_into(">I", candidate_bytes, at, word)
    relocations.append(dict(offset=hex(at), kind=hex(kind), symbol=symbol, original_target=hex(address)))
original_bytes = V.x360rd.rd(START, END - START)
differences = [i for i, (a, b) in enumerate(zip(candidate_bytes, original_bytes)) if a != b]
(PROOF / "glass-original.bin").write_bytes(original_bytes)
(PROOF / "glass-relocated-candidate.bin").write_bytes(candidate_bytes)

def fval(word):
    return struct.unpack(">f", struct.pack(">I", word))[0]

rng = random.Random(0x822BD280)
special = [0, 0x80000000, 1, 0x80000001, 0x007FFFFF, 0x00800000,
           0x3E800000, 0x3F000000, 0x3F7FFFFF, 0x3F800000, 0x3F800001,
           0xBF800000, 0x40000000, 0x7F7FFFFF, 0x7F800000, 0xFF800000,
           0x7FC12345, 0x7F812345]
cases = [(special[i % len(special)], special[(i // len(special)) % len(special)],
          special[(i * 7) % len(special)], special[(i * 11 + 3) % len(special)],
          rng.randbytes(16), bool(i & 1)) for i in range(324)]
cases += [(rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32), rng.randbytes(16), bool(i & 1)) for i in range(700)]

def run(which, case):
    strength, equalization, uvx, uvy, offsets, mutate = case
    object = coff if which == "candidate" else oldcoff
    cpu = object.cpu() if which != "original" else new_cpu()
    cpu.mem.wr(SELF, struct.pack(">2I", uvx, uvy) + b"\xA5" * 8)
    for i in range(3, 11):
        cpu.setg(i, 0xFED00000 + i * 0x10)
    cpu.setg(5, SELF)
    cpu.setf(1, fval(strength))
    cpu.setf(2, fval(equalization))
    cpu.vr[1] = struct.unpack(">4I", offsets)
    trace = []
    if which == "original":
        expected_table = table_address
        address = shader_target
    else:
        expected_table = next(at for at, name in object.unresolved.items() if name == "?mShaderConstantTable@CgsGraphics@@3UShaderConstantTable@@A")
        address = next(at for at, name in object.unresolved.items() if name.startswith("?SetShaderConstantData@ShaderConstantTable@@"))
    def shader(c):
        assert c.g(3) & 0xFFFFFFFF == expected_table, (hex(c.g(3)), hex(expected_table))
        trace.append((c.g(4), tuple(c.vr[1])))
        if mutate and len(trace) == 2:
            c.mem.wr(SELF, struct.pack(">2I", uvx ^ 0x80000000, uvy ^ 0x80000000))
        for i in range(3, 13):
            c.setg(i, 0xBAD00000 + i)
        for i in range(14):
            c.setf(i, i + 0.25)
        for i in range(1, 6):
            c.vr[i] = (0xA5000000 + i,) * 4
    cpu.hooks[address] = shader
    cpu.run(START if which == "original" else object.entry)
    assert cpu.mem.rd(SELF + 8, 8) == b"\xA5" * 8
    return trace

errors, negative = [], 0
for index, case in enumerate(cases):
    original, actual = run("original", case), run("candidate", case)
    if original != actual:
        errors.append(dict(case=index, original=original, candidate=actual))
    assert [item[0] for item in original] == [30, 31, 32]
    assert original[1][1] == struct.unpack(">4I", case[4])
    if index < 128:
        negative += run("baseline", case) != original

result = dict(actual_symbol=coff.fn[0], reference_type="rw::math::fpu::Vector2Template<float> const&", reference_bytes=8, object=str(obj), object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
              original_function="BrnWorld::SetGlassFractureConstants", original_start=hex(START),
              full_cpp_native_pass=True, cases=len(cases), mismatches=errors,
              baseline_wrong_aggregate_negative_controls=negative,
              exact_relocated_instruction_bytes=len(original_bytes) if not differences else 0,
              relocated_bytes_sha256=hashlib.sha256(candidate_bytes).hexdigest(),
              original_bytes_sha256=hashlib.sha256(original_bytes).hexdigest(), differing_byte_offsets=differences,
              relocations=relocations,
              coverage=["f1/f2 scalar parameters, genuine FPU8 referenced UV pair in r5 with guard bytes; Vector4 in v1 and all three shader calls in v1.",
                        "Shader slot/order/128-bit vector payloads, including unmodified raw offset NaN payload bits.",
                        "Positive/zero/negative/subnormal/infinite/NaN strengths, equalization and UV inputs; same instruction interpreter on both sides.",
                        "Services clobber volatile GPR/FPR/vector registers; half the cases mutate referenced UV values before final reload."],
              limits=["ShaderConstantTable callee/GPU behavior is observed at its boundary, not executed.",
                      "Scalar NaN behavior and hardware estimates follow the shared interpreter model; no physical Xbox execution or game boot.",
                      "Byte match covers only this236-byte function after genuine original symbol relocation, not all vector APIs."])
(PROOF / "glass-fpu-caller-validation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
assert not errors and negative == 128
assert not differences
