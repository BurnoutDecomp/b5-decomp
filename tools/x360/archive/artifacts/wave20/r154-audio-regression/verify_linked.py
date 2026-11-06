"""Carry the sealed R153 audio machine proof into R154."""
import argparse
import json
from pathlib import Path
import runpy
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
BASE = "bcc3e02a41cdb7bba938b91d7ab638c46962540d"
BASE_TREE = "610a39be8529a22eeb2d463669c53ede9e46a2fe"
BASE_SEAL = "dd7342db08256ff076090eaa92f40752d2003d490c5d91a06f37a34fa6175c11"
R153 = WAVE / "r153-audio-regression/verify_linked.py"
R153_SHA = "dd7672d849f6006552b26eea6bbd99bcfc404a3b3abcb00fe1224ab8b04db428"
R153_PROOF = R153.with_name("R153_AUDIO_LINKED.json")
R153_PROOF_SHA = "13e626a91846197aa18bc494deb1d04e6afb3c3aa029aaa08851abba871c3412"
NAME_HEADER = "src/GameShared/GameClasses/Sound/Playback/CgsCommon.h"


def seal_inputs(args, front, h, previous, frozen):
    link = args.link
    load = lambda name: front["load"](link / name)
    result, image, invocation = (load(name) for name in ("result.json", "image-verification.json", "invocation.json"))
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] and image["head"] == args.head
    seal_path, release_path = (link / name for name in ("seal.json", "ROOT_RELEASE.json"))
    assert Path(invocation["sealed"]) == seal_path and invocation["sealed_sha256"] == args.seal_sha
    assert Path(invocation["release"]) == release_path
    seal = front["load"](seal_path, args.seal_sha)
    assert (seal["schema"], seal["head"]) == ("r154-native-link-seal-v1", args.head)
    assert front["load"](release_path, invocation["release_sha256"]) == {
        "action": "one strict R154 native link", "sealed_sha256": args.seal_sha
    }
    for key in ("sources", "libraries", "tool_files", "excluded"):
        assert invocation[key] == seal[key]
    selected = seal["selected"]
    assert invocation["objects"] == selected and (len(selected), len(seal["excluded"])) == (3589, 121)
    rsp = link / "r154-strict.rsp"
    assert invocation["command"][1:] == ["@" + str(rsp)]
    arguments = front["pinned"](rsp, invocation["rsp_sha256"]).decode().splitlines()
    assert [line for line in arguments if line.lower().endswith('.obj"')] == [f'"{row["object"]}"' for row in selected]
    order = sorted((h.norm(row["source"]), h.norm(row["object"]), row["sha256"]) for row in selected)
    digest = h.sha(json.dumps(order, separators=(",", ":")).encode())
    assert digest == seal["selected_objects_sha256"]
    assert h.file_sha(link / "link.log") == result["log_sha256"]
    diagnostics = load("diagnostic-parsed.json")
    assert diagnostics["unique_symbols"] == diagnostics["unresolved_messages"] == 0 and not diagnostics["duplicate_messages"]
    boundary = front["from_pin"](seal["boundary"])
    assert (boundary["schema"], boundary["head"], boundary["selected_objects_sha256"]) == (
        "r154-link-boundary-v1", args.head, digest
    )
    assert boundary["no_excluded_cohort_outside_references"] and not boundary["errors"]
    manifest = front["load"](args.manifest, args.manifest_sha)
    assert (manifest["base"], manifest["base_tree"], manifest["projected_tree"]) == (BASE, BASE_TREE, args.tree)
    assert seal["prior_successful_seal"] == manifest["base_seal"] == previous["seal_inputs"]["seal"] == {
        "path": str(WAVE / "r153-link/seal.json"), "sha256": BASE_SEAL
    }
    prior = front["from_pin"](seal["prior_successful_seal"])
    cohort = h.pinned_json(seal["provider_selection"], h.PROVIDER_SHA)["sources"]
    assert prior["head"] == BASE and cohort == seal["provider_cohort"] == prior["provider_cohort"] and len(cohort) == 15
    for key in ("selected", "excluded"):
        assert {row["source"] for row in seal[key]} == {row["source"] for row in prior[key]}
    excluded = {row["source"]: row for row in seal["excluded"]}
    for member in cohort:
        row = excluded[member["source"]]
        assert (row["object"], row["sha256"]) == (member["object"], member["sha256"])
        front["pinned"](row["object"], row["sha256"])

    root = Path(seal["source_root"])
    assert h.git(root, "rev-parse", f"{args.head}^{{tree}}", f"{args.head}^").decode().split() == [args.tree, BASE]
    assert h.git(root, "rev-parse", f"{manifest['candidate']}^{{tree}}").decode().strip() == args.tree
    binding = next(row for row in seal["provenance"] if Path(row["path"]) == args.intake / "commit-binding.json")
    commit = front["from_pin"](binding)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (args.head, BASE, args.manifest_sha)
    changed = h.git(root, "diff", "--name-only", BASE, args.head).decode().splitlines()
    assert sorted(changed) == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    inventory = front["from_pin"](seal["source_inventory"])
    assert inventory["head"] == args.head and inventory["files"] == seal["sources"]
    report = front["from_pin"](seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    objects = {row["source"]: row for row in selected}
    old_objects = {row["source"]: row for row in prior["selected"]}
    report_rows = {row["source"]: row for row in report["results"]}
    closure = frozen["seal_inputs"]["name_header_native_closure"]
    observed = frozen["seal_inputs"]["compiler_observed_name_consumers"]
    assert len(closure) == previous["name_dependency_closure"]["retained_selected_rows"] == 182
    assert len(observed) == 181 and set(observed) < set(closure)
    header_before, header_now = h.git(root, "rev-parse", f"{BASE}:{NAME_HEADER}", f"{args.head}:{NAME_HEADER}").decode().split()
    assert header_before == header_now == previous["name_dependency_closure"]["name_header_git_blob"]
    rebuilds = [path for path in closure if objects[path] != old_objects[path]]
    if rebuilds:
        include_path = args.intake / "actual-includes.json"
        pin = next(row for row in seal["provenance"] if Path(row["path"]) == include_path)
        compiler = front["from_pin"](pin)
        assert compiler["head"] == args.head
        inventory_by_path = {h.norm(row["path"]): row["sha256"] for row in seal["sources"]}
        header_hash = inventory_by_path[h.norm(root / NAME_HEADER)]
        for source in rebuilds:
            row = report_rows[source]
            assert row["passed"] and row["machine"] == 0x1f2 and row["object"] == objects[source]["object"]
            assert Path(row["object"]).parent == args.intake / "native-selected"
            source_includes = {h.norm(path): value for path, value in compiler["rows"][source].items()}
            if source in observed:
                assert source_includes[h.norm(root / NAME_HEADER)] == header_hash
    h.LINK = link
    read, entries, image_pins = h.load_image(result, image)
    for kind in ("xex", "pe", "map"):
        assert image_pins[kind + "_sha256"] == getattr(args, kind + "_sha")
    return {
        "read": read, "entries": entries, "image": image_pins, "root": root, "objects": objects,
        "sources": {h.norm(row["path"]): row["sha256"] for row in seal["sources"]},
        "report": report_rows, "old_objects": old_objects,
        "pins": {"seal": front["pin"](seal_path), "result": front["pin"](link / "result.json"),
                 "image_verification": front["pin"](link / "image-verification.json"), "release": front["pin"](release_path),
                 "response": front["pin"](rsp), "commit_binding": binding, "source_inventory": seal["source_inventory"],
                 "boundary": seal["boundary"], "report": seal["report"], "prior_seal": seal["prior_successful_seal"],
                 "provider_selection": seal["provider_selection"], "selected_objects_sha256": digest},
        "closure": {"predecessor": previous["name_dependency_closure"], "name_header_git_blob": header_now,
                    "native_owners": len(closure), "same_selected_rows": len(closure) - len(rebuilds), "rebuilt": rebuilds},
    }


def measure_rebuilds(inputs, previous, inherited, h):
    """Measure exact machine and canonical relocation deltas before rebinding."""
    old_by_name = {Path(row["object"]).name: row for row in inputs["old_objects"].values()}
    measurements, changed_methods = {}, {}
    functions = dict(previous["functions"])
    callee = previous["direct_notify_result"]["callee"]
    functions["direct_notify_provider"] = callee | {"provider": previous["functions"]["music_notify"]["provider"]}
    for method, check in functions.items():
        old_row = old_by_name[check["provider"]]
        source = old_row["source"]
        new_row = inputs["objects"][source]
        if old_row == new_row:
            continue
        old = h.Coff(h.pinned(old_row["object"], old_row["sha256"]))
        new = h.Coff(h.pinned(new_row["object"], new_row["sha256"]))
        parse = inherited["format_function"] if method in ("hud_cos_init", "hud_scale_init") else lambda coff, symbol: coff.function(symbol)
        try:
            old_symbol, a, a_refs = parse(old, check["symbol"])
            new_symbol, b, b_refs = parse(new, check["symbol"])
        except (ValueError, AssertionError) as error:
            changed_methods[method] = {"provider_symbol": check["symbol"], "error": str(error)}
            continue
        a_signature = h.ref_signature(old_symbol, a_refs)
        b_signature = h.ref_signature(new_symbol, b_refs)
        # R152 explicitly proved these assertion-header path operands. R153 and
        # R154 use the same root so the full canonical signatures must agree.
        code_offsets = [hex(i) for i in range(0, min(len(a), len(b)), 4) if a[i:i+4] != b[i:i+4]]
        relocation_offsets = {hex(i): {"r153": a_signature.get(i), "r154": b_signature.get(i)}
                              for i in sorted(a_signature.keys() | b_signature.keys()) if a_signature.get(i) != b_signature.get(i)}
        if code_offsets or relocation_offsets or len(a) != len(b):
            changed_methods[method] = {"source": source, "bytes": [len(a), len(b)],
                                       "instruction_offsets": code_offsets, "relocations": relocation_offsets}
        else:
            measurements.setdefault(source, {"r153_object": old_row, "r154_object": new_row, "identical_functions": []})
            measurements[source]["identical_functions"].append(method)
    assert not changed_methods, json.dumps(changed_methods, sort_keys=True)
    return measurements


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    for key in ("head", "tree", "manifest-sha", "seal-sha", "xex-sha", "pe-sha", "map-sha"):
        cli.add_argument("--" + key, required=True)
    cli.add_argument("--link", type=Path, default=WAVE / "r154-link")
    cli.add_argument("--intake", type=Path, default=WAVE / "r154-intake")
    cli.add_argument("--manifest", type=Path, default=WAVE / "r154-batch/MANIFEST.json")
    cli.add_argument("--write", action="store_true")
    args = cli.parse_args()
    predecessor = runpy.run_path(str(R153))
    old = runpy.run_path(str(predecessor["PRIOR_VERIFIER"]))
    inherited, voice, front, h, older = old["helpers"]()
    front["pinned"](R153, R153_SHA)
    front["pinned"](predecessor["PRIOR_VERIFIER"], predecessor["PRIOR_VERIFIER_SHA"])
    previous = front["load"](R153_PROOF, R153_PROOF_SHA)
    frozen = front["load"](predecessor["PRIOR_PROOF"], predecessor["PRIOR_PROOF_SHA"])
    assert (previous["head"], previous["tree"], previous["xex_sha256"]) == (
        BASE, BASE_TREE, predecessor["IMAGE_SHA"]["xex_sha256"]
    )
    assert previous["predecessor"]["sha256"] == predecessor["PRIOR_PROOF_SHA"]
    inputs = seal_inputs(args, front, h, previous, frozen)
    rebuilds = measure_rebuilds(inputs, previous, inherited, h)
    old_by_name = {Path(row["object"]).name: source for source, row in inputs["old_objects"].items()}
    scope = {old_by_name[row["provider"]] for row in previous["functions"].values()}
    for source in scope:
        proof = frozen["selected"][source]
        blob = h.git(inputs["root"], "rev-parse", f"{args.head}:{source}").decode().strip()
        assert blob == proof["blob"], source
    prev_args = SimpleNamespace(manifest=Path(frozen["manifest"]["path"]), manifest_sha=frozen["manifest"]["sha256"],
                                targeted=Path(frozen["targeted"]["path"]), targeted_sha=frozen["targeted"]["sha256"],
                                candidate=frozen["candidate"], tree=frozen["tree"])
    _, rows = old["frozen_inputs"](prev_args, front, h)
    rows = {source: row for source, row in rows.items() if source in scope}
    old["bind"].__globals__["LINK"] = args.link
    functions, owners, retained, initializers, entries = old["bind"](
        SimpleNamespace(head=args.head), inherited, voice, front, h, older, rows, inputs
    )
    assert functions.keys() == previous["functions"].keys() and len(functions) == 60
    assert retained == frozen["retained_r151_bodies"] and len(retained) == 32
    proofs = {}
    for name, actual in functions.items():
        before = previous["functions"][name]
        for field in ("symbol", "compiled_sha256", "bytes", "provider"):
            assert actual[field] == before[field], (name, field)
        proofs[name] = {key: actual[key] for key in ("symbol", "address", "bytes", "sha256", "compiled_sha256", "provider")}
        proofs[name]["linked_relocations_sha256"] = h.sha(json.dumps(actual["bindings"], sort_keys=True).encode())
    witnesses = old["linked_witnesses"](inputs, functions, entries, h)
    predecessor["notify_result"].__globals__["LINK"] = args.link
    notify = predecessor["notify_result"](front, h, inputs, functions, owners, entries, old["data_operand"])
    output = {
        "state": "pass", "head": args.head, "tree": args.tree, "base": BASE, "xex_sha256": inputs["image"]["xex_sha256"],
        "image": inputs["image"], "manifest": front["pin"](args.manifest), "verifier": front["pin"](Path(__file__)),
        "predecessor": front["pin"](R153_PROOF), "seal_inputs": inputs["pins"], "name_closure": inputs["closure"],
        "rebuilt_scope_objects": rebuilds,
        "selected_scope_objects": {source: pin for source, (_, _, pin) in owners.items()}, "functions": proofs, "witnesses": witnesses, "direct_notify_result": notify,
        "private_initializers": {group: {name: row["sha256"] for name, row in records.items()}
                                 for group, records in initializers.items()},
        "scope": "Static R154 linked audio regression. The previously recorded Send01 provider difference and runtime are outside scope.",
    }
    if args.write:
        path = HERE / "R154_AUDIO_LINKED.json"
        with path.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(output, stream, indent=2)
            stream.write("\n")
        print(front["pin"](path))
    print(f"PASS {args.head}: {len(proofs)} audio bodies, {len(rebuilds)} rebuilt scope objects")


if __name__ == "__main__":
    main()
