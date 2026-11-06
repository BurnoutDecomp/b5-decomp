"""Revalidate the R161 title-input route in the sealed R162 PPC image."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
HEAD = "cfa383bfbddd126156185e122979242f5a680683"
TREE = "c9a45a7234193c374f89d1161d756928f0ba94b7"
PARENT = "808f548465be6e2c81967f4ea8af1419f6f651fc"
PRIOR = WAVE / "r161-title-input-linked"
PRIOR_SHA = "477d78ee9afa9dbf964f079130df68039c6fb6186d26b7dd9aa91a6ca63dca34"
HELPER_SHA = "cf8b64420879907aa368e17380887a278018eab7af4bfd66be691733c3279de1"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins", type=Path, required=True)
    parser.add_argument("--root-pins-sha256", required=True)
    args = parser.parse_args()
    assert sha(args.root_pins) == args.root_pins_sha256
    root_pins = load(args.root_pins)
    assert (root_pins["head"], root_pins["tree"], root_pins["parent"]) == (HEAD, TREE, PARENT)
    files = root_pins["pins"]
    pins = {key: Path(files[key]["path"]) for key in
            ("seal", "invocation", "result", "pe", "map", "xex", "image", "completion")}
    for key, path in pins.items():
        assert sha(path) == files[key]["sha256"], key
    seal, invocation, result, image_check, completion = (
        load(pins[key]) for key in ("seal", "invocation", "result", "image", "completion"))
    assert seal["head"] == image_check["head"] == completion["head"] == HEAD
    assert invocation["sealed_sha256"] == files["seal"]["sha256"]
    assert invocation["objects"] == seal["selected"]
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    outputs = {Path(path).name: row for path, row in result["outputs"].items()}
    for key in ("pe", "map", "xex"):
        assert outputs[pins[key].name]["sha256"] == files[key]["sha256"], key
    assert image_check["magic"] == "XEX2" and image_check["image_sha256"] == files["xex"]["sha256"]
    assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0

    helper = PRIOR / "verify_title_input.py"
    prior_path = PRIOR / "RECEIPT.json"
    assert sha(helper) == HELPER_SHA and sha(prior_path) == PRIOR_SHA
    previous = load(prior_path)
    assert previous["linked"]["head"] == PARENT
    verified = runpy.run_path(str(helper))
    H = verified["H"]
    assert verified["tree"](HEAD) == TREE
    git = H["TOOLS"]["git"]
    assert git("rev-parse", f"{HEAD}^").decode().strip() == PARENT
    donor_path = WAVE / "r161-native-inputpads/RECEIPT.json"
    assert sha(donor_path) == verified["DONOR_SHA"]
    donor = load(donor_path)
    image = verified["IMAGE"](pins["pe"], pins["map"])
    entries = H["TOOLS"]["PRIOR"]["HELPER"]["map_symbols"](pins["map"].read_bytes())
    rows = {row["source"]: row for row in invocation["objects"]}
    source_root = Path(seal["source_root"])
    sources = {Path(row["path"]).relative_to(source_root).as_posix().lower(): row for row in seal["sources"]
               if Path(row["path"]).is_relative_to(source_root)}
    linked = image, entries, sources
    checks = verified["CHECKS"] | verified["V2_IO"]
    assert set(checks) == set(previous["functions"]) and len(checks) == 9
    functions = {}
    for name, spec in checks.items():
        old = previous["functions"][name]
        now = verified["inspect"](name, spec, rows, HEAD, linked)
        assert now["git_blob"] == old["git_blob"], name
        assert now["coff_masked_body_sha256"] == old["coff_masked_body_sha256"], name
        assert {site: edge["symbol"] for site, edge in now["calls"].items()} == {
            site: edge["symbol"] for site, edge in old["calls"].items()}, name
        now["r161_linked_va"] = old["linked_va"]
        now["r161_object_sha256"] = old["object_sha256"]
        functions[name] = now
    details = verified["extras"](rows, donor, functions, image, entries)
    old_details = previous["native_details"]
    for group in ("edge_loop", "default_mapping", "boot_event_143"):
        for key, expected in old_details[group].items():
            if not key.endswith("va"):
                assert details[group][key] == expected, (group, key)
    for slot, expected in old_details["input_vtable"].items():
        assert details["input_vtable"][slot]["coff_symbol"] == expected["coff_symbol"], slot

    receipt = {
        "head": HEAD, "tree": TREE, "parent": PARENT,
        "root_pins_sha256": args.root_pins_sha256, "prior_receipt_sha256": PRIOR_SHA,
        "prior_verifier_sha256": HELPER_SHA,
        "pins": {key: files[key] for key in pins},
        "functions": functions, "native_details": details,
        "scope": "Static R161 title input carry in R162. Physical input and title progression remain unverified.",
    }
    path = OUT / "RECEIPT.json"
    document = json.dumps(receipt, indent=2) + "\n"
    if path.exists():
        assert path.read_text(encoding="utf-8") == document
    else:
        with path.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(functions), "title input functions",
          sum(len(fn["calls"]) for fn in functions.values()), "relocated calls", sha(path))


if __name__ == "__main__":
    main()
