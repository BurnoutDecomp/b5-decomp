#!/usr/bin/env python3
"""Carry forward the R141 loading-entry and bundle-header linked PPC witnesses."""
from __future__ import annotations
import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "boot-videos"))
import verify_post_link as ppc
from link_context import DEFAULT_HEAD, TARGETS, verify_link_context

WITNESS = HERE / "R141_LOADING_BUNDLE_LINKED.json"
RECEIPT_KEY = "R141_LOADING_BUNDLE_LINKED.json"
FUNCTIONS = {
    "initial_loading_entry": ("src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp",
                              "?OnEnter@MainGameFlowStateInitialLoadingScreen@@UAAXXZ",
                              "BrnGameMainFlowStates-d851f47b.obj"),
    "complete_loading_entry": ("src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp",
                                "?OnEnter@MainGameFlowStateCompleteLoading@@UAAXXZ",
                                "BrnGameMainFlowStates-d851f47b.obj"),
    "bundle_header": ("src/GameShared/GameClasses/System/Resource/CgsBundleLoaderModule.cpp",
                      "?ProcessBundleHeader@BundleLoaderModule@CgsResource@@QAAXXZ",
                      "CgsBundleLoaderModule-04bbc43c.obj"),
}


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    return ppc.sha256(path)


def semantic_relocations(records, function_section):
    answer = []
    for r in records:
        section = r["target_section"]
        relation = "self" if section == function_section else ("external" if section == 0 else "defined")
        answer.append({"offset": r["offset"], "type": r["type"],
                       "symbol": ppc.canonical_symbol(r["symbol"]),
                       "target_relation": relation, "target_value": r["target_value"],
                       "storage_class": r["storage_class"]})
    return sorted(answer, key=lambda x: (x["offset"], x["type"], x["symbol"]))


def selected_object(invocation, source, link_dir):
    rows = [x for x in invocation.get("objects", []) if x.get("source", "").replace("\\", "/").lower() == source.lower()]
    need(len(rows) == 1, f"invocation must select exactly one object for {source}; got {len(rows)}")
    row = rows[0]
    path = Path(row["object"])
    if not path.is_absolute():
        path = (link_dir / path).resolve()
    need(path.is_file(), f"invocation-selected object is missing: {path}")
    actual = sha(path)
    need(row.get("sha256", "").upper() == actual, f"invocation-selected object SHA mismatch for {source}")
    return row, path, actual


