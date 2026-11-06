"""Verify three complete PPC functions and their string and branch relocations."""
import hashlib
import json
from pathlib import Path
import re
import runpy
import struct
import sys

here = Path(__file__).resolve().parent
wave = here.parent
append_check = runpy.run_path(str(wave / "r127-batch/verify_append_coff.py"))
coff = append_check["coff"]
original = append_check["read_original"]
stream = "src/GameShared/GameClasses/Development/CgsStrStream.cpp"
game = "src/GameSource/Game/BrnGameModule.cpp"
source_path = r"d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\module\CgsModuleUtils.h"
messages = {"lpInputBuffer", *(f"lpOutputBuffer{i}" for i in range(5))}
target_names = {
    0x82c08edc: "__savegprlr_25", 0x82c08f2c: "__restgprlr_25",
    0x82817548: "?BeginAssert@Assert@CgsDev@@YAXXZ", 0x82817558: "?EndAssert@Assert@CgsDev@@YAXXZ",
    0x82820810: "?FireAssert@Assert@CgsDev@@YAXPBD0H@Z",
    0x82204a18: "?LockForWrite@IOBuffer@CgsModule@@QAAXXZ",
    0x82204c88: "?LockForRead@IOBuffer@CgsModule@@QBAXXZ",
    0x82204d50: "?UnlockForRead@IOBuffer@CgsModule@@QBAXXZ",
    0x82204bb8: "?UnlockForWrite@IOBuffer@CgsModule@@QAAXXZ",
}
masks = {6: 0x03fffffc, 0x10: 0xffff, 0x11: 0xffff, 0x12: 0}

