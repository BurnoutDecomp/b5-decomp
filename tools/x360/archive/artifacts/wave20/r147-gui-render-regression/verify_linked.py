"""Recheck the frozen R146 GUI and renderer paths in a sealed R147 image."""

import argparse
import json
import re
import runpy
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
BASE = "c218ba0f2ee504d7142bd1a9f7dd9a709b1b8f16"
TREE = "960f33b1c1982482d3ded2f5d38a5789eecc80f6"
HEAD = "5509461903bd56c25d92208d84da7a37171a4fc4"
MANIFEST_SHA = "6c78e0d6ccb80a2bc18ac8f8cffa6547bcdb067d70834dc914fa7e7b6b9516cf"
R146_RECEIPT_SHA = "aab65f6a40a046dcc354d2be7fc435ffd179e5f42ece8c1b01c3ad7c0f465283"
R146_VERIFIER_SHA = "85a693e9f33b4fc9434c342c3ed1d99104b5fb82bfd3bcc1ab51ddfc091a4892"
RECOVERY_SHA = "c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833"
FAILED_SEAL_SHA = "db48703b7b1fae57ff3d4addb72bff3e97eeca5bb0da081a3b2dd402c332b801"
REVIEW_SHA = "98d6bc8a4a30894d4401a9b0dd9261fc2cafad4fd8d6a6e5a276d65ecf8da830"
PRESEAL_CORRECTION_SHA = "a9f67b4336f7525b4e3d70039d20a1e94236181184dcf1dc27b2d6f9fe152522"
R146 = WAVE / "r146-linked-proof"
helper = runpy.run_path(str(R146 / "verify_linked.py"))
sha, load, sealed_link, bind, image_sections, linked_witnesses = (
    helper[name] for name in ("sha", "load", "sealed_link", "bind", "image_sections", "linked_witnesses")
)
assert sha((R146 / "verify_linked.py").read_bytes()) == R146_VERIFIER_SHA


def normalize_debug_metadata(data):
    """Ignore only the timestamp, debug payload and compiler counter names."""
    result = bytearray(data)
    machine, count, _, table, symbols, optional, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional == 0
    result[4:8] = bytes(4)
    for i in range(count):
        name, _, _, size, offset = struct.unpack_from("<8sIIII", data, 20 + i * 40)
        if name == b".debug$S":
            assert 0 < offset < offset + size <= len(data)
            result[offset:offset + size] = bytes(size)
    index = 0
    while index < symbols:
        pos = table + index * 18
        name, _, _, _, storage, aux = struct.unpack_from("<8sIhHBB", data, pos)
        if ((storage == 6 and re.fullmatch(rb"\$M[0-9]+", name.rstrip(b"\0"))) or
                (storage == 3 and re.fullmatch(rb"\$T[0-9]+", name.rstrip(b"\0")))):
            result[pos + 2:pos + 8] = bytes(6)
        index += 1 + aux
    return bytes(result)


