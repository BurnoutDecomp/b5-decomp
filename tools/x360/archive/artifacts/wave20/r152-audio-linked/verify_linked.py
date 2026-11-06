"""Verify the exact selected R152 Xbox audio ABI and lifecycle witnesses."""
import argparse
import json
from pathlib import Path
import runpy
import struct

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
LINK = WAVE / "r152-link"
INTAKE = WAVE / "r152-intake"
BASE = "a632bc6495f5accac5dd267ddadea49c2072f7f7"
BASE_SEAL = "6a73f783a235dc99aaa3914eef0f721311d128a0b4c81eacb002ac19780b6be0"
OLD = WAVE / "r151-audio-linked/verify_linked.py"
OLD_SHA = "3bff33daaaf3fe2e854aacb100de6da36de43bc0226a9148d33dffb55e062b14"
OLD_RECEIPT = OLD.with_name("R151_AUDIO_LINKED.json")
OLD_RECEIPT_SHA = "300ef00029027b02ccd71ee81a110c2671458f77ccc59d4830f3a3d9243ef240"
MANIFEST = WAVE / "r152-batch/MANIFEST.json"
TARGETED = MANIFEST.with_name("TARGETED_OBJECT_BINDING.json")
HUD_PROOF = WAVE / "r152-native-hud-slipstream/R152_HUD_SLIPSTREAM_PROOF.json"
HUD_PROOF_SHA = "0cf7fb6b4877963de0361f3fc2b9850ccbef4324a01b92d539af6d056fb1cbaf"
SNAPSHOT_PROOF = WAVE / "r152-hud-vehicle-snapshot-fix/R152_HUD_VEHICLE_SNAPSHOT_PROOF.json"
SNAPSHOT_PROOF_SHA = "c2d9253f771bc9dfe9c39261e8150ac87f0923cf020b96c87b58a5b73eaf99eb"
NAME_HEADER = "src/GameShared/GameClasses/Sound/Playback/CgsCommon.h"
LOGIC_VOICE = "src/GameShared/GameClasses/Sound/Logic/CgsVoice.cpp"
PLAYBACK_VOICE = "src/GameShared/GameClasses/Sound/Playback/CgsVoice.cpp"
METHODS = {
    "BrnMusicEffect": {
        "music_alloc": "?CreateObject@MusicEffect@", "music_ctor": "??0MusicEffect@",
        "music_attach": "?Attach@MusicEffect@", "music_notify": "?Notify@MusicEffect@",
        "music_preview": "?UpdatePreviewTrack@MusicEffect@", "music_params": "?UpdateParams@MusicEffect@",
    },
    "BrnMusicStream": {
        "music_prepare": "?Prepare@MusicStream@", "music_queue": "?Queue@MusicStream@",
        "music_update": "?Update@MusicStream@",
    },
    LOGIC_VOICE: {"logic_connect": "?Connect@Voice@Logic@"},
    PLAYBACK_VOICE: {
        "find_slot": "?FindNamedSlot@Voice@Playback@", "index_slot": "?GetIndexOfSlot@Voice@Playback@",
        "find_send": "?FindNamedSend@Voice@Playback@", "playback_connect": "?Connect@Voice@Playback@",
    },
    "CgsSoundPlaybackModule": {"module_connect": "?ConnectVoice@Module@1Playback@"},
    "CgsGenericRwacVoice": {"rwac_create": "?CreateVoiceInstance@GenericRwacVoice@Playback@"},
    "CgsGenericRwacFactory": {"rwac_config": "?SetupConfig@GenericRwacFactory@Playback@"},
    "BrnHUDEffect": {
        "hud_amount": "?GetSlipstreamAmount@HUDEffect@", "hud_slipstream": "?UpdateSlipStreaming@HUDEffect@",
        "hud_alloc": "?CreateObject@HUDEffect@", "hud_ctor": "??0HUDEffect@",
        "hud_attach": "?Attach@HUDEffect@", "hud_params": "?UpdateParams@HUDEffect@",
    },
    "BrnInAirEffect": {"inair_detach": "?Detach@InAirEffect@"},
}
HUD_GLOBALS = {
    "gfSlipstreamConeCos": ("??__EgfSlipstreamConeCos@Logic@BrnSound@@YAXXZ", 0x30, 0x38),
    "gfSlipstreamScale": ("??__EgfSlipstreamScale@Logic@BrnSound@@YAXXZ", 0x10, 0x1c),
}
MUSIC_LOCAL = {"KF_JUMP_HPF_REST_FREQUENCY", "KA_CLASSICAL_MUSIC_DATA"}
RWAC_PRIVATE = {
    "skInternalSubmixFeature": b"~InternalSubmixFeature~\0",
    "skInternalSendFeature": b"~InternalSendFeature~\0",
    "skInternalDacFeature": b"~InternalDacFeature~\0",
}


def helpers():
    inherited = runpy.run_path(str(OLD))
    voice, front, h, _ = inherited["helpers"]()
    front["pinned"](OLD, OLD_SHA)
    prior = front["load"](OLD_RECEIPT, OLD_RECEIPT_SHA)
    assert prior["head"] == BASE and prior["seal_inputs"]["seal"]["sha256"] == BASE_SEAL
    return inherited, voice, front, h, prior


