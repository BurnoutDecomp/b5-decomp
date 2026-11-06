"""Rebind the sealed R152 audio proof to the exact R153 PE and XEX."""
import argparse
import json
from pathlib import Path
import runpy
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
LINK = WAVE / "r153-link"
INTAKE = WAVE / "r153-intake"
PREVIOUS_LINK = WAVE / "r152-link"
PRIOR_VERIFIER = WAVE / "r152-audio-linked/verify_linked.py"
PRIOR_PROOF = PRIOR_VERIFIER.with_name("R152_AUDIO_LINKED.json")
HEAD = "bcc3e02a41cdb7bba938b91d7ab638c46962540d"
TREE = "610a39be8529a22eeb2d463669c53ede9e46a2fe"
BASE = "5159415953b6b1c2e9cf9c82bfa67dfa5d5e2ed9"
BASE_TREE = "64d1fe0fb7713085ce642f38892db1ea51dba53e"
BASE_SEAL = "98e8f322bdaa6ff58e2db938962c5bbbfe479f5e9db8ac58b6f20e7e24144ca0"
SEAL = "dd7342db08256ff076090eaa92f40752d2003d490c5d91a06f37a34fa6175c11"
MANIFEST = WAVE / "r153-batch/MANIFEST.json"
MANIFEST_SHA = "d61db6067e9aacb72504ea9e6c432d7068f392a0676d87704aa5b1fb6c1f7d35"
PRIOR_PROOF_SHA = "bed4c325f5ed791d98c99d5329a1b2f41067deac9ae88fd7af483825cdabc70a"
PRIOR_VERIFIER_SHA = "0dc190ccf08b47fbd65ec2d7c07f8e0b3d09d369abf06c32dc53d880e95d4dc5"
IMAGE_SHA = {
    "xex_sha256": "1f474b180ddbefe6f99028d00df0760274ad09771d56b9668b8defb97b7ab2a3",
    "pe_sha256": "49e649068dade68315e79d06df2444a4325ea9d8821b4f95e32f0606012eb077",
    "map_sha256": "474f716c2cf5287e635e9386bc3920c081eade89bbfe0a3e143ec2945e95c0d6",
}


