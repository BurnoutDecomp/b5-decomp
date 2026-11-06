"""Check the original one- through four-source I/O helper code and relocations."""
import collections
import json
from pathlib import Path
import re
import struct

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
observed=collections.defaultdict(list)
objects=sorted(folder.glob("*.obj"))+sorted((folder/"remaining").glob("*.obj"))
assert len(objects)==14
for obj in objects:
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
shared_symbols={rows[0]["symbol"] for rows in observed.values()}
assert len(shared_symbols)==16
assert all(len({row["symbol"] for row in rows})==1 for rows in observed.values())
proof={"scope":"targeted native COFF only","objects_checked":len(objects),"shared_symbol_count":len(shared_symbols),
       "emitted_bodies_checked":sum(map(len,observed.values())) ,
       "families":[{"artist_va":hex(ea),"bytes":size,"source_buffers":sources,"operation":verb,
                    "first_assert_line":first,"instances":observed[first]}
                   for first,(ea,size,sources,verb) in functions.items()]}
out=folder/"R133_SHARED_LOCKS_COFF.json"
with out.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(out,ns["sha"](out),proof["emitted_bodies_checked"])