def frozen_inputs(args, front, h):
    manifest = front["load"](args.manifest, args.manifest_sha)
    assert (manifest["base"], manifest["candidate"], manifest["projected_tree"]) == (BASE, args.candidate, args.tree)
    targeted = front["load"](args.targeted, args.targeted_sha)
    assert (targeted["head"], targeted["tree"]) == (args.candidate, args.tree)
    assert any(Path(row["path"]) == args.targeted and row["sha256"] == args.targeted_sha
               for row in manifest["composed_receipts"])
    compiled = {}
    report_pins = targeted["compile_reports"] if "compile_reports" in targeted else [targeted["compile_report"]]
    for pin in report_pins:
        report = front["from_pin"](pin)
        assert report["state"] == "complete" and report["target"] == "xbox360-ppc-be"
        assert len(report["results"]) == report["requested"]
        overlap = set(compiled).intersection(row["source"] for row in report["results"])
        assert not compiled or overlap == {"src/GameSource/Sound/Global/BrnHUDEffect.cpp"} == {row["source"] for row in report["results"]}
        compiled.update((row["source"], row) for row in report["results"])
    rows = {row["source"]: row for row in targeted["results"]}
    assert rows.keys() == compiled.keys() and len(rows) == 14
    for source, row in rows.items():
        built = compiled[source]
        assert built["passed"] and built["returncode"] == 0 and built["machine"] == row["machine"] == 0x1f2
        for kind, artifact in row["artifacts"].items():
            assert Path(built[kind]) == Path(artifact["path"])
            h.pinned(artifact["path"], artifact["sha256"])
    return manifest, rows


