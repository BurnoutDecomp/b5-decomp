"""Prove native stream and world status code in the compiled object and linked image."""
import json
from pathlib import Path
import re
import runpy
import struct
import sys

here = Path(__file__).resolve().parent
wave = here.parent
reader = wave / "r125-batch/verify_boot_world.py"
base_helpers = {"__file__": str(reader)}
exec(reader.read_text(encoding="utf-8-sig").split("\nlink = Path(sys.argv[1])", 1)[0], base_helpers)
inspect = base_helpers["inspect"]
read_original = base_helpers["READ_ORIGINAL"]
sha = base_helpers["sha"]
coff = base_helpers["COFF"]

link = Path(sys.argv[1]).resolve()
base = wave / "r125-link"
intake = wave / "r125-intake"
sources = {
    "stream": "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
    "world": "src/GameSource/World/BrnWorldModuleIO_UpdateOutputBuffer.cpp",
}
baseline = json.loads((intake / "composite-active-native.json").read_text())
old = {r["source"]: Path(r["object"]) for r in baseline["results"]}
result = json.loads((link / "result.json").read_text())
targeted = "preflight_replacements" in result
if targeted:
    new = {r["source"]: Path(r["replacement"]) for r in result["preflight_replacements"]}
    assert set(new) == set(sources.values()) and result["base_result_sha256"] == sha(base / "result.json")
else:
    invocation = json.loads((link / "invocation.json").read_text())
    new = {r["source"]: Path(r["object"]) for r in invocation["objects"]}

def compare(provider, symbol, address, size):
    source = sources[provider]
    before, _, _ = inspect(base, old[source], symbol, address, size)
    after, _, listing = inspect(link, new[source], symbol, address, size)
    return {"before": before, "after": after}, listing

ctor, ctor_asm = compare("stream", b"??0StrStream@CgsDev@@", 0x821f0158, 80)
append, append_asm = compare("stream", b"?Append@StrStream@CgsDev@@", 0x82815e80, 56)
simple, _ = compare("stream", b"?Append@SimpleStrStream@CgsDev@@", 0x82815ec8, 48)
verify_append = runpy.run_path(str(here / "verify_append_coff.py"))["verify"]
append_exact = verify_append(new[sources["stream"]])
setter_symbol = b"?SetWorldEntityStatusInterface@UpdateOutputBuffer@BrnWorldIO@@"
setter, setter_asm = compare("world", setter_symbol, 0x827a4bd8, 204)
assert (ctor["before"]["compiled_bytes"], ctor["after"]["compiled_bytes"]) == (52, 36)
assert (append["before"]["compiled_bytes"], append["after"]["compiled_bytes"]) == (92, 56)
assert (append["before"]["aligned_coff_words"], append["after"]["aligned_coff_words"]) == (10, 13)
assert (setter["before"]["compiled_bytes"], setter["after"]["compiled_bytes"]) == (140, 152)
assert (setter["before"]["aligned_coff_words"], setter["after"]["aligned_coff_words"]) == (11, 14)
assert re.search(r"\bstb\s+r10,0\(r4\)", ctor_asm)
assert not re.search(r"\b(?:beq|bne|bgt|ble)\b", ctor_asm)
assert len(re.findall(r"\bbne\b", append_asm)) == 1
assert re.search(r"\bb\s+strncat\b", append_asm)
assert not re.search(r"\bbl\s+strlen\b", append_asm)
assert re.search(r"\bli\s+r5,592\b", setter_asm)
assert re.search(r"\bbl\s+\?\?6StrStreamBase", setter_asm)
assert "Not?5locked?5for?5writing?6?" in setter_asm
assert "GameSource?2World?1BrnWor@" in setter_asm
copy_loop = read_original(0x827a4c80, 28)
coff["symbol"] = setter_symbol
for source_map in (old, new):
    _, body, relocations = coff["read_function"](source_map[sources["world"]])
    offset = body.index(copy_loop)
    assert not offset % 4 and not any(offset <= item[0] < offset + len(copy_loop) for item in relocations)

# Prove the actual linked tail branch to the library routine, including its relative operand.
xex = link / "native-game.exe"
image = (link / "native-game.pe").read_bytes()
pe = struct.unpack_from("<I", image, 0x3c)[0]
assert image[pe:pe+4] == b"PE\0\0"
machine, count, _, _, _, optional, _ = struct.unpack_from("<HHIIIHH", image, pe + 4)
base_address = struct.unpack_from("<I", image, pe+52)[0]
assert machine == 0x1f2 and base_address == 0x82000000
def linked_tail_target(method):
    tail_va = int(method["after"]["linked_va"], 16) + method["after"]["compiled_bytes"] - 4
    rva = tail_va - base_address
    for i in range(count):
        _, _, start, size, offset = struct.unpack_from("<8sIIII", image, pe + 24 + optional + i * 40)
        if start <= rva and rva + 4 <= start + size:
            tail = struct.unpack_from(">I", image, offset + rva - start)[0]
            break
    else:
        raise ValueError(hex(tail_va))
    assert tail >> 26 == 18 and tail & 3 == 0
    disp = tail & 0x03fffffc
    return tail_va + disp - (0x04000000 if disp & 0x02000000 else 0)

mapping = (link / "native-game.map").read_text(errors="replace")
native_strncat, = [int(a, 16) for a in re.findall(
    r"(?m)^\s+0003:[0-9a-fA-F]+\s+strncat\s+([0-9a-fA-F]{8})\s", mapping)]
assert linked_tail_target(append) == linked_tail_target(simple) == native_strncat
assert result["exit_code"] == 0 and result["all_post_checks_pass"]
assert sha(xex) == result["outputs"][str(xex)]["sha256"] and xex.read_bytes()[:4] == b"XEX2"
proof = {"xex_sha256": sha(xex), "scope": "targeted objects" if targeted else "full dependency closure",
         "constructor": ctor, "append": append, "world_status_setter": setter,
         "copy_loop_raw_bytes": len(copy_loop), "copy_loop_preserved": True,
         "setter_assert_line": 592, "append_tail_strncat_va": hex(native_strncat),
         "simple_append": simple, "exact_append_code": append_exact}
with (link / "R127_MATCH.json").open("x", encoding="utf-8") as stream:
    json.dump(proof, stream, indent=2)
    stream.write("\n")
print(json.dumps(proof))
