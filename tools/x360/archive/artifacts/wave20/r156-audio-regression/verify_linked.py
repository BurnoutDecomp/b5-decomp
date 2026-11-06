"""Rebind the proven R155 audio and Name ABI evidence to the exact R156 link.

Read-only inputs: the R155 audio receipt, R156 intake-v2 receipts, and sealed
R156 linked PE/XEX/map/object artifacts. --write writes only this directory.
"""
import argparse
import json
from pathlib import Path
import runpy
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent

HEAD = "8157850d32d5d15c6187b22969e939f5ca7c201d"
TREE = "f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a"
BASE = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
BASE_TREE = "6bdcd7954fa913359b809b7506984f0dc05d90ee"
SEAL_SHA = "6376515b0c2ab708d8cd8e6d3ec4eeb635826fa8a7f40bbee77799ec8c43f1c4"
XEX_SHA = "e1dd2fa9321932520dc561f491eb9e32f19029c6cd599a38f0a8a1302a083ed2"
PE_SHA = "d75e7d429422a174df8788ba55bd979460a348ebd7f18339e65a54225a3873c8"
MAP_SHA = "1dfdb006eff111042c95e4ce77de3d2c8e605c1fa748540c0c737189c0f8688e"

LINK = WAVE / "r156-link"
INTAKE = WAVE / "r156-intake-v2"
MANIFEST = WAVE / "r156-batch-v2/MANIFEST.json"
MANIFEST_SHA = "15a48e7ce35c95d3cdf41803445602aa9c013480dafe2109472be9fa61e94b29"
NAME_HEADER = "src/GameShared/GameClasses/Sound/Playback/CgsCommon.h"

R155_VERIFIER = WAVE / "r155-audio-regression/verify_linked.py"
R155_VERIFIER_SHA = "63c1001e92816722f211971726cce43d56f3dd02dc608f2bace7ea50f517a289"
R155_PROOF = R155_VERIFIER.with_name("R155_AUDIO_LINKED.json")
R155_PROOF_SHA = "b17562795c2db653f8212804e505a08ff7cc089c36139ccd3657000fda097a8b"


def provenance(seal, path):
    path = Path(path)
    matches = [row for row in seal["provenance"] if Path(row["path"]) == path]
    assert len(matches) == 1, (str(path), len(matches))
    return matches[0]


def canonical_bindings(row):
    return {offset: (binding["symbol"], binding["kind"])
            for offset, binding in row["bindings"].items()}


