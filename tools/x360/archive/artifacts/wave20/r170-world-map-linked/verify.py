"""Bind R170 map arithmetic, X/Z projections and carried raw bodies to the linked image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

WAVE = Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
OFFICIAL = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "6bc4e0d6d9d5e6af3e664764e9f330fac90d1b93"
TREE = "294aed21c77b70a6e403903e07c0218098c0dd63"
CANDIDATE = "147ef0add704f1bf2f0c7b86de113372c579910d"
PREFLIGHT_SHA = "e59bb80d2ddaa4d1bc901db054bf3eeb7d5fb1fe52cabbccedf4c0a434546ab0"
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
    link = WAVE / "r170-link"
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
    preflight = WAVE / "r170-batch/NATIVE_PPC_PREFLIGHT.json"
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

    lookup = proof["world_lookup"]
    obj, map_va, code, rels = selected_object(lookup["source"],lookup["symbol"])
    assert not rels and len(code)==lookup["body_bytes"]==288
    assert sha(lookup["original_export"]["path"])==lookup["original_export"]["sha256"]
    assert hashlib.sha256(code).hexdigest()==lookup["body_sha256"]
    assert linked_bytes(map_va,len(code))==code
    assert lookup["original_bytes"]==300 and lookup["baseline_scalar_divides"]==2
    mask = proof["projection_mask"]
    original_mask = helpers["original"](int(mask["original_address"],16),16)
    assert original_mask.hex()==mask["bytes"]

    def branch(va,image,relocations,data,call):
        at=int(call["offset"],16)
        (_,index,_), = [row for row in relocations if row[0]==at and row[2]==6]
        symbol,_,_ = helpers["object_symbol"](data,index)
        assert symbol==call["symbol"]
        word=int.from_bytes(image[at:at+4],"big")
        target=helpers["branch_target"](va+at,word)
        assert word & 1 and target==linked_address(symbol)
        return {"address":hex(va+at),"target":hex(target),"symbol":symbol}

    callers={}
    for label in ("positioning","region"):
        section=proof[label]
        owner,va,body,relocations=selected_object(section["source"],section["symbol"])
        assert len(body)==section["body_bytes"]
        image=linked_bytes(va,len(body))
        data=owner.read_bytes()
        permutation=int(section["permutation_offset"],16)
        assert image[permutation:permutation+4]==body[permutation:permutation+4]==bytes.fromhex(section["permutation_word"])
        upper,lower=[int(offset,16) for offset in section["mask_offsets"]]
        for at in (upper,lower):
            (_,index,_), = [row for row in relocations if row[0]==at and row[2] in (16,17)]
            symbol,_,_=helpers["object_symbol"](data,index)
            assert symbol==section["mask_symbol"]
            assert image[at:at+2]==body[at:at+2]
        mask_va=helpers["data_target"](image,upper,lower)
        assert mask_va%16==0 and linked_bytes(mask_va,16)==original_mask
        edge=branch(va,image,relocations,data,section["call"])
        assert edge["target"]==hex(map_va)
        callers[label]={"symbol":section["symbol"],"address":hex(va),"mask_address":hex(mask_va),
                        "mask_bytes":mask["bytes"],"permutation_address":hex(va+permutation),"lookup":edge,
                        "object_sha256":sha(owner)}

    spawn=proof["spawn"]
    spawn_obj,spawn_va,spawn_code,spawn_relocs=selected_object(spawn["source"],spawn["symbol"])
    spawn_image=linked_bytes(spawn_va,len(spawn_code))
    spawn_edges=[branch(spawn_va,spawn_image,spawn_relocs,spawn_obj.read_bytes(),call) for call in spawn["calls"]]
    assert spawn_edges[1]["target"]==callers["positioning"]["address"]
    previous_spawn = WAVE/"r169-spawn-linked/R169_SPAWN_LINKED.json"
    assert sha(previous_spawn)=="bd895aedc33b57a7be7ecc376d2fc4a9f82a1c264195f96e54ab9980f7dfff46"
    args_words=read(previous_spawn)["witnesses"]["spawn"]["argument_words"]
    for offset,word in args_words.items():
        at=int(offset,16)
        assert spawn_image[at:at+4]==spawn_code[at:at+4]==bytes.fromhex(word),offset

    previous = WAVE/"r168-raw-linked/R168_RAW_LINKED.json"
    assert sha(previous)==PREVIOUS_RAW_SHA
    carried=[]
    for row in read(previous)["raw_matches"]:
        owner,va,body,relocations=selected_object(row["source"],row["symbol"])
        size=row["bytes"]
        assert len(body)==size and not relocations
        assert body==helpers["original"](int(row["artist_address"],16),size)==linked_bytes(va,size)
        assert hashlib.sha256(body).hexdigest()==row["bytes_sha256"]
        carried.append({"name":row["name"],"artist_address":row["artist_address"],"address":hex(va),
                        "bytes":size,"body_sha256":row["bytes_sha256"],"object_sha256":sha(owner)})
    assert len(carried)==5 and sum(row["bytes"] for row in carried)==96
    receipt = {
        "status":"pass","head":args.head,"tree":TREE,"parent":BASE,"root_pins_sha256":args.root_pins_sha,
        "pe_sha256":pins["pins"]["pe"]["sha256"],"xex_sha256":pins["pins"]["xex"]["sha256"],"preflight_sha256":PREFLIGHT_SHA,
        "world_lookup":{"address":hex(map_va),"bytes":len(code),"original_bytes":lookup["original_bytes"],
                        "body_sha256":lookup["body_sha256"],"object_sha256":sha(obj),"instruction_counts":lookup["instruction_counts"]},
        "callers":callers,"spawn":{"address":hex(spawn_va),"edges":spawn_edges,"argument_words":args_words},
        "raw_carry":carried,
        "scope":"Linked selected code and resolved projection masks/calls, plus five full raw carries. ARTIST register scheduling and full lookup bytes differ. Runtime unverified.",
    }
    dest=Path(__file__).resolve().parent/"R170_WORLD_MAP_LINKED.json"
    with dest.open("x",encoding="utf-8") as stream:
        json.dump(receipt,stream,indent=2)
        stream.write("\n")
    print("R170_WORLD_MAP_LINKED",sha(dest),len(callers),len(carried))

if __name__=="__main__":
    main()
