"""Verify R171 root-bank diagnostics and prior raw bodies in the sealed link."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

WAVE = Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
OFFICIAL = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "95836632b166748b4b3ed0a1ed5d57be02122f29"
TREE = "48c7575e7cabf2318abe1bc6e2c6f60256f02794"
CANDIDATE = "a8176256cbe97bbf91b1abc59fd7314361e5cbca"
PREFLIGHT_SHA = "6322cf3fc2b0f6434348c16fa4d123d00262719ad2164bfb5cc0f567da346523"
PREVIOUS_RAW_SHA = "f18b3c440a6ab615f6222cf619759fc72ff0714fc440083afcef529e4903fac1"

def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def read(path):
    return json.loads(Path(path).read_bytes())

def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=OFFICIAL, text=True).strip()

def main():
    if not __debug__:
        raise RuntimeError("Verification requires assertions")
    parser = argparse.ArgumentParser()
    parser.add_argument("--root-pins-sha", required=True)
    parser.add_argument("--head", required=True)
    args = parser.parse_args()
    link = WAVE / "r171-link"
    pin_path = link / "ROOT_LINK_PINS.json"
    assert sha(pin_path) == args.root_pins_sha
    pins = read(pin_path)
    assert (pins["head"], pins["tree"], pins["parent"]) == (args.head, TREE, BASE)
    assert git("rev-parse", "HEAD", "HEAD^{tree}", "HEAD^").splitlines() == [args.head, TREE, BASE]
    assert not git("status", "--porcelain")
    for value in pins["pins"].values():
        assert sha(value["path"]) == value["sha256"], value["path"]
    completion = read(pins["pins"]["completion"]["path"])
    result = read(pins["pins"]["result"]["path"])
    diagnostic = read(pins["pins"]["diagnostics"]["path"])
    assert completion["status"] == "SEALED_NO_REGRESSIONS"
    assert (completion["head"], completion["passed"], completion["failed"]) == (args.head, 3710, 0)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert not diagnostic["unique_symbols"] and not diagnostic["duplicate_messages"]
    seal = read(pins["pins"]["seal"]["path"])
    invocation = read(pins["pins"]["invocation"]["path"])
    assert seal["head"] == args.head
    preflight = WAVE / "r171-batch/NATIVE_PPC_PREFLIGHT.json"
    assert sha(preflight) == PREFLIGHT_SHA
    proof = read(preflight)
    assert (proof["status"], proof["head"], proof["tree"], proof["base"]) == ("pass", CANDIDATE, TREE, BASE)
    helpers = runpy.run_path(str(WAVE / "r156-layout-linked/verify_layout_linked.py"))["m"]
    linked_address, linked_bytes = helpers["linked_reader"](link)
    mapping = Path(pins["pins"]["map"]["path"]).read_text(errors="replace")

    def selected_object(source, symbol):
        selected, = [row for row in seal["selected"] if row["source"] == source]
        invoked, = [row for row in invocation["objects"] if row["source"] == source]
        obj = Path(selected["object"])
        assert obj == Path(invoked["object"]) and sha(obj) == selected["sha256"] == invoked["sha256"]
        assert git("rev-parse", args.head + ":" + source) == git("rev-parse", CANDIDATE + ":" + source)
        pattern = rf"(?m)^\s*[0-9a-fA-F]{{4}}:[0-9a-fA-F]+\s+{re.escape(symbol)}\s+([0-9a-fA-F]{{8}})\s+f(?:\s+i)?\s+(\S+)"
        (address, owner), = re.findall(pattern, mapping)
        va = int(address, 16)
        assert va == linked_address(symbol) and obj.name == owner
        code, rels = helpers["collect"](obj, symbol)
        return obj, va, code, rels

    root = proof["root_bank"]
    obj, va, code, rels = selected_object(root["source"],root["symbol"])
    assert len(code)==root["body_bytes"] and hashlib.sha256(code).hexdigest()==root["body_sha256"]
    assert sha(root["original_export"]["path"])==root["original_export"]["sha256"]
    linked=linked_bytes(va,len(code))
    helpers["fixed_equal"](code,linked,rels)
    data=obj.read_bytes()
    names=[(at,index,kind,helpers["object_symbol"](data,index)[0]) for at,index,kind in rels if kind!=18]
    branches=[]
    for at,_,kind,symbol in names:
        if kind==6:
            word=int.from_bytes(linked[at:at+4],"big")
            target=helpers["branch_target"](va+at,word)
            assert target==linked_address(symbol), (hex(at),symbol)
            branches.append({"offset":hex(at),"symbol":symbol,"target":hex(target)})
    for call in [*root["assert_calls"],root["allocate"]]:
        assert any(item["offset"]==call["offset"] and item["symbol"]==call["symbol"] for item in branches)

    for offset,line in root["lines"].items():
        at=int(offset,16)
        assert int.from_bytes(linked[at:at+4],"big")==0x38a00000|line
    strings={}
    names_of_literals={name for _,_,kind,name in names if kind in (16,17) and name.startswith("??_C@")}
    for symbol in sorted(names_of_literals):
        high=[(at,index) for at,index,kind,name in names if kind==16 and name==symbol]
        low=[at for at,_,kind,name in names if kind==17 and name==symbol]
        assert len(high)==len(low)>0,(symbol,high,low)
        for (upper,index),lower in zip(sorted(high),sorted(low)):
            expected=helpers["symbol_literal"](data,index)
            address=helpers["data_target"](linked,upper,lower)
            assert linked_bytes(address,len(expected.encode())+1)==expected.encode()+b"\0"
            strings[expected]=hex(address)
    assert set(root["message_by_line"].values())|{root["source_path"],"ROOT"} <= set(strings)

    stage=proof["prepare"]
    stage_obj,stage_va,stage_code,stage_rels=selected_object(stage["source"],stage["symbol"])
    stage_image=linked_bytes(stage_va,len(stage_code))
    stage_edges=[]
    for call in stage["calls"]:
        at=int(call["offset"],16)
        (_,index,_), = [row for row in stage_rels if row[0]==at and row[2]==6]
        symbol,_,_=helpers["object_symbol"](stage_obj.read_bytes(),index)
        target=helpers["branch_target"](stage_va+at,int.from_bytes(stage_image[at:at+4],"big"))
        assert symbol==call["symbol"] and target==linked_address(symbol)
        stage_edges.append({"offset":hex(at),"symbol":symbol,"target":hex(target)})
    assert stage_edges[-1]["target"]==hex(va)

    previous = WAVE/"r168-raw-linked/R168_RAW_LINKED.json"
    assert sha(previous)==PREVIOUS_RAW_SHA
    carried=[]
    for row in read(previous)["raw_matches"]:
        owner,address,body,relocations=selected_object(row["source"],row["symbol"])
        size=row["bytes"]
        assert len(body)==size and not relocations
        assert body==helpers["original"](int(row["artist_address"],16),size)==linked_bytes(address,size)
        assert hashlib.sha256(body).hexdigest()==row["bytes_sha256"]
        carried.append({"name":row["name"],"address":hex(address),"bytes":size,"body_sha256":row["bytes_sha256"]})
    assert len(carried)==5
    receipt={
        "status":"pass","head":args.head,"tree":TREE,"parent":BASE,"root_pins_sha256":args.root_pins_sha,
        "pe_sha256":pins["pins"]["pe"]["sha256"],"xex_sha256":pins["pins"]["xex"]["sha256"],"preflight_sha256":PREFLIGHT_SHA,
        "root_bank":{"address":hex(va),"bytes":len(code),"selected_object_sha256":sha(obj),"lines":root["lines"],
                     "message_by_line":root["message_by_line"],"literal_addresses":strings,"branch_edges":branches},
        "prepare":{"address":hex(stage_va),"call_order":stage_edges},
        "raw_carry":carried,
        "scope":"Selected linked root-bank code, original diagnostics, allocation and Prepare edges, and five raw carries. Runtime unverified.",
    }
    dest=Path(__file__).resolve().parent/"R171_ROOT_BANK_LINKED.json"
    with dest.open("x",encoding="utf-8") as stream:
        json.dump(receipt,stream,indent=2)
        stream.write("\n")
    print("R171_ROOT_BANK_LINKED",sha(dest),len(root["lines"]),len(carried))

if __name__=="__main__":
    main()