def seal_inputs(args, manifest, front, h):
    load = lambda name: front["load"](LINK / name)
    result, image, invocation = (load(name) for name in ("result.json", "image-verification.json", "invocation.json"))
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] and image["head"] == args.head
    seal_path, release_path, rsp = (LINK / name for name in ("seal.json", "ROOT_RELEASE.json", "r152-strict.rsp"))
    assert Path(invocation["sealed"]) == seal_path and Path(invocation["release"]) == release_path
    assert invocation["sealed_sha256"] == args.seal_sha
    seal = front["load"](seal_path, args.seal_sha)
    assert (seal["schema"], seal["head"]) == ("r152-native-link-seal-v1", args.head)
    assert front["load"](release_path, invocation["release_sha256"]) == {
        "action": "one strict R152 native link", "sealed_sha256": args.seal_sha
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
        "r152-link-boundary-v1", args.head, digest
    )
    assert boundary["no_excluded_cohort_outside_references"] and not boundary["errors"]
    assert seal["prior_successful_seal"] == manifest["base_seal"] == {
        "path": str(WAVE / "r151-link/seal.json"), "sha256": BASE_SEAL
    }
    previous = front["from_pin"](seal["prior_successful_seal"])
    cohort = h.pinned_json(seal["provider_selection"], h.PROVIDER_SHA)["sources"]
    assert previous["head"] == BASE and cohort == seal["provider_cohort"] == previous["provider_cohort"] and len(cohort) == 15
    for key in ("selected", "excluded"):
        assert {row["source"] for row in seal[key]} == {row["source"] for row in previous[key]}
    excluded = {row["source"]: row for row in seal["excluded"]}
    for member in cohort:
        actual = excluded[member["source"]]
        assert (actual["object"], actual["sha256"]) == (member["object"], member["sha256"])
        front["pinned"](actual["object"], actual["sha256"])

    root = Path(seal["source_root"])
    assert h.git(root, "rev-parse", f"{args.head}^{{tree}}", f"{args.head}^").decode().split() == [args.tree, BASE]
    v1 = "03c82845bfd86199130db273aaf446cffac80a6c"
    assert h.git(root, "rev-parse", f"{args.candidate}^{{tree}}", f"{args.candidate}^", f"{args.candidate}^^").decode().split() == [args.tree, v1, BASE]
    bind_path = args.intake / "commit-binding.json"
    binding = next(row for row in seal["provenance"] if Path(row["path"]) == bind_path)
    commit = front["from_pin"](binding)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (args.head, BASE, args.manifest_sha)
    paths = sorted(h.git(root, "diff", "--name-only", BASE, args.head).decode().splitlines())
    assert paths == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    inventory = front["from_pin"](seal["source_inventory"])
    assert inventory["head"] == args.head and inventory["files"] == seal["sources"]
    report = front["from_pin"](seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0

    projection = front["load"](args.intake / "projection.json", commit["projection_sha256"])
    previous_done = front["from_pin"](manifest["base_compile"])
    baseline = front["load"](WAVE / "r151-intake/composite-artifact-audit.json", previous_done["audit_sha256"])
    dependencies = baseline["source_snapshot"]["dependencies"]
    name_users = {NAME_HEADER}
    while True:
        reachable = name_users | {source for source, refs in dependencies.items() if name_users.intersection(refs)}
        if reachable == name_users:
            break
        name_users = reachable
    native = {row["source"] for row in report["results"]}
    name_users &= native
    fresh = set(projection["selected_sources"])
    assert len(name_users) == 182 and len(fresh) == 184 and name_users <= fresh
    assert not name_users.intersection(excluded)
    actual_includes_path = args.intake / "actual-includes.json"
    include_pin = next(row for row in seal["provenance"] if Path(row["path"]) == actual_includes_path)
    actual_includes = front["from_pin"](include_pin)
    assert actual_includes["head"] == args.head and set(actual_includes["rows"]) == fresh
    fresh_objects = {row["source"] for row in report["results"] if Path(row["object"]).parent == args.intake / "native-selected"}
    assert fresh_objects == fresh
    observed_name_users = {source for source, headers in actual_includes["rows"].items()
                           if any(h.norm(path).endswith(NAME_HEADER.lower()) for path in headers)}
    assert observed_name_users <= name_users
    assert name_users - observed_name_users <= {"src/GameShared/GameClasses/Sound/Logic/CgsLogicContentDtor.cpp"}

    h.LINK = LINK
    read, entries, image_pins = h.load_image(result, image)
    for kind in ("xex", "pe", "map"):
        assert image_pins[f"{kind}_sha256"] == getattr(args, kind + "_sha")
    return {
        "read": read, "entries": entries, "image": image_pins, "root": root,
        "sources": {h.norm(row["path"]): row["sha256"] for row in seal["sources"]},
        "objects": {row["source"]: row for row in selected}, "report": {row["source"]: row for row in report["results"]},
        "pins": {"seal": front["pin"](seal_path), "release": front["pin"](release_path),
                 "result": front["pin"](LINK / "result.json"), "image_verification": front["pin"](LINK / "image-verification.json"),
                 "response": front["pin"](rsp), "commit_binding": binding, "source_inventory": seal["source_inventory"],
                 "boundary": seal["boundary"], "report": seal["report"], "prior_seal": seal["prior_successful_seal"],
                 "provider_selection": seal["provider_selection"], "selected_objects_sha256": digest,
                 "actual_includes": include_pin, "name_header_native_closure": sorted(name_users),
                 "compiler_observed_name_consumers": sorted(observed_name_users)},
    }


def method_sources(rows):
    by_source = {}
    for key, methods in METHODS.items():
        if key.startswith("src/"):
            assert key in rows
            source = key
        else:
            [source] = [source for source in rows if Path(source).stem == key]
        by_source[source] = methods
    return by_source


def original(front, voice):
    raw = front["pinned"](voice["ARTIST"], voice["ARTIST_SHA"])
    offset = struct.unpack_from(">I", raw, 8)[0] - 0x82000000

    def read(va, size):
        return raw[offset + va:offset + va + size]

    words = {
        0x8269cc04: "817d0000 917f0020", 0x826f7158: "90610050 38810050",
        0x826acb08: "7c9d2378", 0x826acb28: "817d0000",
        0x826ace70: "7c9d2378", 0x826ace90: "817d0000",
        0x826d2730: "38600250", 0x8269cc90: "917e022c",
        0x826ff76c: "387200ac 8092022c fc20f090",
        0x826ff77c: "3872004c 8092022c", 0x826ff78c: "3872010c 8092022c",
        0x826ff79c: "3bf2016c 8092022c",
        0x8209d74c: "3eb126e9", 0x82c62df0: "d00b60c0",
        0x82c62e14: "c1ab60c0 3d608301 eda06828 ec006824 d00b83e4",
        0x82cd1fec: "82c62db8 82c62e08",
        0x82686be0: "11a0694a", 0x82686c0c: "118a626f",
        0x82686c14: "11ab6b2e",
        0x826f4820: "93df0024 39600001 387f0040 917f0024",
        0x826f4834: "387f0090", 0x826f483c: "387f0264",
        0x826f4844: "387f02b4", 0x826f484c: "387f0214",
        0x826f4854: "93df03b4 39600002 917f0024 39600003 7fe3fb78 917f0024",
        0x826f4870: "38600001", 0x826f4878: "38600000",
        0x826e71d8: "386004d8", 0x826e19f8: "3bbf03a0", 0x826e1a04: "395f0400",
        0x826e1a60: "3b9f0430", 0x8269c488: "397f039c", 0x82701ee4: "1000014a",
        0x82701f2c: "387d0400",
        0x82701dd0: "7c641b78 38610080", 0x82701de8: "38610080",
        0x82701e00: "38610080", 0x82701e14: "38610080", 0x82701e5c: "38610080",
        0x826f4810: "826f4820 826f4824 826f4858 826f4860",
    }
    calls = {
        0x826f7174: 0x8269cb88, 0x826ff778: 0x826871f8, 0x826ff788: 0x826871f8,
        0x826ff798: 0x826871f8, 0x826ff7ac: 0x826871f8,
        0x82c62ddc: 0x821f06b0, 0x826f4838: 0x826cf9c0,
        0x826f4840: 0x826c5270, 0x826f4848: 0x826c5270, 0x826f4850: 0x826c5270,
        0x82701cc8: 0x8269cb88, 0x82701d6c: 0x826871f8, 0x826d9bd8: 0x826acaf8,
        0x82701dd8: 0x826bb258, 0x82701dec: 0x82277b90, 0x82701e04: 0x82277b10,
        0x82701e18: 0x82310240, 0x82701e60: 0x8227d690,
    }
    for va, expected in words.items():
        data = bytes.fromhex(expected)
        assert read(va, len(data)) == data, hex(va)
    for va, target in calls.items():
        op = struct.unpack(">I", read(va, 4))[0]
        assert op & 0xfc000003 == 0x48000001
        assert va + ((op & 0x03fffffc) ^ 0x02000000) - 0x02000000 == target, hex(va)
    return {"raw": front["pin"](voice["ARTIST"]), "windows": {hex(k): v for k, v in words.items()},
            "calls": {hex(k): hex(v) for k, v in calls.items()}}


def machine_witnesses(bodies, word, call, local, require_snapshot):
    def fixed(key, offset, pattern):
        values = [int(x, 16) for x in pattern.split()]
        assert [word(key, offset + 4 * i) for i in range(len(values))] == values, (key, hex(offset))

    def single(key, prefix):
        [offset] = [off for off, name in bodies[key][1].items() if name.startswith(prefix)]
        return offset, call(key, offset, prefix)

    def stack_name(key, offset, prefix, slot=None):
        result = call(key, offset, prefix)
        nearby = [word(key, off) for off in range(max(0, offset - 52), offset, 4)]
        if slot is None:
            [slot] = {op & 0xffff for op in nearby[-7:] if op & 0xffff0000 == 0x38810000}
        assert nearby.count(0x38810000 | slot) == 1, (key, hex(offset), "Name address")
        assert any(op & 0xfc1fffff == 0x90010000 | slot for op in nearby), (key, hex(offset), "Name value")
        return result

    fixed("music_alloc", 0x1c, "38600250")
    call("music_alloc", 0x24, "??2MemBase@")
    call("music_alloc", 0x30, "??0MusicEffect@")
    fixed("music_ctor", 0x30, "387f0050")
    for offset, field in ((0x60, 0x50), (0x68, 0xb0), (0x70, 0x110), (0x78, 0x170)):
        if offset != 0x60:
            fixed("music_ctor", offset - 4, f"{0x387f0000 | field:08x}")
        call("music_ctor", offset, "??0MusicStream@")
    fixed("music_prepare", 0x18, "d00b0040 908b0054 d00b0044 914b003c d00b0048 994b0059 d00b004c 994b005a")
    fixed("music_prepare", 0x3c, "994b005b d1ab0050 994b005c")
    fixed("music_queue", 0x18, "7c9d2378")
    fixed("music_queue", 0x8c, "817d0000 917f0020")
    queue_calls = {}
    for key, expected in {
        "music_notify": [(0x6d4, 0x50), (0x74c, 0x54)], "music_preview": [(0x1d8, 0x50)],
        "music_params": [(0x50c, 0x68), (0x5f0, 0x64), (0x6c8, 0x60), (0x790, 0x6c),
                         (0x8fc, 0x54), (0xb20, 0x58), (0xbe0, 0x5c)],
    }.items():
        assert [off for off, name in bodies[key][1].items() if name.startswith("?Queue@MusicStream@")] == [off for off, _ in expected]
        queue_calls[key] = [stack_name(key, off, "?Queue@MusicStream@", slot) for off, slot in expected]
    hud_queue, _ = single("hud_slipstream", "?Queue@MusicStream@")
    queue_calls["hud_slipstream"] = [stack_name("hud_slipstream", hud_queue, "?Queue@MusicStream@")]
    call("music_attach", 0x38, "?GetStateManager@Environment@")
    fixed("music_attach", 0x44, "907f022c")
    update_calls = []
    for offset, stream in ((0xc50, 0xac), (0xc60, 0x4c), (0xc70, 0x10c), (0xc80, 0x16c)):
        fixed("music_params", offset, f"{0x387e0000 | stream:08x} 809e022c fc20f890")
        update_calls.append(call("music_params", offset + 0xc, "?Update@MusicStream@"))
    for key in ("find_slot", "index_slot", "find_send"):
        fixed(key, 0x10, "7c9a2378")
        fixed(key, 0x5c, "815a0000")
    voice_calls = {
        "logic_to_module": call("logic_connect", 0x74, "?ConnectVoice@Module@"),
        "module_to_playback": stack_name("module_connect", 0x104, "?Connect@Voice@Playback@", 0x58),
        "connect_to_send": stack_name("playback_connect", 0x24, "?FindNamedSend@Voice@Playback@", 0x50),
        "rwac_find_slot": stack_name("rwac_create", 0xc8, "?FindNamedSlot@Voice@Playback@", 0x50),
        "rwac_index_slot": stack_name("rwac_config", 0x40c, "?GetIndexOfSlot@Voice@Playback@", 0x74),
    }

    fixed("hud_alloc", 0x1c, "386004d8")
    call("hud_alloc", 0x24, "??2MemBase@")
    call("hud_alloc", 0x30, "??0HUDEffect@")
    fixed("hud_ctor", 0x144, "387e03a0")
    call("hud_ctor", 0x148, "??0MusicStream@")
    fixed("hud_ctor", 0x150, "397e0400")
    fixed("hud_ctor", 0x178, "387e0430")
    fixed("hud_attach", 0xd8, "387f039c")
    call("hud_attach", 0xe8, "?Prepare@MusicStream@")
    call("hud_params", 0x2c, "?UpdateSlipStreaming@HUDEffect@")
    assert any(word("hud_slipstream", off) == 0x38c00011 for off in range(hud_queue - 24, hud_queue, 4))
    hud_play, _ = single("hud_slipstream", "?PlaySound@HUDEffect@")
    assert hud_play == hud_queue + 16
    fixed("hud_slipstream", hud_play - 12, "38a00011 38800018")
    hud_manager, _ = single("hud_slipstream", "?GetStateManager@Environment@")
    fixed("hud_slipstream", hud_manager - 8, "38800006")
    hud_update, _ = single("hud_slipstream", "?Update@MusicStream@")
    assert hud_update == hud_manager + 16
    fixed("hud_slipstream", hud_update - 12, "7c641b78")
    assert word("hud_slipstream", hud_update - 8) & 0xffe0ffff == 0x386003a0
    hud_amount, _ = single("hud_slipstream", "?GetSlipstreamAmount@HUDEffect@")
    assert word("hud_slipstream", hud_amount + 8) & 0xffe0ffff == 0x38600400
    assert any(word("hud_slipstream", off) & 0xfc00ffff == 0xc000042c
               for off in range(hud_amount + 12, hud_amount + 84, 4))
    fixed("hud_amount", 0x30, "11a0694a")
    fixed("hud_amount", 0x60, "118a626f")
    fixed("hud_amount", 0x6c, "11ab6b2e")
    for key in ("hud_amount", "hud_slipstream"):
        code = bodies[key][0]
        [estimate] = [4 * i for i, (op,) in enumerate(struct.iter_unpack(">I", code))
                      if (op & 0xfc1f07ff) == 0x1000014a]
        if key == "hud_slipstream":
            assert estimate < hud_amount
            fixed(key, estimate + 12, "118a626f 100b032e")
    snapshot = {}
    if require_snapshot:
        inputs = [off for off, name in bodies["hud_slipstream"][1].items() if name.startswith("?GetBrnInputStructure@")]
        assert len(inputs) == 2 and inputs[0] < hud_queue < hud_update < inputs[1]
        assert word("hud_slipstream", hud_update + 4) & 0xfc00ffff == 0x80000008
        hud_get, _ = single("hud_slipstream", "?GetVehicleInterface@RootInputBuffer@")
        hud_copy, copy_binding = single("hud_slipstream", "??0RCEntityActiveRaceCarOutputInterface@")
        assert hud_get == inputs[1] + 4 and hud_copy == hud_get + 12
        fixed("hud_slipstream", hud_copy - 8, "7c641b78")
        snapshot_pointer = word("hud_slipstream", hud_copy - 4)
        assert snapshot_pointer & 0xffff0000 == 0x38610000
        methods = []
        for prefix in ("?IsPlayerCarActive@", "?IsRaceCarActive@", "?GetPlayerRaceCarState@", "?GetRaceCarStateMutable@", "?GetPlayerPosition@"):
            offset, binding = single("hud_slipstream", prefix)
            assert hud_copy < offset < hud_amount
            reg = 0x38810000 if prefix == "?GetPlayerPosition@" else 0x38610000
            assert any(word("hud_slipstream", arg) == reg | (snapshot_pointer & 0xffff)
                       for arg in range(offset - (72 if prefix == "?GetPlayerPosition@" else 4), offset, 4))
            methods.append((offset, binding))
        assert [offset for offset, _ in methods] == sorted(offset for offset, _ in methods)
        snapshot = {"stack_offset": hex(snapshot_pointer & 0xffff), "copy_call": copy_binding,
                    "queries": [binding for _, binding in methods]}
    call("hud_cos_init", 0x24, "?XMVectorCos@@")
    fixed("hud_cos_init", 0x18, "d0010050 7c00540e 1020028c")
    fixed("hud_scale_init", 0x14, "eda06828 ec006824")

    fixed("inair_detach", 0x18, "817f0024 2b0b0003 41990088 3bc00000")
    for index, target in enumerate((0x50, 0x54, 0x88, 0x90)):
        local("inair_detach", 0x40 + index * 4, target)
    fixed("inair_detach", 0x50, "93df0024 39600001 387f0040 917f0024")
    release_calls = []
    for off, member, symbol in ((0x60, 0x40, "?Release@VoiceWrapper@"), (0x68, 0x90, "?Release@VoicePoolBase@"),
                                (0x70, 0x264, "?Release@VoiceWrapper@"), (0x78, 0x2b4, "?Release@VoiceWrapper@"),
                                (0x80, 0x214, "?Release@VoiceWrapper@")):
        fixed("inair_detach", off - (8 if off == 0x60 else 4), f"{0x387f0000 | member:08x}")
        release_calls.append(call("inair_detach", off, symbol))
    fixed("inair_detach", 0x84, "93df03b4 39600002 917f0024 39600003 7fe3fb78 917f0024")
    call("inair_detach", 0x9c, "?Detach@BrnEffectObject@")
    fixed("inair_detach", 0xa0, "38600001 48000008 38600000")
    return {
        "queue_calls": queue_calls, "voice_name_calls": voice_calls,
        "music": {"effect_size": "0x250", "stream_size": "0x60", "complete_manager_offset": "0x230",
                  "stream_update_order_adjusted": ["0xac", "0x4c", "0x10c", "0x16c"], "update_calls": update_calls},
        "hud": {"allocation": "0x4d8", "stream": "0x3a0", "average": "0x400", "game_mode": "0x430",
                "normalizations": ["slipstream relative position", "front-car direction"], "refinements": 1,
                "call_offsets": {"queue": hud_queue, "update": hud_update, "amount": hud_amount}, "vehicle_snapshot": snapshot},
        "inair": {"state_slots": ["0x50", "0x54", "0x88", "0x90"], "release_order": ["0x40", "0x90", "0x264", "0x2b4", "0x214"],
                  "release_calls": release_calls, "clear_collision": "0x3b4", "base_then_true": True, "default_false": True},
    }


def parse_new(inherited, h, rows):
    methods = {}
    for source, names in method_sources(rows).items():
        coff = inherited["frozen_obj"](h, rows[source])
        for key, prefix in names.items():
            methods[key] = source, coff, coff.function(prefix)
        if Path(source).stem == "BrnHUDEffect":
            for name, key in (("gfSlipstreamConeCos", "hud_cos_init"), ("gfSlipstreamScale", "hud_scale_init")):
                methods[key] = source, coff, inherited["format_function"](coff, HUD_GLOBALS[name][0])
    return methods


def preflight(inherited, h, rows, require_snapshot):
    parsed = parse_new(inherited, h, rows)
    hud_crt_object(parsed["hud_cos_init"][1])
    bodies = {key: (body, {off: ref.name for off, (_, ref) in refs.items()})
              for key, (_, _, (_, body, refs)) in parsed.items()}

    def word(key, offset):
        return h.u32(bodies[key][0], offset)

    def call(key, offset, prefix):
        name = bodies[key][1][offset]
        assert name.startswith(prefix) and word(key, offset) & 0xfc000003 == 0x48000001
        return name

    def local(key, offset, dest):
        _, _, (symbol, _, refs) = parsed[key]
        kind, target = refs[offset]
        assert kind == 2 and target.section == symbol.section and target.value == dest

    return machine_witnesses(bodies, word, call, local, require_snapshot)


def carry_preflight(inherited, voice, h, prior, rows):
    old_sources = {Path(pin["selected"]["object"]).name: pin["selected"] for pin in prior["selected"].values()}
    carried = {}
    for key, proof in prior["functions"].items():
        origin = old_sources[proof["provider"]]
        if key == "inair_detach" or origin["source"] not in rows:
            continue
        old = h.Coff(h.pinned(origin["object"], origin["sha256"]))
        compiled = inherited["frozen_obj"](h, rows[origin["source"]])
        _, code, _ = voice["compare"](h, key, compiled, old, proof["symbol"])
        assert h.sha(code) == proof["compiled_sha256"], key
        carried[key] = proof["compiled_sha256"]
    assert len(carried) == 20
    return carried


def data_operand(code, hi, lo, h):
    upper, lower = h.u32(code, hi), h.u32(code, lo)
    assert upper >> 26 == 15 and (upper >> 16 & 31) == 0
    assert upper >> 21 & 31 == lower >> 16 & 31
    return (((upper & 0xffff) << 16) + h.sign(lower & 0xffff, 16)) & 0xffffffff


def bind_music_locals(rows, read, entries, h):
    mapped = {}
    for key in ("music_attach", "music_params"):
        owner, coff, (symbol, code, refs) = rows[key]
        address = h.map_addr(entries, symbol.name, owner)
        linked = read(address, len(code), executable=True)
        wanted = {target for _, target in refs.values() if target.name in MUSIC_LOCAL and target.name not in entries}
        if not wanted:
            continue
        data = h.local_data_destinations(read, coff, symbol.section, code, refs, entries, linked, {})
        for target in wanted:
            mapped[target.name] = {(data[target], owner)}
    return mapped


def hud_crt_object(coff):
    symbols = []
    for name, (initializer, _, _) in HUD_GLOBALS.items():
        symbol = coff.definition(name + "$initializer$")
        section, data, start, count = coff.section(symbol.section)
        assert section == b".CRT$XCU" and data[symbol.value:symbol.value + 4] == bytes(4)
        [target] = [coff.symbols[index].name for offset, index, kind in coff.section_relocations(start, count)
                    if offset == symbol.value and kind == 2]
        assert target == initializer
        symbols.append(symbol)
    cos, scale = symbols
    assert scale.section == cos.section and scale.value == cos.value + 4
    return [cos.value, scale.value]


def bind_hud_globals(rows, read, entries, writable, h):
    mapped = {}
    section_base = None
    for name, key in (("gfSlipstreamConeCos", "hud_cos_init"), ("gfSlipstreamScale", "hud_scale_init")):
        owner, coff, (symbol, code, refs) = rows[key]
        _, hi, lo = HUD_GLOBALS[name]
        target = refs[hi][1]
        assert refs[hi] == (0x10, target) and refs[lo] == (0x11, target)
        assert target.name == name and coff.section(target.section)[0] == b".bss"
        address = h.map_addr(entries, symbol.name, owner)
        actual = data_operand(read(address, len(code), executable=True), hi, lo, h)
        assert writable(actual)
        base = (target.section, actual - target.value)
        if section_base is None:
            section_base = base
        assert base == section_base
        if name in entries:
            assert h.map_addr(entries, name) == actual
        mapped[name] = {(actual, owner)}
    assert mapped["gfSlipstreamConeCos"] != mapped["gfSlipstreamScale"]
    return mapped


def bind_rwac_globals(inherited, row, read, entries, check, writable, h):
    owner, coff, _ = row
    resolved, initializers, bases = {}, {}, set()
    for name, literal in RWAC_PRIVATE.items():
        function, code, refs = inherited["format_function"](coff, "??__E" + name + "@")
        target = refs[0x18][1]
        assert refs[0x18] == (0x10, target) and refs[0x1c] == (0x11, target)
        assert target.name == name and coff.section(target.section)[0] == b".bss"
        address = h.map_addr(entries, function.name, owner)
        linked = read(address, len(code), executable=True)
        dest = data_operand(linked, 0x18, 0x1c, h)
        assert writable(dest) and h.u32(linked, 0x1c) & 0xffff0000 == 0x906b0000
        if name in entries:
            assert h.map_addr(entries, name) == dest
        bases.add((target.section, dest - target.value))
        resolved[name] = {(dest, owner)}
        receipt = check("rwac_initializer", coff, function, code, refs, address, entries | resolved)
        assert read(int(receipt["bindings"]["0x10"]["target"], 16), len(literal)) == literal
        initializers[name] = receipt
    assert len(bases) == 1
    return resolved, initializers


def bind(args, inherited, voice, front, h, prior, rows, inputs):
    parser = runpy.run_path(str(front["READER"]))
    reader = front["body_proof"]
    reader.__globals__["PRIVATE_DATA"].update({"stream": ("__real@3e800000", "__real@3dcccccd"),
                                              "params_update": ("__real@3fa00000",),
                                              "hud_cos_init": ("__real@3eb126e9",),
                                              "hud_scale_init": ("__real@3f800000",)})
    entries, read = dict(inputs["entries"]), inputs["read"]
    private, owners, methods, proofs, retained = {}, {}, {}, {}, {}
    prior_by_filename = {Path(pin["selected"]["object"]).name: pin["selected"] for pin in prior["selected"].values()}
    for source in rows:
        owners[source] = voice["selected_coff"](h, args, inputs, source, rows[source])
    for key, (source, frozen, (old_symbol, old_body, old_refs)) in parse_new(inherited, h, rows).items():
        owner, selected, _ = owners[source]
        if key in ("hud_cos_init", "hud_scale_init"):
            current = inherited["format_function"](selected, old_symbol.name)
            assert (current[0].name, current[1]) == (old_symbol.name, old_body)
            assert h.ref_signature(current[0], current[2]) == h.ref_signature(old_symbol, old_refs)
        elif key == "rwac_config":
            current = selected.function(old_symbol.name)
            symbol, code, refs = current
            assert (symbol.name, code) == (old_symbol.name, old_body)
            before, after = h.ref_signature(old_symbol, old_refs), h.ref_signature(symbol, refs)
            path_offsets = {
                0xd8: "RWAC/CgsGenericRwacVoiceConfig.h", 0xe0: "RWAC/CgsGenericRwacVoiceConfig.h",
                0xf8: "CgsDataStructures.h", 0x100: "CgsDataStructures.h",
            }
            for off, suffix in path_offsets.items():
                kind = 0x10 if off in (0xd8, 0xf8) else 0x11
                relative = "/src/GameShared/GameClasses/Sound/Playback/" + suffix
                for coff, bindings, root in (
                    (frozen, old_refs, "C:/Users/adri1/.codex/worktrees/bp360/debug_format_luna"),
                    (selected, refs, inputs["root"].as_posix()),
                ):
                    assert bindings[off][0] == kind
                    literal = voice["symbol_bytes"](coff, bindings[off][1])
                    assert literal[:-1].decode().replace(chr(92), "/") == root + relative
                before[off] = after[off]
            assert before == after
        else:
            current = voice["compare"](h, key, selected, frozen, old_symbol.name)
        methods[key] = owner, selected, current
    hud_refs = methods["hud_slipstream"][2][2]
    base_seal = front["load"](WAVE / "r151-link/seal.json", BASE_SEAL)
    base_sources = {row["source"]: row for row in base_seal["selected"]}
    for key, prefix in (("snapshot_copy", "??0RCEntityActiveRaceCarOutputInterface@"),
                        ("snapshot_getter", "?GetRaceCarStateMutable@RCEntityActiveRaceCarOutputInterface@")):
        [symbol] = [target.name for kind, target in hud_refs.values() if kind == 6 and target.name.startswith(prefix)]
        [owner] = {provider for _, provider in entries[symbol]}
        [source] = [path for path, row in inputs["objects"].items() if Path(row["object"]).name == owner]
        assert Path(source).stem == "BrnRCEntityActiveRaceCarOutputInterface"
        if source not in owners:
            owners[source] = voice["selected_coff"](h, args, inputs, source, rows.get(source))
        _, selected, _ = owners[source]
        baseline = base_sources[source]
        old = h.Coff(h.pinned(baseline["object"], baseline["sha256"]))
        methods[key] = owner, selected, voice["compare"](h, key, selected, old, symbol)
    for key, old_method in prior["functions"].items():
        if key == "inair_detach":
            continue
        previous_row = prior_by_filename[old_method["provider"]]
        source = previous_row["source"]
        if source not in owners:
            owners[source] = voice["selected_coff"](h, args, inputs, source)
        owner, selected, _ = owners[source]
        old_coff = h.Coff(h.pinned(previous_row["object"], previous_row["sha256"]))
        current = voice["compare"](h, key, selected, old_coff, old_method["symbol"])
        assert h.sha(current[1]) == old_method["compiled_sha256"]
        methods[key] = owner, selected, current
        retained[key] = old_method["compiled_sha256"]

    def check(key, coff, symbol, code, refs, address, bindings):
        return reader(read, bindings, coff, key, symbol, code, refs, address, private, parser)

    image = h.pefile.PE(str(LINK / "native-game.pe"), fast_load=True)

    def writable(address):
        rva = address - image.OPTIONAL_HEADER.ImageBase
        return any(section.VirtualAddress <= rva and rva + 4 <= section.VirtualAddress + section.Misc_VirtualSize
                   and section.Characteristics & 0xa0000000 == 0x80000000 for section in image.sections)

    owner, coff, _ = methods["inair_attach"]
    globals_, inair_initializers = inherited["bind_inair_globals"](coff, read, entries, owner, check, writable, h)
    entries.update(globals_)
    rwac_globals, rwac_initializers = bind_rwac_globals(inherited, methods["rwac_config"], read, entries, check, writable, h)
    entries.update(rwac_globals)
    hud_crt_object(methods["hud_cos_init"][1])
    entries.update(bind_hud_globals(methods, read, entries, writable, h))
    entries.update(bind_music_locals(methods, read, entries, h))
    for key, (owner, coff, (symbol, code, refs)) in methods.items():
        address = h.map_addr(entries, symbol.name, owner)
        row = check(key, coff, symbol, code, refs, address, entries)
        row["provider"] = owner
        row["compiled_sha256"] = h.sha(code)
        proofs[key] = row

    # Bind the R151 vtable slots again using the R152 selected providers.
    for key, table in prior["vtables"].items():
        target = {"streaming_effect_dtor": "streaming_dtor_thunk", "presentation_create_params": "params_get",
                  "presentation_attach": "attach"}[key]
        owner, coff, _ = methods[target]
        offset = 0x14 if key == "presentation_attach" else 0
        expected = coff.vtable_slot(table["symbol"], offset)
        slot = h.map_addr(entries, table["symbol"], owner) + offset
        assert expected == proofs[target]["symbol"] and h.u32(read(slot, 4), 0) == int(proofs[target]["address"], 16)
    return proofs, owners, retained, {"inair": inair_initializers, "rwac": rwac_initializers}, entries


def linked_witnesses(inputs, proofs, entries, h):
    read = inputs["read"]

    def code(key, off, size):
        return read(int(proofs[key]["address"], 16) + off, size, executable=True)

    bodies = {key: (code(key, 0, row["bytes"]), {int(off, 16): ref["symbol"] for off, ref in row["bindings"].items()})
              for key, row in proofs.items()}

    def word(key, off):
        return h.u32(code(key, off, 4), 0)

    def call(key, off, prefix):
        ref = proofs[key]["bindings"][hex(off)]
        assert ref["symbol"].startswith(prefix) and word(key, off) & 0xfc000003 == 0x48000001
        return ref

    def local(key, offset, target):
        assert word(key, offset) == int(proofs[key]["address"], 16) + target

    def literal(key, hi, lo, payload):
        first, second = (proofs[key]["bindings"][hex(off)] for off in (hi, lo))
        assert (first["kind"], second["kind"]) == ("0x10", "0x11")
        assert (first["symbol"], first["target"]) == (second["symbol"], second["target"])
        assert read(int(second["target"], 16), len(payload)) == payload

    result = machine_witnesses(bodies, word, call, local, require_snapshot=True)
    for values in result["queue_calls"].values():
        assert all(row["target"] == proofs["music_queue"]["address"] for row in values)
    assert all(row["target"] == proofs["music_update"]["address"] for row in result["music"]["update_calls"])
    hud_offsets = result["hud"]["call_offsets"]
    for source, offset, target in (("hud_slipstream", hud_offsets["update"], "music_update"),
                                    ("hud_slipstream", hud_offsets["amount"], "hud_amount"),
                                    ("hud_params", 0x2c, "hud_slipstream"), ("hud_attach", 0xe8, "music_prepare"),
                                    ("music_alloc", 0x30, "music_ctor"), ("hud_alloc", 0x30, "hud_ctor")):
        assert proofs[source]["bindings"][hex(offset)]["target"] == proofs[target]["address"]
    for key, target in (("logic_to_module", "module_connect"), ("module_to_playback", "playback_connect"),
                        ("connect_to_send", "find_send"), ("rwac_find_slot", "find_slot"), ("rwac_index_slot", "index_slot")):
        assert result["voice_name_calls"][key]["target"] == proofs[target]["address"]
    snapshot = result["hud"]["vehicle_snapshot"]
    assert snapshot["copy_call"]["target"] == proofs["snapshot_copy"]["address"]
    assert snapshot["queries"][3]["target"] == proofs["snapshot_getter"]["address"]
    assert result["inair"]["release_calls"][1]["target"] == proofs["pool_release"]["address"]
    assert all(result["inair"]["release_calls"][i]["target"] == proofs["release"]["address"] for i in (0, 2, 3, 4))
    literal("hud_cos_init", 0xc, 0x14, bytes.fromhex("3eb126e9"))
    literal("hud_scale_init", 0, 4, bytes.fromhex("3f800000"))
    literal("hud_slipstream", hud_offsets["queue"] - 0x20, hud_offsets["queue"] - 0x1c, b"SlipStream\0")
    for key, hi, lo, payload in (
        ("stream", 0x5c, 0x60, b"MusicVoiceSpec\0"), ("stream", 0x78, 0x88, b"Send01\0"),
        ("stream", 0xa4, 0xb0, bytes.fromhex("3e800000")), ("stream", 0xd0, 0xe0, bytes.fromhex("3dcccccd")),
        ("params_update", 0x14, 0x28, bytes.fromhex("3fa00000")), ("params_update", 0x128, 0x12c, b"OnlineEntry\0"),
        ("play", 0xb8, 0xbc, b"StereoWavVoiceSpec\0"), ("handle", 0x30, 0x34, b"mpObject\0"),
    ):
        literal(key, hi, lo, payload)
    crt_start, crt_end = (h.map_addr(entries, name) for name in ("__xc_a", "__xc_z"))
    assert crt_start < crt_end and (crt_end - crt_start) % 4 == 0
    words = [value for (value,) in struct.iter_unpack(">I", read(crt_start, crt_end - crt_start))]
    cos, scale = (int(proofs[key]["address"], 16) for key in ("hud_cos_init", "hud_scale_init"))
    assert words.count(cos) == words.count(scale) == 1 and words.index(scale) == words.index(cos) + 1
    result["hud"]["crt_slots"] = [hex(crt_start + 4 * words.index(value)) for value in (cos, scale)]
    result["hud"]["globals"] = {name: hex(h.map_addr(entries, name)) for name in HUD_GLOBALS}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preflight", action="store_true")
    parser.add_argument("--first-freeze", action="store_true", help="Recheck the superseded preflight before the HUD copy fix")
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--targeted", type=Path, default=TARGETED)
    parser.add_argument("--intake", type=Path, default=INTAKE)
    for key in ("candidate", "tree", "manifest-sha", "targeted-sha", "head", "seal-sha", "xex-sha", "pe-sha", "map-sha"):
        parser.add_argument("--" + key)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    assert all((args.candidate, args.tree, args.manifest_sha, args.targeted_sha))
    inherited, voice, front, h, prior = helpers()
    manifest, rows = frozen_inputs(args, front, h)
    hud = front["load"](HUD_PROOF, HUD_PROOF_SHA)
    assert [hud["implementation"]["layout"][key] for key in ("streamOffset", "averageOffset", "gameModeDataOffset", "objectAllocationSize")] == [
        "+0x3A0", "+0x400", "+0x430", "0x4D8"
    ]
    raw = original(front, voice)
    assert not args.first_freeze or (args.preflight and args.candidate == "03c82845bfd86199130db273aaf446cffac80a6c")
    if not args.first_freeze:
        root = Path(front["load"](WAVE / "r151-link/seal.json", BASE_SEAL)["source_root"])
        [hud_source] = [source for source in rows if Path(source).stem == "BrnHUDEffect"]
        text = h.git(root, "show", f"{args.candidate}:{hud_source}").decode()
        body = text.split("void HUDEffect::UpdateSlipStreaming(", 1)[1].split("\nvoid HUDEffect::", 1)[0]
        assert "const_cast" not in body and "GetRaceCarStateMutable" in body and "lrVehicles" in body
        snapshot = front["load"](SNAPSHOT_PROOF, SNAPSHOT_PROOF_SHA)
        assert snapshot["repository"]["committedFile"]["blob"] == rows[hud_source]["git_blob"]
    targeted = preflight(inherited, h, rows, require_snapshot=not args.first_freeze)
    targeted["r151_bodies_in_targeted_objects"] = carry_preflight(inherited, voice, h, prior, rows)
    if args.preflight:
        assert not args.write
        print(json.dumps(targeted, indent=2))
        return
    assert all((args.head, args.seal_sha, args.xex_sha, args.pe_sha, args.map_sha))
    inputs = seal_inputs(args, manifest, front, h)
    proofs, owners, retained, initializers, entries = bind(args, inherited, voice, front, h, prior, rows, inputs)
    witnesses = linked_witnesses(inputs, proofs, entries, h)
    receipt = {
        "state": "pass", "head": args.head, "tree": args.tree, "base": BASE, "candidate": args.candidate,
        "xex_sha256": inputs["image"]["xex_sha256"], "image": inputs["image"],
        "manifest": front["pin"](args.manifest), "targeted": front["pin"](args.targeted), "verifier": front["pin"](Path(__file__)),
        "r151_audio": front["pin"](OLD_RECEIPT), "hud_layout_preflight": front["pin"](HUD_PROOF), "hud_snapshot_preflight": front["pin"](SNAPSHOT_PROOF), "seal_inputs": inputs["pins"],
        "selected": {source: row for source, (_, _, row) in owners.items()}, "functions": proofs,
        "retained_r151_bodies": retained, "private_initializers": initializers, "artist": raw, "witnesses": witnesses,
        "scope": "Selected Xbox PPC audio ABI and lifecycle checks. Runtime and the unrelated Send01 provider difference remain outside scope.",
    }
    if args.write:
        out = HERE / "R152_AUDIO_LINKED.json"
        with out.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
        print(front["pin"](out))
    print(f"PASS {args.head}: {len(proofs)} linked bodies, {len(retained)} retained from R151")


if __name__ == "__main__":
    main()
