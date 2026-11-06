"""Compare five complete R168 PPC functions with ARTIST in the sealed linked image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

WAVE = Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
OFFICIAL = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "76113b97467468cb447d283947fd782ee1260ac6"
TREE = "e7e3d90864f8c55b11d03e9e1a6d733cc829112b"
CANDIDATE = "a524878627dd1c9e205b49b8d47e0df9c9f3f179"
FOCUSED_SHA = "edebe3bba157b5888f6f216ab4c6c6e9447a1d49425423bb6c7b96c39e941ac0"

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
    link = WAVE / "r168-link"
    pin_path = link / "ROOT_LINK_PINS.json"
    assert sha(pin_path) == args.root_pins_sha
    pins = read(pin_path)
    assert (pins["head"], pins["tree"], pins["parent"]) == (args.head, TREE, BASE)
    assert git("rev-parse", "HEAD", "HEAD^{tree}", "HEAD^").splitlines() == [args.head, TREE, BASE]
    assert not git("status", "--porcelain")
    for value in pins["pins"].values():
        assert sha(value["path"]) == value["sha256"], value["path"]
    assert all(Path(pins["pins"][kind]["path"]).parent == link for kind in ("map", "pe", "xex"))
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
    preflight = WAVE / "r168-batch/RAW_MATCH_PREFLIGHT.json"
    assert sha(preflight) == FOCUSED_SHA
    proof = read(preflight)
    assert (proof["state"], proof["head"], proof["tree"], proof["base"]) == ("pass", CANDIDATE, TREE, BASE)
    assert len(proof["anchors"]) == 5
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

    anchors = []
    for row in proof["anchors"]:
        assert sha(row["artist_export"]["path"]) == row["artist_export"]["sha256"]
        obj, va, code, rels = selected_object(row["source"], row["symbol"])
        original = helpers["original"](int(row["artist_address"], 16), row["bytes"])
        linked = linked_bytes(va, len(code))
        assert len(code) == row["bytes"] and not rels
        assert linked == code == original and original.hex() != row["r167_linked_bytes"]
        assert hashlib.sha256(linked).hexdigest() == row["bytes_sha256"]
        anchors.append({**{k:row[k] for k in ("name","symbol","source","artist_address","bytes","bytes_sha256",
                                               "r167_linked_address","artist_post_return_alignment_bytes")},
                        "linked_address":hex(va),"selected_object":str(obj),"selected_object_sha256":sha(obj),
                        "coff_relocations":0,"raw_exact":True})

    bridge = proof["countdown_consumer"]
    obj, va, code, rels = selected_object(bridge["source"], bridge["symbol"])
    linked = linked_bytes(va, len(code))
    for offset, word in bridge["words"].items():
        at = int(offset, 16)
        assert linked[at:at+4] == code[at:at+4] == bytes.fromhex(word), offset
    branch = int(bridge["event_branch_offset"], 16)
    (_, index, _), = [row for row in rels if row[0] == branch and row[2] == 6]
    symbol, _, _ = helpers["object_symbol"](obj.read_bytes(), index)
    assert symbol == bridge["event_target"]
    branch_to = helpers["branch_target"](va + branch, int.from_bytes(linked[branch:branch+4], "big"))
    assert branch_to == linked_address(symbol)
    receipt = {
        "status":"pass", "head":args.head, "tree":TREE, "parent":BASE, "root_pins_sha256":args.root_pins_sha,
        "pe_sha256":pins["pins"]["pe"]["sha256"], "xex_sha256":pins["pins"]["xex"]["sha256"], "focused_sha256":FOCUSED_SHA,
        "raw_matches":anchors,
        "countdown_consumer":{**{k:bridge[k] for k in ("layout","mode_stop_action","action_size","controller_state_offset",
                                                       "controller_state_value","known_payload_difference","words")},
                              "linked_address":hex(va), "event_target":symbol, "event_address":hex(branch_to)},
        "scope":"Full raw linked instruction bodies for five named functions and the specified countdown consumer instructions. Runtime unverified.",
    }
    dest = Path(__file__).resolve().parent / "R168_RAW_LINKED.json"
    with dest.open("x", encoding="utf-8") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print("R168_RAW_LINKED", sha(dest), len(anchors), sum(row["bytes"] for row in anchors))

if __name__ == "__main__":
    main()
