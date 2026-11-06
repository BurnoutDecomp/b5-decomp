"""Verify the linked native lock helpers and post-world replay queue path."""
import collections
import difflib
from functools import lru_cache
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
raw=ns["original"]
functions={
    238:(0x823b6f30,88,0,"Lock"),248:(0x823b6f88,88,0,"Unlock"),
    259:(0x823b6fe0,124,1,"Lock"),272:(0x823b7060,124,1,"Unlock"),
    286:(0x823b70e0,172,2,"Lock"),302:(0x823b7190,172,2,"Unlock"),
    319:(0x823b7240,220,3,"Lock"),338:(0x823b7320,220,3,"Unlock"),
    358:(0x823b7400,268,4,"Lock"),380:(0x823b7510,268,4,"Unlock"),
    403:(0x823b7620,316,5,"Lock"),428:(0x823b7760,316,5,"Unlock"),
    454:(0x823b78a0,364,6,"Lock"),482:(0x823b7a10,364,6,"Unlock"),
    511:(0x823b7b80,412,7,"Lock"),542:(0x823b7d20,412,7,"Unlock"),
}
targets={**ns["target_names"]}
for register in range(23,32):
    targets[0x82c08edc+4*(register-25)]=f"__savegprlr_{register}"
    targets[0x82c08f2c+4*(register-25)]=f"__restgprlr_{register}"
link=Path(sys.argv[1]).resolve()
address,loaded=ns["linked_reader"](link)
address=lru_cache(maxsize=None)(address)
mapping=(link/"native-game.map").read_text(errors="replace")
mapped={item[0] for item in re.findall(r"(?m)^\s+[0-9a-fA-F]{4}:[0-9a-fA-F]+\s+(\?(?:Unl|L)ockBuffersForIO@CgsModule@@\S+)\s+([0-9a-fA-F]{8})\s",mapping)}
invocation=json.loads((link/"invocation.json").read_text())
projection=json.loads((wave/"r133-intake/projection.json").read_text())
objects={r["source"]:Path(r["object"]) for r in invocation["objects"]}
selected=set(projection["selected_sources"])
assert len(selected)==14 and selected <= set(objects)
observed=collections.defaultdict(list)
discarded=collections.Counter()
for obj in sorted(objects[s] for s in selected):
    data=obj.read_bytes()
    object_sha=ns["sha"](obj)
    listing=obj.with_suffix(".asm").read_text(errors="replace")
    for item in re.finditer(r"(?ms)^; Begin code for function: (\?(?:Unl|L)ockBuffersForIO@CgsModule@@\S+).*?^; End code for function:",listing):
        symbol=item[1]
        lines=[int(n) for n in re.findall(r"\bli\s+r5,(\d+)\b",item[0])]
        if not lines or lines[0] not in functions:
            continue
        first=lines[0]
        ea,size,sources,verb=functions[first]
        assert lines==list(range(first,first+sources+1))
        if symbol not in mapped:
            discarded[first]+=1
            continue
        code,relocations=ns["collect"](obj,symbol)
        linked_va=address(symbol)
        linked=loaded(linked_va,size)
        ns["fixed_equal"](code,linked,relocations)
        artist=raw(ea,size)
        ns["fixed_equal"](artist,code,relocations)
        pairs={}
        branches=[]
        for offset,index,kind in relocations:
            if kind==6:
                name,_,_=ns["object_symbol"](data,index)
                original_target=ns["branch_target"](ea+offset,struct.unpack_from(">I",artist,offset)[0])
                assert targets[original_target]==name,(symbol,offset,name,hex(original_target))
                linked_target=ns["branch_target"](linked_va+offset,struct.unpack_from(">I",linked,offset)[0])
                assert linked_target==address(name),(symbol,offset,name,hex(linked_target))
                branches.append(name)
            elif kind in (16,17):
                assert kind not in pairs.setdefault(index,{})
                pairs[index][kind]=offset
        found=[]
        for index,pair in pairs.items():
            literal=ns["symbol_literal"](data,index)
            literal_address=ns["data_target"](artist,pair[16],pair[17])
            assert raw(literal_address,len(literal)+1)==literal.encode()+bytes(1)
            linked_literal=ns["data_target"](linked,pair[16],pair[17])
            assert loaded(linked_literal,len(literal)+1)==literal.encode()+bytes(1)
            found.append(literal)
        assert set(found)=={ns["source_path"],"lpInputBuffer",*(f"lpOutputBuffer{i}" for i in range(sources))}
        assert [sum(n.startswith("?"+w+"Assert@") for n in branches) for w in ("Begin","Fire","End")]==[sources+1]*3
        observed[first].append({"object":str(obj),"object_sha256":object_sha,"symbol":symbol,
                                "all_fixed_bits_match":True,"linked_va":hex(linked_va),"branch_symbols":branches})
