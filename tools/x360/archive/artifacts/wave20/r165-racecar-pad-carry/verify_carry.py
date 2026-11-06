
"""Carry the verified RaceCar map and pad PPC through the pinned R165 rebuild."""
import argparse
import json
from pathlib import Path
import runpy
import struct
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
HEAD = "451d95889337e064441dec06a1726179187e48cf"
TREE = "cdfbeb9f10e3b32c23e42da60c5ea72a5dcde843"
PARENT = "d9d487ef61d4bab2e80d9e4c2fc64c20a600b08a"
PARENT_PINS_SHA = "bd72374e4d7e73ddd06603a7d871475a979a5b0fca9ab64fcfc681c80ce27033"
MAP_DIR = WAVE / "r163-racecar-stage0-preflight"
MAP_RECEIPT_SHA = "bc6990368fd28e5e21fec33f05004f6f22a503a9865b0ae19c44c67bb8dd8f9d"
MAP_CHECK_SHA = "ae32feea82e726f7fbc4e36832566a2da283dbb19e80edc7181d2ff3f8422d61"
MAP_VERIFIER_SHA = "9587f51e69a64649a1ca3b4f5dcf55d2c4d71d48fbeF270e29ec9790d1680429".lower()
PAD_RECEIPT_SHA = "148b526fc6dc4e60659c39d0b2be03851e99176c1390043f9055c9335f55bad2"
PAD_CHECK_SHA = "c1f2487281c5712858a7ef87a4705de333e486b7fc3cfaf12fe6c01eabb8ce53"
H = runpy.run_path(str(WAVE / "r160-gui-linked/verify_gui.py"))
IMAGE, COFF, load, sha, digest, word, canonical, pe_bytes, split_address, tree, blob, inspect = (
    H[key] for key in ("IMAGE", "COFF", "load", "sha", "digest", "word", "canonical",
                       "pe_bytes", "split_address", "tree", "blob", "inspect")
)
map_symbols = H["TOOLS"]["PRIOR"]["HELPER"]["map_symbols"]
git = H["TOOLS"]["git"]
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import rd

RACE = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
PADS = "src/GameShared/GameClasses/System/Input/CgsInputPads.cpp"
INIT = "?InitializePads@InputPads@CgsInput@@AAAXXZ"
UPDATE = "?Update@InputPads@CgsInput@@"
DEVICE = "?Construct@DeviceX360Pad@CgsInput@@QAAXXZ"
MAP = "?Construct@WorldMap2D@CgsWorld@@"


def pinned_json(path, expected):
    assert sha(path) == expected, path
    return load(path)


def verified_selection(root):
    p = root["pins"]
    for key in ("seal", "invocation", "result", "pe", "map", "xex", "image", "completion"):
        assert sha(Path(p[key]["path"])) == p[key]["sha256"], key
    seal, inv, result, image, completion = (
        load(Path(p[key]["path"])) for key in ("seal", "invocation", "result", "image", "completion"))
    assert seal["head"] == image["head"] == completion["head"] == root["head"]
    assert inv["sealed_sha256"] == p["seal"]["sha256"] and inv["objects"] == seal["selected"]
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert image["magic"] == "XEX2" and image["image_sha256"] == p["xex"]["sha256"]
    assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0
    return seal, {row["source"]: row for row in inv["objects"]}


