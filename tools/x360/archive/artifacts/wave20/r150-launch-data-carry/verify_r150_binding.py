"""Bind the R145 linked-code checks to R150's sealed image, source tree, and provider set.

Run verify_linked.py first; this script validates its receipt and adds the R150-specific bindings.
It reads only the sealed R150/R149 artifacts and Git metadata, then writes the R150 receipt.
"""
from __future__ import annotations

import hashlib
import json
import subprocess
from pathlib import Path

WAVE = Path(__file__).resolve().parent.parent
LINK = WAVE / "r150-link"
R149_LINK = WAVE / "r149-link"
R150_HEAD = "d6e42e0945a067b457ed58da16133d9a4d6202f9"
R150_TREE = "943b2311c7a8592ca02ae848d589669b55aa4745"
R149_HEAD = "28769133a5e88f5cc530003d41b059022fb557f6"
R144_PROOF = WAVE / "r144-launch-codegen-proof" / "R144_CODEGEN_LINKED.json"
LINKED_VERIFIER = WAVE / "r145-launch-data-regression" / "verify_linked.py"
PROVIDER_SELECTION = WAVE / "r147-xmedia-provider-proof" / "PROVIDER_SELECTION.json"
SOURCE_RECEIPT = LINK / "R150_LAUNCH_DATA_LINKED.json"


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def output_record(result, path: Path):
    for key, record in result["outputs"].items():
        if Path(key).resolve() == path.resolve():
            assert record["sha256"] == sha256(path), (key, record)
            return record
    raise AssertionError(f"{path} absent from link result")


def old_check_signature(groups):
    return {
        group: [
            (entry.get("old_address", entry.get("old_call")), entry.get("words"))
            for entry in entries
        ]
        for group, entries in groups.items()
    }


def validate_link(link: Path, expected_head: str):
    seal_path = link / "seal.json"
    inventory_path = link / "source-inventory.json"
    invocation_path = link / "invocation.json"
    image_path = link / "image-verification.json"
    result_path = link / "result.json"
    root_release_path = link / "ROOT_RELEASE.json"
    seal = read_json(seal_path)
    inventory = read_json(inventory_path)
    invocation = read_json(invocation_path)
    image = read_json(image_path)
    result = read_json(result_path)
    root_release = read_json(root_release_path)

    assert seal["head"] == inventory["head"] == image["head"] == expected_head
    assert seal["source_inventory"]["sha256"] == sha256(inventory_path)
    assert invocation["sealed_sha256"] == sha256(seal_path)
    assert root_release["sealed_sha256"] == sha256(seal_path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] is True
    assert image["magic"] == "XEX2"

    output_hashes = {}
    for name in ("native-game.exe", "native-game.map", "native-game.pe", "native-game.xdb"):
        path = link / name
        record = output_record(result, path)
        output_hashes[name] = {"sha256": record["sha256"], "size": record["size"]}

    xex_path = link / "native-game.exe"
    assert image["image_sha256"] == output_hashes["native-game.exe"]["sha256"]
    assert image["bytes"] == output_hashes["native-game.exe"]["size"]
    assert xex_path.read_bytes()[:4] == b"XEX2"
    return {
        "seal": seal,
        "inventory": inventory,
        "invocation": invocation,
        "image": image,
        "result": result,
        "root_release": root_release,
        "seal_path": seal_path,
        "inventory_path": inventory_path,
        "invocation_path": invocation_path,
        "image_path": image_path,
        "result_path": result_path,
        "root_release_path": root_release_path,
        "output_hashes": output_hashes,
    }


