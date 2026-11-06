"""Check the original one- through four-source I/O helper code and relocations."""
import collections
import difflib
import json
from pathlib import Path
import re
import struct

folder=Path(__file__).resolve().parent
wave=folder.parent.parent
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
observed=collections.defaultdict(list)
game,=folder.glob("BrnGameModule-*.obj")
inputs=[game,*sorted(folder.parent.glob("BrnWorldModule-*.obj")),*sorted(folder.parent.glob("BrnPhysicsModule-*.obj"))]
assert len(inputs)==3
for obj in inputs:
    data=obj.read_bytes()
    object_sha=ns["sha"](obj)
    listing=obj.with_suffix(".asm").read_text(errors="replace")
    for item in re.finditer(r"(?ms)^; Begin code for function: (\?\?\$(?:Unl|L)ockBuffersForIO@\S+).*?^; End code for function:",listing):
        symbol=item[1]
        lines=[int(n) for n in re.findall(r"\bli\s+r5,(\d+)\b",item[0])]
        if not lines or lines[0] not in functions:
            continue
        first=lines[0]
        ea,size,sources,verb=functions[first]
        assert lines==list(range(first,first+sources+1))
        code,relocations=ns["collect"](obj,symbol)
        artist=raw(ea,size)
        ns["fixed_equal"](artist,code,relocations)
        pairs={}
        branches=[]
        for offset,index,kind in relocations:
            if kind==6:
                name,_,_=ns["object_symbol"](data,index)
                address=ns["branch_target"](ea+offset,struct.unpack_from(">I",artist,offset)[0])
                assert targets[address]==name,(symbol,offset,name,hex(address))
                branches.append(name)
            elif kind in (16,17):
                assert kind not in pairs.setdefault(index,{})
                pairs[index][kind]=offset
        found=[]
        for index,pair in pairs.items():
            literal=ns["symbol_literal"](data,index)
            literal_address=ns["data_target"](artist,pair[16],pair[17])
            assert raw(literal_address,len(literal)+1)==literal.encode()+bytes(1)
            found.append(literal)
        assert set(found)=={ns["source_path"],"lpInputBuffer",*(f"lpOutputBuffer{i}" for i in range(sources))}
        assert [sum(n.startswith("?"+w+"Assert@") for n in branches) for w in ("Begin","Fire","End")]==[sources+1]*3
        observed[first].append({"object":str(obj),"object_sha256":object_sha,"symbol":symbol,
                                "all_fixed_bits_match":True,"branch_symbols":branches})
assert set(observed)==set(functions) and all(observed.values())
proof={"scope":"targeted native COFF only","instantiations_checked":sum(map(len,observed.values())),
       "families":[{"artist_va":hex(ea),"bytes":size,"source_buffers":sources,"operation":verb,
                    "first_assert_line":first,"instances":observed[first]}
                   for first,(ea,size,sources,verb) in functions.items()]}

game_base=wave/"r131-intake/native-selected/BrnGameModule-f766a093.obj"
coff=ns["coff"]
coff["symbol"]=b"?DoUpdate_GameStatePostWorld@BrnGameModule@BrnGame@@"
symbol,before,_=coff["read_function"](game_base)
_,after,_=coff["read_function"](game)
original=raw(0x823e92a8,620)
aligned=lambda x:sum(m.size for m in difflib.SequenceMatcher(
    None,coff["words"](original),coff["words"](x),autojunk=False).get_matching_blocks())
assert (len(before),len(after),aligned(before),aligned(after))==(628,572,41,77)
listing=game.with_suffix(".asm").read_text(errors="replace")
body=listing.split("; Begin code for function: "+symbol,1)[1].split("; End code for function:",1)[0]
calls=re.findall(r"\sbl\s+(\S+)",body)
lock,=[x for x in calls if x.startswith("??$LockBuffersForIO@")]
unlock,=[x for x in calls if x.startswith("??$UnlockBuffersForIO@")]
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

out=folder/"R132_COMBINED_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out),proof["instantiations_checked"])
