"""Bind the ARTIST-proved CarScoreData assignment to the official R166 image."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import subprocess
import sys

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
GIT = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
CANDIDATE = "9a99e86aedecb6b7933fe8f56fa74feda3986031"
TREE = "77910cc622803ecd685ea93afca520619840d51c"
PARENT = "451d95889337e064441dec06a1726179187e48cf"
SOURCE = "src/GameSource/GameState/BrnGameStateSharedIO.cpp"
BLOB = "57325b044b1b54b70eb7e8d1aa2d5e1ccb7d348e"
FOCUSED = WAVE / "r166-carscore-assignment"
FOCUSED_PROOF_SHA = "5230f0a6260998a6d1e892443f91cb7e3c02a2a6fdfd5d5fa361fde4688e97b4"
FOCUSED_VERIFIER_SHA = "2ff6b0a952c8b13b684f9d6d1cdfbd68e563d547b97f394ac2340c1eeeb34cba"
PE_HELPER = WAVE / "r154-racecar-prescene-linked/verify_linked.py"
PE_HELPER_SHA = "d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked(path, sha):
    data = Path(path).read_bytes()
    assert digest(data) == sha, path
    return data


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(GIT), *args])


def git_text(*args):
    return git(*args).decode().strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins-sha256", required=True)
    args = parser.parse_args()
    root_pin_path = WAVE / "r166-link/ROOT_LINK_PINS.json"
    root_sha = args.root_pins_sha256.lower()
    root = json.loads(checked(root_pin_path, root_sha))
    head = root["head"]
    assert root["tree"] == TREE and root["parent"] == PARENT
    assert git_text("rev-parse", f"{head}^{{tree}}", f"{head}^").splitlines() == [TREE, PARENT]
    assert git_text("rev-parse", f"{CANDIDATE}^{{tree}}", f"{CANDIDATE}^").splitlines() == [TREE, PARENT]
    assert git_text("rev-parse", f"{head}:{SOURCE}") == BLOB

    pins = root["pins"]
    pinned = {
        key: checked(pins[key]["path"], pins[key]["sha256"])
        for key in ("completion", "seal", "invocation", "result", "pe", "map", "xex", "image", "diagnostics")
    }
    completion, seal, invocation, result, image, diagnostics = (
        json.loads(pinned[key]) for key in ("completion", "seal", "invocation", "result", "image", "diagnostics")
    )
    assert completion["head"] == seal["head"] == image["head"] == head
    assert completion["status"] == "SEALED_NO_REGRESSIONS"
    assert completion["requested"] == completion["passed"] == 3710 and completion["failed"] == 0
    assert invocation["sealed_sha256"] == pins["seal"]["sha256"]
    assert invocation["objects"] == seal["selected"]
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert diagnostics["unique_symbols"] == 0 and not diagnostics["duplicate_messages"]
    assert image["magic"] == "XEX2" and pinned["xex"][:4] == b"XEX2" and image["image_sha256"] == pins["xex"]["sha256"]
    for key in ("pe", "map", "xex"):
        [row] = [row for path, row in result["outputs"].items() if Path(path) == Path(pins[key]["path"])]
        assert row["sha256"] == pins[key]["sha256"]

    [source] = [row for row in seal["sources"]
                if row["path"].replace("\\", "/").lower().endswith("/" + SOURCE.lower())]
    source_data = checked(source["path"], source["sha256"])
    assert source_data.replace(b"\r\n", b"\n") == git("show", f"{head}:{SOURCE}").replace(b"\r\n", b"\n")
    [selected] = [row for row in invocation["objects"] if row["source"] == SOURCE]
    object_data = checked(selected["object"], selected["sha256"])

    verifier = FOCUSED / "verify_copy.py"
    checked(verifier, FOCUSED_VERIFIER_SHA)
    proof = json.loads(checked(FOCUSED / "PPC_COPY_PROOF.json", FOCUSED_PROOF_SHA))
    copy = runpy.run_path(str(verifier))
    symbol, body, refs = copy["Coff"](object_data).function(proof["symbol"])
    assert symbol.name == proof["symbol"] and not refs
    assert len(body) == proof["candidate_instruction_bytes"] and digest(body) == proof["candidate_body_sha256"]

    checked(PE_HELPER, PE_HELPER_SHA)
    image_reader = runpy.run_path(str(PE_HELPER))["Image"]
    pe = image_reader(Path(pins["pe"]["path"]), Path(pins["map"]["path"]))
    assert pe.base == 0x82000000
    helper = runpy.run_path(str(copy["HELPER"]))
    owner = Path(selected["object"]).name
    entries = helper["map_symbols"](pinned["map"])
    [address] = {va for va, obj in entries[symbol.name] if obj.lower() == owner.lower()}
    assert pe.symbols[symbol.name] == address
    linked = b"".join(pe.word(pc).to_bytes(4, "big") for pc in range(address, address + len(body), 4))
    assert linked == body

    artist = copy["x360rd"].rd(copy["ARTIST_ENTRY"], copy["ARTIST_SIZE"])
    assert digest(artist) == proof["artist_body_sha256"]
    for label, self_assignment in (("distinct", False), ("self_assignment", True)):
        assert copy["trace"](artist, self_assignment) == proof[label]["artist"]
        assert copy["trace"](linked, self_assignment) == proof[label]["candidate"]
    receipt = {
        "head": head, "tree": TREE, "parent": PARENT, "candidate": CANDIDATE,
        "root_pins_sha256": root_sha, "focused_proof_sha256": FOCUSED_PROOF_SHA,
        "source": source, "source_git_blob": BLOB, "selected": selected, "symbol": symbol.name,
        "linked_va": hex(address), "linked_body_sha256": digest(linked), "linked_body_bytes": len(linked),
        "relocations": len(refs), "distinct": proof["distinct"], "self_assignment": proof["self_assignment"],
        "scope": "Static linked assignment only. No launch or runtime validation.",
    }
    output = OUT / "RECEIPT.json"
    output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("PASS", hex(address), digest(output.read_bytes()))


if __name__ == "__main__":
    main()
