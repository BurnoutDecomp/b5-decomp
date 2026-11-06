"""Check the composed R147 camera methods in a completed native link."""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess

here = Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r147-camera-compose")
wave = here.parent
checkout = Path("C:/Users/adri1/.codex/worktrees/bp360/perfmon_ui_luna")
helper = wave / "r128-batch/verify_matches.py"
readers = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], readers)
sha = readers["sha"]

symbols = {
    "ResetVideoPackets": "?ResetVideoPackets@CameraX360@BrnNetwork@@QAAXXZ",
    "Prepare": "?Prepare@CameraX360@BrnNetwork@@QAA_NPAVHeapMalloc@CgsMemory@@@Z",
    "Release": "?Release@CameraX360@BrnNetwork@@QAA_NXZ",
    "OpenSocket": "?OpenSocket@CameraX360@BrnNetwork@@AAAXXZ",
    "CloseSocket": "?CloseSocket@CameraX360@BrnNetwork@@AAAXXZ",
    "ResetRemotePlayers": "?ResetRemotePlayers@CameraX360@BrnNetwork@@QAAXXZ",
    "PrepareTextures": "?PrepareTextures@CameraX360@BrnNetwork@@QAAXPAVHeapMalloc@CgsMemory@@@Z",
}

def verify(folder, head, selected_camera=None):
    composition_path = here / "R147_CAMERA_COMPOSITION.json"
    composition = json.loads(composition_path.read_text())
    artifacts = composition["retained_artifacts"]
    assert composition["state"] == "pass" and composition["exact_tree_match"]
    tree = subprocess.check_output(["git", "-C", str(checkout), "rev-parse", head + "^{tree}"]).decode().strip()
    assert tree == "610a39be8529a22eeb2d463669c53ede9e46a2fe"
    inventory = json.loads((folder / "source-inventory.json").read_text())
    assert inventory["head"] == head
    result = json.loads((folder / "result.json").read_text())
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    image_hashes = {}
    for name in ("native-game.map", "native-game.pe", "native-game.exe"):
        path = folder / name
        digest = sha(path)
        assert result["outputs"][str(path)]["sha256"] == digest
        image_hashes[name] = digest

    proof = artifacts["preflight_receipt"]
    proof_path = Path(proof["path"])
    assert sha(proof_path) == proof["sha256"]
    preflight = json.loads(proof_path.read_text())
    assert preflight["state"] == "pass" and preflight["tree"] == composition["preflight_tree"]
    for item in artifacts["objects"].values():
        assert sha(Path(item["path"])) == item["sha256"]
    camera_obj, = (Path(item["path"]) for source, item in artifacts["objects"].items()
                   if Path(source).stem == "BrnNetworkCameraX360")
    camera_obj = selected_camera or camera_obj
    camera_data = camera_obj.read_bytes()
    address, load = readers["linked_reader"](folder)
    mapping = (folder / "native-game.map").read_text(errors="replace")
    xcam_address = address("XCamCreateStreamEngine")
    assert re.search(rf"\b{xcam_address:08x}\s+f\s+xcam:", mapping, re.IGNORECASE)
    assert address(symbols["Prepare"]) != address(symbols["Release"])

    methods = {}
    for label, symbol in symbols.items():
        code, rels = readers["collect"](camera_obj, symbol)
        va = address(symbol)
        actual = load(va, len(code))
        readers["fixed_equal"](code, actual, rels)
        calls = {}
        data_fields = {}
        pending = {}
        for offset, index, kind in rels:
            name, _, section = readers["object_symbol"](camera_data, index)
            if kind == 6:
                word, = struct.unpack_from(">I", actual, offset)
                target = readers["branch_target"](va + offset, word)
                assert target == address(name), (label, name, hex(target))
                calls[hex(offset)] = {"target": name, "va": hex(target)}
            elif kind == 0x10:
                assert index not in pending
                pending[index] = offset
            elif kind == 0x11:
                high = pending.pop(index)
                target = readers["data_target"](actual, high, offset)
                if name == "KAC_CAMERA_FILE" or name.startswith("??_C@"):
                    value = readers["symbol_literal"](camera_data, index).encode() + b"\0"
                    assert load(target, len(value)) == value, (label, name)
                elif name == "__real@00000000":
                    assert load(target, 4) == bytes(4)
                else:
                    assert name == "??_7StrStreamBase@CgsDev@@6B@" or section == 0
                    assert target == address(name), (label, name, hex(target))
                data_fields[hex(high)] = {"target": name, "va": hex(target)}
            else:
                assert kind == 0x12 and name == "@comp.id"
        assert not pending
        methods[label] = {"va": hex(va), "bytes": len(code), "branches": calls, "data": data_fields}

    entry_paths = {}
    for label, path in preflight["entry_paths"].items():
        owner_va = address(path["owner"])
        call_va = owner_va + int(path["call_offset"], 16)
        word, = struct.unpack(">I", load(call_va, 4))
        assert word & 1
        target = readers["branch_target"](call_va, word)
        assert target == address(symbols[path["target"]]), label
        entry_paths[label] = {"call_va": hex(call_va), "target": path["target"], "target_va": hex(target)}
    return {
        "state": "pass",
        "head": head,
        "tree": tree,
        "link_folder": str(folder),
        "composition_sha256": sha(composition_path),
        "image_sha256": image_hashes,
        "methods": methods,
        "entry_paths": entry_paths,
        "xcam_create_stream_engine": hex(xcam_address),
    }

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--output", type=Path, help="New JSON receipt path")
    args = parser.parse_args()
    assert re.fullmatch(r"[0-9a-f]{40}", args.expected_head)
    receipt = json.dumps(verify(args.link.resolve(), args.expected_head), indent=2) + "\n"
    if args.output:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(receipt)
    print(receipt, end="")



