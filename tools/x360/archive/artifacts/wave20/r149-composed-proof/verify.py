"""Verify the exact R149 composed Xbox PPC COFF and original ARTIST witnesses."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess
import zlib

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
CHECKOUT = Path("C:/Users/adri1/.codex/worktrees/bp360/debug_format_luna")
HEAD = "33d20058fb914df3c56cf5fe507029ba64b174b4"
TREE = "bd200df2bf44260dee9d59f4cef5dd7dceb63c46"
BASE = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
PREFLIGHT = WAVE / "r149-composed-preflight"
ARTIST = WAVE / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex"
ARTIST_SHA = "6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655"
LANGUAGE = "r149-language-view-preflight"
HASH = "r149-resource-hash-preflight"
MUSIC = "r149-post-title-music"
PRESENTATION = "r150-presentation-stream-requests"
PREFIXES = {
    "view": "?ProcessIncomingLanguageEvent@ViewModule@",
    "dispatch": "?ProcessIncomingViewEvents@ViewModule@",
    "add": "?AddStringPointer@LanguageManager@",
    "remove": "?RemoveStringPointer@LanguageManager@",
    "payload": "?SetStringRemoved@GuiEventLocalisedTextPointerRemoved@",
    "queue": "?AddEvent@?$VariableEventQueue@$0BAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z",
    "hash": "?HashString@ID@CgsResource@@",
    "sku": "?Update@Sku@CgsLanguage@@",
    "boot": "?Update@BootLegal@BrnGui@@",
    "title": "?Update@PostTitleScreenLoad@BrnGui@@",
    "notify": "?Notify@MusicEffect@Logic@BrnSound@@",
    "play": "?Play@PresentationEffect@Logic@BrnSound@@",
    "attach": "?Attach@PresentationEffect@Logic@BrnSound@@",
    "get_params": "?GetCreateParams@PresentationEffect@Logic@BrnSound@@",
    "find_voice": "?FindOrStealAVoice@PresentationEffect@Logic@BrnSound@@",
}
FUNCTIONS = {
    "CgsGuiViewModule": ("view", "dispatch", "queue"),
    "CgsLanguageManager": ("add", "remove"),
    "CgsGuiEventLocalisedTextPointerRemoved": ("payload",),
    "VariableEventQueue_256_16": ("queue",),
    "CgsResourceID": ("hash",),
    "CgsSku": ("sku",),
    "BrnBootLegal": ("boot",),
    "BrnPostTitleScreenLoad": ("title",),
    "BrnMusicEffect": ("notify",),
    "BrnPresentationEffect": ("play", "attach", "get_params", "find_voice"),
}
HASH_U64 = "?HashString@ID@CgsResource@@SA_KPBE@Z"
HASH_OLD = "?HashString@ID@CgsResource@@SAHPBE@Z"
MAKE_HASH = "?MakeHash@Name@Playback@CgsSound@@SAIPBD@Z"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def pin(path):
    return {"path": str(path), "sha256": sha(path.read_bytes())}


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(CHECKOUT), *args])


def read_report(directory, expected_count):
    report = json.loads((directory / "compile-results.json").read_text())
    assert (report["state"], report["stage"], report["target"], report["requested"]) == (
        "complete", "compile-only", "xbox360-ppc-be", expected_count
    )
    rows = {Path(row["source"]).stem: row for row in report["results"]}
    assert len(rows) == len(report["results"]) == expected_count
    for row in rows.values():
        assert row["passed"] and row["returncode"] == 0 and row["machine"] == 0x1f2
        assert Path(row["object"]).parent == directory
        assert Path(row["object"]).read_bytes()[:2] == b"\xf2\x01"
    return rows


def word(code, offset=0):
    return struct.unpack_from(">I", code, offset)[0]


def words(code, offset, expected):
    want = bytes.fromhex(expected)
    assert code[offset:offset + len(want)] == want, (hex(offset), expected)


def call(code, refs, offset, target):
    assert word(code, offset) & 0xfc000003 == 0x48000001, hex(offset)
    kind, name, _, _ = refs[offset]
    assert kind == 6 and name.startswith(target), (hex(offset), name)


def selected_literal(path, method, hi, lo, expected, support):
    code, rels = support["collect"](path, method)
    bindings = {(offset, kind): index for offset, index, kind in rels}
    index = bindings[hi, 0x10]
    assert bindings[lo, 0x11] == index
    data = path.read_bytes()
    name, offset, section = support["object_symbol"](data, index)
    assert section > 0
    size, pointer = struct.unpack_from("<II", data, 20 + (section - 1) * 40 + 16)
    assert offset + len(expected) <= size
    assert data[pointer + offset:pointer + offset + len(expected)] == expected, name
    return name


def language(read, branch, methods):
    for address, expected in {
        0x8285ed1c: "2f06000a 419a00b0 2f06000b 419a0084 2f06000c 419a0028",
        0x8285ed58: "817f0000 387e7c0c 2f0b0000 409a0014 389f0008 80bf0004",
        0x8285ed90: "3c7e0001 38c00004 38a0000d 38810050 3863ddc0",
        0x8285edb4: "38a0029f",
        0x8285edd0: "817f0008 2b0b0001 41980044 419a0028",
        0x8285ede8: "38a0028e",
        0x8285ee04: "38c00080 c03f000c 38810060 387e7c0c",
        0x8285ee1c: "38c00080 c03f000c 38810060 387e7c0c",
        0x8285ee30: "3c7e0001 80bf0004 38e00000 809f0000 38c10060 3863e010",
        0x825663e8: "93a10050",
        0x825664e0: "39600001",
        0x825664e8: "91610050",
        0x82846820: "907f0000",
    }.items():
        words(read(address, len(bytes.fromhex(expected))), 0, expected)
    for pc, target in {
        0x8285ed70: 0x82864a08, 0x8285ed80: 0x82864b30, 0x8285ed8c: 0x828467e0,
        0x8285eda4: 0x823800f8, 0x8285edac: 0x82817548, 0x8285edc4: 0x82820810,
        0x8285edc8: 0x82817558, 0x8285ee14: 0x82863328, 0x8285ee2c: 0x82862c60,
        0x8285ee48: 0x82853c28, 0x82566408: 0x825658c8, 0x82566500: 0x825658c8,
        0x8284681c: 0x828154a0,
    }.items():
        assert branch(pc) == target

    _, view, refs = methods["view"]
    for offset, expected in {
        0x1c: "2f06000a 419a00b0 2f06000b 419a0084 2f06000c 419a0028",
        0x58: "817f0000 387e7c0c 2f0b0000 419a0038 3bff0008 7fe4fb78",
        0x74: "7fe4fb78 38610050",
        0x80: "3c7e0001 38c00004 38a0000d 38810050 3863ddc0",
        0x98: "480000b8 389f0008 80bf0004",
        0xb4: "38a0029f",
        0xcc: "48000084 817f0008 2f0b0000 409a001c",
        0xdc: "38c00080 c03f000c 38810060 387e7c0c",
        0xf0: "48000044 2f0b0001 409a001c 38c00080 c03f000c 38810060 387e7c0c",
        0x110: "48000024",
        0x11c: "38a0028e",
        0x134: "3c7e0001 80bf0004 38e00000 809f0000 38c10060 3863e010",
    }.items():
        words(view, offset, expected)
    for offset, target in {
        0x70: PREFIXES["remove"], 0x7c: PREFIXES["payload"], 0x94: PREFIXES["queue"],
        0xa4: PREFIXES["add"], 0xac: "?BeginAssert@", 0xc4: "?FireAssert@", 0xc8: "?EndAssert@",
        0xec: "?FormatMinutesAndSecondsString@", 0x10c: "?FormatSecondsString@",
        0x114: "?BeginAssert@", 0x12c: "?FireAssert@", 0x130: "?EndAssert@",
        0x14c: "?UpdateFlashComponent@",
    }.items():
        call(view, refs, offset, target)
    _, dispatch, bindings = methods["dispatch"]
    call(dispatch, bindings, 0x190, PREFIXES["view"])
    _, payload, bindings = methods["payload"]
    call(payload, bindings, 0x3c, "?CalculateHash@")
    words(payload, 0x40, "907f0000")
    return {
        "artist_handler": "0x8285ed00", "coff_dispatch_call": "0x190",
        "case_10": {"format_tag_offset": 8, "f32_offset": 12, "format_buffer_capacity": 128,
                    "format_calls": ["0xec", "0x10c"], "flash_update": "0x14c"},
        "case_11": {"assert_line": 671, "coff_begin_fire_end": ["0xac", "0xc4", "0xc8"]},
        "case_12": {"marker_offset": 0, "text_pointer_offset": 4, "string_id_offset": 8,
                    "zero_add_call": "0xa4", "nonzero_remove_and_notify": ["0x70", "0x7c", "0x94"],
                    "event": 13, "payload_size": 4, "output_queue_offset": "0xddc0"},
    }


def resource_hash(read, methods):
    words(read(0x828d854c, 8), 0, "7d2b48f8 79630020")
    words(read(0x828d8568, 4), 0, "79230020")
    name, code, _ = methods["hash"]
    assert name == HASH_U64
    words(code, 0x3c, "897f0000 2b0b0000 409a001c 38600000")
    words(code, 0x184, "7d4b50f8 79630020")
    _, sku, refs = methods["sku"]
    for offset, store in ((0x70, "f9610070"), (0xd4, "f9610090")):
        call(sku, refs, offset, HASH_U64)
        words(sku, offset + 0xc, "796b0020")
        words(sku, offset + 0x14, store)
    crc = zlib.crc32(b"english.lang")
    assert crc == 0xd8d53bfa
    return {
        "symbol": name, "artist_zero_extend": "0x828d8550", "coff_zero_extend": "0x188",
        "coff_empty_return": "0x48", "sku_unsigned_call_sites": ["0x70", "0xd4"],
        "english_lang_reference_crc32_u64": f"0x{crc:016x}",
    }


def music(read, branch, methods, rows, support):
    literal = b"Guns_And_Roses\0"
    for upper, hash_pc in ((0x8247e388, 0x8247e390), (0x8247782c, 0x82477834)):
        pointer = support["data_target"](read(upper, 8), 0, 4)
        assert pointer == 0x82f2ce5c and word(read(pointer, 4)) == 0x820a9adc
        assert read(0x820a9adc, len(literal)) == literal and branch(hash_pc) == 0x82689a50
    assert branch(0x8247e3ac) == branch(0x8247784c)
    assert branch(0x826bbfa0) == 0x826bb9b0
    words(read(0x826bbfa4, 16), 0, "81630000 917e0220 897f0014 997e0245")
    checks = {
        "title": ("BrnPostTitleScreenLoad", 0x60, 0x64, 0x68, 0x84),
        "boot": ("BrnBootLegal", 0x54c, 0x550, 0x554, 0x56c),
    }
    names = set()
    for key, (provider, hi, lo, hash_pc, post_pc) in checks.items():
        name, code, refs = methods[key]
        names.add(selected_literal(Path(rows[provider]["object"]), name, hi, lo, literal, support))
        call(code, refs, hash_pc, MAKE_HASH)
        call(code, refs, post_pc, "??$OutputGuiEvent@VGuiEventPlayMusicOnMenuStream@")
    assert len(names) == 1
    _, title, refs = methods["title"]
    words(title, 0x6c, "3bc00000 38810050 90610050 807f001c 9bc10054 9bc10055")
    call(title, refs, 0x90, "??$PostCommand16@$0BD@@")
    call(title, refs, 0x9c, "??$PostCommand16@$0EG@@")
    words(title, 0xa0, "39600002 917f0040")
    _, notify, refs = methods["notify"]
    call(notify, refs, 0x33c, "?GetStreamFromVideoName@MusicEffect@Logic@BrnSound@@")
    words(notify, 0x340, "57cb063e 907f025c 9bdf0275 2b0b0000 9bbf0276")
    assert not any("GunsAndRoses" in target for _, target, _, _ in refs.values())
    assert not any(kind == 6 and target == MAKE_HASH for offset, (kind, target, _, _) in refs.items()
                   if 0x330 <= offset < 0x358)
    return {"artist_string": "0x820a9adc", "coff_string": names.pop(),
            "title_hash_post": ["0x68", "0x84"], "boot_hash_post": ["0x554", "0x56c"],
            "notify_lookup_store": ["0x33c", "0x344"], "notify_alias_hash_calls": 0}


def presentation(read, branch, methods, rows, support):
    for pc, target in {
        0x826f7960: 0x8269e418, 0x8269e4fc: 0x826835d8, 0x8269e508: 0x826834f0,
        0x826f79e4: 0x826d2ad8, 0x826f7a6c: 0x82689a50,
    }.items():
        assert branch(pc) == target
    for address, expected in {
        0x826f7920: "2b0bfffe 419a001c",
        0x8269e46c: "915f025c",
        0x8269e488: "395f0038 39000001 38e00004 c00b3f40",
        0x8269e4a0: "d0010054 93bf0270 38810050 7fc3f378 91410050 90e10064",
        0x8269e4cc: "91410060 d001006c c1abbb14",
        0x8269e4e0: "d1a10068",
        0x8269e500: "38810060 7fc3f378",
        0x826d2bec: "3bff0080",
        0x82003f40: "3e800000",
        0x820abb14: "3dcccccd",
    }.items():
        words(read(address, len(bytes.fromhex(expected))), 0, expected)
    wav = b"StereoWavVoiceSpec\0"
    assert read(0x820b782c, len(wav)) == wav
    assert support["data_target"](read(0x826f7a40, 16), 0, 0xc) == 0x820b782c

    name, play, refs = methods["play"]
    obj = Path(rows["BrnPresentationEffect"]["object"])
    for offset, expected in {
        0x14: "a17e0014 2b0bfffe 409a00a0",
        0x5c: "815e0010",
        0x64: "3bfd0038 38610050 7fe4fb78 c02b0000 915d0290",
        0x8c: "38a00004 7fe4fb78 38610050 c02b0000",
        0xac: "897e0019 997d02bc",
        0x12c: "2b0bffff 91410090 409a003c",
        0x154: "817e0010 9061007c 91610084",
        0x200: "a17e0014 2b0bffff 409a0008",
    }.items():
        words(play, offset, expected)
    for offset, target in {
        0x78: "??0StreamStopRequest@", 0x84: "?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@QAAXABUStreamStopRequest@",
        0x9c: "??0StreamRequest@", 0xa8: "?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@QAAXABUStreamRequest@",
        0xc4: PREFIXES["find_voice"], 0x150: MAKE_HASH,
        0x1fc: "?Create@VoiceWrapper@", 0x218: "?Play@VoiceWrapper@",
    }.items():
        call(play, refs, offset, target)
    selected_literal(obj, name, 0x60, 0x70, bytes.fromhex("3e800000"), support)
    selected_literal(obj, name, 0x88, 0x98, bytes.fromhex("3dcccccd"), support)
    selected_literal(obj, name, 0x140, 0x144, wav, support)
    words(methods["get_params"][1], 0, "38630244 4e800020")
    words(methods["find_voice"][1], 0x34, "394a0088")
    return {
        "artist_stream_helper": "0x8269e418", "stream_splice": "0xfffe", "interface_this_offset": "0x38",
        "stop": {"artist_post": "0x8269e4fc", "coff_ctor_post": ["0x78", "0x84"], "fade_seconds": 0.25},
        "play": {"artist_post": "0x8269e508", "coff_ctor_post": ["0x9c", "0xa8"], "priority": 4,
                 "lag_f32": "0x3dcccccd", "coff_mixer_output_store_after_both": "0xb0"},
        "resident_wav": {"splice": "0xffff", "artist_arm": "0x826f79dc",
                         "coff_name_hash_create_play": ["0x150", "0x1fc", "0x218"]},
        "remaining_create_params_layout": {
            "artist_aging_voice_stride": "0x80", "coff_aging_voice_stride": "0x88",
            "artist_effect_offset": "0x25c", "coff_effect_offset": "0x27c", "delta": "0x20",
            "scope": "CreateParams field placement and its initialization timing remain outside this request-order proof.",
        },
    }


def verify():
    reader_path = WAVE / "r143-linked-proof/verify_linked.py"
    support_path = WAVE / "r128-batch/verify_matches.py"
    helper = runpy.run_path(str(reader_path))
    support = {"__file__": str(support_path)}
    exec(support_path.read_text().split("\nif len(sys.argv)==2:", 1)[0], support)
    coff = helper["coff_functions"]
    coff.__globals__["PREFIX"].update(PREFIXES)
    canon = helper["canonical_bindings"]

    assert git("rev-parse", HEAD + "^{tree}", HEAD + "^").decode().splitlines() == [TREE, BASE]
    candidate_path = PREFLIGHT / "CANDIDATE.json"
    candidate = json.loads(candidate_path.read_text())
    assert (candidate["head"], candidate["tree"], candidate["base"]) == (HEAD, TREE, BASE)
    assert set(git("diff-tree", "--no-commit-id", "-r", "--name-only", HEAD).decode().splitlines()) == set(candidate["paths"])
    for group in candidate["groups"]:
        assert sha(Path(group["patch"]).read_bytes()) == group["sha256"]
        for path in group["paths"]:
            assert git("rev-parse", f"{HEAD}:{path}") == git("rev-parse", f"{group['head']}:{path}")

    rows = read_report(PREFLIGHT, 15)
    assert {row["source"] for row in rows.values()} == set(candidate["sources"])
    components = {}
    for folder, count in ((LANGUAGE, 5), (HASH, 6), (MUSIC, 3), (PRESENTATION, 1)):
        component_rows = read_report(WAVE / folder, count)
        assert not components.keys() & component_rows.keys()
        components.update(component_rows)
    assert components.keys() == rows.keys()
    music_receipt = WAVE / MUSIC / "R149_POST_TITLE_MUSIC_COFF.json"
    frozen_music = json.loads(music_receipt.read_text())
    assert frozen_music["state"] == "pass"
    assert frozen_music["compile_report_sha256"] == pin(WAVE / MUSIC / "compile-results.json")["sha256"]
    for entry in frozen_music["objects"].values():
        assert pin(Path(entry["path"]))["sha256"] == entry["sha256"]

    artifacts = {}
    methods = {}
    for provider, row in rows.items():
        source = row["source"]
        obj = Path(row["object"])
        prior = Path(components[provider]["object"])
        assert source == components[provider]["source"]
        assert Path(row["command"][-1]) == Path(source)
        with Path(row["assembly"]).open(errors="replace") as listing:
            assert str(CHECKOUT / source).lower() in listing.read(512).lower()
        assert all(Path(row[key]).parent == PREFLIGHT for key in ("assembly", "log"))
        entry = artifacts[source] = {
            "source_blob": git("rev-parse", f"{HEAD}:{source}").decode().strip(),
            "object": pin(obj), "assembly": pin(Path(row["assembly"])), "log": pin(Path(row["log"])),
            "component_object": pin(prior), "compared_functions": {},
        }
        if provider not in FUNCTIONS:
            continue
        compiled = coff(obj.read_bytes(), FUNCTIONS[provider])
        component = coff(prior.read_bytes(), FUNCTIONS[provider])
        for key, (name, body, refs) in compiled.items():
            prior_name, prior_body, prior_refs = component[key]
            assert (name, body) == (prior_name, prior_body), key
            expected = canon(prior_refs)
            exception = {"boot": 0x824, "title": 0xf8}.get(key)
            if exception is not None:
                assert expected[exception] == (6, HASH_OLD, None)
                expected[exception] = (6, HASH_U64, None)
            assert canon(refs) == expected, key
            methods[key] = (name, body, refs)
            entry["compared_functions"][key] = {
                "symbol": name, "bytes": len(body), "body_sha256": sha(body),
                "relocations_sha256": sha(json.dumps(expected, sort_keys=True).encode()),
                "abi_relocation_change": hex(exception) if exception is not None else None,
            }

    raw = ARTIST.read_bytes()
    assert raw[:4] == b"XEX2" and sha(raw) == ARTIST_SHA
    xex_offset = word(raw, 8) - 0x82000000

    def read(address, size):
        begin = xex_offset + address
        assert 0 <= begin <= begin + size <= len(raw)
        return raw[begin:begin + size]

    def branch(pc):
        opcode = word(read(pc, 4))
        assert opcode & 0xfc000003 == 0x48000001
        return support["branch_target"](pc, opcode)

    return {
        "state": "pass", "head": HEAD, "tree": TREE, "base": BASE,
        "scope": "Targeted composed PPC COFF and original ARTIST words. This receipt covers compile-only objects.",
        "candidate": pin(candidate_path), "compile_report": pin(PREFLIGHT / "compile-results.json"),
        "artist": {"path": str(ARTIST), "sha256": ARTIST_SHA},
        "verifier": pin(Path(__file__)),
        "proof_inputs": [pin(path) for path in (
            reader_path, support_path, music_receipt, WAVE / HASH / "abi-review.md",
            *(WAVE / folder / "compile-results.json" for folder in (LANGUAGE, HASH, MUSIC, PRESENTATION)),
        )],
        "artifacts": artifacts,
        "language_view": language(read, branch, methods),
        "resource_hash": resource_hash(read, methods),
        "menu_music": music(read, branch, methods, rows, support),
        "presentation": presentation(read, branch, methods, rows, support),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    report = verify()
    if args.write:
        out = HERE / "R149_COMPOSED_PPC.json"
        with out.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
        print(pin(out))
    print(f"PASS {report['head']} ({len(report['artifacts'])} PPC objects)")


if __name__ == "__main__":
    main()