assert set(observed)==set(functions) and all(observed.values())
xex=link/"native-game.exe"
result=json.loads((link/"result.json").read_text())
digest=ns["sha"](xex)
assert result["exit_code"]==0 and result["all_post_checks_pass"] and digest==result["outputs"][str(xex)]["sha256"]
proof={"scope":"strict linked image","xex_sha256":digest,"instantiations_checked":sum(map(len,observed.values())),
       "unmapped_compiled_instances":dict(discarded),
       "families":[{"artist_va":hex(ea),"bytes":size,"source_buffers":sources,"operation":verb,
                    "first_assert_line":first,"instances":observed[first]}
                   for first,(ea,size,sources,verb) in functions.items()]}
game=objects["src/GameSource/Game/BrnGameModule.cpp"]
game_base=wave/"r131-intake/native-selected/BrnGameModule-f766a093.obj"
coff=ns["coff"]
coff["symbol"]=b"?DoUpdate_GameStatePostWorld@BrnGameModule@BrnGame@@"
symbol,before,_=coff["read_function"](game_base)
_,after,caller_relocs=coff["read_function"](game)
original=raw(0x823e92a8,620)
aligned=lambda x:sum(m.size for m in difflib.SequenceMatcher(
    None,coff["words"](original),coff["words"](x),autojunk=False).get_matching_blocks())
assert (len(before),len(after),aligned(before),aligned(after))==(628,572,41,77)
listing=game.with_suffix(".asm").read_text(errors="replace")
body=listing.split("; Begin code for function: "+symbol,1)[1].split("; End code for function:",1)[0]
calls=re.findall(r"\sbl\s+(\S+)",body)
lock,=[x for x in calls if x.startswith("?LockBuffersForIO@CgsModule@@")]
unlock,=[x for x in calls if x.startswith("?UnlockBuffersForIO@CgsModule@@")]
replay,=[x for x in calls if x.startswith("?GetGameEventQueue@OutputBuffer_PostSim@ReplayIO@BrnReplays@@QBAPBV")]
append,=[x for x in calls if x.startswith("??$Append@$0GAA@$0BA@@?$VariableEventQueue@$0GAA@$0BA@@CgsModule@@")]
director_reads=[i for i,x in enumerate(calls) if x.startswith("?GetDirectorOutputInterface@OutputBuffer@DirectorIO@BrnDirector@@")]
assert len(director_reads)==2
assert calls.index(lock)<director_reads[0]<director_reads[1]<calls.index(replay)<calls.index(append)<calls.index(unlock)
assert ns["branch_target"](0x823e93d0,int.from_bytes(raw(0x823e93d0,4),"big"))==0x823bb478
assert ns["branch_target"](0x823e9358,int.from_bytes(raw(0x823e9358,4),"big"))==0x823b7620
assert ns["branch_target"](0x823e9494,int.from_bytes(raw(0x823e9494,4),"big"))==0x823b7760
assert any(x["symbol"]==lock for x in observed[403])
assert any(x["symbol"]==unlock for x in observed[428])
proof["postworld"]={"artist_va":"0x823e92a8","artist_bytes":620,"before_bytes":628,"after_bytes":572,
                    "before_aligned_words":41,"after_aligned_words":77,"replay_read_accessor":replay,
                    "direct_call_order":[lock,replay,append,unlock],"director_read_calls":len(director_reads),
                    "object_sha256":ns["sha"](game)}

