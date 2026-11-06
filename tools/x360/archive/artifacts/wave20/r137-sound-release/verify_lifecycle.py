"""Check both RootSound lifecycle bodies and vtable targets against ARTIST."""
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
word = lambda data, off: struct.unpack_from(">I", data, off)[0]
source = "src/GameSource/Sound/Module/BrnRootSoundModule.cpp"
link = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None
def objects(link):
    return {r["source"]: Path(r["object"]) for r in json.loads((link/"invocation.json").read_text())["objects"]}
prior = objects(wave/"r136-link")
if link:
    active = objects(link)
    obj = active[source]
    address, loaded = ns["linked_reader"](link)
else:
    active = prior
    obj, = (folder/"lifecycle").glob("*.obj")
blob = obj.read_bytes()
release = "?Release@RootSoundModule@Module@BrnSound@@UAA_NXZ"
destruct = "?Destruct@RootSoundModule@Module@BrnSound@@UAAXXZ"
vtable = "??_7RootSoundModule@Module@BrnSound@@6B@"
system = "?Release@System@core@audio@rw@@SAXPAV1234@@Z"
playback = "?Release@Module@1Playback@CgsSound@@UAA_NXZ"
assert ns["collect"](active["vendor/renderware/src/rw/audio/core/System.cpp"], system)[0]
assert ns["collect"](active["src/GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.cpp"], playback)[0]

branch_targets = {
    0x82c08ee0: "__savegprlr_26", 0x82c08f30: "__restgprlr_26",
    0x82b6f998: system,
    0x8286e970: "?Release@ModuleSingleBuffered@CgsModule@@UAA_NXZ",
    0x8286e040: "?Destruct@ModuleSingleBuffered@CgsModule@@UAAXXZ",
    0x82817548: "?BeginAssert@Assert@CgsDev@@YAXXZ",
    0x82820810: "?FireAssert@Assert@CgsDev@@YAXPBD0H@Z",
    0x82817558: "?EndAssert@Assert@CgsDev@@YAXXZ",
}
literal_fields = {0x104: 0x10c, 0x110: 0x114}
proof = {"object": str(obj), "sha256": ns["sha"](obj), "functions": {}}
ns["masks"][2] = 0xffffffff
for symbol, artist, size in [(release, 0x82682898, 300), (destruct, 0x826829c8, 88)]:
    code, rels = ns["collect"](obj, symbol)
    original = raw(artist, size)
    ns["fixed_equal"](original, code, rels)
    expected = {6: 8, 2: 8, 16: 3, 17: 3, 18: 6} if symbol == release else {6: 1}
    assert {k: sum(r[2] == k for r in rels) for k in {r[2] for r in rels}} == expected
    row = {"artist": hex(artist), "bytes": size, "exact_after_verified_address_fields": True,
           "branches": [], "jump_table_target_offsets": [], "literals": []}
    if link:
        va = address(symbol)
        bound = loaded(va, size)
        ns["fixed_equal"](code, bound, rels)
        row["linked"] = hex(va)
    for off, index, kind in rels:
        name, value, section = ns["object_symbol"](blob, index)
        if kind == 6:
            target = ns["branch_target"](artist + off, word(original, off))
            assert name == branch_targets[target]
            if link:
                assert ns["branch_target"](va + off, word(bound, off)) == address(name)
            row["branches"].append(name)
        elif kind == 2:
            assert name.startswith("$LN") and section > 0 and word(code, off) == 0
            assert word(original, off) == artist + value
            if link:
                assert word(bound, off) == va + value
            row["jump_table_target_offsets"].append(value)
        elif kind == 16:
            if off == 0x34:
                assert name.startswith("$LN") and value == 0x4c
                assert ns["data_target"](original, off, 0x38) == artist + value
                if link:
                    assert ns["data_target"](bound, off, 0x38) == va + value
            else:
                lower = literal_fields[off]
                literal = ns["symbol_literal"](blob, index).encode() + bytes(1)
                assert raw(ns["data_target"](original, off, lower), len(literal)) == literal
                if link:
                    assert loaded(ns["data_target"](bound, off, lower), len(literal)) == literal
                row["literals"].append(literal[:-1].decode())
    proof["functions"][symbol] = row
assert word(raw(0x82682898, 300), 0x108) == 0x38a0030b
proof["assert_line"] = 779

def slots(obj):
    table, rels = ns["collect"](obj, vtable)
    assert len(table) == 68 and [(p,k) for p,_,k in rels] == [(p,2) for p in range(0,68,4)]
    data = obj.read_bytes()
    return [ns["object_symbol"](data, i)[0] for _,i,_ in rels]

before, after = slots(prior[source]), slots(obj)
assert [i for i,(a,b) in enumerate(zip(before,after,strict=True)) if a != b] == [2,3]
assert after[2:4] == [release,destruct]
table_va = 0x820d1100
native = raw(table_va, 68)
assert struct.unpack_from(">II",native,8) == (0x82682898,0x826829c8)
if link:
    table = loaded(address(vtable),68)
    assert [word(table,p) for p in range(0,68,4)] == [address(name) for name in after]
proof["vtable"] = {"artist": hex(table_va), "changed_slots": [8,12],
                   "entries": [{"slot": p, "artist_target": hex(word(native,p)), "symbol": after[p//4]} for p in range(0,68,4)]}
proof["scope"] = "strict linked image" if link else "targeted SDK COFF"
if link:
    proof["xex_sha256"] = ns["sha"](link/"native-game.exe")
out = link/"R137_MATCH.json" if link else folder/"R137_SOUND_COFF.json"
with out.open("x", encoding="utf-8") as f:
    json.dump(proof, f, indent=2)
    f.write("\n")
print(out, ns["sha"](out))
