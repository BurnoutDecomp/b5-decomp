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

def verify(folder, head):
    composition_path = here / "R147_CAMERA_COMPOSITION.json"
    composition = json.loads(composition_path.read_text())
    artifacts = composition["retained_artifacts"]
    assert composition["state"] == "pass" and composition["exact_tree_match"]
    tree = subprocess.check_output(["git", "-C", str(checkout), "rev-parse", head + "^{tree}"]).decode().strip()
    assert tree == "292de86d70d0b034d502995355f6169a766a7d7e"
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
    import hashlib
    base = wave / "r147-sdk-link"
    target = wave / "r148-link"
    oldhead = "5509461903bd56c25d92208d84da7a37171a4fc4"
    head = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
    oldreceipt = base / "R147_CAMERA_LINKED.json"
    assert sha(oldreceipt) == "d11aad1ce82acc0f1ed11344950099f9e2922c8b8117141807b4ff35bc798166"
    old = json.loads(oldreceipt.read_text())
    assert old["state"] == "pass" and old["head"] == oldhead
    receipt = verify(target, head)
    assert receipt["image_sha256"]["native-game.exe"] == "c20f222730c7406e70c77b1555aa96ab91789589c83b7331c7e00d640909a63d"
    image = json.loads((target / "image-verification.json").read_text())
    assert image["head"] == head and image["image_sha256"] == receipt["image_sha256"]["native-game.exe"] and image["magic"] == "XEX2"
    paths = ["src/GameSource/Network/Managers/X360/BrnNetworkCameraX360.cpp", "src/GameSource/Network/Managers/X360/BrnNetworkCameraX360.h"]
    source_blobs = {}
    for path in paths:
        blobs = [subprocess.check_output(["git", "-C", str(checkout), "rev-parse", h + ":" + path]).decode().strip() for h in (oldhead, head)]
        assert blobs[0] == blobs[1]
        source_blobs[path] = blobs[0]
    invs = [json.loads((p / "invocation.json").read_text()) for p in (base, target)]
    selected = [next(x for x in inv["objects"] if x["source"] == paths[0]) for inv in invs]
    assert selected[0] == selected[1] and sha(Path(selected[1]["object"])) == selected[1]["sha256"]
    cohort_names = ["R125_MATCH.json", "R127_MATCH.json", "R133_MATCH.json", "R134_MATCH.json", "R135_MATCH.json", "R136_MATCH.json", "R137_MATCH.json", "R138_MATCH.json", "R140_MATCH.json", "R141_BOOT_VIDEOS_LINKED.json", "R141_LOADING_BUNDLE_LINKED.json", "R141_RACE_CAR_READY_LINKED.json", "R148_AEMS_LINKED.json", "R148_PROFILE_FLOW_LINKED.json", "PUBLISHED_MATCH.json"]
    cohort = {}
    for name in cohort_names:
        p = target / name
        data = json.loads(p.read_text())
        digest = data.get("xex_sha256") or data.get("link_context", {}).get("xex_sha256") or (data.get("image_sha256", {}).get("native-game.exe") if isinstance(data.get("image_sha256"), dict) else data.get("image_sha256"))
        assert digest and digest.lower() == receipt["image_sha256"]["native-game.exe"], name
        cohort[name] = sha(p)
    receipt.update({"historical_receipt": {"path": str(oldreceipt), "sha256": sha(oldreceipt)}, "historical_preflight_tree": json.loads((here / "R147_CAMERA_COMPOSITION.json").read_text())["preflight_tree"], "unchanged_source_git_blobs": source_blobs, "unchanged_selected_object": selected[1], "inherited_cohort_receipts": cohort, "image_verification_sha256": sha(target / "image-verification.json")})
    output = Path(__file__).parent / "R148_CAMERA_REGRESSION.json"
    with output.open("x", encoding="utf-8", newline="\n") as f: json.dump(receipt, f, indent=2); f.write("\n")
    print(output, sha(output))
