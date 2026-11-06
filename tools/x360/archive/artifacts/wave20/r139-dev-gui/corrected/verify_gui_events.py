"""Verify the ARTIST event dispatch table and native menu-music publisher."""
import json
from pathlib import Path
import struct
import sys

folder = Path(__file__).resolve().parent
wave = folder.parents[1]
reader = wave/"r128-batch/verify_matches.py"
ns = {"__file__": str(reader)}
exec(reader.read_text().split("\nif len(sys.argv)==2:",1)[0],ns)
raw = ns["original"]
word = lambda data, off: struct.unpack_from(">I",data,off)[0]
link = Path(sys.argv[1]).resolve() if len(sys.argv)==2 else None
prior = wave/"r138-link"
rows = json.loads(((link or prior)/"invocation.json").read_text())["objects"]
objects = {r["source"]:Path(r["object"]) for r in rows}
cache_source = "src/GameSource/Gui/BrnGuiCache.cpp"
post_source = "src/GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.cpp"
wrapper_source = "src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface_OutputGuiEvent_Inst.cpp"
cache = objects[cache_source] if link else next(folder.glob("BrnGuiCache-*.obj"))
post = objects[post_source] if link else next((folder.parent/"diagnostics").glob("BrnPostTitleScreenLoad-*.obj"))
wrapper = objects[wrapper_source]
recv = "?RecEvent@GuiCache@BrnGui@@QAAXPBUEvent@CgsModule@@H@Z"
update = "?Update@PostTitleScreenLoad@BrnGui@@UAAXXZ"
output = "??$OutputGuiEvent@VGuiEventPlayMusicOnMenuStream@CgsGui@@@StateInterface@CgsGui@@QAAXAAVGuiEventPlayMusicOnMenuStream@1@@Z"

assert word(raw(0x8250de1c,4),0)==0x3965fffc
table_words = raw(0x8250de28,8)
original_table = ns["data_target"](table_words,0,4)
assert original_table==0x8250de40
targets = {event:word(raw(original_table+(event-4)*4,4),0) for event in [77,79,81]}
assert targets=={77:0x82510f34,79:0x8250ee0c,81:0x8250ee20}
assert raw(targets[79],8).hex()=="897e0000997f4b57"
assert raw(targets[81],8).hex()=="817e0000917f4b70"

code, rels = ns["collect"](cache,recv)
blob = cache.read_bytes()
assert word(code,0x88)==0x3965fff2
table_index, = [i for p,i,k in rels if p==0x94 and k==16]
name, table, section = ns["object_symbol"](blob,table_index)
assert name.startswith("$LN") and table==0xac and section>0
case = {}
for event in [77,79,81]:
    offset = table+(event-14)*4
    index, = [i for p,i,k in rels if p==offset and k==2]
    label, target, section = ns["object_symbol"](blob,index)
    assert label.startswith("$LN") and section>0 and word(code,offset)==0
    case[event] = (offset,target,label)
assert case[77][2]=="$LN1"
assert code[case[79][1]:case[79][1]+8].hex()=="897f0000997e4b57"
assert code[case[81][1]:case[81][1]+8].hex()=="817f0000917e4b70"

body, post_relocs = ns["collect"](post,update)
post_blob = post.read_bytes()
calls = [p for p,i,k in post_relocs if k==6 and ns["object_symbol"](post_blob,i)[0]==output]
assert calls==[0x84,0x144]
assert [ns["branch_target"](va,word(raw(va,4),0)) for va in [0x8247e3ac,0x8247e490]]==[0x82476c00]*2
wrap, wrap_relocs = ns["collect"](wrapper,output)
assert len(wrap)==len(raw(0x82476c00,100))==100
assert [word(wrap,p) for p in (0x10,0x14,0x1c)]==[0x38c00014,0x38a00028,0x3863000c]
assert [word(wrap,p) for p in (0x38,0x40,0x48)]==[0x39600008,0x39600017,0x3960000c]
assert [word(wrap,p)&0xffff for p in (0x3c,0x44,0x4c)]==[0x50,0x54,0x58]
add_off, add_index, kind = wrap_relocs[0]
add_name = ns["object_symbol"](wrapper.read_bytes(),add_index)[0]
assert len(wrap_relocs)==1 and kind==6 and add_off==0x50
assert add_name.startswith("?AddEvent@?$VariableEventQueue@$0BAAAA@$0BA@@")
assert ns["branch_target"](0x82476c50,word(raw(0x82476c50,4),0))==0x82435188
proof = {"scope":"linked static PPC" if link else "targeted native COFF",
         "artist_table":hex(original_table),"original_targets":{e:hex(t) for e,t in targets.items()},
         "candidate_cases":case,"menu_music_calls":calls,"menu_music_event":23,
         "menu_music_channel":40,"menu_music_record_header":[8,23,12],"menu_music_record_bytes":20,
         "objects":{str(p):ns["sha"](p) for p in (cache,post,wrapper)}}
if link:
    address, loaded = ns["linked_reader"](link)
    for event,(offset,target,_) in case.items():
        assert word(loaded(address(recv)+offset,4),0)==address(recv)+target
        assert loaded(address(recv)+target,8)==code[target:target+8]
    va = address(update)
    ns["fixed_equal"](body,loaded(va,len(body)),post_relocs)
    assert [ns["branch_target"](va+off,word(loaded(va+off,4),0)) for off in calls]==[address(output)]*2
    va = address(output)
    bound = loaded(va,len(wrap))
    ns["fixed_equal"](wrap,bound,wrap_relocs)
    assert ns["branch_target"](va+add_off,word(bound,add_off))==address(add_name)
    proof["xex_sha256"]=ns["sha"](link/"native-game.exe")
out = link/"R139_GUI_MATCH.json" if link else folder/"R139_GUI_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out))