if __name__ == "__main__":
    target = wave / "r153-link"
    previous = wave / "r152-camera-regression/R152_CAMERA_REGRESSION.json"
    assert sha(previous) == "1ae20a7cb7607fc12c397b4ed381c6cdab2443df5bec7fc7807da70f6cd6580b"
    prior = json.loads(previous.read_text())
    assert prior["state"] == "pass" and prior["head"] == "5159415953b6b1c2e9cf9c82bfa67dfa5d5e2ed9"
    binding = json.loads((wave / "r153-intake/commit-binding.json").read_text())
    head = binding["head"]
    assert head == "bcc3e02a41cdb7bba938b91d7ab638c46962540d"
    assert binding["base"] == prior["head"] and binding["manifest_sha256"] == "d61db6067e9aacb72504ea9e6c432d7068f392a0676d87704aa5b1fb6c1f7d35"
    assert sha(wave / "r153-batch/MANIFEST.json") == binding["manifest_sha256"]
    inv = json.loads((target / "invocation.json").read_text())
    oldinv = json.loads((wave / "r152-link/invocation.json").read_text())
    composition = json.loads((here / "R147_CAMERA_COMPOSITION.json").read_text())
    owner_pins = {}
    for source in composition["retained_artifacts"]["objects"]:
        old = next(x for x in oldinv["objects"] if x["source"] == source)
        new = next(x for x in inv["objects"] if x["source"] == source)
        assert sha(Path(new["object"])) == new["sha256"]
        if not source.endswith("BrnGameModule.cpp"):
            assert old["sha256"] == new["sha256"]
        source_blobs = [subprocess.check_output(["git", "-C", str(checkout), "rev-parse", h + ":" + source]).decode().strip() for h in (prior["head"], head)]
        assert source_blobs[0] == source_blobs[1]
        owner_pins[source] = {"source_blob": source_blobs[0], "previous_selected_object": old, "selected_object": new, "object_hash_unchanged": old["sha256"] == new["sha256"]}
    selected = next(x for x in inv["objects"] if x["source"] == "src/GameSource/Network/Managers/X360/BrnNetworkCameraX360.cpp")
    assert sha(Path(selected["object"])) == selected["sha256"]
    historical = verify(target, head)
    actual = verify(target, head, Path(selected["object"]))
    assert set(actual["methods"]) == set(historical["methods"]) and len(actual["methods"]) == 7
    assert actual["entry_paths"] == historical["entry_paths"] and len(actual["entry_paths"]) == 3
    assert actual["image_sha256"]["native-game.exe"] == "1f474b180ddbefe6f99028d00df0760274ad09771d56b9668b8defb97b7ab2a3"
    image = json.loads((target / "image-verification.json").read_text())
    assert image["head"] == head and image["image_sha256"] == actual["image_sha256"]["native-game.exe"] and image["magic"] == "XEX2"
    blobs = {}
    for path in prior["unchanged_source_git_blobs"]:
        values = [subprocess.check_output(["git", "-C", str(checkout), "rev-parse", h + ":" + path]).decode().strip() for h in (prior["head"],head)]
        assert values[0] == values[1] == prior["unchanged_source_git_blobs"][path]
        blobs[path] = values[0]
    actual.update({"base": prior["head"], "manifest_sha256": binding["manifest_sha256"], "previous_receipt": {"path":str(previous),"sha256":sha(previous)}, "historical_preflight_tree":prior["historical_preflight_tree"], "unchanged_source_git_blobs":blobs, "selected_object":selected, "retained_owner_pins":owner_pins, "historical_preflight_methods":historical["methods"], "image_verification_sha256":sha(target / "image-verification.json"), "invocation_sha256":sha(target / "invocation.json"), "result_sha256":sha(target / "result.json")})
    output = Path(__file__).parent / "R153_CAMERA_REGRESSION.json"
    with output.open("x",encoding="utf-8",newline="\n") as f: json.dump(actual,f,indent=2);f.write("\n")
    print(output,sha(output))
