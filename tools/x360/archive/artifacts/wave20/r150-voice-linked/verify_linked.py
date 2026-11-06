"""Verify the reviewed R150 native voice layout against the selected linked PPC."""
import argparse
import json
from pathlib import Path
import runpy
import struct

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
LINK = WAVE / "r150-link"
BASE = "28769133a5e88f5cc530003d41b059022fb557f6"
CANDIDATE = "bc30792cd4dea0d57746da746209563d97c9bb2c"
TREE = "943b2311c7a8592ca02ae848d589669b55aa4745"
MANIFEST_SHA = "331da34c84aa87323a2112f31510c77796ccc148fa40740d615f9665f645dc1c"
TARGETED_SHA = "392faf238b6c46ca313e1afb118a96014c55382441f1350f8363890620f38577"
BASE_SEAL_SHA = "3aaf4324436412210e31653f70d7dbc6cb3fd52bb12e9f9027277634e632626f"
FRONTEND = WAVE / "r149-frontend-linked/verify_linked.py"
BOOT = WAVE / "r149-boot-presentation-regression/verify_linked.py"
ARTIST = WAVE / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex"
ARTIST_SHA = "6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655"
HELPER_PINS = {
    FRONTEND: "bacec188cc23a9472e70f886b7629d6d53e7adff6f5589082c91f1182e9d69f9",
    BOOT: "4a1a0fa921099a75dd6791c143d20bfb3983fb322bc79dfa90b61d5447b4d334",
}
HANDLE = "??C?$Handle@UContent@Playback@CgsSound@@@Playback@CgsSound@@QAAPAUContent@12@XZ"
STREAM = "?PlayStream@PresentationEffect@Logic@BrnSound@@AAAXI@Z"
GROUPS = {
    "CgsVoiceWrapper": {
        "create": "?Create@VoiceWrapper@", "update": "?Update@VoiceWrapper@",
        "release": "?Release@VoiceWrapper@", "voice_play": "?Play@VoiceWrapper@",
    },
    "BrnPresentationEffect": {
        "stream": STREAM, "params_update": "?UpdateParams@PresentationEffect@",
        "params_get": "?GetCreateParams@PresentationEffect@", "find": "?FindOrStealAVoice@PresentationEffect@",
        "allocate": "?CreateObject@PresentationEffect@", "attach": "?Attach@PresentationEffect@",
        "play": "?Play@PresentationEffect@", "ctor": "??0PresentationEffect@",
    },
    "CgsVoicePool": {"pool_prepare": "?Prepare@VoicePoolBase@", "pool_update": "?Update@VoicePoolBase@"},
    "BrnFxEffect": {"fx_detach": "?Detach@FxEffect@"},
}


def helpers():
    front = runpy.run_path(str(FRONTEND))
    for path, digest in HELPER_PINS.items():
        front["pinned"](path, digest)
    boot = runpy.run_path(str(BOOT))
    return front, boot["helpers"]()


def pinned_inputs(args, front, h):
    manifest = front["load"](args.manifest, args.manifest_sha)
    assert manifest["base"] == BASE and manifest["projected_tree"] == args.tree
    assert manifest["candidate"] == args.candidate == CANDIDATE
    pin = next(row for row in manifest["composed_receipts"] if Path(row["path"]).name == "TARGETED_OBJECT_BINDING.json")
    assert pin["sha256"] == TARGETED_SHA
    targeted = front["from_pin"](pin)
    for report in targeted["compile_reports"]:
        front["from_pin"](report)
    assert (targeted["head"], targeted["tree"]) == (args.candidate, args.tree)
    rows = {Path(row["source"]).stem: row for row in targeted["results"]}
    assert rows.keys() >= GROUPS.keys() and len(rows) == 8
    for row in rows.values():
        assert row["machine"] == 0x1f2
        for artifact in row["artifacts"].values():
            h.pinned(artifact["path"], artifact["sha256"])
    return manifest, rows, pin