def main():
    assert SOURCE_RECEIPT.is_file(), "run the R145 linked verifier first"
    receipt = read_json(SOURCE_RECEIPT)
    r150 = validate_link(LINK, R150_HEAD)
    r149 = validate_link(R149_LINK, R149_HEAD)

    source_root = Path(r150["seal"]["source_root"])
    git_info = subprocess.check_output(
        ["git", "-C", str(source_root), "show", "-s", "--format=%H%n%T", R150_HEAD],
        text=True,
        encoding="utf-8",
    ).splitlines()
    assert git_info == [R150_HEAD, R150_TREE], git_info

    selection_path = Path(r150["seal"]["provider_selection"]["path"])
    selection = read_json(selection_path)
    cohort = r150["seal"]["provider_cohort"]
    assert r150["seal"]["provider_selection"]["sha256"] == sha256(selection_path)
    assert selection["state"] == "pass"
    assert selection["head"] == "5509461903bd56c25d92208d84da7a37171a4fc4"
    assert selection["sources"] == cohort
    assert selection["selected"] == len(r150["seal"]["selected"])
    assert selection["excluded"] == len(r150["seal"]["excluded"])
    provider_hashes_ok = 0
    for provider in cohort:
        obj = Path(provider["object"])
        assert obj.is_file() and sha256(obj) == provider["sha256"], provider
        provider_hashes_ok += 1

    r149_receipt_path = R149_LINK / "R149_LAUNCH_DATA_LINKED.json"
    r149_receipt = read_json(r149_receipt_path)
    r144_proof_sha = sha256(R144_PROOF)
    assert r149_receipt["r144_receipt_sha256"] == r144_proof_sha
    assert receipt["r144_receipt_sha256"] == r144_proof_sha
    assert old_check_signature(r149_receipt["selected_linked_checks"]) == old_check_signature(
        receipt["selected_linked_checks"]
    )

    r150_receipt_sha = sha256(SOURCE_RECEIPT)
    previous_binding = receipt.get("r150_binding", {})
    base_receipt_sha = previous_binding.get("r145_linked_receipt_sha256", r150_receipt_sha)
    provider_cohort_sha = hashlib.sha256(
        json.dumps(cohort, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    selected_count = len(r150["seal"]["selected"])
    excluded_count = len(r150["seal"]["excluded"])
    coverage_count = selected_count + excluded_count

    receipt["tree"] = R150_TREE
    receipt["r150_binding"] = {
        "schema": "r150-launch-data-carry-v1",
        "head": R150_HEAD,
        "tree": R150_TREE,
        "source_root": str(source_root),
        "source_inventory": {
            "path": str(r150["inventory_path"]),
            "sha256": sha256(r150["inventory_path"]),
            "file_record_count": len(r150["inventory"]["files"]),
            "selected_source_count": selected_count,
            "excluded_source_count": excluded_count,
            "accounted_source_count": coverage_count,
            "selected_objects_sha256": r150["seal"]["selected_objects_sha256"],
        },
        "selected_providers": {
            "selection_path": str(selection_path),
            "selection_sha256": sha256(selection_path),
            "selection_head": selection["head"],
            "cohort_count": len(cohort),
            "cohort_sha256": provider_cohort_sha,
            "verified_object_hashes": provider_hashes_ok,
            "cohort": cohort,
        },
        "final_xex": {
            "path": str(LINK / "native-game.exe"),
            "sha256": r150["image"]["image_sha256"],
            "bytes": r150["image"]["bytes"],
            "magic": r150["image"]["magic"],
            "xex_dump_sha256": r150["image"]["xex_dump_sha256"],
            "pe_sha256": receipt["pe_sha256"],
            "map_sha256": receipt["map_sha256"],
        },
        "sealed_link": {
            "result_path": str(r150["result_path"]),
            "result_sha256": sha256(r150["result_path"]),
            "exit_code": r150["result"]["exit_code"],
            "all_post_checks_pass": r150["result"]["all_post_checks_pass"],
            "seal_path": str(r150["seal_path"]),
            "seal_sha256": sha256(r150["seal_path"]),
            "root_release_sealed_sha256": r150["root_release"]["sealed_sha256"],
            "invocation_sha256": sha256(r150["invocation_path"]),
        },
        "r145_linked_verifier": {
            "path": str(LINKED_VERIFIER),
            "sha256": sha256(LINKED_VERIFIER),
            "r144_proof_path": str(R144_PROOF),
            "r144_proof_sha256": r144_proof_sha,
            "r145_linked_receipt_sha256": base_receipt_sha,
        },
        "prior_r149_evidence": {
            "receipt_path": str(r149_receipt_path),
            "receipt_sha256": sha256(r149_receipt_path),
            "head": r149_receipt["head"],
            "xex_sha256": r149_receipt["xex_sha256"],
            "seal_sha256": r149_receipt["seal_sha256"],
            "r144_proof_sha256": r149_receipt["r144_receipt_sha256"],
            "linked_check_group_counts": {
                name: len(entries)
                for name, entries in r149_receipt["selected_linked_checks"].items()
            },
        },
    }

    temp_path = SOURCE_RECEIPT.with_suffix(".json.tmp")
    temp_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    temp_path.replace(SOURCE_RECEIPT)
    print(json.dumps({
        "receipt": str(SOURCE_RECEIPT),
        "receipt_sha256": sha256(SOURCE_RECEIPT),
        "head": R150_HEAD,
        "tree": R150_TREE,
        "xex_sha256": receipt["xex_sha256"],
        "provider_count": len(cohort),
        "selected_source_count": selected_count,
        "excluded_source_count": excluded_count,
        "accounted_source_count": coverage_count,
        "linked_check_groups": {k: len(v) for k, v in receipt["selected_linked_checks"].items()},
    }, indent=2))


if __name__ == "__main__":
    main()
