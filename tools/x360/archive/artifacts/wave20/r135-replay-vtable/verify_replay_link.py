"""Verify Replay vtable order, native dispatch offsets and release diagnostics."""
import difflib
import json
from pathlib import Path
import re
import struct
import sys

folder=Path(__file__).resolve().parent
wave=folder.parent
reader=wave/"r128-batch/verify_matches.py"
ns={"__file__":str(reader)}
exec(reader.read_text(encoding="utf-8-sig").split("\nif len(sys.argv)==2:",1)[0],ns)
coff,raw=ns["coff"],ns["original"]
assert len(sys.argv) in (1,2)
link=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else None
if link:
    inv=json.loads((link/"invocation.json").read_text())
    inputs={r["source"]:Path(r["object"]) for r in inv["objects"]}
    game=inputs["src/GameSource/Game/BrnGameModule.cpp"]
    replay=inputs["src/GameSource/Replays/BrnReplayModule.cpp"]
    release=inputs["src/GameSource/Replays/BrnReplayModule_Prepare.cpp"]
    flow=inputs["src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"]
    address,loaded=ns["linked_reader"](link)
else:
    game,=(folder/"release").glob("BrnGameModule-*.obj")
    replay,=(folder/"release").glob("BrnReplayModule-*.obj")
    release,=(folder/"release").glob("BrnReplayModule_Prepare-*.obj")
    flow,=(folder/"flow").glob("BrnGameMainFlowStates-*.obj")
earlier=wave/"r134-intake/native-selected/BrnGameModule-f766a093.obj"

vtable="??_7ReplayModule@BrnReplays@@6B@"
values,rels=ns["collect"](replay,vtable)
assert len(values)==76 and [(p,k) for p,_,k in rels]==[(p,2) for p in range(0,76,4)]
expected=(
    (0x82656e20,"?Construct@ReplayModule@BrnReplays@@"),
    (0x8286e7a0,"?Prepare@ModuleSingleBuffered@CgsModule@@"),
    (0x8264b400,"?Release@ReplayModule@BrnReplays@@"),
    (0x82652878,"?Destruct@ReplayModule@BrnReplays@@"),
    (0x8284cb38,"?Update@ModuleSingleBuffered@CgsModule@@"),
    (0x8286e058,"?SetMultiThreaded@ModuleSingleBuffered@CgsModule@@"),
    (0x8286eb48,"?LockForInput@ModuleSingleBuffered@CgsModule@@"),
    (0x8286ebf0,"?UnlockForInput@ModuleSingleBuffered@CgsModule@@"),
    (0x8286ec98,"?LockForOutput@ModuleSingleBuffered@CgsModule@@"),
    (0x8286ed40,"?UnlockForOutput@ModuleSingleBuffered@CgsModule@@"),
    (0x827dccf8,"?DestroyInputDataStructure@ModuleSingleBuffered@CgsModule@@"),
    (0x827dcd98,"?DestroyOutputDataStructure@ModuleSingleBuffered@CgsModule@@"),
    (0x827dce38,"?PrepareDataStructures@ModuleSingleBuffered@CgsModule@@"),
    (0x827dced8,"?ReleaseDataStructures@ModuleSingleBuffered@CgsModule@@"),
    (0x827dcbb8,"?CreateInputDataStructure@ModuleSingleBuffered@CgsModule@@"),
    (0x827dcc58,"?CreateOutputDataStructure@ModuleSingleBuffered@CgsModule@@"),
    (0x82652768,"?Prepare@ReplayModule@BrnReplays@@"),
    (0x82660e78,"?Update_PreSim@ReplayModule@BrnReplays@@"),
    (0x8265f618,"?Update_PostSim@ReplayModule@BrnReplays@@"),
)
assert raw(0x827e0414,4).hex()=="396bfef0" and raw(0x827e0424,4).hex()=="917f0000"
replay_data=replay.read_bytes()
slots=[]
if link:
    table_va=address(vtable)
    linked_table=loaded(table_va,76)
for (off,idx,_),(native,prefix) in zip(rels,expected,strict=True):
    name,_,_=ns["object_symbol"](replay_data,idx)
    assert name.startswith(prefix)
    assert int.from_bytes(raw(0x820cfef0+off,4),"big")==native
    row={"offset":off,"artist_target":hex(native),"symbol":name}
    if link:
        bound=struct.unpack_from(">I",linked_table,off)[0]
        assert bound==address(name)
        row["linked_target"]=hex(bound)
    slots.append(row)

callers={}
for method,native,slot in (("DoUpdate_ReplaysPreSim",0x823dd370,0x44),("DoUpdate_ReplaysPostSim",0x823dd56c,0x48)):
    coff["symbol"]=f"?{method}@BrnGameModule@BrnGame@@".encode()
    mangled,old,old_relocs=coff["read_function"](earlier)
    new,new_relocs=ns["collect"](game,mangled)
    assert len(old)==len(new)
    assert [(p,k) for p,_,k in old_relocs]==[(p,k) for p,_,k in new_relocs]
    a,b=coff["words"](old),coff["words"](new)
    (offset,before,after),=[(i*4,int(x,16),int(y,16)) for i,(x,y) in enumerate(zip(a,b)) if x!=y]
    assert before==0x816b0000|(slot-4) and after==0x816b0000|slot
    assert raw(native,12)==new[offset:offset+12]
    if link:
        linked=loaded(address(mangled),len(new))
        ns["fixed_equal"](new,linked,new_relocs)
        assert linked[offset:offset+12]==raw(native,12)
    callers[method]={"old_slot":slot-4,"native_slot":slot,"artist_dispatch":hex(native),
                     "compiled_instruction_offset":offset,"caller_bytes":len(new)}

