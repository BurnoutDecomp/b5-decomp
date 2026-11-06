"""Verify the retained R140 composition in the completed R143 link."""
import argparse
import json
from pathlib import Path
import re
import struct

from verify_profile_flow_regression import (
    LINK_BASE, LINK_TREE, R142_LINK, TARGET_MANIFEST_SHA, read as read_json, sha256, target_pins,
)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("link", type=Path)
parser.add_argument("--expected-head", required=True)
args = parser.parse_args()
if not __debug__:
    parser.error("verification requires Python assertions")
link = args.link.resolve()
manifest, seal, invocation, result, outputs = target_pins(link, args.expected_head)

folder = Path(__file__).resolve().parent.parent / "r140-composed-preflight"
wave = folder.parent
reader = wave/"r128-batch/verify_matches.py"
ns = {"__file__":str(reader)}
exec(reader.read_text().split("\nif len(sys.argv)==2:",1)[0],ns)
word = lambda data,off:struct.unpack_from(">I",data,off)[0]
raw = ns["original"]
composed_receipt = folder / "R140_COMPOSED_COFF.json"
historical_report = folder / "compile-results.json"
assert sha256(composed_receipt) == "5fc57b112dfbc639ab2c33c23c55221f0aa8295f99ab1966a9e30d23b13760d8"
assert sha256(historical_report) == "8b1c2dc6b3ae724be4f3e4dde27213f203ffe4a3634019606500ec14c5d2b70c"
frozen = read_json(composed_receipt)
assert frozen["tree"] == "43d23d4258501ccd488869c7354a57f17b8bebf3"
report = read_json(historical_report)
assert (report["passed"], report["failed"]) == (8, 0)
assert len(report["results"]) == 8 and all(r["machine"] == 0x1F2 for r in report["results"])
baseline_path = R142_LINK / "R140_MATCH.json"
baseline_sha = sha256(baseline_path)
assert baseline_sha == "c5d702f18683c065d1d721b053c823a404252387fd8596c6334855d28e04a7f2"
baseline = read_json(baseline_path)
assert baseline["tree"] == manifest["base_tree"]
assert baseline["xex_sha256"] == sha256(R142_LINK / "native-game.exe")
assert baseline["linked_profile_receipt_sha256"] == sha256(R142_LINK / "R142_PROFILE_FLOW_LINKED.json")
assert baseline["gui_event_targets"] == frozen["gui_event_targets"]
assert baseline["preserved_functions_bytes"] == frozen["preserved_functions_bytes"]
objects = {}
for item in report["results"]:
    source = item["source"]
    stem = Path(source).stem
    reference_obj = Path(item["object"])
    assert sha256(reference_obj) == frozen["object_sha256"][stem]
    invoked, = [row for row in invocation["objects"] if row["source"] == source]
    selected, = [row for row in seal["selected"] if row["source"] == source]
    assert invoked == selected
    obj = Path(invoked["object"])
    assert sha256(obj) == invoked["sha256"]
    with obj.open("rb") as stream:
        assert struct.unpack("<H", stream.read(2))[0] == 0x1F2
    objects[stem] = obj
image = link / "native-game.exe"
address, loaded = ns["linked_reader"](link)


def read(obj,prefix):
    ns["coff"]["symbol"] = prefix.encode()
    symbol, code, rels = ns["coff"]["read_function"](obj)
    blob = obj.read_bytes()
    return symbol, code, rels, [(p,k,ns["object_symbol"](blob,i)[0]) for p,i,k in rels]

def normal(name):
    return re.sub(r"\?A0x[0-9a-f]{8}@", "?anonymous@", name)

targets = [
    ("r140-sound-ready/native-selected","BrnSoundLogicModule","?Prepare@SoundLogicModule@Module@BrnSound@@"),
    ("r140-sound-ready/native-selected","BrnSoundLogicModule","?ResourcesAreReady@SoundLogicModule@Module@BrnSound@@"),
    ("r140-apt-gc/native","AptValue","?Release@AptValue@@"),
    ("r140-signin-gate","BrnGuiCache","?Construct@GuiCache@BrnGui@@"),
    ("r140-signin-gate","BrnGuiProfile","?SigninStateChanged@ProfileManager@BrnGui@@"),
    ("r140-signin-gate","BrnPostTitleScreenLoad","?Update@PostTitleScreenLoad@BrnGui@@"),
]
preserved = {}
for path,stem,prefix in targets:
    old, = (wave/path).glob(stem+"-*.obj")
    s0,c0,_,r0 = read(old,prefix)
    symbol,code,rels,refs = read(objects[stem],prefix)
    assert s0==symbol and c0==code
    assert [(p,k,normal(n)) for p,k,n in r0]==[(p,k,normal(n)) for p,k,n in refs]
    if link:
        va = address(symbol)
        bound = loaded(va,len(code))
        blob = objects[stem].read_bytes()
        for off,index,kind in rels:
            if kind != 2:
                continue
            label,target,section = ns["object_symbol"](blob,index)
            assert label.startswith("$LN") and section>0 and 0<=target<len(code)
            assert word(bound,off)==va+target+word(code,off)
        ns["masks"][2] = 0xffffffff
        ns["fixed_equal"](code,bound,rels)
    preserved[symbol] = len(code)