caller_va=address(symbol)
caller=loaded(caller_va,len(after))
ns["fixed_equal"](after,caller,caller_relocs)
linked_targets=[ns["branch_target"](caller_va+offset,struct.unpack_from(">I",caller,offset)[0])
                for offset,_,kind in caller_relocs if kind==6]
expected=[address(name) for name in (lock,replay,append,unlock)]
assert all(linked_targets.count(target)==1 for target in expected)
assert [linked_targets.index(target) for target in expected]==sorted(linked_targets.index(target) for target in expected)
director=next(name for name in calls if name.startswith("?GetDirectorOutputInterface@OutputBuffer@DirectorIO@BrnDirector@@"))
assert linked_targets.count(address(director))==2
proof["postworld"]["linked_va"]=hex(caller_va)
proof["postworld"]["linked_call_targets"]=[hex(x) for x in expected]


shared={first:{(row["symbol"],row["linked_va"]) for row in rows} for first,rows in observed.items()}
assert len(shared)==16 and all(len(x)==1 for x in shared.values())
assert len({va for row in shared.values() for _,va in row})==16
assert not re.search(r"(?m)^\s+[0-9a-fA-F]{4}:[0-9a-fA-F]+\s+\?\?\$(?:Unl|L)ockBuffersForIO@",mapping)
prior=json.loads((wave/"r132-link/R132_MATCH.json").read_text())
prior_bodies={row["linked_va"]:family["bytes"] for family in prior["families"] for row in family["instances"]}
assert len(prior_bodies)==177
proof["shared_addresses"]={str(first):next(iter(row))[1] for first,row in shared.items()}
proof["unique_bodies_before"]=len(prior_bodies)
proof["unique_bodies_after"]=len(shared)
proof["helper_code_bytes_before"]=sum(prior_bodies.values())
proof["helper_code_bytes_after"]=sum(family[1] for family in functions.values())

previous_game=wave/"r132-intake/native-selected/BrnGameModule-f766a093.obj"
callers={}
for caller_name,pair in (("DoUpdate_GameStatePostWorld",(403,428)),("DoUpdate_World",(403,428)),
                         ("DoUpdate_Sound",(403,428)),("DoUpdate_Effects",(454,482))):
    coff["symbol"]=f"?{caller_name}@BrnGameModule@BrnGame@@".encode()
    mangled,earlier,earlier_relocs=coff["read_function"](previous_game)
    current,current_relocs=ns["collect"](game,mangled)
    assert earlier==current and [(o,k) for o,_,k in earlier_relocs]==[(o,k) for o,_,k in current_relocs]
    caller_address=address(mangled)
    linked_code=loaded(caller_address,len(current))
    ns["fixed_equal"](current,linked_code,current_relocs)
    destinations=[ns["branch_target"](caller_address+off,struct.unpack_from(">I",linked_code,off)[0])
                  for off,_,kind in current_relocs if kind==6]
    pair_targets=[int(proof["shared_addresses"][str(first)],16) for first in pair]
    assert all(destinations.count(x)==1 for x in pair_targets)
    assert destinations.index(pair_targets[0])<destinations.index(pair_targets[1])
    callers[caller_name]={"object_instruction_bytes":len(current),"same_object_bytes_as_r132":True,
                          "linked_lock_pair":[hex(x) for x in pair_targets]}
proof["unchanged_callers"]=callers

out=link/"R133_MATCH.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out),proof["instantiations_checked"])