def verify_recovery(link, seal):
    assert link == (WAVE / "r147-sdk-link").resolve()
    assert seal["schema"] == "r147-sdk-recovery-link-seal-v1" and seal["head"] == HEAD
    old_link = WAVE / "r147-link"
    approved = WAVE / "r147-xmedia-provider-proof"
    original_path = old_link / "seal.json"
    proof_path = approved / "PROVIDER_SELECTION.json"
    review_path = approved / "REVIEW_V2.json"
    for field, expected in (("original_seal", original_path), ("provider_selection", proof_path), ("review", review_path)):
        assert Path(seal[field]["path"]).resolve() == expected.resolve()
    assert seal["original_seal"]["sha256"] == FAILED_SEAL_SHA
    assert seal["provider_selection"]["sha256"] == RECOVERY_SHA
    original = load(original_path, FAILED_SEAL_SHA)
    proof = load(proof_path, RECOVERY_SHA)
    assert seal["review"]["sha256"] == REVIEW_SHA
    review = load(review_path, REVIEW_SHA)
    assert original["head"] == proof["head"] == review["head"] == HEAD
    assert proof["original_seal_sha256"] == review["original_seal_sha256"] == FAILED_SEAL_SHA
    assert proof["schema"] == "r147-xmedia-provider-selection-v1" and proof["state"] == "pass"
    assert review["verdict"].lower() == "pass" and review["provider_selection_sha256"] == RECOVERY_SHA
    assert proof["missing_count"] == proof["strong_conflict_count"] == 0
    for name, digest in review["scripts_sha256"].items():
        assert sha((link / name).read_bytes()) == digest, name
    correction = load(link / "PRESEAL_CORRECTION.json", PRESEAL_CORRECTION_SHA)
    assert correction["prepare_sha256"] == review["scripts_sha256"]["prepare_seal.py"]
    assert sha((link / "prepare_seal-attempt1.py").read_bytes()) == correction["prior_prepare_sha256"]
    assert sha(Path(correction["preserved_report"]).read_bytes()) == correction["preserved_report_sha256"]
    failed_result = load(old_link / "result.json", proof["original_result_sha256"])
    assert failed_result["exit_code"] == 1169
    assert sha((old_link / "link.log").read_bytes()) == proof["original_log_sha256"]

    assert (len(seal["selected"]), len(seal["excluded"])) == (proof["selected"], proof["excluded"]) == (3589, 121)
    old_selected = {row["source"]: row for row in original["selected"]}
    selected = {row["source"]: row for row in seal["selected"]}
    removed = {row["source"]: row for row in proof["sources"]}
    assert len(removed) == 15 and old_selected.keys() - selected.keys() == removed.keys()
    for source, row in removed.items():
        assert (row["object"], row["sha256"]) == (old_selected[source]["object"], old_selected[source]["sha256"])
    assert all(row == old_selected[source] for source, row in selected.items())
    return {"proof_sha256": RECOVERY_SHA, "review_sha256": REVIEW_SHA, "preseal_correction_sha256": PRESEAL_CORRECTION_SHA,
            "failed_seal_sha256": FAILED_SEAL_SHA, "selected": 3589, "excluded": 121}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path, nargs="?")
    parser.add_argument("--expected-head")
    parser.add_argument("--self-test", action="store_true", help="rerun and compare the sealed R146 proof")
    args = parser.parse_args()
    manifest = load(WAVE / "r147-batch/MANIFEST.json", MANIFEST_SHA)
    assert (manifest["base"], manifest["projected_tree"]) == (BASE, TREE)
    frozen = load(R146 / "R146_GUI_RENDER_LINKED.json", R146_RECEIPT_SHA)
    assert frozen["head"] == BASE and frozen["tree"] == manifest["base_tree"]
    composed = load(WAVE / "r146-composed-proof/R146_COMPOSED_COFF.json", frozen["composed_coff_sha256"])
    base_link = WAVE / "r146-link"
    _, base_objects, base_entries, base_read, base_hashes = sealed_link(base_link, BASE)
    assert base_hashes == frozen["link_sha256"]
    for stem, (_, data, row) in base_objects.items():
        assert row == frozen["providers"][stem] and sha(data) == row["sha256"]
    base_data = {stem: data for stem, (_, data, _) in base_objects.items()}

    if args.self_test:
        linked, proofs = bind(base_objects, base_data, base_entries, base_read, image_sections(base_link))
        witnesses = linked_witnesses(linked, proofs, base_entries, base_read, composed)
        assert proofs == frozen["functions"] and witnesses == frozen["witnesses"]
        print(f"Sealed R146 baseline matches frozen proof for {len(proofs)} functions")
        return

    if not args.link_dir or not re.fullmatch(r"[0-9a-f]{40}", args.expected_head or ""):
        parser.error("provide link_dir and --expected-head, or use --self-test")
    link = args.link_dir.resolve()
    seal, objects, entries, read, hashes = sealed_link(link, args.expected_head)
    assert args.expected_head == HEAD
    recovery = verify_recovery(link, seal)
    git = ["git", "--no-optional-locks", "-C", seal["source_root"]]
    revision = subprocess.check_output([*git, "rev-parse", f"{args.expected_head}^{{tree}}", f"{args.expected_head}^"],
                                       text=True).splitlines()
    assert revision == [TREE, BASE], revision
    diff = subprocess.check_output([*git, "diff", "--name-only", BASE, args.expected_head], text=True).splitlines()
    assert set(diff) == {row["path"] for row in manifest["final_paths"]}

    sources = {}
    for stem, (_, data, row) in objects.items():
        old = frozen["providers"][stem]
        assert row["source"] == old["source"], stem
        metadata_only = data != base_data[stem]
        if metadata_only:
            assert stem == "GameBridgeGameStateToX_EventFlowGuiEvents", stem
            assert normalize_debug_metadata(data) == normalize_debug_metadata(base_data[stem]), stem
        else:
            assert row["sha256"] == old["sha256"], stem
        source = row["source"]
        blob = subprocess.check_output([*git, "rev-parse", f"{BASE}:{source}", f"{args.expected_head}:{source}"],
                                       text=True).splitlines()
        assert len(blob) == 2 and blob[0] == blob[1], source
        sources[source] = {"blob": blob[0], "r146_object_sha256": old["sha256"],
                           "r147_object_sha256": row["sha256"], "metadata_only_recompile": metadata_only}

    linked, proofs = bind(objects, base_data, entries, read, image_sections(link))
    assert proofs.keys() == frozen["functions"].keys()
    witnesses = linked_witnesses(linked, proofs, entries, read, composed)
    receipt = {
        "head": args.expected_head, "tree": TREE, "base": BASE, "manifest_sha256": MANIFEST_SHA,
        "r146_linked_proof_sha256": R146_RECEIPT_SHA, "r146_linked_verifier_sha256": R146_VERIFIER_SHA,
        "link_sha256": hashes, "recovery": recovery, "source_and_object_pins": sources,
        "witnesses": witnesses, "functions": proofs, "runtime": "not run",
    }
    output = ROOT / "R147_GUI_RENDER_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output.read_bytes()))


if __name__ == "__main__":
    main()