cache = objects["BrnGuiCache"]
symbol, code, rels, _ = read(cache,"?RecEvent@GuiCache@BrnGui@@")
blob = cache.read_bytes()
assert word(code,0x88)==0x3965fff2
table_index, = [i for p,i,k in rels if p==0x94 and k==16]
_,table,section = ns["object_symbol"](blob,table_index)
assert table==0xac and section>0
artist = 0x8250de40
artist_targets = {event:word(raw(artist+(event-4)*4,4),0) for event in [77,79,81,86,87]}
assert artist_targets=={77:0x82510f34,79:0x8250ee0c,81:0x8250ee20,86:0x8250f24c,87:0x8250f230}
cases = {}
for event in artist_targets:
    off = table+(event-14)*4
    index, = [i for p,i,k in rels if p==off and k==2]
    label,target,section = ns["object_symbol"](blob,index)
    assert section>0 and word(code,off)==0
    if link:
        va = address(symbol)
        assert word(loaded(va+off,4),0)==va+target
        assert loaded(va+target,16)==code[target:target+16]
    cases[event] = {"target":target,"label":label,"first_words":code[target:target+16].hex()}
assert cases[77]["label"]=="$LN1"
assert cases[79]["first_words"].startswith("897f0000997e4b57")
assert cases[81]["first_words"].startswith("817f0000917e4b70")
assert cases[86]["first_words"]=="3d60000139400001616b2f0b7d5e59ae"
assert cases[87]["first_words"]=="3d60000139400001616b2f0a7d5e59ae"

flow = objects["BrnGameMainFlowStates"]
invite = {}
for getter,expected,artist_offset in [
    ("IsPerformingInvite","3d600018616bd8387c6358ae4e800020",0x18d838),
    ("IsLoginNeededForInvite","3d600018616bd8397c6358ae4e800020",0x18d839),
]:
    symbol,code,relocs,_ = read(flow,"?"+getter+"@GuiModule@BrnGui@@")
    assert code.hex()==expected and not relocs
    invite[symbol] = hex(artist_offset)
if link:
    profile = json.loads((link/"R143_PROFILE_FLOW_LINKED.json").read_text())
    seal = json.loads((link/"seal.json").read_text())
    assert seal["schema"]=="r143-native-link-seal-v1" and seal["head"]==profile["head"]
    assert profile["tree"] == LINK_TREE
    assert profile["xex_sha256"]==ns["sha"](image)
    assert profile["pe_sha256"]==ns["sha"](link/"native-game.pe")
    assert profile["map_sha256"]==ns["sha"](link/"native-game.map")
    assert profile["invite_offsets_from_gui_module"]==["0x18D838","0x18D839"]
    for caller, off, instruction in [
        ("?OnEnter@MainGameFlowStateMemoryCard@@UAAXXZ",0x60,"6108d838"),
        ("?OnEnter@MainGameFlowStateMemoryCard@@UAAXXZ",0x78,"6129d839"),
        ("?Update@MainGameFlowStateMemoryCard@@UAAXXZ",0x2c,"6169d838"),
        ("?Update@MainGameFlowStateMemoryCard@@UAAXXZ",0x48,"6129d839"),
        ("?Update@MainGameFlowStateStartScreen@@UAAXXZ",0x1c,"614a8258"),
    ]:
        va = address(caller)
        assert int(profile["functions"][caller]["r143_va"],16)==va
        assert loaded(va+off,4).hex()==instruction
_,producer,_,_ = read(objects["BrnGuiModule"],"?PerformInvite@GuiModule@BrnGui@@")
assert word(producer,0x14)==0x396bd838 and word(producer,0x2c)==0x614ad839
_,update,_,_ = read(flow,"?Update@MainGameFlowStateMemoryCard@@")
assert word(update,0x64)&0xffff==0x0648 and word(update,0x88)&0xffff==0x0625

proof = {"scope":"linked static PPC" if link else "eight targeted SDK TUs in composed R140 tree",
         "head": args.expected_head, "tree": LINK_TREE, "base": LINK_BASE,
         "target_manifest_sha256": TARGET_MANIFEST_SHA, "r142_composed_receipt_sha256": baseline_sha,
         "object_sha256":{stem:ns["sha"](obj) for stem,obj in objects.items()},
         "preserved_functions_bytes":preserved,"gui_event_targets":cases,
         "invite_getter_offsets":invite,"gui_phase_offset":"0x9A0648","stream_wait_offset":"0x9A0625"}
if link:
    proof["xex_sha256"]=ns["sha"](link/"native-game.exe")
    proof["invite_linkage"]="inlined in linked MemoryCard and StartScreen callers"
    proof["linked_profile_receipt_sha256"]=ns["sha"](link/"R143_PROFILE_FLOW_LINKED.json")
assert preserved == baseline["preserved_functions_bytes"]
assert cases == {int(key): value for key, value in baseline["gui_event_targets"].items()}
assert invite == baseline["invite_getter_offsets"]
out = link / "R140_MATCH.json"
with out.open("x",encoding="utf-8") as stream:
    json.dump(proof,stream,indent=2)
    stream.write("\n")
print(out,ns["sha"](out))
