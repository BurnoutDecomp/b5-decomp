"""Verify R172 sound Prepare and native pool queue in the sealed link."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

WAVE = Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
OFFICIAL = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "b7b549c7f423d38c8ddc63ba089c11152e66d8fd"
TREE = "27d9d594a2eea753b0057137f4898cbc1d3c3900"
CANDIDATE = "34cbedc5316a1ee0592385ccb02c76cf06927365"
PREFLIGHT_SHA = "cd2a143692b0714554ed5428fa9f6570932e92eb2ea7f7bf7c35e9e9b10ade36"
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
    link = WAVE / "r172-link"
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
    preflight = WAVE / "r172-batch/NATIVE_PPC_PREFLIGHT.json"
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

    sound=proof["sound_prepare"]
    owner,va,code,rels=selected_object(sound["source"],sound["symbol"])
    artist=int(sound["original_address"],16)
    assert len(code)==sound["body_bytes"]==20 and not rels and code[-4:]==bytes.fromhex("4e800020")
    assert code==helpers["original"](artist,20)==linked_bytes(va,20)==bytes.fromhex(sound["original_bytes"])
    assert helpers["original"](artist+20,4)==bytes(4)
    assert hashlib.sha256(code).hexdigest()==sound["body_sha256"]
    original_table=int(sound["original_vtable"],16)
    assert int.from_bytes(helpers["original"](original_table+0xc,4),"big")==artist
    linked_table=linked_address("??_7StateManager@Logic@CgsSound@@6B@")
    assert int.from_bytes(linked_bytes(linked_table+8,4),"big")==va

    assert "CreatePoolBank" not in mapping
    pool=proof["native_pool"]
    pool_obj,pool_va,pool_code,pool_rels=selected_object(pool["source"],pool["symbol"])
    pool_image=linked_bytes(pool_va,len(pool_code))
    call=int(pool["call"]["offset"],16)
    (_,index,_), = [row for row in pool_rels if row[0]==call and row[2]==6]
    symbol,_,_=helpers["object_symbol"](pool_obj.read_bytes(),index)
    assert symbol==pool["call"]["symbol"]
    target=helpers["branch_target"](pool_va+call,int.from_bytes(pool_image[call:call+4],"big"))
    assert target==linked_address(symbol)
    event_args=bytes.fromhex("38c000ac38a00000")
    assert pool_image[call-16:call-8]==pool_code[call-16:call-8]==event_args
    assert pool["event_size"]==172 and not pool["helper_present"]

    previous=WAVE/"r168-raw-linked/R168_RAW_LINKED.json"
    assert sha(previous)==PREVIOUS_RAW_SHA
    carried=[]
    for row in read(previous)["raw_matches"]:
        carried_obj,address,body,relocations=selected_object(row["source"],row["symbol"])
        size=row["bytes"]
        assert len(body)==size and not relocations
        assert body==helpers["original"](int(row["artist_address"],16),size)==linked_bytes(address,size)
        assert hashlib.sha256(body).hexdigest()==row["bytes_sha256"]
        carried.append({"name":row["name"],"address":hex(address),"bytes":size,"body_sha256":row["bytes_sha256"]})
    assert len(carried)==5
    receipt={
        "status":"pass","head":args.head,"tree":TREE,"parent":BASE,"root_pins_sha256":args.root_pins_sha,
        "pe_sha256":pins["pins"]["pe"]["sha256"],"xex_sha256":pins["pins"]["xex"]["sha256"],"preflight_sha256":PREFLIGHT_SHA,
        "sound_prepare":{"address":hex(va),"original":hex(artist),"bytes":20,"body_sha256":sound["body_sha256"],
                         "selected_object_sha256":sha(owner),"current_vtable_slot":"0x8","original_vtable_slot":"0xc"},
        "native_pool":{"address":hex(pool_va),"add_event_site":hex(pool_va+call),"target":hex(target),
                       "event":0,"bytes":172,"selected_object_sha256":sha(pool_obj)},
        "raw_carry":carried,
        "scope":"Complete linked sound Prepare bytes and current slot, native queued pool request and five raw carries. Full sound vtable and runtime unverified.",
    }
    dest=Path(__file__).resolve().parent/"R172_SOUND_POOL_LINKED.json"
    with dest.open("x",encoding="utf-8") as stream:
        json.dump(receipt,stream,indent=2)
        stream.write("\n")
    print("R172_SOUND_POOL_LINKED",sha(dest),20,len(carried))

if __name__=="__main__":
    main()
