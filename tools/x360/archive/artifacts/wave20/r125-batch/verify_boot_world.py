"""Verify the restored boot-world queue calls and their PPC object provenance."""
import difflib
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
READER = WAVE / "r120-debug2d-match/compare.py"
COFF = {"__file__": str(READER)}
exec(READER.read_text().split("\nrows = []", 1)[0], COFF)
READ_ORIGINAL = COFF["x360rd"].rd

MODULE = "src/GameSource/World/BrnWorldModule.cpp"
OUTPUT = "src/GameSource/World/BrnWorldModuleIO_UpdateOutputBuffer.cpp"
BRIDGE = "src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp"
BOOT_SYMBOL = b"?UpdateForBootUpVideo@WorldModule@BrnWorld@@"
GETTER_SYMBOL = b"?GetGuiEventQueue@UpdateOutputBuffer@BrnWorldIO@@QAA"
MASKS = {0x6: 0x03fffffc, 0x7: 0xfffc, 0x10: 0xffff, 0x11: 0xffff, 0x12: 0}

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def word_list(data):
    return [item[0] for item in struct.iter_unpack(">I", data)]

def inspect(link, path, symbol, artist_address, artist_bytes):
    COFF["symbol"] = symbol
    name, code, relocations = COFF["read_function"](path)
    mapping = (link / "native-game.map").read_text(errors="replace")
    address, = [int(x, 16) for x in re.findall(
        rf"(?m)^\s+0003:[0-9a-fA-F]+\s+{re.escape(name)}\s+([0-9a-fA-F]{{8}})\s", mapping)]
    image = (link / "native-game.pe").read_bytes()
    pe = struct.unpack_from("<I", image, 0x3c)[0]
    assert image[pe:pe+4] == b"PE\0\0"
    machine, count, _, _, _, optional, _ = struct.unpack_from("<HHIIIHH", image, pe + 4)
    base = struct.unpack_from("<I", image, pe + 52)[0]
    assert machine == 0x1f2 and base == 0x82000000
    for i in range(count):
        _, _, start, size, offset = struct.unpack_from("<8sIIII", image, pe + 24 + optional + i * 40)
        rva = address - base
        if start <= rva and rva + len(code) <= start + size:
            linked = image[offset + rva - start:offset + rva - start + len(code)]
            break
    else:
        raise ValueError(hex(address))
    masks = {}
    for offset, _, kind in relocations:
        masks[offset // 4] = masks.get(offset // 4, 0) | MASKS[kind]
    built, loaded = word_list(code), word_list(linked)
    assert all(not ((a ^ b) & ~masks.get(i, 0)) for i, (a, b) in enumerate(zip(built, loaded)))
    original = READ_ORIGINAL(artist_address, artist_bytes)
    a, b = word_list(original), built
    common = sum(m.size for m in difflib.SequenceMatcher(None, a, b, autojunk=False).get_matching_blocks())
    asm = path.with_suffix(".asm").read_text(errors="replace")
    asm = asm.split("; Begin code for function: " + name, 1)[1].split("; End code for function: ", 1)[0]
    targets = []
    for i, word in enumerate(loaded):
        if word >> 26 == 18 and word & 3 == 1:
            offset = word & 0x03fffffc
            if offset & 0x02000000:
                offset -= 0x04000000
            targets.append(address + i * 4 + offset)
    return {"object": str(path), "object_sha256": sha(path), "linked_va": hex(address),
            "artist_va": hex(artist_address), "artist_bytes": artist_bytes, "compiled_bytes": len(code),
            "aligned_coff_words": common, "linked_raw_exact": original == linked, "relocations": len(relocations)}, targets, asm

link = Path(sys.argv[1]).resolve()
baseline = WAVE / "r124-link"
baseline_report = json.loads((WAVE / "r124-intake/composite-active-native.json").read_text())
old = {row["source"]: Path(row["object"]) for row in baseline_report["results"]}
before_boot, before_calls, _ = inspect(baseline, old[MODULE], BOOT_SYMBOL, 0x827cfde0, 692)
before_getter, _, _ = inspect(baseline, old[BRIDGE], GETTER_SYMBOL, 0x827a4e08, 168)

result = json.loads((link / "result.json").read_text())
if "preflight_replacements" in result:
    objects = {row["source"]: Path(row["replacement"]) for row in result["preflight_replacements"]}
    assert set(objects) == {MODULE, OUTPUT, BRIDGE}
    assert result["base_result_sha256"] == sha(baseline / "result.json")
else:
    invocation = json.loads((link / "invocation.json").read_text())
    objects = {row["source"]: Path(row["object"]) for row in invocation["objects"]}
after_boot, after_calls, _ = inspect(link, objects[MODULE], BOOT_SYMBOL, 0x827cfde0, 692)
after_getter, _, asm = inspect(link, objects[OUTPUT], GETTER_SYMBOL, 0x827a4e08, 168)
assert before_calls.count(int(before_getter["linked_va"], 16)) == 0
assert after_calls.count(int(after_getter["linked_va"], 16)) == 1
assert before_boot["aligned_coff_words"] == 12 and after_boot["aligned_coff_words"] == 56
assert before_boot["compiled_bytes"] == 504 and after_boot["compiled_bytes"] == 524
assert after_getter["compiled_bytes"] == 140 and after_getter["aligned_coff_words"] == 7
for pattern in (r"\brlwinm\s+r11,r11,29,31,31", r"\baddis\s+r3,r30,3", r"\baddi\s+r3,r3,-26432",
                r"\bli\s+r5,608", r"\bbl\s+\?\?6StrStreamBase", r"\bbl\s+\?FireAssert"):
    assert re.search(pattern, asm), pattern
assert READ_ORIGINAL(0x82006294, 23) == b"Not locked for writing\n"
assert READ_ORIGINAL(0x82035f0c, 45).split(b"\0")[0] == b"..\\..\\..\\GameSource\\World/BrnWorldModuleIO.h"
xex = link / "native-game.exe"
assert result["exit_code"] == 0 and result["all_post_checks_pass"]
assert sha(xex) == result["outputs"][str(xex)]["sha256"] and xex.read_bytes()[:4] == b"XEX2"
proof = {"xex_sha256": sha(xex), "scope": "targeted objects" if "preflight_replacements" in result else "full dependency closure",
         "boot_before": before_boot, "boot_after": after_boot, "getter_before": before_getter,
         "getter_after": after_getter, "direct_boot_getter_calls": 1, "queue_offset": 170176, "assert_line": 608}
with (link / "R125_MATCH.json").open("x", encoding="utf-8") as stream:
    json.dump(proof, stream, indent=2)
    stream.write("\n")
print(json.dumps(proof))
