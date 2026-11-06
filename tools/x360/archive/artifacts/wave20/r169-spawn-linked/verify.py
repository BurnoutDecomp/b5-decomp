"""Bind the native race-car spawn handoff to the reviewed R169 linked image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

WAVE = Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
OFFICIAL = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "6bf19bb5d1c842569c8cda83229bf84ead257827"
TREE = "e58f26ae0bcc68f66bb71b197b8b72ab7b70e4ec"
CANDIDATE = "1c2dbb7325823abf3942de7d0dbe5cb29fbc0f75"
PREFLIGHT_SHA = "001f790414f129f91f3783129597fa72ff2a81ce60315c5a92f96d6f4a3a9f49"
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
    link = WAVE / "r169-link"
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
    preflight = WAVE / "r169-batch/SPAWN_PPC_PREFLIGHT.json"
    assert sha(preflight) == PREFLIGHT_SHA
    proof = read(preflight)
    assert (proof["status"], proof["head"], proof["tree"], proof["base"]) == ("pass", CANDIDATE, TREE, BASE)
    assert proof["map_offset"] == "0x18300"
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

    witnesses = {}
    for label in ("reset", "spawn", "positioning", "xz_projection", "prepare"):
        section = proof[label]
        obj, va, code, rels = selected_object(section["source"], section["symbol"])
        assert len(code) == section["body_bytes"]
        image = linked_bytes(va, len(code))
        for offset, word in section.get("arg_words", {}).items():
            at = int(offset, 16)
            assert image[at:at+4] == code[at:at+4] == bytes.fromhex(word), (label, offset)
        edges = []
        for call in section.get("calls", []):
            at = int(call["offset"], 16)
            (_, index, _), = [row for row in rels if row[0] == at and row[2] == 6]
            target, _, _ = helpers["object_symbol"](obj.read_bytes(), index)
            assert target == call["symbol"]
            word = int.from_bytes(image[at:at+4], "big")
            target_va = helpers["branch_target"](va + at, word)
            assert word & 1 and target_va == linked_address(target), (label, call)
            edges.append({"branch_address":hex(va+at), "symbol":target, "target_address":hex(target_va)})
        witnesses[label] = {"source":section["source"], "symbol":section["symbol"], "linked_address":hex(va),
                            "object_sha256":sha(obj), "calls":edges, "argument_words":section.get("arg_words", {})}
    assert witnesses["reset"]["calls"][0]["target_address"] == witnesses["spawn"]["linked_address"]
    assert witnesses["spawn"]["calls"][1]["target_address"] == witnesses["positioning"]["linked_address"]
    assert witnesses["positioning"]["calls"][0]["target_address"] == witnesses["xz_projection"]["linked_address"]

    previous = WAVE / "r168-raw-linked/R168_RAW_LINKED.json"
    assert sha(previous) == PREVIOUS_RAW_SHA
    countdown, = [row for row in read(previous)["raw_matches"] if "CountdownState::OnLeave" in row["name"]]
    obj, va, code, rels = selected_object(countdown["source"], countdown["symbol"])
    assert len(code) == countdown["bytes"] == 16 and not rels
    original = helpers["original"](int(countdown["artist_address"],16), 16)
    assert code == original == linked_bytes(va, 16)
    assert hashlib.sha256(code).hexdigest() == countdown["bytes_sha256"]
    receipt = {
        "status":"pass", "head":args.head, "tree":TREE, "parent":BASE, "root_pins_sha256":args.root_pins_sha,
        "pe_sha256":pins["pins"]["pe"]["sha256"], "xex_sha256":pins["pins"]["xex"]["sha256"], "preflight_sha256":PREFLIGHT_SHA,
        "map_offset":"0x18300", "witnesses":witnesses,
        "countdown_raw_carry":{"bytes":16,"original_address":countdown["artist_address"],"linked_address":hex(va),
                               "body_sha256":countdown["bytes_sha256"],"object_sha256":sha(obj)},
        "scope":"Linked spawn, reset, positioning and map call/argument witnesses and raw countdown carry. Other spawn instructions and runtime remain unverified.",
    }
    dest = Path(__file__).resolve().parent / "R169_SPAWN_LINKED.json"
    with dest.open("x", encoding="utf-8") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print("R169_SPAWN_LINKED", sha(dest), "call edges", sum(len(row["calls"]) for row in witnesses.values()))

if __name__ == "__main__":
    main()
