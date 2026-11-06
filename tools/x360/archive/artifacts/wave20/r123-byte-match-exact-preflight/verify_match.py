"""Verify the changed functions in the exact R123 PPC objects and linked PE."""
import difflib
import hashlib
import json
from pathlib import Path
import re
import struct


here = Path(__file__).resolve().parent
wave = here.parent
r122 = wave / "r122-byte-match-exact-preflight/verify_match.py"
preamble = r122.read_text().split("\ntext = subprocess.check_output",1)[0]
assert preamble.count('out = wave / "r122-byte-match-exact-preflight"') == 1
preamble = preamble.replace('out = wave / "r122-byte-match-exact-preflight"', 'out = wave / "r123-byte-match-exact-preflight"')
image = {"__file__": str(r122)}
exec(compile(preamble,str(r122),"exec"),image)
mapping, read_linked = image["mapping"], image["read_linked"]
x360rd = image["check"]["x360rd"]


def mapped(symbol):
    pattern = rf"(?m)^\s+0003:[0-9a-fA-F]+\s+{re.escape(symbol)}\S+\s+([0-9A-Fa-f]{{8}})\s"
    address, = re.findall(pattern,mapping)
    return int(address,16)


script = wave / "r120-debug2d-match/compare.py"
coff = {"__file__":str(script)}
exec(script.read_text().split("\nrows = []",1)[0],coff)
debug = {}
for name,original_address,size in (("Do2DBoxesIntersect",0x82816e50,108),("Is2DPointOnScreen",0x82818838,188)):
    key = f"?{name}@Debug2DImmediateRender@CgsDev@@"
    coff["symbol"] = key.encode()
    _,compiled,relocs = coff["read_function"](here/"CgsDebug2DImmediateRender-preflight.obj")
    original = x360rd.rd(original_address,size)
    linked_address = mapped(key)
    assert not relocs and compiled == read_linked(linked_address,len(compiled))
    matcher = difflib.SequenceMatcher(None,coff["words"](original),coff["words"](compiled),autojunk=False)
    debug[name] = {"artist_address":hex(original_address),"linked_address":hex(linked_address),
                   "bytes":len(compiled),"raw_exact":original==compiled,"relocations":len(relocs),
                   "aligned_words":sum(x.size for x in matcher.get_matching_blocks())}
assert debug["Do2DBoxesIntersect"]["raw_exact"] and debug["Do2DBoxesIntersect"]["bytes"]==108
assert debug["Is2DPointOnScreen"]["aligned_words"]==40

io = {}
script = wave / "iobuffer-byte-match-terra/compare.py"
exec(script.read_text().split("\nres={}",1)[0],io)
functions = io["extract"](here / "CgsIOBuffer-preflight.obj")
masks = {0x6:0x03fffffc,0x7:0xfffc,0x10:0xffff,0x11:0xffff,0x12:0}
for name in ("UnlockForWrite","LockForRead","UnlockForRead"):
    original=bytes.fromhex(io["orig"][io["fs"][name]]["bytes"])
    raw,relocs,_=functions[name]
    fields={}
    for offset,_,kind in relocs:
        fields[offset//4]=fields.get(offset//4,0)|masks[kind]
    assert len(raw)==len(original)
    assert all(not (old[0]^new[0]) & ~fields.get(i,0) for i,(old,new) in enumerate(
        zip(struct.iter_unpack(">I",original),struct.iter_unpack(">I",raw))))
lock,_,_=functions["LockForWrite"]
assert len(lock)==340
lock_listing=(here/"CgsIOBuffer-preflight.asm").read_text(errors="replace")
for offset in ("00084","00100"):
    assert re.search(rf"(?m)^\s+{offset}\s+48000001\s+bl\s+\?\?6StrStreamBase@CgsDev@@QAAAAU01@PBD@Z",lock_listing)
for offset in (0x94,0x110):
    assert struct.unpack_from(">I",lock,offset)[0] >> 26 == 34
    assert struct.unpack_from(">I",lock,offset+8)[0] == 0x7d640774

coff["symbol"]=b"?RenderBlackOverlay@LoadingScreenRenderer@BrnGame@@"
_,overlay,_=coff["read_function"](here/"BrnLoadingScreenRenderer-preflight.obj")
assert len(overlay)==436
listing=(here/"BrnLoadingScreenRenderer-preflight.asm").read_text(errors="replace").splitlines()
start=next(i for i,line in enumerate(listing) if line.startswith("?RenderBlackOverlay@LoadingScreenRenderer@BrnGame@@") and "PROC NEAR" in line)
end=next(i for i in range(start+1,len(listing)) if listing[i].startswith("?RenderBlackOverlay@LoadingScreenRenderer@BrnGame@@") and "ENDP" in listing[i])
block="\n".join(listing[start:end])
assert not re.search(r"\bbl\s+\?EmitQuad",block) and re.search(r"\bbl\s+\?Render@\?\$ImRenderer",block)
assert [x360rd.u32(p) for p in (0x82f24080,0x82f24084,0x82f240a0,0x82f240a4)]==[0x44a00000,0x44340000,0x45000000,0x44800000]
result=json.loads((here/"result.json").read_text())
xex=here/"native-game.exe"
digest=hashlib.sha256(xex.read_bytes()).hexdigest()
assert result["outputs"][str(xex)]["sha256"]==digest
report={"commit":"afbc2910cff536d3f3d48272cecfcb948f9c20d7","private_xex_sha256":digest,
        "debug":debug,"add_command_linked_address_normalized_exact":True,
        "io":{"preserved_address_normalized_functions":3,"lock_bytes":len(lock),
              "diagnostic_append_offsets":[0x84,0x100],"subsequent_signed_flag_read_offsets":[0x94,0x110]},
        "overlay":{"bytes":len(overlay),"direct_native_quad_call":True,"startup_U":0.625,"startup_V":0.703125}}
with (here/"R123_MATCH.json").open("x",encoding="utf-8") as f:
    json.dump(report,f,indent=2)
    f.write("\n")
print(json.dumps(report))
