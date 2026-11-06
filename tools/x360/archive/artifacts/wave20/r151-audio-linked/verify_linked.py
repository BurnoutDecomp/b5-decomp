"""Bind R151 audio lifecycle witnesses to the exact selected Xbox PPC link."""
import argparse
import json
from pathlib import Path
import runpy
import struct

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
LINK = WAVE / "r151-link"
BASE = "d6e42e0945a067b457ed58da16133d9a4d6202f9"
CANDIDATE = "19368322ef39f735c81948fae621ab875669717c"
TREE = "e5a7b8962ff39181fd7ccc77f8708851418bbd44"
MANIFEST_SHA = "bd40b0b369d4aef1130c395f928d98c9e977772b7c633b9309db36d1b127df5e"
TARGETED_SHA = "60b3f9ff9bb6c8aa0577056972bd165512f46917d9e734a8caf7e538abe7214b"
BASE_SEAL_SHA = "32d74ced3cbd26e3b1ffc568379f9281d6e8c71fbdb91c2f352c348c3d718a90"
PRIOR_PROOF_SHA = "7d8aaf2c385d4e0d36db647bbd037f82e42c3ef20f6e343678c293a0143aaaa1"
PRIOR_VERIFIER_SHA = "3980ef2fbdb936a2773942644be9597bea57120607456c7591e9d0e9cdc9ae15"
PRIOR_VERIFIER = WAVE / "r150-voice-linked/verify_linked.py"
PRIOR_PROOF = PRIOR_VERIFIER.with_name("R150_VOICE_LINKED.json")
MANIFEST = WAVE / "r151-batch/MANIFEST.json"
TARGETED = WAVE / "r151-batch/TARGETED_OBJECT_BINDING.json"
STREAM_SCALAR = "??_GStreamingEffect@Streaming@Logic@BrnSound@@UAAPAXI@Z"
STREAM_VECTOR = "??_EStreamingEffect@Streaming@Logic@BrnSound@@UAAPAXI@Z"
STREAM_THUNK = "??_EStreamingEffect@Streaming@Logic@BrnSound@@W3AAPAXI@Z"
NEW = {
    "CgsVoicePool": {
        "pool_ctor": "??0?$VoicePool@$03@", "pool_dtor": "??1?$VoicePool@$03@",
        "pool_prepare": "?Prepare@VoicePoolBase@", "pool_release": "?Release@VoicePoolBase@",
        "pool_update": "?Update@VoicePoolBase@",
    },
    "CgsVoiceWrapper": {"wrapper_dtor": "??1VoiceWrapper@"},
    "BrnInAirEffect": {"inair_ctor": "??0InAirEffect@", "inair_dtor": "??1InAirEffect@",
                       "inair_attach": "?Attach@InAirEffect@", "inair_detach": "?Detach@InAirEffect@"},
    "BrnStreamingEffect": {
        "streaming_alloc": "?CreateObject@StreamingEffect@", "streaming_ctor": "??0StreamingEffect@",
        "streaming_scalar_dtor": STREAM_SCALAR, "streaming_dtor_thunk": STREAM_THUNK,
        "streaming_attach": "?Attach@StreamingEffect@", "streaming_detach": "?Detach@StreamingEffect@",
        "streaming_process": "?ProcessUpdate@StreamingEffect@", "streaming_params": "?UpdateParams@StreamingEffect@",
        "streaming_notify": "?Notify@StreamingEffect@",
    },
}
PRIVATE_INAIR = {"KU_SEND01": b"Send01\0", "KU_AEMS_HEIGHT": b"AEMS_height\0"}
RESET = "?ResetDeferredState@VoiceWrapper@"
ORDINARY_STREAM_DTOR = "??1StreamingEffect@"


def helpers():
    old = runpy.run_path(str(PRIOR_VERIFIER))
    front, h = old["helpers"]()
    front["pinned"](PRIOR_VERIFIER, PRIOR_VERIFIER_SHA)
    previous = front["load"](PRIOR_PROOF, PRIOR_PROOF_SHA)
    assert previous["head"] == BASE
    return old, front, h, previous