def seal_inputs(args, manifest, front, h):
    result = h.read_json(LINK / "result.json")
    image = h.read_json(LINK / "image-verification.json")
    invoke = h.read_json(LINK / "invocation.json")
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] and image["head"] == args.head
    seal_path = LINK / "seal.json"
    assert Path(invoke["sealed"]) == seal_path
    seal = front["load"](seal_path, invoke["sealed_sha256"])
    assert (seal["schema"], seal["head"]) == ("r150-native-link-seal-v1", args.head)
    release_path = LINK / "ROOT_RELEASE.json"
    assert Path(invoke["release"]) == release_path
    assert front["load"](release_path, invoke["release_sha256"]) == {
        "action": "one strict R150 native link", "sealed_sha256": invoke["sealed_sha256"]
    }
    for key, sealed_key in (("objects", "selected"), ("excluded", "excluded"), ("libraries", "libraries"),
                            ("sources", "sources"), ("tool_files", "tool_files")):
        assert invoke[key] == seal[sealed_key]
    rsp = LINK / "r150-strict.rsp"
    assert invoke["command"][1:] == ["@" + str(rsp)]
    args_text = h.pinned(rsp, invoke["rsp_sha256"]).decode().splitlines()
    assert [line for line in args_text if line.lower().endswith('.obj"')] == [
        f'"{row["object"]}"' for row in seal["selected"]
    ]
    assert h.file_sha(LINK / "link.log") == result["log_sha256"]
    diag = h.read_json(LINK / "diagnostic-parsed.json")
    assert diag["unique_symbols"] == diag["unresolved_messages"] == 0 and not diag["duplicate_messages"]
    assert (len(seal["selected"]), len(seal["excluded"])) == (3589, 121)
    order = sorted((h.norm(row["source"]), h.norm(row["object"]), row["sha256"]) for row in seal["selected"])
    digest = h.sha(json.dumps(order, separators=(",", ":")).encode())
    assert digest == seal["selected_objects_sha256"]
    boundary = front["from_pin"](seal["boundary"])
    assert (boundary["schema"], boundary["head"], boundary["selected_objects_sha256"]) == (
        "r150-link-boundary-v1", args.head, digest
    )
    assert boundary["no_excluded_cohort_outside_references"] and not boundary["errors"]
    prior_path = WAVE / "r149-link/seal.json"
    assert seal["prior_successful_seal"] == manifest["base_seal"]
    assert Path(seal["prior_successful_seal"]["path"]) == prior_path
    prior = front["load"](prior_path, BASE_SEAL_SHA)
    cohort = h.pinned_json(seal["provider_selection"], h.PROVIDER_SHA)["sources"]
    assert prior["head"] == BASE and cohort == prior["provider_cohort"] == seal["provider_cohort"] and len(cohort) == 15
    for group in ("selected", "excluded"):
        assert {row["source"] for row in prior[group]} == {row["source"] for row in seal[group]}
    excluded = {row["source"]: row for row in seal["excluded"]}
    for provider in cohort:
        actual = excluded[provider["source"]]
        assert (actual["object"], actual["sha256"]) == (provider["object"], provider["sha256"])
        h.pinned(actual["object"], actual["sha256"])

    root = Path(seal["source_root"])
    assert h.git(root, "rev-parse", f"{args.head}^{{tree}}", f"{args.head}^").decode().split() == [args.tree, BASE]
    assert h.git(root, "rev-parse", f"{args.candidate}^{{tree}}").decode().strip() == args.tree
    bind_path = WAVE / "r150-intake/commit-binding.json"
    binding = next(row for row in seal["provenance"] if Path(row["path"]) == bind_path)
    commit = front["from_pin"](binding)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (args.head, BASE, args.manifest_sha)
    paths = h.git(root, "diff", "--name-only", BASE, args.head).decode().splitlines()
    assert sorted(paths) == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    inventory = front["from_pin"](seal["source_inventory"])
    assert inventory["head"] == args.head and inventory["files"] == seal["sources"]
    report = front["from_pin"](seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    h.LINK = LINK
    read, entries, image_pins = h.load_image(result, image)
    return {
        "read": read, "entries": entries, "image": image_pins, "root": root,
        "sources": {h.norm(row["path"]): row["sha256"] for row in seal["sources"]},
        "objects": {row["source"]: row for row in seal["selected"]},
        "report": {row["source"]: row for row in report["results"]},
        "pins": {"seal": front["pin"](seal_path), "result": front["pin"](LINK / "result.json"),
                 "image_verification": front["pin"](LINK / "image-verification.json"), "release": front["pin"](release_path),
                 "response": front["pin"](rsp), "commit_binding": binding, "source_inventory": seal["source_inventory"],
                 "boundary": seal["boundary"], "report": seal["report"], "prior_seal": seal["prior_successful_seal"],
                 "provider_selection": seal["provider_selection"], "selected_objects_sha256": digest},
    }


def symbol_bytes(coff, target):
    _, data, _, _ = coff.section(target.section)
    return coff.literal(target, data.index(b"\0", target.value) + 1 - target.value)


def compare(h, method, actual_coff, frozen_coff, prefix):
    symbol, code, refs = actual_coff.function(prefix)
    old_symbol, old_code, old_refs = frozen_coff.function(prefix)
    assert h.stable(symbol.name) == h.stable(old_symbol.name) and code == old_code, method
    expected, actual = h.ref_signature(old_symbol, old_refs), h.ref_signature(symbol, refs)
    assert expected.keys() == actual.keys()
    for offset, signature in expected.items():
        if signature == actual[offset]:
            continue
        assert method == "handle" and offset in (0x24, 0x2c), (method, hex(offset), signature, actual[offset])
        kind, current = refs[offset]
        prior_kind, prior = old_refs[offset]
        assert kind == prior_kind and kind in (0x10, 0x11)
        for coff, target in ((actual_coff, current), (frozen_coff, prior)):
            assert symbol_bytes(coff, target).replace(b"\\", b"/").endswith(
                b"/src/GameShared/GameClasses/Sound/Playback/CgsHandle.h\0"
            )
    return symbol, code, refs


def selected_coff(h, args, inputs, source, proof=None):
    row = inputs["objects"][source]
    build = inputs["report"][row["source"]]
    assert build["object"] == row["object"] and build["passed"] and build["machine"] == 0x1f2
    source = row["source"]
    root = inputs["root"]
    blob_id = h.git(root, "rev-parse", f"{args.head}:{source}").decode().strip()
    source_data = h.pinned(root / source, inputs["sources"][h.norm(root / source)])
    assert source_data.replace(b"\r\n", b"\n") == h.git(root, "show", f"{args.head}:{source}").replace(b"\r\n", b"\n")
    obj = Path(row["object"])
    pin = {"selected": row, "blob": blob_id}
    if proof is not None:
        assert proof["source"] == source and proof["git_blob"] == blob_id
        assert Path(proof["artifacts"]["object"]["path"]).name == obj.name
        pin["targeted"] = proof["artifacts"]["object"]
    return obj.name, h.Coff(h.pinned(obj, row["sha256"])), pin


def bind(args, inputs, targeted, front, h):
    check = front["body_proof"]
    local = check.__globals__
    local["PRIVATE_DATA"].update({"stream": ("__real@3e800000", "__real@3dcccccd"),
                                  "params_update": ("__real@3fa00000",)})
    parser = runpy.run_path(str(front["READER"]))
    entries = dict(inputs["entries"])
    read = inputs["read"]
    proofs, pins, private = {}, {}, {}
    compiled = {}
    baseline = {}
    for stem, methods in GROUPS.items():
        provider, coff, pins[stem] = selected_coff(h, args, inputs, targeted[stem]["source"], targeted[stem])
        pin = targeted[stem]["artifacts"]["object"]
        old = h.Coff(h.pinned(pin["path"], pin["sha256"]))
        baseline[stem] = old
        for method, prefix in methods.items():
            compiled[method] = provider, coff, *compare(h, method, coff, old, prefix)

    if STREAM not in entries:
        provider, _, symbol, code, refs = compiled["play"]
        address = h.map_addr(entries, symbol.name, provider)
        [offset] = [off for off, (kind, target) in refs.items() if kind == 6 and target.name == STREAM]
        op = h.u32(read(address + offset, 4, executable=True), 0)
        assert op & 0xfc000003 == 0x48000001
        entries[STREAM] = {(address + offset + h.sign(op & 0x03fffffc, 26), provider)}

    # The Handle<Content> COMDAT can come from any of its actual selected owners.
    [owner] = {owner for _, owner in entries[HANDLE]}
    [handle_source] = [source for source, row in inputs["objects"].items() if Path(row["object"]).name == owner]
    provider, coff, handle_pin = selected_coff(h, args, inputs, handle_source)
    pins["handle_owner"] = handle_pin
    compiled["handle"] = provider, coff, *compare(h, "handle", coff, baseline["CgsVoiceWrapper"], HANDLE)

    for method, (provider, coff, symbol, code, refs) in compiled.items():
        address = h.map_addr(entries, symbol.name, provider)
        report = check(read, entries, coff, method, symbol, code, refs, address, private, parser)
        report["provider"] = provider
        report["compiled_sha256"] = h.sha(code)
        proofs[method] = report
    presentation_path = Path(pins["BrnPresentationEffect"]["selected"]["object"])
    presentation_targeted = pins["BrnPresentationEffect"]["targeted"]
    table_functions = {}
    for table_role, method in (("get_create_params", "params_get"), ("attach", "attach")):
        function = proofs[method]
        table_functions[table_role] = {"symbol": function["symbol"], "address": int(function["address"], 16)}
    tables = h.verify_presentation_vtables(
        read, entries, presentation_path, h.pinned(presentation_path, pins["BrnPresentationEffect"]["selected"]["sha256"]),
        h.pinned(presentation_targeted["path"], presentation_targeted["sha256"]), table_functions,
    )
    return proofs, pins, tables


def machine_witnesses(bodies, words, call, literal):
    def fixed(method, offset, expected):
        words(method, offset, bytes.fromhex(expected))

    def one_call(method, prefix):
        [offset] = [off for off, name in bodies[method][1].items() if name.startswith(prefix)]
        return offset, call(method, offset, prefix)

    fixed("create", 0x10, "38a00030 3bdf0004 7fc3f378")
    call("create", 0x1c, "memcpy")
    fixed("create", 0x20, "817f0040")
    call("create", 0x12c, "?CreateContent@")
    call("create", 0x140, "??4?$Handle@UContent@")
    call("create", 0x150, "?Release@Object@Playback@")
    call("create", 0x1b8, "??4?$Handle@UContent@")
    fixed("create", 0x1e4, "39600001 917f0048")
    fixed("release", 0x5c, "83df0040")
    call("release", 0x74, "?BeginRemove@Content@Playback@")
    call("release", 0x84, "?Release@Object@Playback@")
    fixed("release", 0x88, "39600000 917f0040 997f004c 917f0044 917f0048")
    fixed("voice_play", 0x18, "815f0048 997f004c")
    fixed("update", 0x134, "387f0040")
    handle = call("update", 0x138, HANDLE)
    fixed("update", 0x13c, "8963001e 556b067e 2b0b0003 409a0098")
    fixed("handle", 0x14, "817f0000 2b0b0000 409a0024")
    for offset, prefix in ((0x20, "?BeginAssert@"), (0x38, "?FireAssert@"), (0x3c, "?EndAssert@")):
        call("handle", offset, prefix)
    literal("handle", 0x30, 0x34, b"mpObject\0")
    fixed("handle", 0x40, "807f0000")
    fixed("fx_detach", 0x30, "3bdf0034 3be00004 7fc3f378")
    call("fx_detach", 0x3c, "?Release@VoiceWrapper@")
    fixed("fx_detach", 0x40, "3bffffff 3bde0050 2b1f0000 409affec")
    fixed("pool_prepare", 0x98, "7fe3fb78 d3ff0050")
    call("pool_prepare", 0xa0, "?Release@VoiceWrapper@")
    fixed("pool_prepare", 0xb0, "3b9c005c")
    fixed("pool_update", 0x88, "3bde005c")

    fixed("params_get", 0, "38630224 4e800020")
    fixed("find", 0x34, "394a0080")
    fixed("allocate", 0x1c, "386002a8")
    call("allocate", 0x24, "??2MemBase@CgsSound@")
    call("allocate", 0x30, "??0PresentationEffect@")
    fixed("ctor", 0xe4, "d00b0068 39290080 396b0080")
    fixed("ctor", 0x128, "93fe025c")
    fixed("ctor", 0x134, "387e028c")
    fixed("ctor", 0x168, "93fe029c 93fe02a0")
    assert not any((word & 0xfc000000) == 0x98000000 and (word & 0xffff) == 0x2a4
                   for (word,) in struct.iter_unpack(">I", bodies["ctor"][0]))
    fixed("attach", 0x74, "387f0288")
    fixed("attach", 0x7c, "39600009 93df0298 38600001 93df029c 997f02a0")

    # The manager null path asserts and falls through to the first parameter store.
    call("stream", 0x20, "?GetStateManager@")
    fixed("stream", 0x24, "7c7d1b78 2b1d0000 409a0024")
    for offset, prefix in ((0x30, "?BeginAssert@"), (0x48, "?FireAssert@"), (0x4c, "?EndAssert@")):
        assert one_call("stream", prefix)[0] == offset
    literal("stream", 0x40, 0x44, b"lpStreamingStateMan\0")
    stream, _ = bodies["stream"]
    stores = {}
    for offset in range(0, len(stream), 4):
        op = struct.unpack_from(">I", stream, offset)[0]
        if op >> 26 == 36 and (op >> 16 & 31) == 31 and 0x25c <= (op & 0xffff) <= 0x288:
            assert op & 0xffff not in stores
            stores[op & 0xffff] = offset
    assert stores == {0x25c: 0x50, 0x264: 0x68, 0x268: 0x74, 0x270: 0x7c, 0x274: 0x9c,
                      0x278: 0xbc, 0x27c: 0x94, 0x288: 0x98}
    for offset, pattern in {
        0x50: "93df025c", 0x68: "917f0264", 0x74: "907f0268", 0x7c: "939f0270 39200001 39000000",
        0x94: "913f027c 911f0288 915f0274", 0xa8: "3bdf0038 7fc4f378", 0xbc: "917f0278",
        0xd4: "38a00004 7fc4f378 38610050",
    }.items():
        fixed("stream", offset, pattern)
    call("stream", 0x54, "?GenericRwacFactorySkName@")
    call("stream", 0x6c, "?MakeHash@Name@Playback@")
    call("stream", 0xa0, "?MakeHash@Name@Playback@")
    stop_ctor_call = call("stream", 0xc0, "??0StreamStopRequest@")
    stop_post_call = call("stream", 0xcc, "?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@QAAXABUStreamStopRequest@")
    play_ctor_call = call("stream", 0xe4, "??0StreamRequest@")
    play_post_call = call("stream", 0xf0, "?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@QAAXABUStreamRequest@")
    literal("stream", 0xa4, 0xb0, bytes.fromhex("3e800000"))
    literal("stream", 0xd0, 0xe0, bytes.fromhex("3dcccccd"))
    strings = {
        "MusicVoiceSpec": literal("stream", 0x5c, 0x60, b"MusicVoiceSpec\0"),
        "Send01": literal("stream", 0x78, 0x88, b"Send01\0"),
    }

    fixed("play", 0x14, "a17e0014 2b0bfffe 409a001c 809e0010")
    play_stream = call("play", 0x24, STREAM)
    fixed("play", 0x28, "897e0019 997b02a4")
    fixed("play", 0xa4, "2b0bffff 91410070 409a003c")
    literal("play", 0xb8, 0xbc, b"StereoWavVoiceSpec\0")
    fixed("play", 0x13c, "3bbc0004 38810050 7fa3eb78")
    fixed("play", 0x14c, "397c0058")
    fixed("play", 0x168, "d01c0078 b3fc0000")
    call("play", 0x174, "?Create@VoiceWrapper@")
    call("play", 0x190, "?Play@VoiceWrapper@")

    fixed("params_update", 0x20, "3bc00004 3bfd0088")
    literal("params_update", 0x14, 0x28, bytes.fromhex("3fa00000"))
    fixed("params_update", 0x54, "c01f002c 897f0024 ec1e002a d01f002c 2b0b0001 409a0014 ff00f800 4099000c 387fffb8")
    call("params_update", 0x78, "?Release@VoiceWrapper@")
    fixed("params_update", 0x7c, "3bdeffff 3bff0080 2b1e0000 409affa4")
    call("params_update", 0x90, "?GetBrnInputStructure@")
    call("params_update", 0x94, "?GetGameModeInterface@")
    fixed("params_update", 0x98, "7c7f1b78 2b1f0000 409a0024")
    for offset, prefix in ((0xa4, "?BeginAssert@"), (0xbc, "?FireAssert@"), (0xc0, "?EndAssert@")):
        assert one_call("params_update", prefix)[0] == offset
    literal("params_update", 0xb4, 0xb8, b"lpGameModeInterface\0")
    fixed("params_update", 0xc4, "817f0008 815d0298 2f0b000f 409a0020 817f000c 2f0b0006 419a000c 2f0b0001")
    fixed("params_update", 0xf4, "917d0298 556b003e 915d029c 554a003e 7f0b5000 419a0010 2f0b0001")
    literal("params_update", 0x128, 0x12c, b"OnlineEntry\0")
    call("params_update", 0x130, "?MakeHash@Name@Playback@")
    fixed("params_update", 0x134, "7c641b78 387dfffc")
    transition_stream = call("params_update", 0x13c, STREAM)
    fixed("params_update", 0x140, "39600009 997d02a0")
    return {
        "voice": {"span": "0x50", "handle_offset": "0x40", "wrapper_release_clears": ["0x40", "0x44", "0x48", "0x4c"],
                  "wait_handle": handle, "wait_content_state_offset": "0x1e", "voice_pool_stride": "0x5c"},
        "presentation": {"allocation": "0x2a8", "aging_stride": "0x80", "create_params": "0x25c",
                         "action_list": "0x28c", "mode_fields": ["0x29c", "0x2a0"], "mixer_output": "0x2a4",
                         "first_output": "Attach writes 9", "aging_loop": "four slots; elapsed only; release strictly above 1.25",
                         "input_assert": "one non-gating lpGameModeInterface assertion after aging and input lookup",
                         "online_entry": transition_stream, "streamed_splice": play_stream, "resident_wave": "0xffff"},
        "stream": {"interface_this_offset": "0x38", "create_params_stores": {hex(k): hex(v) for k, v in sorted(stores.items())},
                   "manager_assert_before_create_params": True, "stop_seconds": 0.25,
                   "play_priority": 4, "play_lag_f32": "0x3dcccccd", "calls": [stop_ctor_call, stop_post_call, play_ctor_call, play_post_call],
                   "literals": strings},
    }


def original(front):
    raw = front["pinned"](ARTIST, ARTIST_SHA)
    payload = struct.unpack_from(">I", raw, 8)[0] - 0x82000000

    def read(address, length):
        offset = payload + address
        assert 0 <= offset <= offset + length <= len(raw)
        return raw[offset:offset + length]

    fixed = {
        0x826dc714: "387f0040 917f0048", 0x826dc720: "8963001e",
        0x8268e0dc: "817f0000 2b0b0000 409a0024", 0x8268e108: "807f0000",
        0x826e7604: "3bff0050", 0x826f7788: "386002a8",
        0x826d2950: "3bc00004 3bfc0088", 0x826d2984: "c01f002c 897f0024 ec1f002a d01f002c",
        0x826d29a0: "ff006800 4099000c 387fffb8", 0x826d29b0: "3bdeffff 3bff0080 2b1e0000 409affa0",
        0x826d2a4c: "813f0008 39600000 815f000c 2f09000f 409a0018 2f0a0006 419a000c 2f0a0001",
        0x826d2a78: "917c0298 556b003e 915c029c", 0x826d2ac4: "39600009 997c02a0",
        0x8269e434: "2b1e0000 409a0024", 0x8269e46c: "915f025c",
        0x8269e488: "395f0038 39000001 38e00004 c00b3f40",
        0x8269e4a0: "d0010054 93bf0270 38810050 7fc3f378 91410050 90e10064",
        0x8269e4cc: "91410060 d001006c c1abbb14", 0x8269e4e0: "d1a10068",
        0x826e7714: "93df029c 93df02a0", 0x8269e34c: "39600009 93bf0298 38600001 93bf029c 997f02a0",
        0x82003f40: "3e800000", 0x820abb14: "3dcccccd", 0x820aa55c: "3fa00000",
    }
    for address, pattern in fixed.items():
        data = bytes.fromhex(pattern)
        assert read(address, len(data)) == data, hex(address)
    calls = {
        0x826dc71c: 0x8268e0c8, 0x8268e0e8: 0x82817548, 0x8268e100: 0x82820810, 0x8268e104: 0x82817558,
        0x826f77a8: 0x826e7628, 0x826d29ac: 0x826c5270,
        0x826d2a20: 0x826945f8, 0x826d2a30: 0x82817548, 0x826d2a44: 0x82820810, 0x826d2a48: 0x82817558,
        0x826d2ab4: 0x82689a50, 0x826d2ac0: 0x8269e418,
        0x8269e43c: 0x82817548, 0x8269e454: 0x82820810, 0x8269e458: 0x82817558,
        0x8269e4fc: 0x826835d8, 0x8269e508: 0x826834f0,
    }
    for address, target in calls.items():
        op = struct.unpack(">I", read(address, 4))[0]
        assert op & 0xfc000003 == 0x48000001
        assert address + ((op & 0x03fffffc) ^ 0x02000000) - 0x02000000 == target
    assert read(0x820b4128, 12) == b"OnlineEntry\0"
    return {"image": front["pin"](ARTIST), "fixed_windows": {hex(k): v for k, v in fixed.items()},
            "direct_calls": {hex(k): hex(v) for k, v in calls.items()}}


def preflight(front, h, targeted):
    compiled = {}
    for stem, methods in GROUPS.items():
        obj = targeted[stem]["artifacts"]["object"]
        coff = h.Coff(h.pinned(obj["path"], obj["sha256"]))
        for key, prefix in methods.items():
            compiled[key] = coff, coff.function(prefix)
        if stem == "CgsVoiceWrapper":
            compiled["handle"] = coff, coff.function(HANDLE)
    bodies = {key: (code, {at: ref.name for at, (_, ref) in refs.items()})
              for key, (_, (_, code, refs)) in compiled.items()}

    def words(key, off, data):
        assert bodies[key][0][off:off + len(data)] == data, (key, hex(off))

    def call(key, off, prefix):
        code, refs = bodies[key]
        assert h.u32(code, off) & 0xfc000003 == 0x48000001 and refs[off].startswith(prefix), (key, hex(off))
        return refs[off]

    def literal(key, high, low, data):
        coff, (_, _, refs) = compiled[key]
        assert refs[high][0] == 0x10 and refs[low][0] == 0x11 and refs[high][1] == refs[low][1]
        assert coff.literal(refs[high][1], len(data)) == data, (key, hex(high))
        return refs[high][1].name

    original(front)
    return machine_witnesses(bodies, words, call, literal)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preflight", action="store_true")
    parser.add_argument("--head")
    parser.add_argument("--tree", default=TREE, choices=(TREE,))
    parser.add_argument("--candidate", default=CANDIDATE, choices=(CANDIDATE,))
    parser.add_argument("--manifest", type=Path, default=WAVE / "r150-batch-v2/MANIFEST.json")
    parser.add_argument("--manifest-sha", default=MANIFEST_SHA, choices=(MANIFEST_SHA,))
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    front, h = helpers()
    manifest, targeted, pin = pinned_inputs(args, front, h)
    if args.preflight:
        assert not args.write
        print(json.dumps(preflight(front, h, targeted), indent=2))
        return
    assert args.head
    inputs = seal_inputs(args, manifest, front, h)
    proofs, pins, tables = bind(args, inputs, targeted, front, h)
    read = inputs["read"]

    def code(key, off, size):
        return read(int(proofs[key]["address"], 16) + off, size, executable=True)

    bodies = {key: (code(key, 0, row["bytes"]), {int(off, 16): ref["symbol"] for off, ref in row["bindings"].items()})
              for key, row in proofs.items()}

    def words(key, off, data):
        assert code(key, off, len(data)) == data, (key, hex(off))

    def call(key, off, prefix):
        row = proofs[key]["bindings"][hex(off)]
        assert row["symbol"].startswith(prefix) and h.u32(code(key, off, 4), 0) & 0xfc000003 == 0x48000001
        return row

    def literal(key, hi, lo, data):
        a, b = (proofs[key]["bindings"][hex(off)] for off in (hi, lo))
        assert (a["kind"], b["kind"]) == ("0x10", "0x11") and (a["target"], a["symbol"]) == (b["target"], b["symbol"])
        assert read(int(a["target"], 16), len(data)) == data, (key, hex(hi))
        return b

    witness = machine_witnesses(bodies, words, call, literal)
    assert witness["voice"]["wait_handle"]["target"] == proofs["handle"]["address"]
    assert witness["presentation"]["online_entry"]["target"] == witness["presentation"]["streamed_splice"]["target"] == proofs["stream"]["address"]
    receipt = {
        "state": "pass", "head": args.head, "tree": args.tree, "base": BASE, "candidate": args.candidate,
        "xex_sha256": inputs["image"]["xex_sha256"], "image": inputs["image"],
        "manifest": front["pin"](args.manifest), "targeted": pin, "verifier": front["pin"](Path(__file__)),
        "seal_inputs": inputs["pins"], "selected": pins, "functions": proofs, "presentation_vtables": tables, "witnesses": witness,
        "artist": original(front),
        "scope": "Static linked PPC. The Send01 MakeHash versus original interned provider remains a codegen difference. Adjacent effect and pool trailer layouts and runtime are outside scope.",
    }
    if args.write:
        out = HERE / "R150_VOICE_LINKED.json"
        with out.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
        print(front["pin"](out))
    print(f"PASS {args.head}: {len(proofs)} linked PPC bodies")


if __name__ == "__main__":
    main()