def seal_inputs(front, h, frozen):
    load = lambda name: front["load"](LINK / name)
    result, image, invocation = (load(name) for name in ("result.json", "image-verification.json", "invocation.json"))
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] and image["head"] == HEAD
    seal_path, release_path, rsp = (LINK / name for name in ("seal.json", "ROOT_RELEASE.json", "r153-strict.rsp"))
    assert Path(invocation["sealed"]) == seal_path and invocation["sealed_sha256"] == SEAL
    assert Path(invocation["release"]) == release_path
    seal = front["load"](seal_path, SEAL)
    assert (seal["schema"], seal["head"]) == ("r153-native-link-seal-v1", HEAD)
    assert front["load"](release_path, invocation["release_sha256"]) == {
        "action": "one strict R153 native link", "sealed_sha256": SEAL
    }
    for key in ("sources", "libraries", "tool_files", "excluded"):
        assert invocation[key] == seal[key]
    selected = seal["selected"]
    assert invocation["objects"] == selected and (len(selected), len(seal["excluded"])) == (3589, 121)
    assert invocation["command"][1:] == ["@" + str(rsp)]
    arguments = front["pinned"](rsp, invocation["rsp_sha256"]).decode().splitlines()
    assert [line for line in arguments if line.lower().endswith('.obj"')] == [f'"{row["object"]}"' for row in selected]
    order = sorted((h.norm(row["source"]), h.norm(row["object"]), row["sha256"]) for row in selected)
    digest = h.sha(json.dumps(order, separators=(",", ":")).encode())
    assert digest == seal["selected_objects_sha256"]
    assert h.file_sha(LINK / "link.log") == result["log_sha256"]
    diagnostics = load("diagnostic-parsed.json")
    assert diagnostics["unique_symbols"] == diagnostics["unresolved_messages"] == 0 and not diagnostics["duplicate_messages"]
    boundary = front["from_pin"](seal["boundary"])
    assert (boundary["schema"], boundary["head"], boundary["selected_objects_sha256"]) == (
        "r153-link-boundary-v1", HEAD, digest
    )
    assert boundary["no_excluded_cohort_outside_references"] and not boundary["errors"]
    manifest = front["load"](MANIFEST, MANIFEST_SHA)
    assert (manifest["base"], manifest["base_tree"], manifest["projected_tree"]) == (BASE, BASE_TREE, TREE)
    assert seal["prior_successful_seal"] == manifest["base_seal"] == frozen["seal_inputs"]["seal"]
    assert Path(seal["prior_successful_seal"]["path"]) == PREVIOUS_LINK / "seal.json"
    assert seal["prior_successful_seal"]["sha256"] == BASE_SEAL
    prior = front["from_pin"](seal["prior_successful_seal"])
    cohort = h.pinned_json(seal["provider_selection"], h.PROVIDER_SHA)["sources"]
    assert prior["head"] == BASE and cohort == seal["provider_cohort"] == prior["provider_cohort"] and len(cohort) == 15
    for key in ("selected", "excluded"):
        assert {row["source"] for row in seal[key]} == {row["source"] for row in prior[key]}
    excluded = {row["source"]: row for row in seal["excluded"]}
    for provider in cohort:
        row = excluded[provider["source"]]
        assert (row["object"], row["sha256"]) == (provider["object"], provider["sha256"])
        front["pinned"](row["object"], row["sha256"])

    root = Path(seal["source_root"])
    for revision in (HEAD, manifest["candidate"]):
        assert h.git(root, "rev-parse", f"{revision}^{{tree}}", f"{revision}^").decode().split() == [TREE, BASE]
    assert h.git(root, "rev-parse", f"{BASE}^{{tree}}").decode().strip() == BASE_TREE
    binding = next(row for row in seal["provenance"] if Path(row["path"]) == INTAKE / "commit-binding.json")
    commit = front["from_pin"](binding)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (HEAD, BASE, MANIFEST_SHA)
    changed = h.git(root, "diff", "--name-only", BASE, HEAD).decode().splitlines()
    assert sorted(changed) == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    assert len(changed) == 3 and all(path.startswith("src/GameSource/World/") for path in changed)
    inventory = front["from_pin"](seal["source_inventory"])
    assert inventory["head"] == HEAD and inventory["files"] == seal["sources"]
    report = front["from_pin"](seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    objects = {row["source"]: row for row in selected}
    prior_objects = {row["source"]: row for row in prior["selected"]}
    assert not set(changed).intersection(frozen["selected"])
    for source, proof in frozen["selected"].items():
        assert objects[source] == prior_objects[source] == proof["selected"]
        assert h.git(root, "rev-parse", f"{HEAD}:{source}", f"{BASE}:{source}").decode().split() == [proof["blob"]] * 2
    prior_closure = frozen["seal_inputs"]["name_header_native_closure"]
    actual_consumers = frozen["seal_inputs"]["compiler_observed_name_consumers"]
    assert len(prior_closure) == 182 and len(actual_consumers) == 181
    assert set(actual_consumers) < set(prior_closure) and not set(changed).intersection(prior_closure)
    header = "src/GameShared/GameClasses/Sound/Playback/CgsCommon.h"
    before_header, after_header = h.git(root, "rev-parse", f"{BASE}:{header}", f"{HEAD}:{header}").decode().split()
    assert before_header == after_header
    for source in prior_closure:
        assert objects[source] == prior_objects[source]
    h.LINK = LINK
    read, entries, image_pins = h.load_image(result, image)
    assert {name: image_pins[name] for name in IMAGE_SHA} == IMAGE_SHA
    return {
        "read": read, "entries": entries, "image": image_pins, "root": root, "objects": objects,
        "sources": {h.norm(row["path"]): row["sha256"] for row in seal["sources"]},
        "report": {row["source"]: row for row in report["results"]},
        "pins": {"seal": front["pin"](seal_path), "result": front["pin"](LINK / "result.json"),
                 "image_verification": front["pin"](LINK / "image-verification.json"), "release": front["pin"](release_path),
                 "response": front["pin"](rsp), "commit_binding": binding, "source_inventory": seal["source_inventory"],
                 "boundary": seal["boundary"], "report": seal["report"], "prior_seal": seal["prior_successful_seal"],
                 "provider_selection": seal["provider_selection"], "selected_objects_sha256": digest},
        "closure": {"r152_compiler_include_receipt": frozen["seal_inputs"]["actual_includes"],
                    "native_owners": len(prior_closure), "compiler_observed": len(actual_consumers),
                    "retained_selected_rows": len(prior_closure), "name_header_git_blob": after_header},
    }


def notify_result(front, h, inputs, functions, owners, entries, decode_address):
    caller = functions["music_notify"]
    binding = caller["bindings"]["0x33c"]
    symbol = "?GetStreamFromVideoName@MusicEffect@Logic@BrnSound@@ABAII@Z"
    assert binding["symbol"] == symbol and binding["kind"] == "0x6"
    address = int(caller["address"], 16)
    read = inputs["read"]
    assert read(address + 0x340, 20, executable=True) == bytes.fromhex("57cb063e 907f0220 9bdf0245 2b0b0000 9bbf0246")
    assert not any(row["symbol"].startswith("?MakeHash@Name@Playback@")
                   for offset, row in caller["bindings"].items() if 0x330 <= int(offset, 16) < 0x358)
    [source] = [path for path in owners if Path(path).stem == "BrnMusicEffect"]
    provider, coff, _ = owners[source]
    name, machine, refs = coff.function(symbol)
    target = h.map_addr(entries, symbol, provider)
    assert binding["target"] == hex(target)
    parser = runpy.run_path(str(front["READER"]))
    linked = read(target, len(machine), executable=True)
    image = h.pefile.PE(str(LINK / "native-game.pe"), fast_load=True)
    local = {}
    for hi, lows, prefix in ((0xc, (0x1c, 0x38), "?$S2@"), (0x18, (0x40, 0x48), "?KU_INTRO_VIDEO_NAME@")):
        kind, item = refs[hi]
        assert kind == 0x10 and item.name.startswith(prefix) and item.value == 0
        assert coff.section(item.section)[0] == b".bss"
        assert all(refs[lo] == (0x11, item) for lo in lows)
        [destination] = {decode_address(linked, hi, lo, h) for lo in lows}
        rva = destination - image.OPTIONAL_HEADER.ImageBase
        assert destination % 4 == 0 and any(
            section.VirtualAddress <= rva and rva + 4 <= section.VirtualAddress + section.Misc_VirtualSize
            and section.Characteristics & 0xa0000000 == 0x80000000 for section in image.sections
        )
        if item.name in entries:
            assert h.map_addr(entries, item.name) == destination
        local[item.name] = {(destination, provider)}
    assert len(set().union(*local.values())) == 2
    callee = front["body_proof"](read, entries | local, coff, "notify_lookup", name, machine, refs, target, {}, parser)
    return {"call_address": hex(address + 0x33c), "result_store_address": hex(address + 0x344),
            "result_store_receiver_offset": "0x220", "callee": callee}


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--write", action="store_true")
    args = cli.parse_args()
    prior = runpy.run_path(str(PRIOR_VERIFIER))
    inherited, voice, front, h, older = prior["helpers"]()
    front["pinned"](PRIOR_VERIFIER, PRIOR_VERIFIER_SHA)
    frozen = front["load"](PRIOR_PROOF, PRIOR_PROOF_SHA)
    assert (frozen["head"], frozen["tree"], frozen["xex_sha256"]) == (
        BASE, BASE_TREE, "c99cfab9c8aeca5c692a84491b88840e8260fbb24267c335008d9783ee120daa"
    )
    inputs = seal_inputs(front, h, frozen)
    prev_args = SimpleNamespace(manifest=Path(frozen["manifest"]["path"]), manifest_sha=frozen["manifest"]["sha256"],
                                targeted=Path(frozen["targeted"]["path"]), targeted_sha=frozen["targeted"]["sha256"],
                                candidate=frozen["candidate"], tree=BASE_TREE)
    _, rows = prior["frozen_inputs"](prev_args, front, h)
    prior["bind"].__globals__["LINK"] = LINK
    functions, owners, retained, initializers, entries = prior["bind"](
        SimpleNamespace(head=HEAD), inherited, voice, front, h, older, rows, inputs
    )
    assert functions.keys() == frozen["functions"].keys() and len(functions) == 60
    assert retained == frozen["retained_r151_bodies"] and len(retained) == 32
    assert {source: row for source, (_, _, row) in owners.items()} == frozen["selected"]
    checks = {}
    for key, actual in functions.items():
        old = frozen["functions"][key]
        for field in ("symbol", "compiled_sha256", "bytes", "provider"):
            assert actual[field] == old[field], (key, field)
        canonical = lambda row: {offset: (ref["symbol"], ref["kind"]) for offset, ref in row["bindings"].items()}
        assert canonical(actual) == canonical(old), key
        checks[key] = {name: actual[name] for name in ("symbol", "address", "bytes", "sha256", "compiled_sha256", "provider")}
        checks[key]["linked_relocations_sha256"] = h.sha(json.dumps(actual["bindings"], sort_keys=True).encode())
    witnesses = prior["linked_witnesses"](inputs, functions, entries, h)
    notify = notify_result(front, h, inputs, functions, owners, entries, prior["data_operand"])
    receipt = {
        "state": "pass", "head": HEAD, "tree": TREE, "base": BASE, "xex_sha256": inputs["image"]["xex_sha256"],
        "image": inputs["image"], "manifest": front["pin"](MANIFEST), "verifier": front["pin"](Path(__file__)),
        "predecessor": front["pin"](PRIOR_PROOF), "predecessor_verifier": front["pin"](PRIOR_VERIFIER),
        "seal_inputs": inputs["pins"], "name_dependency_closure": inputs["closure"], "functions": checks,
        "private_initializers": {group: {name: row["sha256"] for name, row in values.items()}
                                 for group, values in initializers.items()},
        "witnesses": witnesses, "direct_notify_result": notify,
        "scope": "Linked R153 audio regression only. The R152 Send01 provider difference remains outside the proof.",
    }
    if args.write:
        path = HERE / "R153_AUDIO_LINKED.json"
        with path.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
        print(front["pin"](path))
    print(f"PASS {HEAD}: {len(checks)} retained audio bodies and the Notify lookup provider")


if __name__ == "__main__":
    main()
