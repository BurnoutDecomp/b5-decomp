"""Verify the native SatNav view publication, monitor calls, and diagnostics."""
import difflib
import json
from pathlib import Path
import struct
import sys

folder = Path(__file__).resolve().parent
wave = folder.parent
reader = wave / "r128-batch/verify_matches.py"
ns = {"__file__": str(reader)}
exec(reader.read_text().split("\nif len(sys.argv)==2:", 1)[0], ns)
raw = ns["original"]
word = lambda data, offset: struct.unpack_from(">I", data, offset)[0]
source = "src/GameSource/Gui/SatNav/BrnSatNavComponent.cpp"
link = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None

def object_for(link):
    rows = json.loads((link/"invocation.json").read_text())["objects"]
    return next(Path(r["object"]) for r in rows if r["source"] == source)

prior = object_for(wave/"r137-link")
obj = object_for(link) if link else next((folder/"diagnostics").glob("*.obj"))
blob = obj.read_bytes()
artist = 0x82469378
original = raw(artist, 876)
symbol = "?Update@SatNavComponent@BrnGui@@QAAXXZ"
before, _ = ns["collect"](prior, symbol)
code, relocs = ns["collect"](obj, symbol)
refs = [(off,kind,index,ns["object_symbol"](blob,index)[0]) for off,index,kind in relocs]
calls = [(off,name) for off,kind,_,name in refs if kind == 6]

def original_branch(va):
    return ns["branch_target"](va,word(original,va-artist))

monitor_sites = [0x82469390,0x8246953c,0x82469554,0x824696d8]
assert [original_branch(va) for va in monitor_sites] == [0x821f1198,0x821f1198,0x821f1308,0x821f1308]
monitors = [(off,name[1:].split("@")[0]) for off,name in calls if "@PerfMonCpu@" in name]
assert [name for _,name in monitors] == ["StartMonitor","StartMonitor","StopMonitor","StopMonitor"]
handles = [name[1:].split("@")[0] for _,kind,_,name in refs if kind == 17 and "@GuiPerfmons@" in name]
assert handles == ["miSatNavUpdate","miMapIconMgrUpdate","miMapIconMgrUpdate","miSatNavUpdate"]
initializer = raw(0x824ef050,0x220)
assert ns["data_target"](initializer,0x1d4,0x1e8) == 0x82f27644
assert ns["data_target"](initializer,0x1f8,0x210) == 0x82f27648

artist_fire = [0x824693bc,0x824693ec,0x82469520,0x82469678]
artist_lines = [0x824693b0,0x824693e0,0x82469514,0x8246966c]
assert [original_branch(va) for va in artist_fire] == [0x82820810]*4
lines = [word(original,va-artist)&0xffff for va in artist_lines]
assert lines == [228,231,292,333]
fire = [off for off,name in calls if name.startswith("?FireAssert@")]
assert len(fire) == 4
for off,line in zip(fire,lines,strict=True):
    assert [word(code,p)&0xffff for p in range(off-16,off,4)
            if word(code,p)&0xffff0000 == 0x38a00000] == [line]
assert sum(name.startswith("??0StrStream@") for _,name in calls) == 1
assert sum(name.startswith("??6StrStreamBase@") for _,name in calls) == 4
assert original_branch(0x8246947c) == 0x821f0e50
assert any(name.endswith("@H@Z") and name.startswith("??6StrStreamBase@") for _,name in calls)

literals = {}
for upper,kind,index,name in refs:
    if kind != 16 or not name.startswith("??_C@"):
        continue
    lower, = [p for p,k,_,n in refs if n == name and k == 17 and upper < p <= upper+16]
    literal = ns["symbol_literal"](blob,index)
    literals[literal] = (upper,lower)
expected_literals = {
    (0x8246939c,0x824693a0): r"..\..\..\GameSource\Gui/SatNav/BrnSatNav.cpp",
    (0x824693ac,0x824693b4): "mpGuiCache",
    (0x824693dc,0x824693e4): "mpIconManager",
    (0x82469510,0x82469518): "mpPlayerInfo",
    (0x82469644,0x8246964c): " state interface is invalid ",
    (0x8246945c,0x82469460): "MAPICONMANAGER: SatNavComponent is calling SetOwnerParameters with OwnerID ",
    (0x82469464,0x82469468): "(E_SATNAV_MAP).\n",
}
for (upper,lower),literal in expected_literals.items():
    expected = literal.encode()+bytes(1)
    assert raw(ns["data_target"](original,upper-artist,lower-artist),len(expected)) == expected
    assert literal in literals

queue_call, = [off for off,name in calls if name.startswith("?AddEvent@?$VariableEventQueue@$0BAAAA@$0BA@@")]
assert original_branch(0x824696d0) == 0x82435188
assert not any(name.startswith("?GetOutputEventQueue@") for _,name in calls)
assert word(code,queue_call-4) == 0x386b000c
assert all(bytes.fromhex(x) in code[queue_call-84:queue_call]
           for x in ["39400030","394000d4","39400010","38c00040","38a00029"])
assert all(bytes.fromhex(x) in original[0x330:0x35c]
           for x in ["39600030","396000d4","39600010","38c00040","38a00029","386b000c"])
assert bytes.fromhex("39200006") in code[queue_call-104:queue_call]
assert bytes.fromhex("39200006") in original[0x30c:0x35c]
assert monitors[-1][0] > queue_call

def aligned(body):
    return sum(m.size for m in difflib.SequenceMatcher(None,ns["coff"]["words"](original),
                ns["coff"]["words"](body),autojunk=False).get_matching_blocks())

assert (len(before),aligned(before),len(code),aligned(code)) == (688,84,844,118)
proof = {"scope":"strict linked image" if link else "targeted SDK COFF",
         "artist_va":hex(artist),"artist_bytes":len(original),"before_bytes":len(before),
         "before_aligned_words":aligned(before),"after_bytes":len(code),"after_aligned_words":aligned(code),
         "assert_lines":lines,"checked_literals":list(expected_literals.values()),
         "monitor_order":[name for _,name in monitors],"handles":handles,
         "channel":41,"record_bytes":64,"record_header":[48,212,16],"queue_member_offset":12,
         "object":str(obj),"object_sha256":ns["sha"](obj)}
if link:
    address, loaded = ns["linked_reader"](link)
    va = address(symbol)
    bound = loaded(va,len(code))
    ns["fixed_equal"](code,bound,relocs)
    assert [ns["branch_target"](va+off,word(bound,off)) for off,_ in calls] == [address(name) for _,name in calls]
    for literal in expected_literals.values():
        upper,lower = literals[literal]
        expected = literal.encode()+bytes(1)
        assert loaded(ns["data_target"](bound,upper,lower),len(expected)) == expected
    proof["linked_va"] = hex(va)
    proof["xex_sha256"] = ns["sha"](link/"native-game.exe")
out = link/"R138_MATCH.json" if link else folder/"diagnostics/R138_SATNAV_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out))