def replay_r155(r155, carrier, predecessor, old, inherited, voice, front, h, older, proof):
    """Re-run the static R155 binding so the source receipt is itself checked."""
    r154_proof = front["load"](r155["R154_PROOF"], r155["R154_PROOF_SHA"])
    r152_proof = front["load"](predecessor["PRIOR_PROOF"], predecessor["PRIOR_PROOF_SHA"])
    assert proof["predecessor"] == front["pin"](r155["R154_PROOF"])
    assert (proof["head"], proof["tree"]) == ("1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1", r155["TREE"])

    r155_seal_pin = proof["seal_inputs"]["seal"]
    r155_link = Path(r155_seal_pin["path"]).parent
    r155_args = SimpleNamespace(
        head=proof["head"],
        tree=proof["tree"],
        manifest=Path(proof["manifest"]["path"]),
        manifest_sha=proof["manifest"]["sha256"],
        intake=Path(proof["seal_inputs"]["commit_binding"]["path"]).parent,
        link=r155_link,
        seal_sha=r155_seal_pin["sha256"],
        xex_sha=proof["image"]["xex_sha256"],
        pe_sha=proof["image"]["pe_sha256"],
        map_sha=proof["image"]["map_sha256"],
    )
    h.LINK = r155_link
    r155_inputs = r155["seal_inputs"](r155_args, front, h, r154_proof, r152_proof)
    rebuilt = carrier["measure_rebuilds"](r155_inputs, r154_proof, inherited, h)
    assert rebuilt == proof["rebuilt_scope_objects"]

    old_by_name = {Path(row["object"]).name: source
                   for source, row in r155_inputs["old_objects"].items()}
    scope = {old_by_name[row["provider"]]
             for row in r154_proof["functions"].values()}
    assert scope == set(proof["selected_scope_objects"])
    old_args = SimpleNamespace(
        manifest=Path(r152_proof["manifest"]["path"]),
        manifest_sha=r152_proof["manifest"]["sha256"],
        targeted=Path(r152_proof["targeted"]["path"]),
        targeted_sha=r152_proof["targeted"]["sha256"],
        candidate=r152_proof["candidate"],
        tree=r152_proof["tree"],
    )
    _, rows = old["frozen_inputs"](old_args, front, h)
    rows = {source: row for source, row in rows.items() if source in scope}
    assert rows.keys() <= scope

    old["bind"].__globals__["LINK"] = r155_link
    functions, owners, retained, initializers, entries = old["bind"](
        SimpleNamespace(head=proof["head"]),
        inherited, voice, front, h, older, rows, r155_inputs
    )
    assert functions.keys() == proof["functions"].keys() and len(functions) == 60
    assert retained == r152_proof["retained_r151_bodies"] and len(retained) == 32
    assert {source: pin for source, (_, _, pin) in owners.items()} == \
        proof["selected_scope_objects"]

    for name, actual in functions.items():
        expected = proof["functions"][name]
        for field in ("symbol", "compiled_sha256", "bytes", "provider"):
            assert actual[field] == expected[field], (name, field)
        assert h.sha(json.dumps(actual["bindings"], sort_keys=True).encode()) == \
            expected["linked_relocations_sha256"], name

    witnesses = old["linked_witnesses"](r155_inputs, functions, entries, h)
    predecessor["notify_result"].__globals__["LINK"] = r155_link
    notify = predecessor["notify_result"](
        front, h, r155_inputs, functions, owners, entries, old["data_operand"]
    )
    assert witnesses == proof["witnesses"]
    assert notify == proof["direct_notify_result"]
    current_initializers = {
        group: {name: row["sha256"] for name, row in values.items()}
        for group, values in initializers.items()
    }
    assert current_initializers == proof["private_initializers"]
    return {
        "inputs": r155_inputs,
        "scope": scope,
        "rows": rows,
        "functions": functions,
        "owners": owners,
        "entries": entries,
        "witnesses": witnesses,
        "notify": notify,
        "initializers": initializers,
        "proof": r152_proof,
    }


