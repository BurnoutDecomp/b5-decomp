"""Verify the compiled and linked functions in the exact R121 preflight."""
import hashlib
import json
from pathlib import Path
import re
import runpy
import struct
import subprocess
import sys


wave = Path(__file__).resolve().parent.parent
out = wave / "r121-byte-match-exact-preflight"
result = json.loads((out / "result.json").read_text())
assert result["exit_code"] == 0 and result["all_post_checks_pass"]

check = runpy.run_path(str(wave / "r121-loading-command-match/compare.py"))
artist = check["original"]
original_targets = check["original_targets"]
before = check["compare"](wave / "r25-intake-repaired/native-selected/BrnLoadingScreenRenderer-e5e683c8.obj")
after = check["compare"](out / "BrnLoadingScreenRenderer-preflight.obj")
assert len(before["fixed_bit_differences"]) == 21 and not after["fixed_bit_differences"]
assert after["resolved_switch_offsets"] == original_targets and after["switch_base_relocations"] == {12:36,16:36}

mapping = (out / "native-game.map").read_text(errors="replace")
addresses = re.findall(r"(?m)^\s+0003:[0-9a-fA-F]+\s+\?AddCommand@LoadingScreenRenderer@BrnGame@@\S+\s+([0-9A-Fa-f]{8})\s", mapping)
address, = map(lambda x: int(x,16), addresses)
data = (out / "native-game.pe").read_bytes()
pe = struct.unpack_from("<I", data, 0x3c)[0]
assert data[pe:pe+4] == b"PE\0\0"
machine, n_sections, _, _, _, optional, _ = struct.unpack_from("<HHIIIHH", data, pe+4)
image_base = struct.unpack_from("<I",data,pe+24+28)[0]
assert machine == 0x1f2 and image_base == 0x82000000


def read_linked(va, length):
    rva = va - image_base
    for i in range(n_sections):
        _, _, base, size, raw = struct.unpack_from("<8sIIII", data, pe+24+optional+40*i)
        if base <= rva and rva+length <= base+size:
            file = raw+rva-base
            return data[file:file+length]
    raise ValueError(hex(va))


linked = read_linked(address, len(artist))
words = struct.unpack(">49I", linked)
original_words = struct.unpack(">49I", artist)
assert [p-address for p in words[9:14]] == original_targets


def resolve(high, low):
    return ((high & 0xffff) << 16) + struct.unpack(">h", (low & 0xffff).to_bytes(2,"big"))[0]


assert resolve(words[3],words[4]) == address+0x24
literal_values = [int.from_bytes(read_linked(resolve(words[hi],words[lo]),4),"big")
                  for hi,lo in ((31,34),(40,42))]
assert literal_values == [0,0x3f99999a]
address_words = {3,4,31,34,40,42}
for i,(a,b) in enumerate(zip(original_words,words)):
    mask = 0xffffffff if 9 <= i <= 13 else (0xffff if i in address_words else 0)
    assert not (a ^ b) & ~mask

text = subprocess.check_output([sys.executable,"-X","utf8","-B",
    str(wave / "r120-debug2d-match/compare.py"),str(out / "CgsDebug2DImmediateRender-preflight.obj")],text=True)
debug = [json.loads(line) for line in text.splitlines() if line.startswith("{")]
assert [x["aligned_identical_words"] for x in debug] == [2,36] and debug[1]["original_words"] == 47

terra = wave / "iobuffer-byte-match-terra"
ns = {}
exec((terra / "compare.py").read_text().split("\nres={}",1)[0],ns)
io_object = out / "CgsIOBuffer-preflight.obj"
io_data = io_object.read_bytes()
io = ns["extract"](io_object)
mask_for = {0x6:0x03fffffc,0x7:0xfffc,0x10:0xffff,0x11:0xffff,0x12:0}
io_checks = {}
for name,va in ns["fs"].items():
    obj, relocs, _ = io[name]
    original = bytes.fromhex(ns["orig"][va]["bytes"])
    masks = {}
    for offset, _, kind in relocs:
        masks[offset//4] = masks.get(offset//4,0) | mask_for[kind]
    exact = len(obj) == len(original) and all(not (int.from_bytes(a,"big") ^ int.from_bytes(b,"big")) & ~masks.get(i,0)
        for i,(a,b) in enumerate(zip(struct.iter_unpack(">4s",original),struct.iter_unpack(">4s",obj)))
        for a,b in [(a[0],b[0])])
    io_checks[name] = {"artist_bytes":len(original),"object_bytes":len(obj),"raw_exact":original==obj,
                      "equal_after_masking_relocation_fields":exact}
assert all(io_checks[x]["equal_after_masking_relocation_fields"] for x in ("UnlockForWrite","LockForRead","UnlockForRead"))

_, _, _, sym_start, n_symbols, _, _ = struct.unpack_from("<HHIIIHH",io_data)
strings = sym_start + n_symbols * 18
call_targets = {}
for offset,index,kind in io["LockForWrite"][1]:
    if kind != 6 or offset not in (0x98,0x108):
        continue
    entry = io_data[sym_start+18*index:sym_start+18*index+8]
    assert entry[:4] == bytes(4)
    name = strings+struct.unpack_from("<I",entry,4)[0]
    call_targets[offset] = io_data[name:io_data.index(bytes(1),name)].decode()
assert sorted(call_targets) == [0x98,0x108]
assert all(s == "??6StrStreamBase@CgsDev@@QAAAAU01@H@Z" for s in call_targets.values())

report = {"commit":"8c0d37f438af7ec851a4c7ad4167918d3ed33442","base":"f642eb75191c13eb3b595a1d36f56427f493109a",
          "private_xex_sha256":hashlib.sha256((out/"native-game.exe").read_bytes()).hexdigest(),
          "add_command":{"before":before,"after":after,"linked_address":hex(address),
                         "linked_raw_exact":linked==artist,"linked_address_normalized_exact":True,
                         "resolved_literals":literal_values,"resolved_switch_offsets":original_targets},
          "io_buffer":io_checks,"signed_diagnostic_calls":call_targets,"debug_point":debug}
with (out/"R121_MATCH.json").open("x",encoding="utf-8") as f:
    json.dump(report,f,indent=2)
    f.write("\n")
print(report["private_xex_sha256"],"AddCommand linked",hex(address),"196 bytes match after resolving addresses")
