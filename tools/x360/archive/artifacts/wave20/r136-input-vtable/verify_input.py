"""Verify native Input vtable order and its three dispatch instructions."""
import json
from pathlib import Path
import struct
import sys

folder=Path(__file__).resolve().parent
wave=folder.parent
reader=wave/"r128-batch/verify_matches.py"
ns={"__file__":str(reader)}
exec(reader.read_text(encoding="utf-8-sig").split("\nif len(sys.argv)==2:",1)[0],ns)
coff,raw=ns["coff"],ns["original"]
game_source="src/GameSource/Game/BrnGameModule.cpp"
flow_source="src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates_wBT_01.cpp"
assert len(sys.argv) in (1,2)
link=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else None
if link:
    inv=json.loads((link/"invocation.json").read_text())
    inputs={r["source"]:Path(r["object"]) for r in inv["objects"]}
    game,flow=inputs[game_source],inputs[flow_source]
    address,loaded=ns["linked_reader"](link)
else:
    game,=folder.glob("BrnGameModule-*.obj")
    flow,=(folder/"loading").glob("BrnGameMainFlowStates_wBT_01-*.obj")
prior=json.loads((wave/"r135-intake/composite-active-native.json").read_text())
before={r["source"]:Path(r["object"]) for r in prior["results"]}

vtable="??_7InputModule@CgsInput@@6B@"
data=game.read_bytes()
code,relocs=ns["collect"](game,vtable)
assert len(code)==76 and [(off,kind) for off,_,kind in relocs]==[(p,2) for p in range(0,76,4)]
native=[
    (0x828f83d0,"?Construct@InputModule@CgsInput@@"),
    (0x8286e7a0,"?Prepare@ModuleSingleBuffered@CgsModule@@"),
    (0x828ef100,"?Release@InputModule@CgsInput@@"),
    (0x828f8438,"?Destruct@InputModule@CgsInput@@"),
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
    (0x828eefd8,"?Prepare@InputModule@CgsInput@@"),
    (0x82903328,"?PreWorldUpdate@InputModule@CgsInput@@"),
    (0x828f8478,"?PostWorldUpdate@InputModule@CgsInput@@"),
]
assert raw(0x827de870,4).hex()=="394bf648" and raw(0x827de87c,4).hex()=="915f0000"
table=[]
if link:
    table_va=address(vtable)
    loaded_table=loaded(table_va,76)
for (off,index,_),(target,prefix) in zip(relocs,native,strict=True):
    symbol,_,_=ns["object_symbol"](data,index)
    assert symbol.startswith(prefix)
    assert int.from_bytes(raw(0x820cf648+off,4),"big")==target
    row={"slot":off,"artist_target":hex(target),"symbol":symbol}
    if link:
        bound,=struct.unpack_from(">I",loaded_table,off)
        assert bound==address(symbol)
        row["linked_target"]=hex(bound)
    table.append(row)

callers={}
for prefix,source,actual,artist,old_slot,new_slot in (
    ("?DoUpdate_InputPreWorld@BrnGameModule@BrnGame@@",game_source,game,0x823c5738,0x40,0x44),
    ("?DoUpdate_InputPostWorld@BrnGameModule@BrnGame@@",game_source,game,0x823c5b64,0x44,0x48),
    ("?LoadControllerModule@LoadingScriptedState@@",flow_source,flow,0x823c6a98,0x48,0x40),
):
    coff["symbol"]=prefix.encode()
    symbol,old,old_relocs=coff["read_function"](before[source])
    new,rels=ns["collect"](actual,symbol)
    assert len(old)==len(new) and [(p,k) for p,_,k in old_relocs]==[(p,k) for p,_,k in rels]
    old_words,new_words=coff["words"](old),coff["words"](new)
    (off,a,b),=[(i*4,int(x,16),int(y,16)) for i,(x,y) in enumerate(zip(old_words,new_words)) if x!=y]
    assert a==0x816b0000|old_slot and b==0x816b0000|new_slot
    assert raw(artist,12)==new[off:off+12]
    row={"artist_dispatch":hex(artist),"previous_slot":old_slot,"native_slot":new_slot,"caller_bytes":len(new),
         "object_instruction_offset":off,"only_one_changed_word":True}
    if link:
        va=address(symbol)
        linked_code=loaded(va,len(new))
        ns["fixed_equal"](new,linked_code,rels)
        assert linked_code[off:off+12]==raw(artist,12)
        row["linked_dispatch"]=hex(va+off)
    callers[symbol]=row

proof={"scope":"strict linked image" if link else "targeted native COFF only",
       "artist_vtable":"0x820cf648","slots":table,"callers":callers,
       "objects":{str(p):ns["sha"](p) for p in (game,flow)}}
if link:
    xex=link/"native-game.exe"
    result=json.loads((link/"result.json").read_text())
    proof["xex_sha256"]=ns["sha"](xex)
    assert result["exit_code"]==0 and result["all_post_checks_pass"]
    assert proof["xex_sha256"]==result["outputs"][str(xex)]["sha256"]
    proof["linked_vtable"]=hex(table_va)
    out=link/"R136_MATCH.json"
else:
    out=folder/"R136_INPUT_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out))
