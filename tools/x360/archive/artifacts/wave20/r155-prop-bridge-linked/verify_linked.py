
"""Verify the linked R155 World-to-Prop bridge against original ARTIST PPC."""
import argparse
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

here = Path(__file__).resolve().parent
wave = here.parent
workflow = Path("D:/Reverse/BP-Decomp_Workflow")
checkout = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
bridge_source = "src/GameSource/World/Bridges/WorldBridgeWorldModuleToPropModule.cpp"
world_source = "src/GameSource/World/BrnWorldModule.cpp"
bridge_prefix = "?BridgeWorldModuleToPropModule_PreScene@WorldModule@@"
world_prefix = "?Update@WorldModule@BrnWorld@@"
pre_scene_prefix = "?EntityModulePreSceneUpdate@WorldModule@BrnWorld@@"
artist_bridge = 0x827AACF8
prior_head = "13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d"
tree = "6bdcd7954fa913359b809b7506984f0dc05d90ee"

def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))

def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(checkout), *args],
                                   text=True).strip()

def pin(path):
    return {"path":str(path),"sha256":sha(path)}

def parser():
    helper = wave / "r120-debug2d-match/compare.py"
    functions = [node for node in ast.parse(helper.read_text(encoding="utf-8")).body
                 if isinstance(node, ast.FunctionDef) and node.name == "read_function"]
    assert len(functions) == 1
    ctx = {"struct": struct}
    exec(compile(ast.Module(body=functions,type_ignores=[]), str(helper), "exec"), ctx)
    return ctx

def read_coff(path,prefix,ctx):
    ctx["symbol"] = prefix.encode()
    return ctx["read_function"](Path(path))

def all_words(data):
    return tuple(word for (word,) in struct.iter_unpack(">I",data))

def resolve_branch(word, address):
    assert word >> 26 == 18 and not (word & 2)
    relative = word & 0x03FFFFFC
    if relative & 0x02000000:
        relative -= 0x04000000
    return address + relative

def file_va_reader(data):
    header = struct.unpack_from("<I",data,0x3C)[0]
    assert data[header:header+4] == b"PE\0\0"
    machine, count, _, _, _, optional, _ = struct.unpack_from("<HHIIIHH",data,header+4)
    base = struct.unpack_from("<I",data,header+52)[0]
    assert (machine,base) == (0x1F2,0x82000000)
    sections = [struct.unpack_from("<8sIIII",data,header+24+optional+40*i) for i in range(count)]
    def readva(address,size):
        rva = address-base
        for _,_,start,length,offset in sections:
            if start <= rva and rva+size <= start+length:
                offset += rva-start
                return data[offset:offset+size]
        raise ValueError(hex(address))
    return readva

def map_address(text, symbol):
    hit = re.findall(r"(?m)^\s+0003:[0-9a-fA-F]+\s+"+re.escape(symbol)+r"\s+([0-9a-fA-F]{8})\s",text)
    assert len(hit) == 1, (symbol,hit)
    return int(hit[0],16)

def selected(link):
    invocation = read(link/"invocation.json")
    seal = read(link/"seal.json")
    rows = {row["source"]:row for row in invocation["objects"]}
    signed = {row["source"]:row for row in seal["selected"]}
    assert invocation["sealed_sha256"]==sha(link/"seal.json") and rows.keys()==signed.keys()
    for path in (bridge_source,world_source):
        assert rows[path]["object"]==signed[path]["object"]
        assert rows[path]["sha256"]==signed[path]["sha256"]==sha(rows[path]["object"])
    return rows

