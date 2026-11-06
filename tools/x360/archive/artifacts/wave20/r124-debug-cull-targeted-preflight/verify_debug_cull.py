"""Verify R124 native culling functions in the PPC object and linked image."""
import difflib
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


here = Path(__file__).resolve().parent
wave = here.parent
link = Path(sys.argv[1]) if len(sys.argv) > 1 else here
obj = Path(sys.argv[2]) if len(sys.argv) > 2 else here / "CgsDebug2DImmediateRender-preflight.obj"
script = wave / "r120-debug2d-match/compare.py"
coff = {"__file__":str(script)}
exec(script.read_text().split("\nrows = []",1)[0],coff)
x360rd = coff["x360rd"]
exports = Path("D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX")
map_text = (link/"native-game.map").read_text(errors="replace")
data = (link/"native-game.pe").read_bytes()
pe = struct.unpack_from("<I",data,0x3c)[0]
assert data[pe:pe+4] == b"PE\0\0"
machine,n_sections,_,_,_,optional,_ = struct.unpack_from("<HHIIIHH",data,pe+4)
base = struct.unpack_from("<I",data,pe+24+28)[0]
assert machine == 0x1f2 and base == 0x82000000


def read_linked(address, size):
    rva=address-base
    for i in range(n_sections):
        _,_,start,raw_size,offset=struct.unpack_from("<8sIIII",data,pe+24+optional+40*i)
        if start<=rva and rva+size<=start+raw_size:
            file_offset=offset+rva-start
            return data[file_offset:file_offset+size]
    raise ValueError(hex(address))


def branch_targets(address, body):
    targets=[]
    for i,(word,) in enumerate(struct.iter_unpack(">I",body)):
        if word>>26 != 18 or word & 2:
            continue
        delta=word & 0x03fffffc
        if delta & 0x02000000:
            delta-=0x04000000
        targets.append(address+4*i+delta)
    return targets


methods={
    "Do2DBoxesIntersect":0x82816e50,"Is2DPointOnScreen":0x82818838,
    "Is2DPolygonOnScreen":0x828188f8,"Is2DLineOnScreen":0x8281a318,"Do2DLinesIntersect":0x82818988,
}
relocation_masks={0x6:0x03fffffc,0x7:0xfffc,0x10:0xffff,0x11:0xffff,0x12:0}
reports={}
targets={}
for name,ea in methods.items():
    listing=json.loads((exports/f"0x{ea:08X}.json").read_text())["assembly"]
    addresses=[int(m[1],16) for line in listing.splitlines() if (m:=re.match(r"^0x([0-9A-Fa-f]+)\s+(?!\.)\w",line))]
    assert addresses == list(range(ea,ea+4*len(addresses),4))
    original=x360rd.rd(ea,4*len(addresses))
    key=f"?{name}@Debug2DImmediateRender@CgsDev@@"
    coff["symbol"]=key.encode()
    _,compiled,relocs=coff["read_function"](obj)
    linked_address,=map(lambda value:int(value,16),re.findall(
        rf"(?m)^\s+0003:[0-9A-Fa-f]+\s+{re.escape(key)}\S+\s+([0-9A-Fa-f]{{8}})\s",map_text))
    linked=read_linked(linked_address,len(compiled))
    masks={}
    for offset,_,kind in relocs:
        masks[offset//4]=masks.get(offset//4,0)|relocation_masks[kind]
    for i,(a,b) in enumerate(zip(struct.iter_unpack(">I",compiled),struct.iter_unpack(">I",linked))):
        assert not (a[0]^b[0]) & ~masks.get(i,0)
    aligned=sum(m.size for m in difflib.SequenceMatcher(None,coff["words"](original),
                  coff["words"](compiled),autojunk=False).get_matching_blocks())
    reports[name]={"artist_address":hex(ea),"linked_address":hex(linked_address),
        "artist_bytes":len(original),"compiled_bytes":len(compiled),"relocations":len(relocs),
        "linked_raw_exact":original==linked,"aligned_coff_words":aligned}
    targets[name]={"artist":branch_targets(ea,original),"linked":branch_targets(linked_address,linked)}

assert reports["Do2DBoxesIntersect"]["linked_raw_exact"] and reports["Do2DBoxesIntersect"]["relocations"]==0
assert reports["Is2DPointOnScreen"]["aligned_coff_words"]==40
assert reports["Is2DPolygonOnScreen"]["aligned_coff_words"]==30

for layout in ("artist","linked"):
    addresses = methods if layout=="artist" else {key:int(row["linked_address"],16) for key,row in reports.items()}
    assert targets["Is2DPolygonOnScreen"][layout].count(addresses["Do2DBoxesIntersect"])==1
    assert targets["Is2DLineOnScreen"][layout].count(addresses["Do2DLinesIntersect"])==4
    assert targets["Is2DLineOnScreen"][layout].count(addresses["Is2DPointOnScreen"])==2

result=json.loads((link/"result.json").read_text())
xex=link/"native-game.exe"
digest=hashlib.sha256(xex.read_bytes()).hexdigest()
assert result["all_post_checks_pass"] and result["outputs"][str(xex)]["sha256"]==digest
report={"xex_sha256":digest,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),
        "scope":"targeted object replacement" if link==here else "official full header closure",
        "methods":reports,"polygon_calls_box":True,"line_endpoint_calls":2,"line_edge_calls":4}
report_name="R124_MATCH.json" if link==here else "PUBLISHED_MATCH.json"
with (link/report_name).open("x",encoding="utf-8") as output:
    json.dump(report,output,indent=2)
    output.write("\n")
print(json.dumps(report))
