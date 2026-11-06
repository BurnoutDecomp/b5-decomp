"""Common strict-link provenance checks for the R153 BootVideos/loading regressions."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TARGETS = json.loads((ROOT / "TARGETS.json").read_text(encoding="utf-8-sig"))
DEFAULT_HEAD = TARGETS["r153_head"].lower()


class LinkContextError(Exception):
    pass


def need(condition, message):
    if not condition:
        raise LinkContextError(message)


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest().upper()


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def verify_link_context(folder: Path, expected_head: str | None = None):
    folder = folder.resolve()
    expected_head = (expected_head or DEFAULT_HEAD).lower()
    required = ["source-inventory.json", "seal.json", "invocation.json", "ROOT_RELEASE.json",
                "result.json", "image-verification.json", "native-game.exe", "native-game.pe", "native-game.map"]
    for name in required:
        need((folder / name).is_file(), f"strict link is missing {name}")
    inventory_path = folder / "source-inventory.json"
    seal_path = folder / "seal.json"
    invocation_path = folder / "invocation.json"
    release_path = folder / "ROOT_RELEASE.json"
    result_path = folder / "result.json"
    image_path = folder / "image-verification.json"
    inventory = read(inventory_path)
    seal = read(seal_path)
    invocation = read(invocation_path)
    release = read(release_path)
    result = read(result_path)
    image = read(image_path)
    need(inventory.get("head", "").lower() == expected_head,
         f"source inventory HEAD {inventory.get('head')} does not match pinned {expected_head}")
    need(seal.get("head", "").lower() == expected_head,
         f"seal HEAD {seal.get('head')} does not match pinned {expected_head}")
    inventory_sha = sha(inventory_path)
    seal_sha = sha(seal_path)
    release_sha = sha(release_path)
    need(seal.get("source_inventory", {}).get("sha256", "").upper() == inventory_sha,
         "seal does not bind the current source inventory")
    prior_inventory_record = TARGETS["r152_source_inventory"]
    prior_inventory_path = Path(prior_inventory_record["original_path"])
    need(prior_inventory_path.is_file() and sha(prior_inventory_path) == prior_inventory_record["sha256"].upper(),
         "pinned R143 source inventory is missing or changed")
    prior_inventory = read(prior_inventory_path)
    need(prior_inventory.get("head", "").lower() == TARGETS["r152_baseline_head"].lower(),
         "pinned R152 source inventory has the wrong HEAD")
    current_source_records = inventory.get("files", [])
    source_comparison = []
    for relative, prior_sha in TARGETS["r152_source_files"].items():
        suffix = relative.replace("\\", "/").lower()
        rows = [row for row in current_source_records
                if row.get("path", "").replace("\\", "/").lower().endswith(suffix)]
        need(len(rows) == 1, f"source inventory must contain exactly one {relative}; got {len(rows)}")
        current_sha = rows[0].get("sha256", "").upper()
        source_comparison.append({"path": relative, "r152_sha256": prior_sha.upper(),
                                  "r153_sha256": current_sha, "unchanged": current_sha == prior_sha.upper()})
    hashstring_comparison = []
    for relative, prior_sha in TARGETS["r152_hashstring_files"].items():
        suffix = relative.replace("\\", "/").lower()
        rows = [row for row in current_source_records
                if row.get("path", "").replace("\\", "/").lower().endswith(suffix)]
        need(len(rows) == 1, f"source inventory must contain exactly one {relative}; got {len(rows)}")
        current_sha = rows[0].get("sha256", "").upper()
        hashstring_comparison.append({"path": relative, "r152_sha256": prior_sha.upper(),
                                      "r153_sha256": current_sha, "unchanged": current_sha == prior_sha.upper()})
    name_header_record = TARGETS["r152_name_header"]
    name_suffix = name_header_record["path"].replace("\\", "/").lower()
    name_rows = [row for row in current_source_records
                 if row.get("path", "").replace("\\", "/").lower().endswith(name_suffix)]
    need(len(name_rows) == 1, f"source inventory must contain exactly one {name_header_record['path']}; got {len(name_rows)}")
    name_header_comparison = {"path": name_header_record["path"], "r152_sha256": name_header_record["sha256"].upper(),
                              "r153_sha256": name_rows[0].get("sha256", "").upper(),
                              "unchanged": name_rows[0].get("sha256", "").upper() == name_header_record["sha256"].upper()}
    need(invocation.get("sealed_sha256", "").upper() == seal_sha,
         "invocation does not bind the current seal")
    need(invocation.get("release_sha256", "").upper() == release_sha,
         "invocation does not bind the current root release")
    need(release.get("sealed_sha256", "").upper() == seal_sha,
         "root release does not bind the current seal")
    need(result.get("exit_code") == 0 and result.get("all_post_checks_pass") is True,
         "link result is not complete with all post checks passing")
    outputs = result.get("outputs", {})
    bound = {}
    for name in ("native-game.exe", "native-game.pe", "native-game.map"):
        path = folder / name
        rows = [entry for output, entry in outputs.items() if Path(output).name.lower() == name]
        need(len(rows) == 1, f"result.json must bind exactly one {name}")
        row = rows[0]
        actual_sha = sha(path)
        need(path.stat().st_size == row.get("size") and actual_sha == row.get("sha256", "").upper(),
             f"{name} differs from result.json")
        bound[name] = {"size": path.stat().st_size, "sha256": actual_sha}
    image_sha = sha(image_path)
    image_file = Path(image.get("image", ""))
    need(image.get("head", "").lower() == expected_head,
         f"image verification HEAD {image.get('head')} does not match pinned {expected_head}")
    need(image_file.resolve() == (folder / "native-game.exe").resolve(),
         "image verification refers to a different executable")
    need(image.get("image_sha256", "").upper() == bound["native-game.exe"]["sha256"],
         "image verification digest differs from result-bound executable")
    need(image.get("magic") == "XEX2", "linked image verification does not identify an XEX2 image")
    predecessor_receipts = {}
    for name, record in TARGETS["r141_linked_witnesses"].items():
        witness = ROOT / record["witness_copy"]
        need(witness.is_file() and sha(witness) == record["sha256"].upper(),
             f"copied R141 linked witness receipt changed: {witness}")
    for name, record in TARGETS["previous_linked_receipts"].items():
        witness = ROOT / record["witness_copy"]
        need(witness.is_file() and sha(witness) == record["sha256"].upper(),
             f"copied R152 predecessor receipt changed: {witness}")
        predecessor_receipts[name] = record["sha256"].upper()
    return {"head": expected_head, "source_inventory_sha256": inventory_sha,
            "seal_sha256": seal_sha, "invocation_sha256": sha(invocation_path),
            "root_release_sha256": release_sha, "result_sha256": sha(result_path),
            "image_verification_sha256": image_sha, "xex_sha256": bound["native-game.exe"]["sha256"],
            "outputs": bound, "predecessor_r152_receipts": predecessor_receipts,
            "source_comparison_vs_r152": source_comparison,
            "hashstring_source_comparison_vs_r152": hashstring_comparison,
            "name_header_comparison_vs_r152": name_header_comparison,
            "source_line_change_note": ("BootVideos, main loading entry, bundle-header, and HashString sources are unchanged from R152."
                                        if all(row["unchanged"] for row in source_comparison)
                                        else "Selected source inventory hashes changed from R152; inspect those exact paths for line-level differences.")}
