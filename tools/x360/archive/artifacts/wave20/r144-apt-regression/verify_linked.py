"""Carry the sealed R143 Apt proof forward to the reviewed R144 link."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

import pefile


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
R143 = WAVE / "r143-linked-proof"
BASE = "c8ca8a670d2e3c3b112c7503d8b3cf6e559cb220"
BASE_TREE = "8d44b165d78b7dbc7cf3d7480ed4dc2b86c076ef"
EXPECTED_TREE = "1f3fa79da6c7a2373fc3dd8377bc145db7cbdf84"
MANIFEST_SHA = "ac9c282272bf716002a1d07ab3b511d19660ce4a9943aed293e06d204f0c289f"
R143_VERIFIER_SHA = "bcccaf6d775764472f93c662b2dbb19caeceabdca7c04ecbcfa958ad957b81b1"
R143_RECEIPT_SHA = "fa21354b9df670313976a40da25de7e506837e6d91c7f51ae82e5503f32dbe8e"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def checked_json(path, expected_sha):
    data = path.read_bytes()
    assert sha(data) == expected_sha, path
    return json.loads(data)


def verified_manifest(link, head, seal):
    manifest = checked_json(WAVE / "r144-batch" / "MANIFEST.json", MANIFEST_SHA)
    assert manifest["schema"] == "r144-native-intake-v1"
    assert (manifest["base"], manifest["base_tree"], manifest["projected_tree"]) == (BASE, BASE_TREE, EXPECTED_TREE)
    assert sha((WAVE / "r144-batch" / "r144.patch").read_bytes()) == manifest["patch"]["sha256"]
    assert seal["schema"] == "r144-native-link-seal-v1" and seal["head"] == head
    provenance = {Path(row["path"]).resolve(): row["sha256"] for row in seal["provenance"]}
    intake = WAVE / "r144-intake"
    binding_path = (intake / "commit-binding.json").resolve()
    release_path = (intake / "ROOT_RELEASE.json").resolve()
    binding = checked_json(binding_path, provenance[binding_path])
    release = checked_json(release_path, provenance[release_path])
    assert binding["head"] == head and binding["base"] == BASE
    assert release["approved"] is True
    assert binding["manifest_sha256"] == release["manifest_sha256"] == MANIFEST_SHA
    assert binding["patch_sha256"] == release["patch_sha256"] == manifest["patch"]["sha256"]
    assert binding["review_sha256"] == release["review_sha256"]
    review = checked_json(WAVE / "r144-batch" / "REVIEW.json", binding["review_sha256"])
    assert review["verdict"] == "pass" and review["manifest_sha256"] == MANIFEST_SHA
    assert review["projected_tree"] == EXPECTED_TREE and review["base"] == BASE
    inventory_record = seal["source_inventory"]
    inventory = checked_json(link / Path(inventory_record["path"]).name, inventory_record["sha256"])
    assert inventory["head"] == head and inventory["schema"] == "r144-source-inventory-v1"

    git = ["git", "--no-optional-locks", "-C", seal["source_root"]]

    def git_read(*args):
        return subprocess.check_output([*git, *args], text=True).strip()

    assert git_read("rev-parse", f"{head}^", f"{head}^{{tree}}", f"{BASE}^{{tree}}").splitlines() == [
        BASE, EXPECTED_TREE, BASE_TREE,
    ]
    paths = [row["path"] for row in manifest["final_paths"]]
    assert len(set(paths)) == len(paths) and binding["paths"] == paths
    assert sorted(git_read("diff", "--name-only", BASE, head).splitlines()) == sorted(paths)
    for rev, side in ((BASE, "old"), (head, "new")):
        entries = dict(line.split("\t", 1)[::-1] for line in git_read("ls-tree", "-r", rev, "--", *paths).splitlines())
        assert entries == {row["path"]: row[side] for row in manifest["final_paths"]}
    return {
        "manifest_sha256": MANIFEST_SHA, "intake_binding_sha256": provenance[binding_path],
        "intake_release_sha256": provenance[release_path], "source_inventory_sha256": inventory_record["sha256"],
        "changed_paths": paths,
    }


def binding_signature(proof, stable_name):
    start = int(proof["address"], 16)
    return {
        offset: (binding["type"],
                 int(binding["target"], 16) - start if binding["symbol"].startswith("$LN")
                 else stable_name(binding["symbol"]),
                 stable_name(binding["alias_target"]) if "alias_target" in binding else None)
        for offset, binding in proof["bindings"].items()
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path)
    parser.add_argument("--expected-head", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.expected_head):
        parser.error("--expected-head must be a full Git commit ID")
    link = args.link_dir.resolve()
    result = json.loads((link / "result.json").read_bytes())
    image = json.loads((link / "image-verification.json").read_bytes())
    seal_bytes = (link / "seal.json").read_bytes()
    seal = json.loads(seal_bytes)
    invocation = json.loads((link / "invocation.json").read_bytes())
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] is True
    assert image["head"] == seal["head"] == args.expected_head
    seal_sha = sha(seal_bytes)
    assert seal_sha == invocation["sealed_sha256"]
    link_release = checked_json(link / Path(invocation["release"]).name, invocation["release_sha256"])
    assert link_release == {"action": "one strict R144 native link", "sealed_sha256": seal_sha}
    manifest = verified_manifest(link, args.expected_head, seal)

    verifier = R143 / "verify_linked.py"
    assert sha(verifier.read_bytes()) == R143_VERIFIER_SHA
    core = runpy.run_path(str(verifier))
    baseline = checked_json(R143 / "R143_APT_LINKED.json", R143_RECEIPT_SHA)
    assert (baseline["head"], baseline["tree"]) == (BASE, BASE_TREE)
    assert baseline["preflight_head"] == core["PREFLIGHT_HEAD"]
    assert baseline["preflight_candidate_sha256"] == core["PREFLIGHT_CANDIDATE_SHA"]
    assert baseline["preflight_compile_report_sha256"] == core["PREFLIGHT_REPORT_SHA"]
    assert len(baseline["functions"]) == 24 and set(baseline["compiled_only"]) == {"SetInCtor"}
    preflight = core["PREFLIGHT"]
    candidate = checked_json(preflight / "CANDIDATE.json", core["PREFLIGHT_CANDIDATE_SHA"])
    assert (candidate["head"], candidate["tree"], candidate["base"]) == (
        core["PREFLIGHT_HEAD"], BASE_TREE, core["INTEGRATED_R142"],
    )
    report = checked_json(preflight / "compile-results.json", core["PREFLIGHT_REPORT_SHA"])
    assert report["passed"] == len(core["FUNCTIONS"]) and report["failed"] == 0

    def output(name):
        [record] = [row for path, row in result["outputs"].items() if Path(path).name == name]
        data = (link / name).read_bytes()
        assert len(data) == record["size"] and sha(data) == record["sha256"], name
        return data

    xex = output("native-game.exe")
    assert xex[:4] == b"XEX2" and sha(xex) == image["image_sha256"]
    pe = output("native-game.pe")
    map_data = output("native-game.map")
    entries = core["read_map"](map_data)
    pe_image = pefile.PE(data=pe, fast_load=True)
    assert pe_image.FILE_HEADER.Machine == 0x1F2

    def read(address, size):
        rva = address - pe_image.OPTIONAL_HEADER.ImageBase
        section = next(s for s in pe_image.sections if s.VirtualAddress <= rva and
                       rva + size <= s.VirtualAddress + s.SizeOfRawData)
        offset = section.PointerToRawData + rva - section.VirtualAddress
        return pe[offset:offset + size]

    [rsp_name] = [arg[1:] for arg in invocation["command"] if arg.startswith("@")]
    rsp_bytes = (link / Path(rsp_name).name).read_bytes()
    assert sha(rsp_bytes) == invocation["rsp_sha256"]
    rsp = rsp_bytes.decode().splitlines()
    linked, proofs, providers, compiled_only = {}, {}, {}, {}
    rows = {Path(row["source"]).stem: row for row in report["results"]}
    assert set(rows) == set(core["FUNCTIONS"])
    for stem, methods in core["FUNCTIONS"].items():
        row = rows[stem]
        source = row["source"]
        assert source.startswith(f'{core["APT"]}/') and row["passed"] and row["machine"] == 0x1F2
        [selected] = [entry for entry in invocation["objects"] if entry["source"] == source]
        [sealed] = [entry for entry in seal["selected"] if entry["source"] == source]
        assert selected == sealed
        path = Path(selected["object"])
        assert rsp.count(f'"{path}"') == 1
        data = path.read_bytes()
        assert sha(data) == selected["sha256"]
        frozen_name = Path(row["object"]).name
        frozen = (preflight / frozen_name).read_bytes()
        assert path.name == frozen_name and sha(frozen) == core["PREFLIGHT_OBJECTS"][stem]
        old = baseline["providers"][source]
        assert sha(frozen) == old["frozen_sha256"]
        providers[source] = {
            "object": str(path), "sha256": selected["sha256"], "frozen_sha256": sha(frozen),
            "r143_selected_sha256": old["selected_sha256"],
            "same_selected_object_as_r143": selected["sha256"] == old["selected_sha256"],
        }

        frozen_definitions = core["coff_functions"](frozen, methods)
        for method, (name, code, refs) in core["coff_functions"](data, methods).items():
            frozen_symbol, frozen_code, frozen_refs = frozen_definitions[method]
            stable = core["stable_name"]
            assert stable(name) == stable(frozen_symbol) and code == frozen_code, method
            assert core["canonical_bindings"](refs) == core["canonical_bindings"](frozen_refs), method
            if method == "SetInCtor":
                assert name not in entries and not refs, name
                core["expect_words"](code, 0, 0x8163000C, 0x508BD908, 0x9163000C, 0x4E800020)
                old_setter = baseline["compiled_only"][method]
                assert name == old_setter["symbol"] and sha(code) == old_setter["sha256"]
                compiled_only[method] = {
                    "symbol": name, "sha256": sha(code), "status": "selected COFF matched; symbol absent from map",
                }
                continue
            address = core["map_address"](entries, name, path.name)
            linked[method], proof = core["verify_function"](entries, read, address, code, refs)
            proof["symbol"] = name
            old_proof = baseline["functions"][method]
            assert stable(name) == stable(old_proof["symbol"]) and proof["bytes"] == old_proof["bytes"], method
            assert binding_signature(proof, stable) == binding_signature(old_proof, stable), method
            proof["r143_address"] = old_proof["address"]
            proofs[method] = proof
    assert set(proofs) == set(baseline["functions"]) and set(compiled_only) == {"SetInCtor"}

    receipt = {
        "schema": "r144-apt-linked-regression-v1", "head": args.expected_head, "tree": EXPECTED_TREE, "base": BASE,
        "manifest": manifest, "r143_receipt_sha256": R143_RECEIPT_SHA, "r143_verifier_sha256": R143_VERIFIER_SHA,
        "preflight_head": core["PREFLIGHT_HEAD"], "preflight_candidate_sha256": core["PREFLIGHT_CANDIDATE_SHA"],
        "preflight_compile_report_sha256": core["PREFLIGHT_REPORT_SHA"], "seal_sha256": seal_sha,
        "result_sha256": sha((link / "result.json").read_bytes()),
        "image_verification_sha256": sha((link / "image-verification.json").read_bytes()),
        "response_sha256": sha(rsp_bytes), "xex_sha256": sha(xex), "pe_sha256": sha(pe), "map_sha256": sha(map_data),
        "providers": providers, "linked_definition_count": len(proofs),
        "dispatch_initializer_slots": core["verify_dispatch"](linked, proofs, entries),
        "witnesses": core["verify_paths"](linked, proofs, entries, read),
        "class_witnesses": core["verify_class_paths"](linked, proofs),
        "functions": proofs, "compiled_only": compiled_only, "scope": "Static linked PE and selected COFF regression.",
    }
    path = ROOT / "R144_APT_LINKED.json"
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(path, sha(path.read_bytes()))


if __name__ == "__main__":
    main()
