"""Carry the sealed R147 GUI/render PPC proof forward to R148."""

import argparse
import json
import runpy
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
BASE = "5509461903bd56c25d92208d84da7a37171a4fc4"
HEAD = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
TREE = "292de86d70d0b034d502995355f6169a766a7d7e"
MANIFEST_SHA = "46d182100c312d64f1b6cb3f900122dd2a7df95ecdaa41093538074fa0795339"
R147_SHA = "aee439770244488d6d714699ce0b5114189087ce2fa767b76257490fcf0f9f4c"
VERIFIER_SHA = "5ebff9aefb8029d8659d2b1047799945eb81aa697a2be44f0641f55e40e699da"
R146_SHA = "24642060a7512d9ed7424387f843554d042f8d41db829913471c2168168a8820"
BASE_SEAL_SHA = "f3431bee73fb501ec8d9349f3f824e781123c15f5ae40b753910eb3ea7c165d5"
BASE_LINK = WAVE / "r147-sdk-link"
BASE_PROOF = WAVE / "r147-gui-render-regression"
helper = runpy.run_path(str(BASE_PROOF / "verify_linked.py"))
sha, load, sealed_link, bind, image_sections, linked_witnesses = (
    helper[name] for name in ("sha", "load", "sealed_link", "bind", "image_sections", "linked_witnesses")
)
assert sha((BASE_PROOF / "verify_linked.py").read_bytes()) == VERIFIER_SHA


def verify_cohort(seal, baseline):
    proof_path = WAVE / "r147-xmedia-provider-proof/PROVIDER_SELECTION.json"
    proof_digest = helper["RECOVERY_SHA"]
    selection = load(proof_path, proof_digest)
    assert seal["schema"] == "r148-native-link-seal-v1" and seal["head"] == HEAD
    for key, path, digest in (("prior_successful_seal", BASE_LINK / "seal.json", BASE_SEAL_SHA),
                              ("provider_selection", proof_path, proof_digest)):
        assert Path(seal[key]["path"]).resolve() == path.resolve()
        assert seal[key]["sha256"] == digest == sha(path.read_bytes())
    assert seal["provider_cohort"] == baseline["provider_cohort"] == selection["sources"]
    assert len(seal["provider_cohort"]) == 15
    assert (len(seal["selected"]), len(seal["excluded"])) == (3589, 121)
    for group in ("selected", "excluded"):
        assert {row["source"] for row in seal[group]} == {row["source"] for row in baseline[group]}
    excluded = {row["source"]: row for row in seal["excluded"]}
    for provider in selection["sources"]:
        actual = excluded[provider["source"]]
        assert (actual["object"], actual["sha256"]) == (provider["object"], provider["sha256"])
    return {"provider_selection_sha256": proof_digest, "prior_seal_sha256": BASE_SEAL_SHA,
            "sdk_provider_count": 15, "selected": 3589, "excluded": 121}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="reproduce the exact sealed R147 proof")
    args = parser.parse_args()
    manifest = load(WAVE / "r148-batch/MANIFEST.json", MANIFEST_SHA)
    assert (manifest["base"], manifest["projected_tree"]) == (BASE, TREE)
    frozen = load(BASE_PROOF / "R147_GUI_RENDER_LINKED.json", R147_SHA)
    assert (frozen["head"], frozen["tree"]) == (BASE, manifest["base_tree"])
    composed = load(WAVE / "r146-composed-proof/R146_COMPOSED_COFF.json", R146_SHA)
    base_seal, base_objects, base_entries, base_read, base_hashes = sealed_link(BASE_LINK, BASE)
    assert base_hashes == frozen["link_sha256"] and base_hashes["seal"] == BASE_SEAL_SHA
    assert helper["verify_recovery"](BASE_LINK, base_seal) == frozen["recovery"]
    base_data = {stem: data for stem, (_, data, _) in base_objects.items()}
    base_rows = {row["source"]: row for _, _, row in base_objects.values()}
    assert base_rows.keys() == frozen["source_and_object_pins"].keys()
    for source, row in base_rows.items():
        assert row["sha256"] == frozen["source_and_object_pins"][source]["r147_object_sha256"]

    if args.self_test:
        linked, proofs = bind(base_objects, base_data, base_entries, base_read, image_sections(BASE_LINK))
        witnesses = linked_witnesses(linked, proofs, base_entries, base_read, composed)
        assert proofs == frozen["functions"] and witnesses == frozen["witnesses"]
        print(f"Sealed R147 baseline matches all {len(proofs)} frozen functions and witnesses")
        return

    link = WAVE / "r148-link"
    seal, objects, entries, read, hashes = sealed_link(link, HEAD)
    cohort = verify_cohort(seal, base_seal)
    git = ["git", "--no-optional-locks", "-C", seal["source_root"]]
    revision = subprocess.check_output([*git, "rev-parse", f"{HEAD}^{{tree}}", f"{HEAD}^"], text=True).splitlines()
    assert revision == [TREE, BASE], revision
    diff = subprocess.check_output([*git, "diff", "--name-only", BASE, HEAD], text=True).splitlines()
    assert len(diff) == 4 and set(diff) == {row["path"] for row in manifest["final_paths"]}

    pins = {}
    for stem, (_, data, row) in objects.items():
        source = row["source"]
        previous = base_objects[stem][2]
        assert source == previous["source"] and row["sha256"] == previous["sha256"] and data == base_data[stem], stem
        blob = subprocess.check_output([*git, "rev-parse", f"{BASE}:{source}", f"{HEAD}:{source}"], text=True).splitlines()
        assert blob == [frozen["source_and_object_pins"][source]["blob"]] * 2, source
        pins[source] = {"blob": blob[0], "object_sha256": row["sha256"]}
    linked, proofs = bind(objects, base_data, entries, read, image_sections(link))
    assert proofs.keys() == frozen["functions"].keys()
    witnesses = linked_witnesses(linked, proofs, entries, read, composed)
    receipt = {
        "head": HEAD, "tree": TREE, "base": BASE, "manifest_sha256": MANIFEST_SHA,
        "r147_linked_proof_sha256": R147_SHA, "r147_linked_verifier_sha256": VERIFIER_SHA,
        "link_sha256": hashes, "retained_sdk_cohort": cohort, "source_and_object_pins": pins,
        "witnesses": witnesses, "functions": proofs, "runtime": "not run",
    }
    output = ROOT / "R148_GUI_RENDER_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output.read_bytes()))


if __name__ == "__main__":
    main()
