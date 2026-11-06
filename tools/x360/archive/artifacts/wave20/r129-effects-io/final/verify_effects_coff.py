"""Check the exact six-source helpers and the changed effects caller in PPC COFF."""
import difflib
import json
from pathlib import Path
import re
import struct

folder=Path(__file__).resolve().parent
wave=folder.parent.parent
reader=wave/"r128-batch/verify_matches.py"
base_helpers={"__file__":str(reader)}
exec(reader.read_text(encoding="utf-8-sig").split("\nif len(sys.argv)==2:",1)[0],base_helpers)
obj,=folder.glob("BrnGameModule-*.obj")
old=wave/"r128-intake/native-selected/BrnGameModule-f766a093.obj"
coff=base_helpers["coff"]
collect=base_helpers["collect"]
raw=base_helpers["original"]
symbol=b"?DoUpdate_Effects@BrnGameModule@BrnGame@@"
coff["symbol"]=symbol
name,a,_=coff["read_function"](old)
_,b,_=coff["read_function"](obj)
original=raw(0x823dd0a8,500)
aligned=lambda blob: sum(x.size for x in difflib.SequenceMatcher(
    None,coff["words"](original),coff["words"](blob),autojunk=False).get_matching_blocks())
assert (len(a),len(b),aligned(a),aligned(b))==(724,448,34,52)
asm=obj.with_suffix(".asm").read_text(errors="replace")
body=asm.split("; Begin code for function: "+name,1)[1].split("; End code for function:",1)[0]
replay="?GetStatusInterface@OutputBuffer_PreSim@ReplayIO@BrnReplays@@QBAPBUStatusInterface@23@XZ"
assert replay in body and "?GetStatusInterface@OutputBuffer_PreSim@ReplayIO@BrnReplays@@QA" not in body
assert base_helpers["branch_target"](0x823dd1dc,int.from_bytes(raw(0x823dd1dc,4),"big"))==0x823bb080
assert re.search(r"\blwz\s+r11,34h\(r3\).*?\brlwinm\s+r11,r11,0,30,30",body,re.S)
assert raw(0x823dd18c,4).hex()=="556bfffe"
targets={**base_helpers["target_names"],0x82c08ed8:"__savegprlr_24",0x82c08f28:"__restgprlr_24"}
data=obj.read_bytes()
expected_strings=base_helpers["messages"]|{base_helpers["source_path"],"lpOutputBuffer5"}
methods={}
for label,ea,first_line in (("LockBuffersForIO",0x823b78a0,454),("UnlockBuffersForIO",0x823b7a10,482)):
    sym,=re.findall(r"\sbl\s+(\?\?\$"+label+r"@\S+)",body)
    code,rels=collect(obj,sym)
    artist=raw(ea,364)
    base_helpers["fixed_equal"](artist,code,rels)
    pairs={}
    branches=[]
    for offset,index,kind in rels:
        if kind==6:
            name,_,_=base_helpers["object_symbol"](data,index)
            original_target=base_helpers["branch_target"](ea+offset,struct.unpack_from(">I",artist,offset)[0])
            assert targets[original_target]==name
            branches.append(name)
        elif kind in (16,17):
            pairs.setdefault(index,{})[kind]=offset
    strings=[]
    for index,pair in pairs.items():
        text=base_helpers["symbol_literal"](data,index)
        address=base_helpers["data_target"](artist,pair[16],pair[17])
        assert raw(address,len(text)+1)==text.encode()+bytes(1)
        strings.append(text)
    assert set(strings)==expected_strings and len(branches)==30
    assert [sum(x.startswith("?"+w+"Assert@") for x in branches) for w in ("Begin","Fire","End")]==[7,7,7]
    methods[label]={"artist_va":hex(ea),"bytes":len(code),"matched_after_relocation_fields":True,
                    "symbol":sym,"branches":branches,"diagnostics":strings,"assert_lines":list(range(first_line,first_line+7))}
proof={"scope":"targeted native COFF only","object":str(obj),"object_sha256":base_helpers["sha"](obj),"methods":methods,
       "effects_caller":{"artist_bytes":len(original),"old_bytes":len(a),"new_bytes":len(b),
                         "old_aligned_words":aligned(a),"new_aligned_words":aligned(b),
                         "replay_read_getter":replay,"restart_status_offset":52,"restart_status_bit":1}}
with (folder/"R129_EFFECTS_COFF.json").open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print("R129 targeted exact methods",[(k,v["bytes"]) for k,v in methods.items()])