def verify_one(name, record, prior_r146_record, link_dir, invocation, map_symbols, image):
    source, symbol, basename = FUNCTIONS[name]
    input_info = TARGETS["r141_supporting_witnesses"][
        "loading_initial_and_complete_object" if basename.startswith("BrnGame") else "bundle_header_object"]
    witness_path = HERE / "witness-objects" / basename
    need(witness_path.is_file() and sha(witness_path) == input_info["sha256"],
         f"copied R141 witness object changed: {witness_path}")
    need(record["object_sha256"].upper() == input_info["sha256"], f"R141 linked witness object hash differs for {name}")
    source_row, selected_path, selected_hash = selected_object(invocation, source, link_dir)

    witness_obj = ppc.Coff(witness_path)
    selected_obj = ppc.Coff(selected_path)
    witness_symbol, witness_section, witness_relocs = witness_obj.function(symbol)
    current_symbol, current_section, current_relocs = selected_obj.function(symbol)
    need(len(witness_section["bytes"]) == record["body_bytes"], f"R141 witness body size differs for {name}")
    need(current_section["size"] == record["body_bytes"], f"R143 object body size differs for {name}")
    expected_calls = {str(k): v for k, v in record["direct_calls"].items()}
    actual_calls = {str(r["offset"]): r["symbol"] for r in current_relocs if r["type"] == 0x06}
    need(actual_calls == expected_calls, f"R143 direct-call map differs for {name}: expected {expected_calls}, got {actual_calls}")
    need(semantic_relocations(witness_relocs, witness_symbol["section"]) ==
         semantic_relocations(current_relocs, current_symbol["section"]),
         f"R143 semantic relocations differ from the R141 witness for {name}")

    witness_norm = ppc.normalize_code(witness_section["bytes"], witness_relocs)
    object_norm = ppc.normalize_code(current_section["bytes"], current_relocs)
    need(object_norm == witness_norm, f"R143 object PPC differs from R141 for {name}: {ppc.first_word_difference(witness_norm, object_norm)}")
    mapped = ppc.map_symbol(map_symbols, symbol)
    need(Path(mapped["object"] or "").name.lower() == selected_path.name.lower(),
         f"map owner for {symbol} does not match the invocation-selected object")
    linked = image.bytes_at_va(mapped["va"], record["body_bytes"])
    linked_norm = ppc.normalize_code(linked, current_relocs)
    need(linked_norm == witness_norm, f"linked PE PPC differs from R141 for {name}: {ppc.first_word_difference(witness_norm, linked_norm)}")
    function = {"symbol": symbol, "section": current_symbol["section"],
                "relocations": ppc.relocation_manifest(current_relocs)}
    relocation_proof = ppc.verify_linked_relocations(function, linked, mapped["va"], map_symbols, image)
    return {"artist_va": record["artist"], "r142_linked_address": record["linked_address"],
            "r143_linked_address": prior_r146_record["r143_linked_address"],
            "r144_linked_address": prior_r146_record["r144_linked_address"],
            "r145_linked_address": prior_r146_record["r145_linked_address"],
            "r146_linked_address": prior_r146_record["r146_linked_address"],
            "r147_linked_address": f"0x{mapped['va']:08X}", "symbol": symbol,
            "source": source, "selected_object": str(selected_path), "selected_object_sha256": selected_hash,
            "r141_witness_object": str(witness_path), "r141_witness_object_sha256": sha(witness_path),
            "body_bytes": record["body_bytes"],
            "r141_normalized_ppc_sha256": ppc.hashlib.sha256(witness_norm).hexdigest().upper(),
            "r143_normalized_ppc_sha256": ppc.hashlib.sha256(linked_norm).hexdigest().upper(),
            "matches_r141_ppc": True, "direct_calls": actual_calls,
            "linked_relocations": relocation_proof}


def verify(link_dir: Path, expected_head: str = DEFAULT_HEAD):
    link_dir = link_dir.resolve()
    context = verify_link_context(link_dir, expected_head)
    prior = json.loads(WITNESS.read_text(encoding="utf-8-sig"))
    prior_hash = sha(WITNESS)
    expected_prior_hash = TARGETS["r141_linked_witnesses"][RECEIPT_KEY]["sha256"]
    need(prior_hash == expected_prior_hash, "copied R141_LOADING_BUNDLE_LINKED witness changed")
    prior_r146_path = ROOT / TARGETS["previous_linked_receipts"][RECEIPT_KEY]["witness_copy"]
    prior_r146 = json.loads(prior_r146_path.read_text(encoding="utf-8-sig"))
    prior_r146_hash = sha(prior_r146_path)
    need(prior_r146_hash == TARGETS["previous_linked_receipts"][RECEIPT_KEY]["sha256"],
         "copied R146 loading/bundle predecessor receipt changed")
    invocation = json.loads((link_dir / "invocation.json").read_text(encoding="utf-8-sig"))
    image = ppc.PeImage(link_dir / "native-game.pe")
    map_symbols = ppc.read_map(link_dir / "native-game.map")
    proofs = {name: verify_one(name, prior["records"][name], prior_r146["records"][name],
                               link_dir, invocation, map_symbols, image)
              for name in FUNCTIONS}
    return {"scope": "linked PPC", "link_context": context,
            "prior_r141_linked_witness_sha256": prior_hash,
            "prior_r146_linked_receipt_sha256": prior_r146_hash,
            "records": proofs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path)
    parser.add_argument("--expected-head", default=DEFAULT_HEAD)
    parser.add_argument("--report", type=Path, help="write the successful linked receipt to a new file (never overwrites)")
    args = parser.parse_args()
    try:
        report = verify(args.link, args.expected_head)
        if args.report:
            path = args.report.resolve()
            need(path.parent.is_dir(), f"report directory does not exist: {path.parent}")
            need(not path.exists(), f"refusing to overwrite existing receipt: {path}")
            report["linked_receipt_path"] = str(path)
            path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
        return 0
    except Exception as exc:
        print(json.dumps({"result": "fail", "error": str(exc)}, indent=2), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