def load_r156_link(front, h, predecessor_pin):
    result_path = LINK / "result.json"
    image_path = LINK / "image-verification.json"
    invocation_path = LINK / "invocation.json"
    seal_path = LINK / "seal.json"
    release_path = LINK / "ROOT_RELEASE.json"
    rsp_path = LINK / "r156-strict.rsp"

    result = front["load"](result_path)
    image = front["load"](image_path)
    invocation = front["load"](invocation_path)
    seal = front["load"](seal_path, SEAL_SHA)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert (seal["schema"], seal["head"]) == ("r156-native-link-seal-v1", HEAD)
    assert image["head"] == HEAD
    assert Path(invocation["sealed"]) == seal_path and invocation["sealed_sha256"] == SEAL_SHA
    assert Path(invocation["release"]) == release_path
    assert invocation["command"][1:] == ["@" + str(rsp_path)]
    assert seal["prior_successful_seal"] == predecessor_pin

    release = front["load"](release_path, invocation["release_sha256"])
    assert release == {"action": "one strict R156 native link", "sealed_sha256": SEAL_SHA}

    for key in ("sources", "libraries", "tool_files", "excluded"):
        assert invocation[key] == seal[key]
    selected = seal["selected"]
    assert invocation["objects"] == selected
    assert (len(selected), len(seal["excluded"])) == (3589, 121)

    arguments = front["pinned"](rsp_path, invocation["rsp_sha256"]).decode().splitlines()
    object_args = [line for line in arguments if line.lower().endswith('.obj"')]
    assert object_args == [f'"{row["object"]}"' for row in selected]
    order = sorted((h.norm(row["source"]), h.norm(row["object"]), row["sha256"])
                   for row in selected)
    selected_digest = h.sha(json.dumps(order, separators=(",", ":")).encode())
    assert selected_digest == seal["selected_objects_sha256"]
    assert h.file_sha(LINK / "link.log") == result["log_sha256"]

    diagnostics = front["load"](LINK / "diagnostic-parsed.json")
    assert diagnostics["unique_symbols"] == diagnostics["unresolved_messages"] == 0
    assert not diagnostics["duplicate_messages"]

    boundary = front["from_pin"](seal["boundary"])
    assert (boundary["schema"], boundary["head"], boundary["selected_objects_sha256"]) == (
        "r156-link-boundary-v1", HEAD, selected_digest
    )
    assert boundary["no_excluded_cohort_outside_references"] and not boundary["errors"]

    inventory = front["from_pin"](seal["source_inventory"])
    assert inventory["head"] == HEAD and inventory["files"] == seal["sources"]
    report = front["from_pin"](seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    assert all(row["passed"] and row["machine"] == 0x1f2 for row in report["results"])

    root = Path(seal["source_root"])
    assert h.git(root, "rev-parse", f"{HEAD}^{{tree}}", f"{HEAD}^").decode().split() == [
        TREE, BASE
    ]
    manifest = front["load"](MANIFEST, MANIFEST_SHA)
    assert (manifest["base"], manifest["base_tree"], manifest["projected_tree"]) == (
        BASE, BASE_TREE, TREE
    )
    assert manifest["base_seal"] == predecessor_pin
    assert h.git(root, "rev-parse", f"{manifest['candidate']}^{{tree}}").decode().strip() == TREE

    commit_pin = provenance(seal, INTAKE / "commit-binding.json")
    commit = front["from_pin"](commit_pin)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (
        HEAD, BASE, MANIFEST_SHA
    )
    changed = h.git(root, "diff", "--name-only", BASE, HEAD).decode().splitlines()
    assert sorted(changed) == sorted(commit["paths"]) == \
        sorted(row["path"] for row in manifest["final_paths"])

    h.LINK = LINK
    read, entries, image_pins = h.load_image(result, image)
    assert {
        "xex_sha256": image_pins["xex_sha256"],
        "pe_sha256": image_pins["pe_sha256"],
        "map_sha256": image_pins["map_sha256"],
    } == {"xex_sha256": XEX_SHA, "pe_sha256": PE_SHA, "map_sha256": MAP_SHA}

    inputs = {
        "read": read,
        "entries": entries,
        "image": image_pins,
        "root": root,
        "sources": {h.norm(row["path"]): row["sha256"] for row in seal["sources"]},
        "objects": {row["source"]: row for row in selected},
        "report": {row["source"]: row for row in report["results"]},
    }
    pins = {
        "seal": front["pin"](seal_path),
        "result": front["pin"](result_path),
        "image_verification": front["pin"](image_path),
        "release": front["pin"](release_path),
        "response": front["pin"](rsp_path),
        "boundary": seal["boundary"],
        "source_inventory": seal["source_inventory"],
        "report": seal["report"],
        "prior_seal": seal["prior_successful_seal"],
        "selected_objects_sha256": selected_digest,
        "commit_binding": commit_pin,
    }
    return inputs, seal, pins, manifest, commit, changed


def verify_intake(front, seal, manifest, commit, changed):
    prep_path = INTAKE / "PREPARATION_BINDING.json"
    completion_pin = provenance(seal, INTAKE / "completion.json")
    audit_pin = provenance(seal, INTAKE / "composite-artifact-audit.json")
    includes_pin = provenance(seal, INTAKE / "actual-includes.json")
    report_pin = seal["report"]
    prep = front["load"](prep_path)
    completion = front["from_pin"](completion_pin)
    audit = front["from_pin"](audit_pin)
    includes = front["from_pin"](includes_pin)
    report = front["from_pin"](report_pin)
    projection_path = INTAKE / "projection.json"
    projection = front["load"](projection_path, prep["projection_sha256"])
    guards_path = INTAKE / "named-guards.json"
    guards = front["load"](guards_path)

    assert prep["base"] == BASE and prep["projected_tree"] == TREE
    assert prep["manifest_sha256"] == MANIFEST_SHA
    assert prep["selected_count"] == 1004 and prep["retained_count"] == 2706
    assert prep["prior_trace_count"] == 49
    assert completion["head"] == HEAD and completion["status"] == "SEALED_NO_REGRESSIONS"
    assert (completion["requested"], completion["passed"], completion["failed"]) == (3710, 3710, 0)
    assert (completion["fresh"], completion["retained"]) == (1004, 2706)
    assert completion["summary"]["prior_passes_retained"] == 3710
    assert not completion["summary"]["regressions"] and not completion["summary"]["timeouts"]
    assert completion["report_sha256"] == report_pin["sha256"]
    assert completion["audit_sha256"] == audit_pin["sha256"]
    assert report["state"] == "complete" and report["target"] == "xbox360-ppc-be"
    assert report["requested"] == report["passed"] == 3710 and report["failed"] == 0
    assert audit["head"] == HEAD and audit["base"] == BASE
    assert includes["head"] == HEAD
    assert set(includes["rows"]) == set(projection["selected_sources"])
    assert len(includes["rows"]) == prep["selected_count"]
    guard_rows = completion["named_guards"]
    assert guard_rows and all(row["gate_satisfied"] for row in guard_rows)
    assert {row["name"] for row in guard_rows} == {row["name"] for row in guards}
    assert sorted(commit["paths"]) == sorted(changed)

    gate_pins = {
        "preparation_binding": front["pin"](prep_path),
        "completion": completion_pin,
        "report": report_pin,
        "artifact_audit": audit_pin,
        "actual_includes": includes_pin,
        "projection": front["pin"](projection_path),
        "named_guards": front["pin"](guards_path),
        "commit_binding": provenance(seal, INTAKE / "commit-binding.json"),
        "netconn_codegen": provenance(seal, INTAKE / "netconn-codegen.json"),
        "entropy_frame": provenance(seal, INTAKE / "entropy-frame-verification.json"),
    }
    return {
        "state": completion["status"],
        "requested": completion["requested"],
        "passed": completion["passed"],
        "failed": completion["failed"],
        "fresh": completion["fresh"],
        "retained": completion["retained"],
        "named_guard_count": len(guard_rows),
        "pins": gate_pins,
        "projection": projection,
        "actual_includes": includes,
        "report": report,
    }


def verify_name_closure(front, h, r155_proof, r155_replay, r156_inputs,
                        r156_seal, intake_gate, manifest):
    r152_proof = r155_replay["proof"]
    closure = set(r152_proof["seal_inputs"]["name_header_native_closure"])
    prior_observed = set(r152_proof["seal_inputs"]["compiler_observed_name_consumers"])
    assert len(closure) == 182 and len(prior_observed) == 181
    assert r155_proof["name_closure"]["native_owners"] == len(closure)

    r155_seal = front["from_pin"](r155_proof["seal_inputs"]["seal"])
    old_selected = {row["source"]: row for row in r155_seal["selected"]}
    new_selected = r156_inputs["objects"]
    assert closure <= set(old_selected) and closure <= set(new_selected)
    root = r156_inputs["root"]
    old_sources = {h.norm(row["path"]): row["sha256"] for row in r155_seal["sources"]}
    new_sources = {h.norm(row["path"]): row["sha256"] for row in r156_seal["sources"]}
    header_key = h.norm(root / NAME_HEADER)
    assert header_key in old_sources and header_key in new_sources
    header_blob_old = h.git(root, "rev-parse", f"{r155_proof['head']}:{NAME_HEADER}").decode().strip()
    header_blob_new = h.git(root, "rev-parse", f"{HEAD}:{NAME_HEADER}").decode().strip()
    expected_header_blob = r155_proof["name_closure"]["name_header_git_blob"]
    assert header_blob_old == header_blob_new == expected_header_blob
    assert old_sources[header_key] == new_sources[header_key]
    header_bytes = h.git(root, "show", f"{HEAD}:{NAME_HEADER}")
    assert front["pinned"](root / NAME_HEADER, new_sources[header_key]).replace(b"\r\n", b"\n") == header_bytes.replace(b"\r\n", b"\n")

    includes = intake_gate["actual_includes"]["rows"]
    fresh_sources = set(intake_gate["projection"]["selected_sources"])
    assert fresh_sources == set(includes)
    report = intake_gate["report"]
    report_rows = {row["source"]: row for row in report["results"]}
    fresh_closure = closure & fresh_sources
    retained_closure = closure - fresh_sources
    observed_header = {
        source for source, headers in includes.items()
        if any(h.norm(path).endswith(NAME_HEADER.lower()) for path in headers)
    }
    assert observed_header == fresh_closure
    assert len(fresh_closure) == 98 and len(retained_closure) == 84

    changed_rows = {source for source in closure if old_selected[source] != new_selected[source]}
    assert changed_rows == fresh_closure
    changed_sources = {
        source for source in closure
        if old_sources[h.norm(root / source)] != new_sources[h.norm(root / source)]
    }
    changed_paths = {row["path"] for row in manifest["final_paths"]}
    assert changed_sources == changed_paths & closure == {
        "src/GameSource/Main/BrnMainX360.cpp"
    }
    assert changed_sources <= fresh_closure

    owners = []
    for source in sorted(closure):
        old = old_selected[source]
        new = new_selected[source]
        front["pinned"](new["object"], new["sha256"])
        if source in fresh_closure:
            built = report_rows[source]
            assert built["object"] == new["object"] and built["passed"]
            assert built["returncode"] == 0 and built["machine"] == 0x1f2
            assert any(h.norm(path).endswith(NAME_HEADER.lower())
                       for path in includes[source])
            state = "fresh_compiled"
        else:
            assert new == old
            state = "retained_from_r155"
        owners.append({
            "source": source,
            "state": state,
            "r155_object_sha256": old["sha256"],
            "r156_object_sha256": new["sha256"],
            "r156_object": new["object"],
            "source_sha256_r155": old_sources[h.norm(root / source)],
            "source_sha256_r156": new_sources[h.norm(root / source)],
        })

    audio_sources = set(r155_proof["selected_scope_objects"])
    audio_header_rebuilds = sorted(audio_sources & fresh_closure)
    audio_rebuilds = sorted(source for source in audio_sources
                            if old_selected.get(source) != new_selected.get(source))
    assert len(audio_sources) == 15
    assert len(audio_header_rebuilds) == 7
    assert len(audio_rebuilds) == 8
    return {
        "header": NAME_HEADER,
        "header_git_blob_r155": header_blob_old,
        "header_git_blob_r156": header_blob_new,
        "header_source_sha256_r155": old_sources[header_key],
        "header_source_sha256_r156": new_sources[header_key],
        "native_owner_count": len(closure),
        "r155_compiler_observed_consumers": len(prior_observed),
        "r156_fresh_selected_count": len(fresh_sources),
        "r156_fresh_header_consumers": len(fresh_closure),
        "r156_retained_owner_count": len(retained_closure),
        "r156_source_changed_within_closure": sorted(changed_sources),
        "audio_owner_count": len(audio_sources),
        "audio_owners_in_closure_recompiled": audio_header_rebuilds,
        "audio_recompiled_owner_count": len(audio_rebuilds),
        "owners": owners,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    r155 = runpy.run_path(str(R155_VERIFIER))
    carrier = runpy.run_path(str(r155["R154"]))
    predecessor = runpy.run_path(str(carrier["R153"]))
    old = runpy.run_path(str(predecessor["PRIOR_VERIFIER"]))
    inherited, voice, front, h, older = old["helpers"]()

    front["pinned"](R155_VERIFIER, R155_VERIFIER_SHA)
    r155_proof = front["load"](R155_PROOF, R155_PROOF_SHA)
    assert r155_proof["state"] == "pass"
    assert r155_proof["verifier"] == front["pin"](R155_VERIFIER)

    r155_replay = replay_r155(
        r155, carrier, predecessor, old, inherited, voice, front, h, older, r155_proof
    )

    r155_seal_pin = r155_proof["seal_inputs"]["seal"]
    r156_inputs, r156_seal, link_pins, manifest, commit, changed = load_r156_link(
        front, h, r155_seal_pin
    )
    intake = verify_intake(front, r156_seal, manifest, commit, changed)

    r155_functions = r155_replay["functions"]
    r156_scope = set(r155_proof["selected_scope_objects"])
    assert r156_scope == set(r155_replay["scope"])
    rows = {source: row for source, row in r155_replay["rows"].items()
            if source in r156_scope}
    assert rows.keys() <= r156_scope

    old["bind"].__globals__["LINK"] = LINK
    current_functions, current_owners, retained, initializers, entries = old["bind"](
        SimpleNamespace(head=HEAD),
        inherited, voice, front, h, older, rows, r156_inputs
    )
    assert current_functions.keys() == r155_proof["functions"].keys()
    assert len(current_functions) == 60 and len(current_owners) == 15
    assert retained == r155_replay["proof"]["retained_r151_bodies"] and len(retained) == 32

    functions = {}
    address_deltas = set()
    for name, current in current_functions.items():
        previous = r155_functions[name]
        receipt = r155_proof["functions"][name]
        for field in ("symbol", "compiled_sha256", "bytes", "provider"):
            assert current[field] == previous[field] == receipt[field], (name, field)
        assert canonical_bindings(current) == canonical_bindings(previous), name
        address_deltas.add(int(current["address"], 16) - int(previous["address"], 16))
        functions[name] = {
            key: current[key]
            for key in ("symbol", "address", "bytes", "sha256", "compiled_sha256", "provider")
        }
        functions[name]["linked_relocations_sha256"] = h.sha(
            json.dumps(current["bindings"], sort_keys=True).encode()
        )
        functions[name]["relocation_symbols_sha256"] = h.sha(
            json.dumps(canonical_bindings(current), sort_keys=True).encode()
        )
        functions[name]["r155_address"] = previous["address"]
        functions[name]["r155_linked_relocations_sha256"] = \
            receipt["linked_relocations_sha256"]

    witnesses = old["linked_witnesses"](r156_inputs, current_functions, entries, h)
    predecessor["notify_result"].__globals__["LINK"] = LINK
    notify = predecessor["notify_result"](
        front, h, r156_inputs, current_functions, current_owners, entries, old["data_operand"]
    )
    old_notify = r155_replay["notify"]
    assert notify["callee"]["symbol"] == old_notify["callee"]["symbol"]
    assert notify["callee"]["bytes"] == old_notify["callee"]["bytes"]
    assert canonical_bindings(notify["callee"]) == canonical_bindings(old_notify["callee"])
    assert notify["callee"]["bindings"]["0x3c"]["symbol"] == \
        "?MakeHash@Name@Playback@CgsSound@@SAIPBD@Z"
    assert notify["call_address"] == hex(int(current_functions["music_notify"]["address"], 16) + 0x33c)
    assert notify["result_store_address"] == \
        hex(int(current_functions["music_notify"]["address"], 16) + 0x344)
    assert notify["result_store_receiver_offset"] == "0x220"

    current_initializers = {
        group: {name: row["sha256"] for name, row in values.items()}
        for group, values in initializers.items()
    }
    name_closure = verify_name_closure(
        front, h, r155_proof, r155_replay, r156_inputs, r156_seal, intake, manifest
    )
    audio_sources = sorted(r156_scope)
    current_rows = {source: pin for source, (_, _, pin) in current_owners.items()}
    assert set(current_rows) == set(audio_sources)
    old_selected = {
        row["source"]: row for row in r155_replay["inputs"]["objects"].values()
    }
    audio_object_rebindings = []
    closure = set(r155_replay["proof"]["seal_inputs"]["name_header_native_closure"])
    for source in audio_sources:
        new = r156_inputs["objects"][source]
        old_row = old_selected[source]
        audio_object_rebindings.append({
            "source": source,
            "r155_object_sha256": old_row["sha256"],
            "r156_object_sha256": new["sha256"],
            "r156_object": new["object"],
            "object_row_changed": old_row != new,
            "name_header_consumer": source in closure,
        })

    receipt = {
        "state": "pass",
        "head": HEAD,
        "tree": TREE,
        "base": BASE,
        "xex_sha256": r156_inputs["image"]["xex_sha256"],
        "image": r156_inputs["image"],
        "manifest": front["pin"](MANIFEST),
        "predecessor": front["pin"](R155_PROOF),
        "predecessor_verifier": front["pin"](R155_VERIFIER),
        "seal_inputs": link_pins,
        "intake_v2_gate": {
            key: value for key, value in intake.items()
            if key not in ("projection", "actual_includes", "report")
        },
        "name_owner_abi": name_closure,
        "selected_scope_objects": current_rows,
        "audio_object_rebindings": audio_object_rebindings,
        "functions": functions,
        "witnesses": witnesses,
        "direct_notify_result": notify,
        "retained_r151_bodies": len(retained),
        "private_initializers": current_initializers,
        "address_deltas_from_r155": sorted(address_deltas),
        "scope": (
            "Static R156 linked audio regression: 60 R155 audio bodies, their linked "
            "Name call sites, and the CgsCommon.h owner ABI closure. Runtime and the "
            "previously recorded Send01 provider difference remain outside this proof."
        ),
        "limits": [
            "No Xenia launch or gameplay/runtime behavior is established by this receipt.",
            "R156 object addresses move with the linked image; relocation-independent PPC body hashes and symbol/kind relocation maps were compared to R155.",
        ],
    }
    receipt["verifier"] = front["pin"](Path(__file__))

    if args.write:
        out = HERE / "R156_AUDIO_LINKED.json"
        with out.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
        print(front["pin"](out))
    print(
        f"PASS {HEAD}: {len(functions)} linked audio bodies; "
        f"{name_closure['native_owner_count']} Name ABI owners "
        f"({name_closure['r156_fresh_header_consumers']} freshly compiled)"
    )


if __name__ == "__main__":
    main()