def checked_object(row):
    path = Path(row["object"])
    payload = path.read_bytes()
    assert digest(payload) == row["sha256"]
    return COFF(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins", type=Path, required=True)
    parser.add_argument("--root-pins-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = pinned_json(args.root_pins, args.root_pins_sha256.lower())
    assert (root["head"], root["tree"], root["parent"]) == (HEAD, TREE, PARENT)
    assert tree(HEAD) == TREE and git("rev-parse", HEAD + "^").decode().strip() == PARENT
    old_root = pinned_json(WAVE / "r164-link/ROOT_LINK_PINS.json", PARENT_PINS_SHA)
    assert old_root["head"] == PARENT
    _, before = verified_selection(old_root)
    seal, selected = verified_selection(root)
    p = root["pins"]
    map_path, pe_path = (Path(p[key]["path"]) for key in ("map", "pe"))
    entries = map_symbols(map_path.read_bytes())
    image = IMAGE(pe_path, map_path)
    source_root = Path(seal["source_root"])
    sources = {Path(row["path"]).relative_to(source_root).as_posix().lower(): row for row in seal["sources"]
               if Path(row["path"]).is_relative_to(source_root)}
    linked = image, entries, sources
    prior_map = pinned_json(MAP_DIR / "R163_LINKED_MAP.json", MAP_RECEIPT_SHA)
    map_proof = pinned_json(WAVE / "r163-batch/COMPOSED_PPC_CHECK.json", MAP_CHECK_SHA)
    assert prior_map["state"] == "pass" and before[RACE]["sha256"] == prior_map["selected_object_sha256"]
    prior_pad = pinned_json(WAVE / "r162-title-input-linked/RECEIPT.json", PAD_RECEIPT_SHA)
    pad_proof = pinned_json(WAVE / "r162-batch/COMPOSED_PPC_CHECK.json", PAD_CHECK_SHA)["pad_initialize"]
    pad_init_call = pad_proof["checked_branch_displacement"]
    assert pad_proof["bytes"] == 136 and pad_proof["callee"] == DEVICE
    checks = {
        "race_map_prepare": (RACE, map_proof["prepare_symbol"], {0x120: MAP},
            {0xF0: 0xFB9F023C, 0x104: 0x104058C3, 0x110: 0x102058C3, 0x118: 0x814B0004}),
        "pad_init": (PADS, INIT, {pad_init_call: DEVICE}, {}),
        "pad_construct": (PADS, "?Construct@InputPads@CgsInput@@", {pad_proof["caller_branch"]: INIT}, {}),
    }
    for label in ("pad_action_edges", "input_pre_world"):
        item = prior_pad["functions"][label]
        checks[label] = (item["source"], item["symbol"],
                         {int(at, 16): call["symbol"] for at, call in item["calls"].items()},
                         {int(at, 16): int(value, 16) for at, value in item["words"].items()})

    functions = {}
    for label, spec in checks.items():
        source, prefix, _, _ = spec
        assert blob(PARENT, source) == blob(HEAD, source)
        _, old_code, old_refs = checked_object(before[source]).function(prefix)
        _, code, refs = checked_object(selected[source]).function(prefix)
        assert canonical(code, refs) == canonical(old_code, old_refs), label
        assert {at: (typ, dst.name) for at, (typ, dst) in refs.items()} == {
            at: (typ, dst.name) for at, (typ, dst) in old_refs.items()}, label
        now = inspect(label, spec, selected, HEAD, linked)
        if label in prior_pad["functions"]:
            assert now["coff_masked_body_sha256"] == prior_pad["functions"][label]["coff_masked_body_sha256"]
        now.update(r164_object_sha256=before[source]["sha256"], r164_object=before[source]["object"])
        functions[label] = now

    def linked_code(label):
        info = functions[label]
        _, code, _ = checked_object(selected[info["source"]]).function(info["symbol"])
        return pe_bytes(image, int(info["linked_va"], 16), len(code))

    original = rd(int(pad_proof["original"], 16), pad_proof["bytes"])
    pad = linked_code("pad_init")
    assert len(pad) == len(original) == 136
    assert pad[:pad_init_call] == original[:pad_init_call]
    assert pad[pad_init_call + 4:] == original[pad_init_call + 4:]
    assert word(pad, pad_init_call) & 0xFC000003 == word(original, pad_init_call) & 0xFC000003
    functions["pad_init"]["artist_raw_bytes_excluding_relocated_branch"] = 132

    verifier = MAP_DIR / "verify_linked.py"
    assert sha(verifier) == MAP_VERIFIER_SHA
    crt_helpers = runpy.run_path(str(verifier))
    objfn = crt_helpers["objfn"]
    fixed_equal = crt_helpers["m"]["fixed_equal"]
    prepared = linked_code("race_map_prepare")
    current_obj, old_obj = (Path(rows[RACE]["object"]) for rows in (selected, before))
    vectors = []
    for expected, prior in zip(map_proof["vectors"], prior_map["vectors"], strict=True):
        assert expected["name"] == prior["name"]
        raw = bytes.fromhex(expected["initialized_bytes"])
        assert raw == bytes.fromhex(prior["initialized_bytes"])
        name = expected["initializer"]
        [address] = {at for at, owner in entries[name] if owner.lower() == current_obj.name.lower()}
        code, refs, _ = objfn(current_obj, name)
        previous, previous_refs, _ = objfn(old_obj, name)
        assert len(code) == len(previous) == expected["ppc_bytes"] and refs
        fixed_equal(previous, code, previous_refs)
        final = pe_bytes(image, address, len(code))
        fixed_equal(code, final, refs)
        high, low = (int(at, 16) for at in expected["address_pair"])
        storage = split_address(prepared, high, low)
        assert split_address(final, 0x20, 0x2C) == storage
        for hi, lo, offset in ((0, 8, 0), (12, 28, 4)):
            assert pe_bytes(image, split_address(final, hi, lo), 4) == raw[offset:offset+4]
        assert final[0x14:0x1C] == bytes.fromhex("9141FFF89141FFFC") and raw[8:] == bytes(8)
        vectors.append({"name": expected["name"], "initializer_va": hex(address),
                        "storage_va": hex(storage), "initialized_bytes": raw.hex()})
    start, end = (min(at for at, _ in entries[name]) for name in ("__xc_a", "__xc_z"))
    assert start < end
    pair = struct.pack(">II", *(int(item["initializer_va"], 16) for item in vectors))
    crt = pe_bytes(image, start, end - start + 4)
    [slot] = [start + offset for offset in range(0, len(crt) - 7, 4) if crt[offset:offset+8] == pair]
    report = {"head": HEAD, "tree": TREE, "parent": PARENT, "root_pins_sha256": args.root_pins_sha256.lower(),
              "r164_root_pins_sha256": PARENT_PINS_SHA, "r163_map_receipt_sha256": MAP_RECEIPT_SHA,
              "r162_title_receipt_sha256": PAD_RECEIPT_SHA, "functions": functions,
              "map_vectors": vectors, "crt_vector_slots": [hex(slot), hex(slot + 4)],
              "scope": "Selected PPC COFF and pinned linked PE. Runtime unverified."}
    document = json.dumps(report, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == document
    else:
        with args.output.open("x", encoding="utf-8") as file:
            file.write(document)
    print("PASS", len(functions), "RaceCar/pad carry functions", sha(args.output))


if __name__ == "__main__":
    main()

