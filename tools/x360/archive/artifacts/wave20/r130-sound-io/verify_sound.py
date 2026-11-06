"""Verify the sound lock/read flow and native effects restart ABI."""
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
coff,raw,sha=ns["coff"],ns["original"],ns["sha"]
collect,branch,fixed=ns["collect"],ns["branch_target"],ns["fixed_equal"]
assert len(sys.argv) in (1,2)
link=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else None
if link:
    inventory=json.loads((link/"invocation.json").read_text())
    objects={row["source"]:Path(row["object"]) for row in inventory["objects"]}
    game=objects["src/GameSource/Game/BrnGameModule.cpp"]
    effects=objects["src/GameSource/Effects/EffectsModule.cpp"]
    address,loaded=ns["linked_reader"](link)
else:
    game,=folder.glob("BrnGameModule-*.obj")
    effects,=folder.glob("EffectsModule-*.obj")
old=wave/"r129-intake/native-selected/BrnGameModule-f766a093.obj"
listing=game.with_suffix(".asm").read_text(errors="replace")

replay="?GetStatusInterface@OutputBuffer_PreSim@ReplayIO@BrnReplays@@QBAPBUStatusInterface@23@XZ"
restart="?RestartEffects@EffectsModule@BrnEffects@@QAAXXZ"
effect_lock="?LockForRead@IOBuffer@CgsModule@@QBAXXZ"
effect_unlock="?UnlockForRead@IOBuffer@CgsModule@@QBAXXZ"
assert branch(0x823dcfdc,int.from_bytes(raw(0x823dcfdc,4),"big"))==0x823bb080
assert branch(0x823dd1a0,int.from_bytes(raw(0x823dd1a0,4),"big"))==0x822793e0
assert raw(0x823dcfc0,4).hex()=="3d60009a" and raw(0x823dcfc8,4).hex()=="616b0bd8"
assert raw(0x823dd190,4).hex()=="61568700"
results={}
for method,ea,size,expected in (("DoUpdate_Sound",0x823dcec0,484,(648,388,36,49)),
                                ("DoUpdate_Effects",0x823dd0a8,500,(448,456,52,62))):
    prefix=f"?{method}@BrnGameModule@BrnGame@@"
    coff["symbol"]=prefix.encode()
    symbol,base_code,_=coff["read_function"](old)
    candidate,rels=collect(game,symbol)
    artist=raw(ea,size)
    words=coff["words"](artist)
    def aligned(code):
        return sum(m.size for m in difflib.SequenceMatcher(None,words,coff["words"](code),autojunk=False).get_matching_blocks())
    counts=(len(base_code),len(candidate),aligned(base_code),aligned(candidate))
    assert counts==expected,counts
    body=listing.split("; Begin code for function: "+symbol,1)[1].split("; End code for function:",1)[0]
    calls=re.findall(r"\sbl\s+(\S+)",body)
    lock,=re.findall(r"\sbl\s+(\?\?\$LockBuffersForIO@\S+)",body)
    unlock,=re.findall(r"\sbl\s+(\?\?\$UnlockBuffersForIO@\S+)",body)
    sequence=[lock,replay,unlock]
    if method=="DoUpdate_Sound":
        sequence=[lock,effect_lock,replay,unlock,effect_unlock]
        assert re.search(r"\bori\s+r11,r11,3032\b.*?\blwzx\s+r5,r31,r11\b.*?\bbl\s+\?BridgeGuiToSound",body,re.S)
        helper_addresses=(0x823b7620,0x823b7760,316)
    else:
        sequence=[lock,restart,replay,unlock]
        assert re.search(r"\bori\s+r23,r10,34560\b.*?\badd\s+r3,r30,r23\b.*?\bbl\s+"+re.escape(restart),body,re.S)
        helper_addresses=(0x823b78a0,0x823b7a10,364)
    assert all(calls.count(x)==1 for x in sequence)
    assert [calls.index(x) for x in sequence]==sorted(calls.index(x) for x in sequence)
    assert not any(x.startswith("?GetStatusInterface@OutputBuffer_PreSim@ReplayIO@BrnReplays@@QA") for x in calls)
    for name,original_va in zip((lock,unlock),helper_addresses[:2]):
        native_code,native_rels=collect(game,name)
        fixed(raw(original_va,helper_addresses[2]),native_code,native_rels)
        if link:
            fixed(native_code,loaded(address(name),len(native_code)),native_rels)
    if link:
        caller_va=address(symbol)
        linked_code=loaded(caller_va,len(candidate))
        fixed(candidate,linked_code,rels)
        dests=[branch(caller_va+off,struct.unpack_from(">I",linked_code,off)[0])
               for off,_,kind in rels if kind==6]
        targets=[address(name) for name in sequence]
        assert all(dests.count(x)==1 for x in targets)
        assert [dests.index(x) for x in targets]==sorted(dests.index(x) for x in targets)
    results[method]={"artist_va":hex(ea),"artist_bytes":size,"before_bytes":counts[0],"after_bytes":counts[1],
                     "before_aligned_words":counts[2],"after_aligned_words":counts[3],
                     "expected_call_order":sequence,"each_helper_bytes":helper_addresses[2]}

new,restart_rels=collect(effects,restart)
assert [(p,k) for p,_,k in restart_rels]==[(0,16),(0,18),(8,17),(8,18)]
assert restart_rels[0][1]==restart_rels[2][1]
flag,_,_=ns["object_symbol"](effects.read_bytes(),restart_rels[0][1])
assert "sbRestartEffects" in flag
artist=raw(0x822793e0,16)
fixed(artist,new,restart_rels)
assert ns["data_target"](artist,0,8)==0x82fab694
if link:
    body=loaded(address(restart),16)
    fixed(new,body,restart_rels)
    assert ns["data_target"](body,0,8)==address(flag)

proof={"scope":"strict linked image" if link else "targeted native COFF only",
       "game_object_sha256":sha(game),"effects_object_sha256":sha(effects),"callers":results,
       "restart":{"artist_va":"0x822793e0","bytes":16,"exact_after_flag_address":True,"flag":flag}}
if link:
    xex=link/"native-game.exe"
    result=json.loads((link/"result.json").read_text())
    assert result["exit_code"]==0 and result["all_post_checks_pass"]
    proof["xex_sha256"]=sha(xex)
    assert proof["xex_sha256"]==result["outputs"][str(xex)]["sha256"]
    out=link/"R130_MATCH.json"
else:
    out=folder/"R130_SOUND_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,sha(out))
