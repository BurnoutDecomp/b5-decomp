"""Carry forward the sealed R148 ICE, language and presentation proof into R149."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
LINK = WAVE / "r149-link"
HEAD = "28769133a5e88f5cc530003d41b059022fb557f6"
TREE = "bd200df2bf44260dee9d59f4cef5dd7dceb63c46"
BASE = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
MANIFEST_SHA = "5f11112152fddbb6291be08c4635e622ea3b3dd08acda4723f1329907e6ce9e3"
BASE_SEAL_SHA = "1e0641bd74316d8270d32ea43e3c407570c062c84cbbeee40d359b73a3bb0b17"
BASE_LINKED = WAVE / "r148-boot-presentation-linked/R148_BOOT_PRESENTATION_LINKED.json"
BASE_LINKED_SHA = "489953193f5d78775dc484f1967478146fe6f6ef511f96565332949ab7ecd471"
BASE_VERIFIER_SHA = "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba"
COMPOSED = WAVE / "r149-composed-proof/R149_COMPOSED_PPC.json"
COMPOSED_SHA = "10692971f27af2d6601506e2954f96e109680e5b9847e5b1cede03074fd615d1"


def helpers():
    sys.dont_write_bytecode = True
    path = BASE_LINKED.with_name("verify_linked.py")
    assert hashlib.sha256(path.read_bytes()).hexdigest() == BASE_VERIFIER_SHA
    spec = importlib.util.spec_from_file_location("r148_linked_helpers", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # The common PE/map/XEX parser uses the module's artifact directory.
    module.LINK = LINK
    return module


def seal_inputs(h):
    result = h.read_json(LINK / "result.json")
    image = h.read_json(LINK / "image-verification.json")
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] and image["head"] == HEAD
    invoke = h.read_json(LINK / "invocation.json")
    seal_path = LINK / "seal.json"
    assert Path(invoke["sealed"]) == seal_path
    seal = json.loads(h.pinned(seal_path, invoke["sealed_sha256"]))
    assert seal["schema"] == "r149-native-link-seal-v1" and seal["head"] == HEAD
    assert seal["selected"] == invoke["objects"] and seal["excluded"] == invoke["excluded"]
    assert seal["libraries"] == invoke["libraries"] and seal["sources"] == invoke["sources"]
    release_path = LINK / "ROOT_RELEASE.json"
    assert Path(invoke["release"]) == release_path
    release = json.loads(h.pinned(release_path, invoke["release_sha256"]))
    assert release == {"action": "one strict R149 native link", "sealed_sha256": invoke["sealed_sha256"]}
    rsp_path = LINK / "r149-strict.rsp"
    assert invoke["command"][1:] == ["@" + str(rsp_path)]
    rsp = h.pinned(rsp_path, invoke["rsp_sha256"]).decode().splitlines()
    assert [line for line in rsp if line.lower().endswith('.obj"')] == [f'"{row["object"]}"' for row in seal["selected"]]
    assert len(seal["selected"]) == 3589 and len(seal["excluded"]) == 121
    selected = {row["source"]: row for row in seal["selected"]}
    ordered = sorted((h.norm(row["source"]), h.norm(row["object"]), row["sha256"]) for row in selected.values())
    assert h.sha(json.dumps(ordered, separators=(",", ":")).encode()) == seal["selected_objects_sha256"]
    assert h.file_sha(LINK / "link.log") == result["log_sha256"]
    diagnostic = h.read_json(LINK / "diagnostic-parsed.json")
    assert diagnostic["unique_symbols"] == diagnostic["unresolved_messages"] == 0 and not diagnostic["duplicate_messages"]
    boundary = h.pinned_json(seal["boundary"])
    assert boundary["schema"] == "r149-link-boundary-v1" and boundary["head"] == HEAD
    assert boundary["no_excluded_cohort_outside_references"] and boundary["selected_objects_sha256"] == seal["selected_objects_sha256"]
    assert Path(seal["prior_successful_seal"]["path"]) == WAVE / "r148-link/seal.json"
    prior = h.pinned_json(seal["prior_successful_seal"], BASE_SEAL_SHA)
    cohort = h.pinned_json(seal["provider_selection"], h.PROVIDER_SHA)["sources"]
    assert prior["head"] == BASE and len(cohort) == 15
    assert cohort == seal["provider_cohort"] == prior["provider_cohort"]
    excluded = {row["source"]: row for row in seal["excluded"]}
    assert set(excluded) == {row["source"] for row in prior["excluded"]}
    for row in cohort:
        actual = excluded[row["source"]]
        assert actual["object"] == row["object"] and actual["sha256"] == row["sha256"] == h.file_sha(row["object"])

    checkout = Path(seal["source_root"])
    assert h.git(checkout, "rev-parse", f"{HEAD}^{{tree}}", f"{HEAD}^").decode().split() == [TREE, BASE]
    commit_path = WAVE / "r149-intake/commit-binding.json"
    commit_pin = next(row for row in seal["provenance"] if Path(row["path"]) == commit_path)
    commit = h.pinned_json(commit_pin)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (HEAD, BASE, MANIFEST_SHA)
    manifest = json.loads(h.pinned(WAVE / "r149-batch/MANIFEST.json", MANIFEST_SHA))
    assert manifest["base"] == BASE and manifest["projected_tree"] == TREE
    changed = h.git(checkout, "diff", "--name-only", BASE, HEAD).decode().splitlines()
    assert sorted(changed) == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    inventory = h.pinned_json(seal["source_inventory"])
    assert inventory["head"] == HEAD and inventory["files"] == seal["sources"]
    report = h.pinned_json(seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    read, entries, image_pins = h.load_image(result, image)
    return {
        "checkout": checkout, "selected": selected,
        "source_hashes": {h.norm(row["path"]): row["sha256"] for row in seal["sources"]},
        "report_rows": {row["source"]: row for row in report["results"]},
        "read": read, "entries": entries, "image": image_pins,
        "seal": {"sha256": invoke["sealed_sha256"], "response_sha256": invoke["rsp_sha256"],
                 "manifest_sha256": MANIFEST_SHA, "commit_binding_sha256": commit_pin["sha256"],
                 "selected_objects_sha256": seal["selected_objects_sha256"], "inherited_cohort": len(cohort)},
    }


def source_inputs(h, current, boot, composed, old):
    objects, providers = {}, {}
    checkout = current["checkout"]
    for source, old_provider in boot["providers"].items():
        row, compiled = current["selected"][source], current["report_rows"][source]
        assert row["object"] == compiled["object"] and compiled["passed"] and compiled["machine"] == 0x1F2
        blob = h.git(checkout, "show", f"{HEAD}:{source}")
        path = checkout / source
        content = path.read_bytes()
        assert h.sha(content) == current["source_hashes"][h.norm(path)], source
        assert content.replace(b"\r\n", b"\n") == blob.replace(b"\r\n", b"\n"), source
        composed_row = composed["artifacts"].get(source)
        if composed_row:
            assert h.git(checkout, "rev-parse", f"{HEAD}:{source}").decode().strip() == composed_row["source_blob"]
        else:
            assert h.sha(blob) == old["sources_and_outputs"][source]["git_blob_sha256"]
        selected = Path(row["object"])
        prior = old_provider["preflight"]
        assert selected.name == Path(prior["path"]).name
        objects[Path(source).stem] = (
            selected, h.pinned(selected, row["sha256"]), h.pinned(prior["path"], prior["sha256"]),
            h.pinned(composed_row["object"]["path"], composed_row["object"]["sha256"]) if composed_row else None,
        )
        providers[source] = {
            "selected": row, "r148_preflight": prior, "source_sha256": h.sha(blob),
            "r149_composed": composed_row["object"] if composed_row else None,
        }
    return objects, providers


def linked_witnesses(h, current, functions, old, composed):
    read = current["read"]

    def target(method, offset, prefix):
        row = functions[method]["bindings"][offset]
        assert row["symbol"].startswith(prefix), row
        return row["address"]

    def words(method, values):
        address = functions[method]["address"]
        for off, word in values.items():
            assert h.u32(read(address + off, 4, executable=True), 0) == word, (method, hex(off))

    style = target("widget_construct", 0x14, "kaMenuStyles")
    assert h.sha(read(style, 224)) == old["ice"]["style_table"]["sha256"]
    assert target("widget_width", 0xC, "KF_ICE_MENU_MARGIN") + 0x38 == style
    assert target("widget_render", 0x60, "KF_ICE_MENU_TEXT_SIZE") + 0x30 == style
    assert target("controller", 0xC8, "?ConstructMenus@ICEController@") == functions["menus"]["address"]
    for at in (0x40, 0x64, 0xBC, 0xE0, 0x104, 0x128, 0x14C):
        assert target("menus", at, "?Construct@ICEWidgetMenu@") == functions["widget_construct"]["address"]
    y = target("menus", 0x34, "KF_ICE_MENU_PAIR_A_X") + 0x28
    assert read(y, 4) == struct.pack(">f", 25.0)
    words("menus", {int(at, 16): int(word, 16) for at, word in old["ice"]["startup_geometry_coff"].items()})
    release = functions["language_release"]["address"]
    assert target("language_release", 0xC, "?UnloadStringTable@LanguageManager@") == functions["language_unload"]["address"]
    assert target("gui_release", 0x9C, "?Release@LanguageManager@") == release
    words("language_release", {0x10: 0x38600001, 0x20: 0x4E800020})
    words("gui_release", {0x98: 0x387E7C0C, 0xA0: 0x546B063E, 0xA4: 0x2B0B0000, 0xA8: 0x419A0060})

    music = target("attach", 0xC4, "??_C@_0P@FLFHBPI@MusicVoiceSpec")
    wave = target("play", 0x144, "??_C@_0BD@LBJMKPLH@StereoWavVoiceSpec")
    assert read(music, 15) == b"MusicVoiceSpec\0" and read(wave, 19) == b"StereoWavVoiceSpec\0"
    assert target("attach", 0xD0, "?MakeHash@Name@Playback@") == target("play", 0x150, "?MakeHash@Name@Playback@")
    words("attach", {0xD8: 0x7C6A1B78, 0xE4: 0x915F0284})
    words("get_create_params", {0: 0x38630244, 4: 0x4E800020})
    words("play", {0x18: 0x2B0BFFFE, 0x1C: 0x409A00A0, 0xB0: 0x997D02BC, 0x12C: 0x2B0BFFFF, 0x158: 0x9061007C})
    presentation = composed["presentation"]
    for key, kind in (("stop", "StreamStopRequest"), ("play", "StreamRequest")):
        ctor, post = (int(at, 16) for at in presentation[key]["coff_ctor_post"])
        target("play", ctor, "??0" + kind + "@")
        target("play", post, "?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@QAAXABU" + kind)
    return {
        "ice": {"table": style, "size": 224, "artist_sha256": old["ice"]["style_table"]["sha256"], "copy_y": y},
        "language": {"release": release, "gui_owner": functions["gui_release"]["address"]},
        "presentation": {
            "music_voice_spec": music, "resident_wav_voice_spec": wave,
            "stream_splice": 0xFFFE, "resident_wav_splice": 0xFFFF,
            "stream_requests": {key: presentation[key] for key in ("stop", "play")},
            "remaining_create_params_layout": presentation["remaining_create_params_layout"],
        },
    }


def verify_frontend(h, path, digest, current, play):
    assert path == WAVE / "r149-frontend-linked/R149_FRONTEND_LINKED.json"
    frontend = json.loads(h.pinned(path, digest))
    assert (frontend["state"], frontend["head"], frontend["tree"], frontend["base"]) == ("pass", HEAD, TREE, BASE)
    assert frontend["manifest"]["sha256"] == MANIFEST_SHA and frontend["frozen_composed"]["sha256"] == COMPOSED_SHA
    assert frontend["provenance"]["seal"]["sha256"] == current["seal"]["sha256"]
    assert frontend["image"] == current["image"]
    witness = frontend["functions"]["play"]
    assert (witness["symbol"], int(witness["address"], 16), witness["bytes"], witness["sha256"]) == (
        play["symbol"], play["address"], play["size"], play["sha256"],
    )
    selected = current["selected"][witness["source"]]
    assert witness["checked_coff"] == selected["object"]
    assert frontend["objects"][witness["source"]]["selected_object"]["sha256"] == selected["sha256"]
    presentation = frontend["witnesses"]["presentation"]
    for offset, row in zip((0x78, 0x84, 0x9C, 0xA8), presentation["calls_in_order"], strict=True):
        own = play["bindings"][offset]
        assert (row["symbol"], int(row["target"], 16)) == (own["symbol"], own["address"])
    assert int(presentation["resident_wave"]["target"], 16) == play["bindings"][0x144]["address"]
    return {"path": str(path), "sha256": digest, "play_sha256": play["sha256"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--frontend-receipt", type=Path, required=True)
    parser.add_argument("--frontend-sha256", required=True)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    assert __debug__ and args.expected_head == HEAD
    h = helpers()
    boot = json.loads(h.pinned(BASE_LINKED, BASE_LINKED_SHA))
    assert boot["head"] == BASE and boot["seal"]["seal_sha256"] == BASE_SEAL_SHA
    old = json.loads(h.pinned(h.FROZEN, h.FROZEN_SHA))
    composed = json.loads(h.pinned(COMPOSED, COMPOSED_SHA))
    assert composed["state"] == "pass" and composed["base"] == BASE and composed["tree"] == TREE
    current = seal_inputs(h)
    assert h.git(current["checkout"], "rev-parse", f"{composed['head']}^{{tree}}").decode().strip() == TREE
    objects, providers = source_inputs(h, current, boot, composed, old)
    assert set(objects) == set(h.GROUPS)
    read, entries = current["read"], current["entries"]
    functions = {}
    for stem, methods in h.GROUPS.items():
        path, data, prior, new = objects[stem]
        obj, original = h.Coff(data), h.Coff(prior)
        rebuilt = h.Coff(new) if new is not None else None
        section_bases = {}
        for label, prefix in methods.items():
            # GUI code before the changed event can still move assert line numbers.
            baseline = rebuilt if label in {"play", "gui_release"} else original
            functions[label] = h.verify_body(
                read, entries, obj, path.name, obj.function(prefix), baseline.function(prefix), section_bases,
            )
    path, selected, prior, _ = objects["BrnPresentationEffect"]
    vtables = h.verify_presentation_vtables(read, entries, path, selected, prior, functions)
    frontend = verify_frontend(h, args.frontend_receipt.resolve(), args.frontend_sha256, current, functions["play"])
    receipt = {
        "schema": "r149-boot-presentation-linked-regression-v1", "status": "pass", "head": HEAD, "tree": TREE, "parent": BASE,
        "r148_linked_sha256": BASE_LINKED_SHA, "r149_composed_sha256": COMPOSED_SHA, "frontend": frontend,
        "seal": current["seal"], "image": current["image"], "providers": providers,
        "functions": functions, "presentation_vtables": vtables, "witnesses": linked_witnesses(h, current, functions, old, composed),
        "scope": "Static COFF/linked PE carry-forward for ICE, language release and presentation routing, with sealed XEX provenance.",
    }
    if args.verify_only:
        print("R149 boot presentation linked regression passed")
        return
    path = ROOT / "R149_BOOT_PRESENTATION_LINKED.json"
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(path, h.file_sha(path))


if __name__ == "__main__":
    main()