def sha(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()

def branch_target(pc, word):
    assert word >> 26 == 18 and not word & 2
    offset = word & 0x03fffffc
    return pc + offset - (0x04000000 if offset & 0x02000000 else 0)

def data_target(body, upper, lower):
    hi = struct.unpack_from(">I", body, upper)[0] & 0xffff
    lo = struct.unpack_from(">I", body, lower)[0] & 0xffff
    return (hi << 16) + lo - (0x10000 if lo & 0x8000 else 0)

def object_symbol(data, index):
    _, _, _, table, count, _, _ = struct.unpack_from("<HHIIIHH", data)
    assert index < count
    pos = table + index * 18
    raw_name = data[pos:pos+8]
    if raw_name.startswith(bytes(4)):
        start = table + count * 18 + struct.unpack_from("<I", raw_name, 4)[0]
        name = data[start:data.index(0, start)].decode()
    else:
        name = raw_name.rstrip(bytes(1)).decode()
    value, section = struct.unpack_from("<Ih", data, pos+8)
    return name, value, section

def symbol_literal(data, index):
    _, value, section = object_symbol(data, index)
    _, _, _, size, raw = struct.unpack_from("<8sIIII", data, 20+(section-1)*40)
    assert section > 0 and 0 <= value < size
    return data[raw+value:data.index(0,raw+value,raw+size)].decode()

def collect(obj, symbol):
    coff["symbol"] = symbol.encode()
    name, code, rels = coff["read_function"](obj)
    assert name == symbol
    return code, rels

def fixed_equal(left, right, rels):
    assert len(left) == len(right)
    by_offset = {}
    for offset, _, kind in rels:
        assert offset % 4 == 0
        by_offset[offset] = by_offset.get(offset, 0) | masks[kind]
    for i, (a, b) in enumerate(zip(struct.iter_unpack(">I",left), struct.iter_unpack(">I",right))):
        assert not (a[0] ^ b[0]) & ~by_offset.get(i*4,0), i*4

def linked_reader(folder):
    mapping = (folder/"native-game.map").read_text(errors="replace")
    pe = (folder/"native-game.pe").read_bytes()
    header = struct.unpack_from("<I",pe,0x3c)[0]
    assert pe[header:header+4] == b"PE\0\0"
    machine,count,_,_,_,optional,_=struct.unpack_from("<HHIIIHH",pe,header+4)
    base=struct.unpack_from("<I",pe,header+52)[0]
    assert machine==0x1f2 and base==0x82000000
    sections = [struct.unpack_from("<8sIIII",pe,header+24+optional+i*40)[2:] for i in range(count)]
    def address(name):
        value, = re.findall(rf"(?m)^\s+[0-9a-fA-F]{{4}}:[0-9a-fA-F]+\s+{re.escape(name)}\s+([0-9a-fA-F]{{8}})\s",mapping)
        return int(value,16)
    def read(va, size):
        rva=va-base
        for start,length,offset in sections:
            if start<=rva and rva+size<=start+length:
                return pe[offset+rva-start:offset+rva-start+size]
        raise ValueError(hex(va))
    return address,read

def verify(stream_obj, game_obj, folder=None):
    append = append_check["verify"](stream_obj)
    listing = game_obj.with_suffix(".asm").read_text(errors="replace")
    caller, = re.findall(r"(?m)^; Begin code for function: (\?DoUpdate_World@BrnGameModule@BrnGame@@\S+)",listing)
    body = listing.split("; Begin code for function: "+caller,1)[1].split("; End code for function:",1)[0]
    rows = {}
    addresses = {}
    if folder:
        addr, loaded = linked_reader(folder)
    for label, obj, artist_va, size, strings in (
        ("StrStreamBase::operator<<(const char*)", stream_obj, 0x821f01a8, 88, {"<NULLSTRING>"}),
        ("LockBuffersForIO", game_obj, 0x823b7620, 316, messages | {source_path}),
        ("UnlockBuffersForIO", game_obj, 0x823b7760, 316, messages | {source_path}),
    ):
        symbol = "??6StrStreamBase@CgsDev@@QAAAAU01@PBD@Z"
        if obj == game_obj:
            symbol, = re.findall(r"\sbl\s+(\?\?\$"+label+r"@\S+)",body)
        code, rels = collect(obj, symbol)
        artist = original(artist_va,size)
        fixed_equal(artist,code,rels)
        data=obj.read_bytes()
        patches={}
        branches=[]
        if folder:
            address=addr(symbol)
            actual=loaded(address,size)
            fixed_equal(code,actual,rels)
            addresses[label]=address
        for offset, index, kind in rels:
            if kind == 6:
                name,_,_=object_symbol(data,index)
                source_target=branch_target(artist_va+offset,struct.unpack_from(">I",artist,offset)[0])
                assert target_names[source_target]==name,(offset,name,hex(source_target))
                if folder:
                    assert branch_target(address+offset,struct.unpack_from(">I",actual,offset)[0])==addr(name)
                branches.append(name)
            elif kind in (0x10,0x11):
                assert kind not in patches.setdefault(index,{})
                patches[index][kind]=offset
        literals=[]
        for index, fields in patches.items():
            upper,lower=fields[0x10],fields[0x11]
            literal=symbol_literal(data,index)
            encoded=literal.encode()+bytes(1)
            assert original(data_target(artist,upper,lower),len(encoded))==encoded
            if folder:
                assert loaded(data_target(actual,upper,lower),len(encoded))==encoded
            literals.append(literal)
        assert set(literals)==strings
        if obj == game_obj:
            assert len(branches)==26
            assert [sum(x.startswith("?"+word+"Assert@") for x in branches) for word in ("Begin","Fire","End")]==[6,6,6]
        else:
            assert not branches
        rows[label]={"artist_va":hex(artist_va),"symbol":symbol,"bytes":size,"coff_sha256":sha(obj),
                     "matched_after_relocation_fields":True,"resolved_named_branches":branches,
                     "resolved_string_values":literals}
        if folder:
            rows[label]["linked_va"]=hex(address)
    if folder:
        caller_code,caller_rels=collect(game_obj,caller)
        caller_va=addr(caller)
        caller_link=loaded(caller_va,len(caller_code))
        fixed_equal(caller_code,caller_link,caller_rels)
        calls=[branch_target(caller_va+offset,struct.unpack_from(">I",caller_link,offset)[0])
               for offset,_,kind in caller_rels if kind==6]
        assert calls.count(addresses["LockBuffersForIO"])==calls.count(addresses["UnlockBuffersForIO"])==1
    return {"scope":"strict linked image" if folder else "targeted objects","methods":rows,"append_regressions":append}

if len(sys.argv)==2:
    link=Path(sys.argv[1]).resolve()
    invocation=json.loads((link/"invocation.json").read_text())
    inputs={r["source"]:Path(r["object"]) for r in invocation["objects"]}
    proof=verify(inputs[stream],inputs[game],link)
    xex=link/"native-game.exe"
    result=json.loads((link/"result.json").read_text())
    assert result["exit_code"]==0 and result["all_post_checks_pass"]
    proof["xex_sha256"]=sha(xex)
    assert proof["xex_sha256"]==result["outputs"][str(xex)]["sha256"]
    destination=link/"R128_MATCH.json"
else:
    assert len(sys.argv)==3
    proof=verify(Path(sys.argv[1]),Path(sys.argv[2]))
    destination=here/"R128_TARGETED.json"
with destination.open("x",encoding="utf-8") as f:
    json.dump(proof,f,indent=2)
    f.write("\n")
print(destination,sha(destination))
