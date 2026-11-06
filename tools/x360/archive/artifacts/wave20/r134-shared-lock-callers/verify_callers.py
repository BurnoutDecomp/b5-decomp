"""Verify the exact sound pre-update body and shared calls in four update paths."""
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
before=wave/"r133-intake/native-selected/BrnGameModule-f766a093.obj"
assert len(sys.argv) in (1,2)
link=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else None
if link:
    invocation=json.loads((link/"invocation.json").read_text())
    after,=[Path(row["object"]) for row in invocation["objects"] if row["source"]=="src/GameSource/Game/BrnGameModule.cpp"]
    address,loaded=ns["linked_reader"](link)
else:
    after,=folder.glob("BrnGameModule-*.obj")
data=after.read_bytes()
listing=after.with_suffix(".asm").read_text(errors="replace")

lock="?LockBuffersForIO@CgsModule@@YAXPAUIOBuffer@1@PBU21@@Z"
unlock="?UnlockBuffersForIO@CgsModule@@YAXPAUIOBuffer@1@PBU21@@Z"
lock5="?LockBuffersForIO@CgsModule@@YAXPAUIOBuffer@1@PBU21@1111@Z"
unlock5="?UnlockBuffersForIO@CgsModule@@YAXPAUIOBuffer@1@PBU21@1111@Z"
target_prefix={
    0x82c08ee0:"__savegprlr_26",0x82c08f30:"__restgprlr_26",
    0x821f1198:"?StartMonitor@PerfMonCpu@CgsDev@@",0x821f1308:"?StopMonitor@PerfMonCpu@CgsDev@@",
    0x826eb928:"?PreUpdate@RootSoundModule@Module@BrnSound@@",
    0x823b6fe0:lock,0x823b7060:unlock,
    0x823b8bb8:"?GetPreUpdateOutput@RootPreUpdateOutputBuffer@Io@Module@BrnSound@@",
    0x8284f238:"?GetGuiEvents@InputBuffer@CgsGuiModuleIO@CgsGui@@",
    0x823daf20:"??$Append@$0BAA@$0BA@@?$VariableEventQueue@$0IAAA@$0BA@@CgsModule@@",
    0x823c63c0:"?BridgeSoundToTraining@BrnGameModule@BrnGame@@",
}
specs=(
    ("DoUpdate_GameStatePreWorld",0x823ee0e8,1008,(1016,936,78,77),(lock,unlock,unlock)),
    ("DoPreUpdate_Sound",0x823ee4d8,168,(256,168,17,29),(lock,unlock)),
    ("DoUpdate_ReplaysPreSim",0x823dd2a0,360,(416,264,40,44),(lock,unlock)),
    ("DoUpdate_ReplaysPostSim",0x823dd408,436,(772,340,19,52),(lock5,unlock5)),
)
results={}
for method,ea,size,expected,expected_locks in specs:
    coff["symbol"]=f"?{method}@BrnGameModule@BrnGame@@".encode()
    symbol,old,_=coff["read_function"](before)
    new,rels=ns["collect"](after,symbol)
    native=raw(ea,size)
    words=coff["words"](native)
    def aligned(code):
        return sum(m.size for m in difflib.SequenceMatcher(None,words,coff["words"](code),autojunk=False).get_matching_blocks())
    assert (len(old),len(new),aligned(old),aligned(new))==expected
    body=listing.split("; Begin code for function: "+symbol,1)[1].split("; End code for function:",1)[0]
    calls=re.findall(r"\sbl\s+(\S+)",body)
    actual_locks=tuple(x for x in calls if x.startswith(("?LockBuffersForIO@","?UnlockBuffersForIO@")))
    assert actual_locks==expected_locks
    row={"artist_va":hex(ea),"artist_bytes":size,"old_bytes":len(old),"new_bytes":len(new),
         "old_aligned_words":aligned(old),"new_aligned_words":aligned(new),"lock_call_order":actual_locks}
    if method=="DoUpdate_ReplaysPostSim":
        calls_between=[x for x in calls[calls.index(lock5)+1:calls.index(unlock5)]
                       if x.startswith(("?GetRequestInterface@","?RegisterSerialiser@"))]
        assert [x.split("@",1)[0] for x in calls_between]==["?GetRequestInterface","?RegisterSerialiser"]*2
        row["serialiser_call_order"]=calls_between
    if link:
        va=address(symbol)
        linked=loaded(va,len(new))
        ns["fixed_equal"](new,linked,rels)
        linked_calls=[ns["branch_target"](va+off,struct.unpack_from(">I",linked,off)[0])
                      for off,_,kind in rels if kind==6]
        targets={address(name) for name in expected_locks}
        assert tuple(x for x in linked_calls if x in targets)==tuple(address(name) for name in expected_locks)
        row["linked_va"]=hex(va)
    if method=="DoPreUpdate_Sound":
        ns["fixed_equal"](native,new,rels)
        branches=[]
        for offset,index,kind in rels:
            assert kind==6
            name,_,_=ns["object_symbol"](data,index)
            original_target=ns["branch_target"](ea+offset,struct.unpack_from(">I",native,offset)[0])
            assert name.startswith(target_prefix[original_target]),(name,hex(original_target))
            if link:
                assert ns["branch_target"](va+offset,struct.unpack_from(">I",linked,offset)[0])==address(name)
            branches.append(name)
        assert len(branches)==13
        row["exact_after_branch_addresses"]=True
        row["named_branches"]=branches
    results[method]=row

proof={"scope":"strict linked image" if link else "targeted native COFF only",
       "game_object_sha256":ns["sha"](after),"methods":results}
if link:
    xex=link/"native-game.exe"
    result=json.loads((link/"result.json").read_text())
    assert result["exit_code"]==0 and result["all_post_checks_pass"]
    proof["xex_sha256"]=ns["sha"](xex)
    assert proof["xex_sha256"]==result["outputs"][str(xex)]["sha256"]
    out=link/"R134_MATCH.json"
else:
    out=folder/"R134_CALLERS_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out))