flow_asm=flow.with_suffix(".asm").read_text(errors="replace")
call_comment="GetReplayModule().Prepare(lpGameDataOutput->GetAllocatorList())"
assert flow_asm.count(call_comment)==1
fragment=flow_asm.split(call_comment,1)[1].split("bctrl",1)[0]
assert re.search(r"\blwz\s+r11,40h\(r20\)",fragment)
assert raw(0x823ef8d8,4).hex()=="81540040" and raw(0x823ef8e4,4).hex()=="4e800421"
if link:
    flow_symbol,=re.findall(r"(?m)^; Begin code for function: (\?Update@MainGameFlowStateInitialLoadingScreen@@\S+)",flow_asm)
    code,relocations=ns["collect"](flow,flow_symbol)
    bound=loaded(address(flow_symbol),len(code))
    jump_relocs=[(off,index) for off,index,kind in relocations if kind==2]
    assert len(jump_relocs)==10
    flow_va=address(flow_symbol)
    flow_data=flow.read_bytes()
    for off,index in jump_relocs:
        name,value,section=ns["object_symbol"](flow_data,index)
        assert name.startswith("$LN") and section>0 and 0<=value<len(code)
        addend=struct.unpack_from(">I",code,off)[0]
        assert struct.unpack_from(">I",bound,off)[0]==flow_va+value+addend
    ns["masks"][2]=0xffffffff
    ns["fixed_equal"](code,bound,relocations)
    del ns["masks"][2]
    assert bytes.fromhex("817400407fa3eb787d6903a64e800421") in bound
callers["InitialLoadingScreen::Update"]={"native_slot":64,"artist_dispatch":"0x823ef8d8"}

release_symbol="?Release@ReplayModule@BrnReplays@@UAA_NXZ"
release_code,release_relocs=ns["collect"](release,release_symbol)
original_release=raw(0x8264b400,164)
assert len(release_code)==148
release_listing=release.with_suffix(".asm").read_text(errors="replace").split(
    "; Begin code for function: "+release_symbol,1)[1].split("; End code for function:",1)[0]
assert re.search(r"\bli\s+r5,268\b",release_listing)
assert len(re.findall(r"\bstw\s+\S+,22Ch\(r31\)",release_listing))==2
assert re.search(r"\bstw\s+\S+,228h\(r31\)",release_listing)
branches=[]
literals=[]
blob=release.read_bytes()
for off,index,kind in release_relocs:
    if kind==6:
        name,_,_=ns["object_symbol"](blob,index)
        branches.append(name)
    if kind==16:
        literals.append(ns["symbol_literal"](blob,index))
assert branches==["?BeginAssert@Assert@CgsDev@@YAXXZ","?FireAssert@Assert@CgsDev@@YAXPBD0H@Z",
                  "?EndAssert@Assert@CgsDev@@YAXXZ","?Release@ModuleSingleBuffered@CgsModule@@UAA_NXZ"]
assert set(literals)=={raw(0x8209dd68,80).split(bytes(1))[0].decode(),
                      raw(0x82009578,16).split(bytes(1))[0].decode()}
if link:
    release_va=address(release_symbol)
    linked_release=loaded(release_va,len(release_code))
    ns["fixed_equal"](release_code,linked_release,release_relocs)
    calls=[ns["branch_target"](release_va+p,struct.unpack_from(">I",linked_release,p)[0])
           for p,_,kind in release_relocs if kind==6]
    assert calls==[address(name) for name in branches]

proof={"scope":"strict linked image" if link else "targeted native COFF only",
       "artist_vtable":"0x820cfef0","slot_count":len(slots),"slots":slots,"callers":callers,
       "release":{"artist_bytes":164,"compiled_bytes":len(release_code),"aligned_words":
                  sum(m.size for m in difflib.SequenceMatcher(None,coff["words"](original_release),
                      coff["words"](release_code),autojunk=False).get_matching_blocks()),
                  "assert_line":268,"named_branches":branches,"diagnostics":literals},
       "objects":{str(p):ns["sha"](p) for p in (game,replay,release,flow)}}
if link:
    result=json.loads((link/"result.json").read_text())
    xex=link/"native-game.exe"
    proof["xex_sha256"]=ns["sha"](xex)
    assert result["exit_code"]==0 and result["all_post_checks_pass"]
    assert proof["xex_sha256"]==result["outputs"][str(xex)]["sha256"]
    proof["linked_vtable"]=hex(table_va)
    out=link/"R135_MATCH.json"
else:
    out=folder/"R135_REPLAY_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out))
