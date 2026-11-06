"""R163 focused stage-0 PPC object witnesses; does not prove runtime or linked PE."""
import hashlib,json,runpy,struct
from pathlib import Path
out=Path(__file__).resolve().parent
m=runpy.run_path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r156-layout-linked/verify_layout_linked.py")["m"]
obj=out/"BrnRaceCarEntityModule-stage0.obj"
symbol="?Prepare@RaceCarEntityModule@BrnWorld@@QAA_NPAUOutputBuffer_Prepare@RaceCarEntityModuleIO@2@UResourceHandle@CgsResource@@@Z"
code,rels=m["collect"](obj,symbol);data=obj.read_bytes()
def word(off):return struct.unpack_from(">I",code,off)[0]
def named(off):
    rows=[(i,k) for o,i,k in rels if o==off and k!=18]; assert len(rows)==1
    return m["object_symbol"](data,rows[0][0])[0],rows[0][1]
def literal(off):
    i,= [i for o,i,k in rels if o==off and k==16];return m["symbol_literal"](data,i)
def branch(off,target):
    name,k=named(off);assert k==6 and name.startswith(target),(off,name);assert word(off)&0xfc000003==0x48000001
assert [word(o) for o in (0x114,0x11c,0x124,0x130,0x144,0x148,0x14c,0x158,0x17c,0x1bc,0x1c8,0x1d0,0x1d4)]==[0x83810120,0x2b1c0000,0x409a0020,0x38a0029f,0x817c0000,0x2b0b0000,0x409a0020,0x38a002a0,0xfaff023c,0x817f023c,0x816b0000,0x814b0004,0x7c8a5a14]
for off,target in [(0x128,"?BeginAssert@Assert@CgsDev@@"),(0x13c,"?FireAssert@Assert@CgsDev@@"),(0x140,"?EndAssert@Assert@CgsDev@@"),(0x150,"?BeginAssert@Assert@CgsDev@@"),(0x164,"?FireAssert@Assert@CgsDev@@"),(0x168,"?EndAssert@Assert@CgsDev@@"),(0x1d8,"?Construct@WorldMap2D@CgsWorld@@")]:branch(off,target)
assert literal(0x12c)=="lDistrictMapResourceHandle.GetResource() != NULL"
assert literal(0x154)=="lDistrictMapResourceHandle.GetResource()->GetMemoryResource() != NULL"
assert literal(0x118)==r"d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
for offset,name in [(0x16c,"__real@c5838000"),(0x190,"__real@c5706000"),(0x19c,"__real@46013800"),(0x1ac,"__real@45bea800")]:assert named(offset)[0]==name
assert word(0x1b4)==0x102050c3 and word(0x1cc)==0x104050c3
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
r={"state":"pass","scope":"focused selected PPC COFF only; no linked PE or runtime proof","source_base":"cfa383bfbddd126156185e122979242f5a680683","source_tree":"c9a45a7234193c374f89d1161d756928f0ba94b7","object_sha256":sha(obj),"assembly_sha256":sha(out/"BrnRaceCarEntityModule-stage0.asm"),"compile_report_sha256":sha(out/"compile-results.json"),"artist":"0x82303F28..0x82303FAC","witnesses":["first pointer and pointee assertion branches rejoin", "exact assertions, original source path and lines 671/672", "8-byte by-value handle store at +0x23c", "two dereferences, +4 offset load, base+offset and WorldMap2D::Construct", "vector arguments in v1/v2; existing scalar constants retained"],"limit":"Static XEX vector storage is zero; source constants are retained from existing reconstruction, not independently validated against runtime initializer."}
(out/"PROOF.json").write_text(json.dumps(r,indent=2)+"\n")
print(json.dumps(r,indent=2))