def pin_targeted(front):
    manifest = front["load"](MANIFEST, MANIFEST_SHA)
    targeted = front["load"](TARGETED, TARGETED_SHA)
    assert (manifest["base"], manifest["candidate"], manifest["projected_tree"]) == (BASE, CANDIDATE, TREE)
    assert (targeted["head"], targeted["tree"]) == (CANDIDATE, TREE)
    assert any(Path(row["path"]) == TARGETED and row["sha256"] == TARGETED_SHA
               for row in manifest["composed_receipts"])
    rows = {Path(row["source"]).stem: row for row in targeted["results"]}
    report = front["from_pin"](targeted["compile_report"])
    assert (report["state"], report["target"], report["requested"]) == ("complete", "xbox360-ppc-be", 8)
    compiled = {row["source"]: row for row in report["results"]}
    assert len(rows) == len(compiled) == 8
    for row in rows.values():
        record = compiled[row["source"]]
        assert record["passed"] and record["returncode"] == 0 and record["machine"] == row["machine"] == 0x1f2
        for kind, artifact in row["artifacts"].items():
            assert Path(record[kind]) == Path(artifact["path"])
            front["pinned"](artifact["path"], artifact["sha256"])
    return manifest, rows


def seal_inputs(args, manifest, front, h):
    def load(name):
        return front["load"](LINK / name)

    result, image, invocation = (load(name) for name in ("result.json", "image-verification.json", "invocation.json"))
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] and image["head"] == args.head
    seal_path, release_path, rsp = (LINK / name for name in ("seal.json", "ROOT_RELEASE.json", "r151-strict.rsp"))
    assert Path(invocation["sealed"]) == seal_path and Path(invocation["release"]) == release_path
    assert invocation["sealed_sha256"] == args.seal_sha
    seal = front["load"](seal_path, args.seal_sha)
    assert (seal["schema"], seal["head"]) == ("r151-native-link-seal-v1", args.head)
    assert front["load"](release_path, invocation["release_sha256"]) == {
        "action": "one strict R151 native link", "sealed_sha256": args.seal_sha
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
    diag = load("diagnostic-parsed.json")
    assert diag["unique_symbols"] == diag["unresolved_messages"] == 0 and not diag["duplicate_messages"]
    boundary = front["from_pin"](seal["boundary"])
    assert (boundary["schema"], boundary["head"], boundary["selected_objects_sha256"]) == (
        "r151-link-boundary-v1", args.head, digest
    )
    assert boundary["no_excluded_cohort_outside_references"] and not boundary["errors"]
    assert seal["prior_successful_seal"] == manifest["base_seal"] == {
        "path": str(WAVE / "r150-link/seal.json"), "sha256": BASE_SEAL_SHA
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
    for commit in (args.head, CANDIDATE):
        assert h.git(root, "rev-parse", f"{commit}^{{tree}}", f"{commit}^").decode().split() == [TREE, BASE]
    binding_path = WAVE / "r151-intake/commit-binding.json"
    binding = next(row for row in seal["provenance"] if Path(row["path"]) == binding_path)
    commit = front["from_pin"](binding)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (args.head, BASE, MANIFEST_SHA)
    paths = sorted(h.git(root, "diff", "--name-only", BASE, args.head).decode().splitlines())
    assert paths == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    inventory = front["from_pin"](seal["source_inventory"])
    assert inventory["head"] == args.head and inventory["files"] == seal["sources"]
    report = front["from_pin"](seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    h.LINK = LINK
    read, entries, image_pins = h.load_image(result, image)
    for kind in ("xex", "pe", "map"):
        assert image_pins[f"{kind}_sha256"] == getattr(args, kind + "_sha"), kind
    return {
        "read": read, "entries": entries, "image": image_pins, "root": root,
        "sources": {h.norm(row["path"]): row["sha256"] for row in seal["sources"]},
        "objects": {row["source"]: row for row in selected}, "report": {row["source"]: row for row in report["results"]},
        "pins": {"seal": front["pin"](seal_path), "result": front["pin"](LINK / "result.json"),
                 "image_verification": front["pin"](LINK / "image-verification.json"), "release": front["pin"](release_path),
                 "response": front["pin"](rsp), "commit_binding": binding, "source_inventory": seal["source_inventory"],
                 "boundary": seal["boundary"], "report": seal["report"], "prior_seal": seal["prior_successful_seal"],
                 "provider_selection": seal["provider_selection"], "selected_objects_sha256": digest},
    }


def old_objects(h, previous):
    return {key: h.Coff(h.pinned(item["selected"]["object"], item["selected"]["sha256"]))
            for key, item in previous["selected"].items()}


def frozen_obj(h, row):
    artifact = row["artifacts"]["object"]
    return h.Coff(h.pinned(artifact["path"], artifact["sha256"]))


def groups(old):
    result = {stem: dict(methods) for stem, methods in NEW.items()}
    for stem in ("CgsVoiceWrapper", "BrnPresentationEffect", "BrnFxEffect"):
        result.setdefault(stem, {}).update(old["GROUPS"][stem])
    return result


def original(front, old):
    prior = old["original"](front)
    raw = front["pinned"](old["ARTIST"], old["ARTIST_SHA"])
    payload = struct.unpack_from(">I", raw, 8)[0] - 0x82000000

    def read(va, size):
        return raw[payload + va:payload + va + size]

    words = {
        0x826e5340: "3bdd0014 3be00003", 0x826e5358: "3bde005c",
        0x826e5380: "3bc30184", 0x826e53ac: "3bdeffa4",
        0x826b65c0: "3908005c",
        0x826b65c8: ("d00b0050 994b004c 914b0044 914b0048 914b0004 914b0008 914b000c 914b0010 "
                     "914b0014 914b0018 914b001c 914b0020 914b0024 914b0028 914b002c 90eb0030 914b0054 994b0058"),
        0x826f460c: "387f0090 38a00004 d01f032c 38830014 91630004 93a3000c 93a30010",
        0x826f4834: "387f0090", 0x826cf9f8: "9b7f0058 937f0054", 0x826cfa08: "3bde005c",
        0x826e2470: "386000e0", 0x826e2498: "38630004", 0x826c9d2c: "387f0068",
        0x826c9dc0: "387f00b8", 0x826c9e0c: "387f00b8", 0x826c9e1c: "387f0068",
        0x826e2550: "3bddfffc", 0x826e2588: "3bfd0064", 0x826e25c8: "c05d00c4",
        0x826e25d0: "c03d00cc", 0x826982a0: "c00300c4 c1a300c8",
        0x82683b24: "815d00d4", 0x82683b34: "997d00d8", 0x826ee984: "3bdf00b4",
        0x826eea30: "d03f00cc", 0x826eea40: "9b5f00d8", 0x826eea4c: "d01f00c4 915f00d4 d01f00c8 d01f00d0",
        0x826eeac4: "d3bf00c8", 0x826eeae4: "d03f00d0",
    }
    calls = {
        0x826e5350: 0x826ada10, 0x826e53b8: 0x826c5270, 0x826f4628: 0x826b6528,
        0x826f4838: 0x826cf9c0, 0x826cf9f4: 0x826c5270, 0x826e2484: 0x826ab5c8,
        0x826e2490: 0x826c9d10, 0x826c9db4: 0x826ada10, 0x826c9dc4: 0x82697400,
        0x826c9e18: 0x8280d100, 0x826c9e20: 0x826c7e80, 0x826e2590: 0x826dc5e0,
    }
    for address, pattern in words.items():
        expected = bytes.fromhex(pattern)
        assert read(address, len(expected)) == expected, hex(address)
    for address, target in calls.items():
        op = struct.unpack(">I", read(address, 4))[0]
        assert op & 0xfc000003 == 0x48000001
        assert address + ((op & 0x03fffffc) ^ 0x02000000) - 0x02000000 == target
    return {"prior_voice_presentation": prior, "pool_streaming_words": {hex(k): v for k, v in words.items()},
            "pool_streaming_calls": {hex(k): hex(v) for k, v in calls.items()}}


def format_function(coff, prefix):
    """Read an InAir dynamic initializer from its exact executable COMDAT."""
    symbol = coff.definition(prefix)
    name, code, start, count = coff.section(symbol.section)
    assert name == b".text$yc" and symbol.value == 0 and symbol.kind == 0x20
    refs = {}
    for offset, index, kind in coff.section_relocations(start, count):
        if kind == 0x12:
            assert index == 0 and refs[offset][0] in (0x10, 0x11)
        else:
            assert offset not in refs
            refs[offset] = kind, coff.symbols[index]
    return symbol, code, refs


def bind_inair_globals(coff, read, entries, owner, proof, writable, h):
    """Resolve the two map-private InAir globals through their real initializers."""
    destinations, bindings = {}, {}
    section_base = None
    for name, literal in PRIVATE_INAIR.items():
        [target] = [row for row in coff.symbols.values() if row.name == name and row.section > 0]
        assert target.name == name and coff.section(target.section)[0] == b".bss"
        fn, code, refs = format_function(coff, "??__E" + name + "@")
        assert refs[0x18] == (0x10, target) and refs[0x1c] == (0x11, target)
        assert coff.literal(refs[0x0c][1], len(literal)) == literal
        address = h.map_addr(entries, fn.name, owner)
        linked = read(address, len(code), executable=True)
        hi, lo = (h.u32(linked, off) for off in (0x18, 0x1c))
        assert hi & 0xffff0000 == 0x3d600000 and lo & 0xffff0000 == 0x906b0000
        dest = (((hi & 0xffff) << 16) + h.sign(lo & 0xffff, 16)) & 0xffffffff
        # PE BSS can be virtual-only, so test its mapping without reading raw zeros.
        assert dest & 3 == 0 and writable(dest)
        if section_base is None:
            section_base = dest - target.value
        assert dest - target.value == section_base
        destinations[name] = {(dest, owner)}
        bindings[name] = (fn, code, refs, address, literal)
    for name, (fn, code, refs, address, literal) in bindings.items():
        row = proof("inair_initializer", coff, fn, code, refs, address, entries | destinations)
        address = int(row["bindings"]["0x10"]["target"], 16)
        assert read(address, len(literal)) == literal
        bindings[name] = row
    return destinations, bindings


def layout_witnesses(bodies, words, call):
    def fixed(key, offset, pattern):
        words(key, offset, bytes.fromhex(pattern))

    def store_set(key, start, end, base):
        code = bodies[key][0]
        return [(op >> 26, op & 0xffff) for (op,) in struct.iter_unpack(">I", code[start:end])
                if op >> 26 in (36, 38, 52) and ((op >> 16) & 31) == base]

    reset = [(38, 0x4c), (36, 0x44), (36, 0x48), *[(36, k) for k in range(4, 0x31, 4)]]
    pooled = [(52, 0x50), *reset, (36, 0x54), (38, 0x58)]
    assert store_set("reset_leaf", 0, len(bodies["reset_leaf"][0]), 3) == reset
    fixed("reset_leaf", 0, "39600000 3940ffff")
    fixed("reset_leaf", 0x40, "91430030 4e800020")
    assert not bodies["reset_leaf"][1]
    for start, end, base in ((0xfc, 0x168, 10), (0x194, 0x1dc, 10), (0x214, 0x230, 11),
                             (0x298, 0x2e0, 10), (0x310, 0x358, 10)):
        expected = pooled if start != 0x214 else pooled[:4]
        assert store_set("pool_prepare", start, end, base) == expected, hex(start)
    assert store_set("pool_prepare", 0x230, 0x290, 10) == pooled[4:]
    assert not any("VoiceWrapper" in symbol for symbol in bodies["pool_prepare"][1].values())
    fixed("pool_prepare", 0x174, "91610078 91610074 394a005c")
    fixed("pool_prepare", 0x2f4, "1cea005c")
    fixed("pool_prepare", 0x35c, "38600001 93bf000c")
    fixed("pool_ctor", 0xc, "39230014")
    fixed("pool_ctor", 0x1c, "39000003")
    fixed("pool_ctor", 0x4c, "3929005c")
    fixed("pool_ctor", 0x9c, "396b005c 4098ffa0")
    assert not bodies["pool_ctor"][1] or all(name.startswith("??_7") for name in bodies["pool_ctor"][1].values())
    fixed("pool_dtor", 0x18, "3be30184")
    fixed("pool_dtor", 0x28, "3bffffa4 7fe3fb78")
    call("pool_dtor", 0x30, "??1VoiceWrapper@")
    fixed("pool_dtor", 0x34, "3bdeffff 2f1e0000 4098ffec")
    assert not any("Release@VoicePool" in name for name in bodies["pool_dtor"][1].values())
    call("wrapper_dtor", 0x20, "?Release@VoiceWrapper@")
    call("pool_release", 0x34, "?Release@VoiceWrapper@")
    fixed("pool_release", 0x38, "9b7f0058 937f0054")
    fixed("pool_release", 0x48, "3bde005c")
    fixed("pool_release", 0x54, "38600001 937c000c 937c0008")
    fixed("inair_ctor", 0x58, "387e0094")
    call("inair_ctor", 0xb8, "??0?$VoicePool@$03@")
    fixed("inair_dtor", 0x4c, "387f0094")
    call("inair_dtor", 0x50, "??1?$VoicePool@$03@")
    fixed("inair_attach", 0x84, "817e0028 387e0090 3be00000 38a00004 38830014 91630004 93e3000c 93e30010")
    prepare = call("inair_attach", 0xa4, "?Prepare@VoicePoolBase@")
    fixed("inair_detach", 0x1c, "387f0090")
    release = call("inair_detach", 0x20, "?Release@VoicePoolBase@")

    fixed("streaming_alloc", 0x1c, "386000e0")
    call("streaming_alloc", 0x24, "??2MemBase@")
    call("streaming_alloc", 0x30, "??0StreamingEffect@")
    fixed("streaming_alloc", 0x38, "38630004")
    fixed("streaming_ctor", 0x14, "397f0068")
    fixed("streaming_ctor", 0x5c, "395f00b8")
    call("streaming_ctor", 0x128, "??0Instance@Attrib@")
    assert not any(0xc8 <= offset < 0xe0 for _, offset in store_set(
        "streaming_ctor", 0, len(bodies["streaming_ctor"][0]), 31
    ))
    fixed("streaming_scalar_dtor", 0x28, "387f00b8")
    call("streaming_scalar_dtor", 0x38, "??1Instance@Attrib@")
    fixed("streaming_scalar_dtor", 0x3c, "387f0068")
    call("streaming_scalar_dtor", 0x40, "??1VoiceWrapper@")
    call("streaming_scalar_dtor", 0x48, "??1BrnEffectObject@")
    fixed("streaming_dtor_thunk", 0, "3863fffc")
    fixed("streaming_process", 0x10, "387ffffc")
    fixed("streaming_process", 0x70, "3bdf0064")
    call("streaming_process", 0x78, "?Update@VoiceWrapper@")
    fixed("streaming_process", 0x94, "c05f00c4 c03f00cc")
    fixed("streaming_params", 0, "c00300c4 c1a300c8 ec01002a eda1682a d00300c4 d1a300c8")
    fixed("streaming_notify", 0x78, "815e00d4")
    fixed("streaming_notify", 0x88, "997e00d8")
    fixed("streaming_attach", 0x9c, "3bbf0064")
    call("streaming_attach", 0xa8, "?Create@VoiceWrapper@")
    call("streaming_attach", 0xb4, "?Play@VoiceWrapper@")
    fixed("streaming_attach", 0xc0, "3bdf00b4")
    call("streaming_attach", 0xd4, "?ChangeWithDefault@Instance@Attrib@")
    fixed("streaming_attach", 0x184, "d03f00cc")
    fixed("streaming_attach", 0x1a0, "d01f00c4 915f00d4 d01f00c8 d01f00d0 997f00d8")
    fixed("streaming_detach", 0x54, "d3ff00c8")
    fixed("streaming_detach", 0x74, "d03f00d0")
    fixed("streaming_detach", 0x118, "897f00d8")
    return {
        "voice_pool": {"slot_stride": "0x5c", "inline_reset_store_offsets": [hex(off) for _, off in pooled],
                       "preserved_handle_offsets": ["0x38", "0x40"], "inair_prepare": prepare, "inair_release": release},
        "streaming": {"allocation": "0xe0", "wrapper": "0x68", "settings": "0xb8",
                      "tail": ["0xc8", "0xcc", "0xd0", "0xd4", "0xd8", "0xdc"],
                      "destruction": ["settings", "wrapper", "base"]},
    }


def preflight(old, h, previous, rows):
    prior = old_objects(h, previous)
    funcs = {}
    for stem, methods in groups(old).items():
        coff = frozen_obj(h, rows[stem]) if stem in rows else prior[stem]
        for key, prefix in methods.items():
            funcs[key] = coff.function(prefix)
            if key in old["GROUPS"].get(stem, {}) and key not in ("pool_prepare",):
                old["compare"](h, key, coff, prior[stem], prefix)
    pool = frozen_obj(h, rows["CgsVoicePool"])
    streaming = frozen_obj(h, rows["BrnStreamingEffect"])
    funcs["reset_leaf"] = pool.function(RESET)
    ordinary = streaming.function(ORDINARY_STREAM_DTOR)
    assert ordinary[2][0x30][1].name.startswith("??1Instance@Attrib@")
    assert ordinary[2][0x38][1].name.startswith("??1VoiceWrapper@")
    assert ordinary[2][0x40][1].name.startswith("??1BrnEffectObject@")
    bodies = {key: (code, {off: ref.name for off, (_, ref) in refs.items()})
              for key, (_, code, refs) in funcs.items()}

    def words(key, off, data):
        assert bodies[key][0][off:off + len(data)] == data, (key, hex(off))

    def call(key, off, prefix):
        code, refs = bodies[key]
        assert h.u32(code, off) & 0xfc000003 == 0x48000001 and refs[off].startswith(prefix), (key, hex(off))
        return refs[off]

    return layout_witnesses(bodies, words, call)


def bind(args, old, front, h, previous, rows, inputs):
    prior = old_objects(h, previous)
    parser = runpy.run_path(str(front["READER"]))
    reader = front["body_proof"]
    reader.__globals__["PRIVATE_DATA"].update({"stream": ("__real@3e800000", "__real@3dcccccd"),
                                              "params_update": ("__real@3fa00000",)})
    entries, read = dict(inputs["entries"]), inputs["read"]
    functions, objects, code_rows, coffs, retained, private = {}, {}, {}, {}, {}, {}

    def check(key, coff, symbol, code, refs, address, bindings):
        proof = reader(read, bindings, coff, key, symbol, code, refs, address, private, parser)
        proof["compiled_sha256"] = h.sha(code)
        return proof

    for stem, methods in groups(old).items():
        row = rows.get(stem)
        source = row["source"] if row else previous["selected"][stem]["selected"]["source"]
        owner, coff, objects[stem] = old["selected_coff"](h, args, inputs, source, row)
        coffs[stem] = owner, coff
        frozen = frozen_obj(h, row) if row else prior[stem]
        for key, prefix in methods.items():
            code_rows[key] = owner, coff, *old["compare"](h, key, coff, frozen, prefix)
            if key in old["GROUPS"].get(stem, {}) and key != "pool_prepare":
                old["compare"](h, key, coff, prior[stem], prefix)
                retained[key] = previous["functions"][key]["compiled_sha256"]

    stream = old["STREAM"]
    if stream not in entries:
        owner, _, symbol, _, refs = code_rows["play"]
        address = h.map_addr(entries, symbol.name, owner)
        [offset] = [off for off, (kind, target) in refs.items() if kind == 6 and target.name == stream]
        op = h.u32(read(address + offset, 4, executable=True), 0)
        assert op & 0xfc000003 == 0x48000001
        entries[stream] = {(address + offset + h.sign(op & 0x03fffffc, 26), owner)}

    handle = old["HANDLE"]
    [owner] = {owner for _, owner in entries[handle]}
    [source] = [source for source, row in inputs["objects"].items() if Path(row["object"]).name == owner]
    owner, coff, objects["handle_owner"] = old["selected_coff"](h, args, inputs, source)
    code_rows["handle"] = owner, coff, *old["compare"](h, "handle", coff, prior["handle_owner"], handle)
    retained["handle"] = previous["functions"]["handle"]["compiled_sha256"]

    image = h.pefile.PE(str(LINK / "native-game.pe"), fast_load=True)

    def writable(address):
        rva = address - image.OPTIONAL_HEADER.ImageBase
        return any(section.VirtualAddress <= rva and rva + 4 <= section.VirtualAddress + section.Misc_VirtualSize
                   and (section.Characteristics & 0xa0000000) == 0x80000000 for section in image.sections)

    owner, coff = coffs["BrnInAirEffect"]
    globals_, initializers = bind_inair_globals(coff, read, entries, owner, check, writable, h)
    entries.update(globals_)
    for key, (owner, coff, symbol, code, refs) in code_rows.items():
        address = h.map_addr(entries, symbol.name, owner)
        functions[key] = check(key, coff, symbol, code, refs, address, entries)
        functions[key]["provider"] = owner
        if key in retained:
            assert functions[key]["compiled_sha256"] == retained[key], key

    def vtable(stem, name, offset, method):
        owner, coff = coffs[stem]
        target = coff.vtable_slot(name, offset)
        assert target == frozen_obj(h, rows[stem]).vtable_slot(name, offset)
        address = h.map_addr(entries, name, owner) + offset
        destination = h.u32(read(address, 4), 0)
        assert target == functions[method]["symbol"] and destination == int(functions[method]["address"], 16)
        return {"symbol": name, "slot": hex(address), "target": hex(destination)}

    tables = {
        "streaming_effect_dtor": vtable("BrnStreamingEffect", "??_7StreamingEffect@Streaming@Logic@BrnSound@@6BEffectObject@2CgsSound@@@", 0, "streaming_dtor_thunk"),
        "presentation_create_params": vtable("BrnPresentationEffect", "??_7PresentationEffect@Logic@BrnSound@@6B@", 0, "params_get"),
        "presentation_attach": vtable("BrnPresentationEffect", "??_7PresentationEffect@Logic@BrnSound@@6BEffectObject@1CgsSound@@@", 0x14, "attach"),
    }
    thunk = functions["streaming_dtor_thunk"]["bindings"]["0x4"]
    assert thunk["symbol"] == STREAM_VECTOR and thunk["target"] == functions["streaming_scalar_dtor"]["address"]
    return functions, objects, retained, initializers, tables, coffs


def linked_witnesses(old, h, inputs, functions, coffs, rows):
    read = inputs["read"]

    def code(key, off, size):
        return read(int(functions[key]["address"], 16) + off, size, executable=True)

    bodies = {key: (code(key, 0, row["bytes"]), {int(off, 16): ref["symbol"] for off, ref in row["bindings"].items()})
              for key, row in functions.items()}
    for key, stem, prefix in (("reset_leaf", "CgsVoicePool", RESET),):
        _, coff = coffs[stem]
        _, machine, refs = old["compare"](h, key, coff, frozen_obj(h, rows[stem]), prefix)
        bodies[key] = machine, {off: ref.name for off, (_, ref) in refs.items()}

    def words(key, off, data):
        assert bodies[key][0][off:off + len(data)] == data, (key, hex(off))

    def call(key, off, prefix):
        row = functions[key]["bindings"][hex(off)]
        assert row["symbol"].startswith(prefix) and h.u32(code(key, off, 4), 0) & 0xfc000003 == 0x48000001
        return row

    result = layout_witnesses(bodies, words, call)
    for caller, offset, method in (("inair_attach", 0xa4, "pool_prepare"), ("inair_detach", 0x20, "pool_release"),
                                   ("inair_ctor", 0xb8, "pool_ctor"), ("inair_dtor", 0x50, "pool_dtor"),
                                   ("pool_release", 0x34, "release"), ("wrapper_dtor", 0x20, "release"),
                                   ("streaming_process", 0x78, "update"), ("update", 0x138, "handle"),
                                   ("params_update", 0x13c, "stream"), ("play", 0x24, "stream")):
        assert functions[caller]["bindings"][hex(offset)]["target"] == functions[method]["address"]
    literals = {}
    for key, hi, lo, literal in (
        ("stream", 0x40, 0x44, b"lpStreamingStateMan\0"), ("stream", 0x5c, 0x60, b"MusicVoiceSpec\0"),
        ("stream", 0x78, 0x88, b"Send01\0"), ("stream", 0xa4, 0xb0, bytes.fromhex("3e800000")),
        ("stream", 0xd0, 0xe0, bytes.fromhex("3dcccccd")), ("params_update", 0x14, 0x28, bytes.fromhex("3fa00000")),
        ("params_update", 0xb4, 0xb8, b"lpGameModeInterface\0"), ("params_update", 0x128, 0x12c, b"OnlineEntry\0"),
        ("play", 0xb8, 0xbc, b"StereoWavVoiceSpec\0"), ("handle", 0x30, 0x34, b"mpObject\0"),
    ):
        a, b = (functions[key]["bindings"][hex(off)] for off in (hi, lo))
        assert (a["kind"], b["kind"]) == ("0x10", "0x11") and (a["target"], a["symbol"]) == (b["target"], b["symbol"])
        assert read(int(b["target"], 16), len(literal)) == literal, (key, hex(hi))
        literals[f"{key}:{hi:#x}"] = {"target": b["target"], "bytes": literal.hex()}
    result["retained_literals"] = literals
    result["compiled_only_reset_leaf"] = "ResetDeferredState is inlined into all five generated Prepare slot paths."
    result["compiled_only_ordinary_streaming_destructor"] = "The linked deleting destructor inlines the ordinary teardown."
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preflight", action="store_true")
    for arg in ("head", "seal-sha", "xex-sha", "pe-sha", "map-sha"):
        parser.add_argument("--" + arg)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    old, front, h, previous = helpers()
    manifest, rows = pin_targeted(front)
    artist = original(front, old)
    targeted = preflight(old, h, previous, rows)
    if args.preflight:
        assert not args.write
        print(json.dumps(targeted, indent=2))
        return
    assert all((args.head, args.seal_sha, args.xex_sha, args.pe_sha, args.map_sha))
    inputs = seal_inputs(args, manifest, front, h)
    functions, objects, retained, initializers, tables, coffs = bind(args, old, front, h, previous, rows, inputs)
    witnesses = linked_witnesses(old, h, inputs, functions, coffs, rows)
    composed_selected = {}
    for stem, row in rows.items():
        pin = objects.get(stem)
        if pin is None:
            _, _, pin = old["selected_coff"](h, args, inputs, row["source"], row)
        composed_selected[row["source"]] = pin
    assert len(composed_selected) == 8
    receipt = {
        "state": "pass", "head": args.head, "tree": TREE, "base": BASE, "candidate": CANDIDATE,
        "xex_sha256": inputs["image"]["xex_sha256"], "image": inputs["image"],
        "manifest": front["pin"](MANIFEST), "targeted": front["pin"](TARGETED),
        "verifier": front["pin"](Path(__file__)), "previous_proof": front["pin"](PRIOR_PROOF),
        "seal_inputs": inputs["pins"], "selected": objects, "composed_selected_objects": composed_selected,
        "functions": functions, "artist": artist,
        "retained_r150_compiled_bodies": retained, "inair_map_private_initializers": initializers,
        "vtables": tables, "witnesses": witnesses,
        "scope": "Static audio PPC. The unchanged InAir Detach state dispatch and post-pool individual voice release order still differ from ARTIST. Send01 provider differences and runtime remain outside this proof.",
    }
    if args.write:
        out = HERE / "R151_AUDIO_LINKED.json"
        with out.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
        print(front["pin"](out))
    print(f"PASS {args.head}: {len(functions)} linked bodies, {len(retained)} carried from R150")


if __name__ == "__main__":
    main()