def main():
    p=argparse.ArgumentParser()
    for arg in ("head","seal","xex","pe","map"):
        p.add_argument("--"+arg,required=True)
    pins=p.parse_args()
    link=wave/"r155-link"
    for path,digest in (("seal.json",pins.seal),("native-game.exe",pins.xex),
                        ("native-game.pe",pins.pe),("native-game.map",pins.map)):
        assert sha(link/path)==digest,path
    seal=read(link/"seal.json")
    result=read(link/"result.json")
    assert seal["head"]==pins.head==git("rev-parse","HEAD") and git("rev-parse","HEAD^{tree}")==tree
    assert git("rev-parse","HEAD^")==prior_head and not git("status","--porcelain=v1")
    assert result["exit_code"]==0 and result["all_post_checks_pass"]
    assert git("rev-parse",pins.head+":"+bridge_source)=="98b9c83875239ee8ee5588200777bd3ee60ae369"
    assert git("rev-parse",pins.head+":"+world_source)==git("rev-parse",prior_head+":"+world_source)
    rows=selected(link)
    prior=wave/"r154-link"
    assert sha(prior/"seal.json")=="024558f410971e92ae448b1482cdf1fc192e8da4a165ec14ff46fda2a797ed75"
    assert read(prior/"seal.json")["head"]==prior_head
    assert sha(prior/"native-game.pe")=="8895808adbacbd4c6847d3c4f5322e5ce6421250ba8f02ddbb75942208669c45"
    old_rows=selected(prior)

    s=importlib.util.spec_from_file_location("coff_refs",wave/"r143-cih-proof/verify.py")
    c=importlib.util.module_from_spec(s)
    s.loader.exec_module(c)
    ctx=parser()
    bridge_obj=rows[bridge_source]["object"]
    world_obj=rows[world_source]["object"]
    bridge_symbol, raw, relocs=read_coff(bridge_obj,bridge_prefix,ctx)
    old_name, old_raw, _=read_coff(old_rows[bridge_source]["object"],bridge_prefix,ctx)
    assert bridge_symbol==old_name and len(old_raw)==432 and len(raw)==128
    bridge_words, bridge_refs=c.coff(Path(bridge_obj),bridge_prefix)
    offsets=[0x04,0x18,0x24,0x2C,0x34,0x40,0x48,0x50,0x5C,0x64,0x7C]
    assert [off for off,_,_ in relocs]==offsets and all(kind==6 for _,_,kind in relocs)
    assert bridge_words==all_words(raw) and set(bridge_refs)=={off//4 for off in offsets}
    world_symbol,world_raw,_=read_coff(world_obj,world_prefix,ctx)
    assert c.coff(Path(world_obj),world_prefix)==c.coff(Path(old_rows[world_source]["object"]),world_prefix)
    world_words, world_refs=c.coff(Path(world_obj),world_prefix)
    assert all_words(world_raw)==world_words
    pre_scene_symbol,pre_scene_raw,_=read_coff(world_obj,pre_scene_prefix,ctx)
    pre_words,pre_refs=c.coff(Path(world_obj),pre_scene_prefix)
    assert all_words(pre_scene_raw)==pre_words
    assert (pre_words,pre_refs)==c.coff(Path(old_rows[world_source]["object"]),pre_scene_prefix)

    sys.path.insert(0,str(workflow/"tools/re"))
    import x360rd
    original=x360rd.rd(artist_bridge,128)
    export=workflow/".ida-exports/BURNOUT_X360_ARTIST.XEX/0x827AACF8.json"
    addresses=[int(match[1],16) for line in read(export)["assembly"].splitlines()
               if (match:=re.match(r"^0x([0-9A-Fa-f]+)\s+\w",line))]
    assert addresses==list(range(artist_bridge,artist_bridge+128,4))
    assert all_words(original)[-6:-2]==(0x3D60000C,0x616B8604,0x7D7E582E,0x917F0788)

    maptext=(link/"native-game.map").read_text(errors="replace")
    readva=file_va_reader((link/"native-game.pe").read_bytes())
    address=map_address(maptext,bridge_symbol)
    world_address=map_address(maptext,world_symbol)
    pre_scene_address=map_address(maptext,pre_scene_symbol)
    linked=all_words(readva(address,128))
    artist_words=all_words(original)
    for i,(ow, cw, lw) in enumerate(zip(artist_words,bridge_words,linked)):
        mask=0x03FFFFFC if i*4 in offsets else 0
        assert ((ow^lw)&~mask)==0 and ((cw^lw)&~mask)==0,hex(i*4)

    artist_edges=[resolve_branch(artist_words[off//4],artist_bridge+off) for off in offsets]
    assert artist_edges[1:-1]==[
        0x827A2B30,0x827A14D0,0x827A8810,
        0x827A2A88,0x827A1380,0x827A88F0,
        0x827A2BD8,0x827A1428,0x827A89D0]
    edges=[]
    for off,original_target in zip(offsets,artist_edges):
        target=bridge_refs[off//4]
        actual=resolve_branch(linked[off//4],address+off)
        assert actual==map_address(maptext,target),(off,target,hex(actual))
        edges.append({"offset":hex(off),"artist":hex(original_target),"linked":hex(actual),"symbol":target})

    world_linked=all_words(readva(world_address,len(world_raw)))
    pre_linked=all_words(readva(pre_scene_address,len(pre_scene_raw)))
    caller={}
    for off,prefix in [(0x260,"?LockBuffersForIO@CgsModule@@"),(0x270,bridge_prefix),
                       (0x27C,"?UnlockBuffersForIO@CgsModule@@")]:
        target=pre_refs[off//4]
        assert target.startswith(prefix),(off,target)
        dest=resolve_branch(pre_linked[off//4],pre_scene_address+off)
        assert dest==map_address(maptext,target),(off,target)
        caller[hex(off)]={"target":hex(dest),"symbol":target}
    for off in (0x258,0x25C,0x264,0x268,0x26C,0x274,0x278):
        assert pre_linked[off//4]==pre_words[off//4]
    for off,prefix in [(0x3B0,"?LockForWrite"),(0x3B8,"?GetPlayerVehicleControls@UpdateInputBuffer"),
                       (0x3C4,"?SetPlayerVehicleControls@UpdateOutputBuffer"),(0x3CC,"?UnlockForWrite")]:
        target=world_refs[off//4]
        assert target.startswith(prefix),(off,target)
        assert resolve_branch(world_linked[off//4],world_address+off)==map_address(maptext,target)

    receipt={"verdict":"pass","head":pins.head,"tree":tree,"pins":vars(pins),"artist_export":pin(export),
             "prior_linked_coff_body_bytes":len(old_raw),"linked_address":hex(address),
             "matched_bytes_after_branch_displacements":128,"branch_displacements":edges,
             "raw_exact":artist_words==linked,"selected_object":pin(bridge_obj),
             "world_caller":{"address":hex(world_address),"selected_object":pin(world_obj),
                             "body_words_and_reference_names_match_r154":True,
                             "prop_pre_scene_address":hex(pre_scene_address),"prop_locks_and_call":caller,
                             "world_controls_lock_get_copy_unlock_retained":True},
             "scope":"Static R155 linked proof. No game launch."}
    with (here/"R155_PROP_BRIDGE_LINKED.json").open("x",encoding="utf-8",newline="\n") as stream:
        json.dump(receipt,stream,indent=2)
        stream.write("\n")
    print("R155 World-to-Prop",hex(address),"128 bytes after 11 branch displacements")

if __name__ == "__main__":
    main()

