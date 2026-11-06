"""Verify the frozen R148 compile-only evidence against Git, prior COFF, and ARTIST."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess
import sys


OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
PREFLIGHT = WAVE / "r148-boot-presentation-preflight"
CHECKOUT = Path("C:/Users/adri1/.codex/worktrees/bp360/native_props")
HEAD = "ebc86db664b0e6a4cd937bd5d89e7d26b9fa9976"
TREE = "292de86d70d0b034d502995355f6169a766a7d7e"
BASE = "5509461903bd56c25d92208d84da7a37171a4fc4"
GUI = "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp"
LANGUAGE = "src/GameShared/GameClasses/Language/CgsLanguageManager.cpp"
PRESENTATION = "src/GameSource/Sound/Global/BrnPresentationEffect.cpp"
ICE = "src/SDKs/Packages/ICE/"
CONTROLLER = ICE + "ICEController.cpp"
MENUS = ICE + "ICEControllerMenus.cpp"
WIDGET = ICE + "ICEWidget/ICEWidgetMenu.cpp"
SOURCE_HEADS = {
    GUI: BASE,
    LANGUAGE: "6b66980c6dcf43a6cec3fba0671c69f5c911084b",
    PRESENTATION: "bc93e214d327c5e1670d7dce1f7f8c69d91c55db",
    CONTROLLER: BASE,
    MENUS: "37d75a02e37dd25d3c0c28157e237ab768936791",
    WIDGET: "37d75a02e37dd25d3c0c28157e237ab768936791",
}
INPUT_PINS = {
    PREFLIGHT / "CANDIDATE.json": "1fc1ab3ca641c01f2f83dc7b47eb3c98afa5759c0f82a9f815cc506d8cf1fd83",
    PREFLIGHT / "compile-results.json": "f1d161a70d55f7c8a91d66027f71a4a305a2061368aaf0e921ea58be7f76cc2a",
    WAVE / "r155-ice-menu-layouts/verify.py": "f9332148bce6147b05f5785c24e25b9878e7cc5a55441b0e08c79260c7a7aaca",
    WAVE / "r155-ice-menu-layouts/PPC_EVIDENCE.json": "130030a74b2aa1159fd70c6dae1b92ce65365cb257088fc79fc46ffdb0d115ec",
    WAVE / "r153-presentation-stream/compile-results.json": "cfbc274e12b8cf00dfb1cc9b7b8d4acdbb52d015513a5d50d7406a75dfc4748d",
    WAVE / "r156-language-release/compile-results.json": "d28b7ed0e0a91ccf52b7773996e907348ef75e88b183ccb9e70826bd0084727a",
}
XDK_FLAGS = [
    "/nologo", "/c", "/X", "/O2", "/Oi", "/Ob2", "/GR-", "/GS-",
    "/EHs-c-", "/Gy", "/FAcs", "/D_XBOX", "/DD_PLATFORM_X360=1",
    "/DCGS_PLATFORM_X360=1", "/FItypes.hpp",
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sha_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(CHECKOUT), *args])


def git_text(*args):
    return git(*args).decode("utf-8").strip()


def normalize_lines(data):
    return data.replace(b"\r\n", b"\n")


def compile_report(path, count):
    report = json.loads(path.read_bytes())
    assert (report["stage"], report["state"], report["target"]) == (
        "compile-only", "complete", "xbox360-ppc-be",
    )
    assert report["passed"] == report["requested"] == len(report["results"]) == count
    assert report["failed"] == 0
    rows = {row["source"]: row for row in report["results"]}
    assert len(rows) == count
    for row in rows.values():
        assert row["passed"] is True and row["returncode"] == 0 and row["machine"] == 0x1F2
        assert row["command"][1:1 + len(XDK_FLAGS)] == XDK_FLAGS
        assert Path(row["command"][0]) == Path(report["sdk"]) / "bin/win32/cl.exe"
    return rows


def checked_artifacts(candidate, coff):
    assert git_text("rev-parse", "HEAD", "HEAD^{tree}", "HEAD^").splitlines() == [HEAD, TREE, BASE]
    assert not git("status", "--porcelain")
    assert (candidate["head"], candidate["tree"], candidate["base"]) == (HEAD, TREE, BASE)
    changed = {source for source, origin in SOURCE_HEADS.items() if origin != BASE}
    assert set(git_text("diff", "--name-only", BASE, HEAD).splitlines()) == set(candidate["paths"]) == changed
    assert set(candidate["sources"]) == set(SOURCE_HEADS)
    covered = set()
    for group in candidate["groups"]:
        assert set(group["paths"]) <= changed - covered
        assert all(SOURCE_HEADS[source] == group["head"] for source in group["paths"])
        patch = Path(group["patch"])
        assert patch.parent == PREFLIGHT and sha_file(patch) == group["sha256"]
        covered.update(group["paths"])
    assert covered == changed

    rows = compile_report(PREFLIGHT / "compile-results.json", 6)
    assert set(rows) == set(SOURCE_HEADS)
    objects, proof = {}, {}
    for source, row in rows.items():
        blob = git("show", f"{HEAD}:{source}")
        assert blob == git("show", f"{SOURCE_HEADS[source]}:{source}")
        source_path = CHECKOUT / source
        current = source_path.read_bytes()
        assert normalize_lines(current) == normalize_lines(blob), source
        command = row["command"]
        assert Path(command[-1]).as_posix() == source
        assert f"/I{CHECKOUT / 'src'}" in command
        assert "/Fo" + row["object"] in command and "/Fa" + row["assembly"] in command
        files = {kind: Path(row[kind]) for kind in ("object", "assembly", "log")}
        assert all(path.parent == PREFLIGHT for path in files.values())
        objects[source] = coff(files["object"])
        proof[source] = {
            "matching_source_head": SOURCE_HEADS[source],
            "git_blob": git_text("rev-parse", f"{HEAD}:{source}"),
            "git_blob_sha256": sha(blob),
            "checkout_sha256": sha(current),
            "checkout_path": str(source_path),
            "command_sha256": sha(json.dumps(command, separators=(",", ":")).encode()),
            "outputs": {kind: {"path": str(path), "sha256": sha_file(path)} for kind, path in files.items()},
        }
    return objects, proof


def prior_object(coff, directory, source, count, expected):
    rows = compile_report(directory / "compile-results.json", count)
    path = Path(rows[source]["object"])
    assert path.parent == directory and sha_file(path) == expected
    return coff(path), {"path": str(path), "sha256": expected}


def same_bodies(new, prior, *prefixes):
    result = {}
    for prefix in prefixes:
        body, refs = new.body(prefix)
        assert (body, refs) == prior.body(prefix), prefix
        result[new.symbol(prefix)[0]] = {
            "bytes": len(body), "body_sha256": sha(body),
            "relocations": {hex(at): {"symbol": name, "type": kind} for at, (name, kind) in refs.items()},
        }
    return result


def branch_target(address, word):
    assert word & 0xFC000002 == 0x48000000
    displacement = word & 0x03FFFFFC
    return address + ((displacement ^ 0x02000000) - 0x02000000)


def raw_bl(raw, address, target):
    word = raw.u32(address)
    assert word & 1 and branch_target(address, word) == target, hex(address)


def raw_string(raw, hi, lo, expected):
    upper, lower = raw.u32(hi), raw.u32(lo)
    assert upper >> 26 == 15 and upper >> 16 & 31 == 0 and lower >> 26 == 14
    assert upper >> 21 & 31 == lower >> 16 & 31 and lower >> 21 & 31 == 3
    signed_low = ((lower & 0xFFFF) ^ 0x8000) - 0x8000
    address = ((upper & 0xFFFF) << 16) + signed_low
    assert raw.rd(address, len(expected)) == expected
    return hex(address)


def verify_ice(objects, h):
    raw, coff, words, relocation, trace = (h[key] for key in ("x360rd", "Coff", "words", "relocation", "trace"))
    directory = WAVE / "r155-ice-menu-layouts"
    historical = json.loads((directory / "PPC_EVIDENCE.json").read_bytes())
    assert historical["head"] == SOURCE_HEADS[WIDGET] == SOURCE_HEADS[MENUS]
    assert sha_file(directory / "native/compile-results.json") == historical["compile_report_sha256"]
    prefixes = {
        WIDGET: ("?Construct@ICEWidgetMenu@", "?GetWidth@ICEWidgetMenu@", "?Render@ICEWidgetMenu@"),
        MENUS: ("?ConstructMenus@ICEController@",),
    }
    same, prior = {}, {}
    for source, functions in prefixes.items():
        original, prior[source] = prior_object(coff, directory / "native", source, 2, historical["objects_sha256"][source])
        same.update(same_bodies(objects[source], original, *functions))
    widget, menus = objects[WIDGET], objects[MENUS]
    table = widget.literal("kaMenuStyles", 7 * 0x20)
    assert table == raw.rd(0x8207ABB8, len(table))
    assert sha(table) == historical["style_table"]["sha256"]
    assert widget.literal("KF_ICE_MENU_MARGIN", 4) == raw.rd(0x82004A20, 4)
    assert raw.u32(0x82531024) == 0x1C8B00A4
    assert menus.literal("KF_ICE_MENU_Y_SPACING", 4) == raw.rd(0x82004FD8, 4)
    assert raw.f32(0x82004FD8) == 25.0
    ctor, ctor_refs = objects[CONTROLLER].body("?Construct@ICEController@")
    words(ctor, 0xC8, 0x4BFFFF39)
    relocation(ctor_refs, 0xC8, "?ConstructMenus@ICEController@", 6)
    raw_bl(raw, 0x8253DAF8, 0x82531F40)

    geometry, geometry_refs = menus.body(prefixes[MENUS][0])
    style_order = (1, 2, 0, 4, 3, 5, 6)
    for at, style in zip((0x40, 0x64, 0xBC, 0xE0, 0x104, 0x128, 0x14C), style_order):
        relocation(geometry_refs, at, "?Construct@ICEWidgetMenu@", 6)
        assert struct.pack(">I", 0x38A00000 | style) in geometry[at - 0x14:at]
    raw_calls = (0x82531F94, 0x82531FB8, 0x82532018, 0x8253203C, 0x82532064, 0x82532094, 0x825320C0)
    raw_style_loads = (0x82531F78, 0x82531FA4, 0x82531FFC, 0x82532028, 0x82532050, 0x82532078, 0x825320AC)
    for call, load, style in zip(raw_calls, raw_style_loads, style_order):
        raw_bl(raw, call, 0x82530F68)
        assert raw.u32(load) == 0x38A00000 | style
    assert raw.u32(0x82531FCC) == 0xC3CB4FD8 and raw.u32(0x82531FE8) == 0xD1260038
    for offset, value in (
        (0x190, raw.f32(0x82F2884C) - raw.f32(0x82004FD8)),
        (0x1A0, raw.f32(0x82F28848) + raw.f32(0x8207B5F8)),
        (0x214, raw.f32(0x8207ACC4)),
        (0x220, raw.f32(0x8207ACC8)),
    ):
        bits = struct.pack(">f", value)
        symbol = "__real@" + bits.hex()
        relocation(geometry_refs, offset, symbol, 0x11)
        assert menus.literal(symbol, 4) == bits

    original_stores = {
        0x82532108: 0xD0BF7140, 0x8253211C: 0x915F7138, 0x82532120: 0xD0DF713C,
        0x825321B0: 0x915F71EC, 0x825321B4: 0xD11F71E4, 0x825321B8: 0xD0FF71E8,
    }
    for address, word in original_stores.items():
        assert raw.u32(address) == word
    return {
        "r155_objects": prior,
        "equal_coff_bodies": same,
        "style_table": historical["style_table"],
        "startup_style_order": style_order,
        "controller_construct_call": {"r148_coff_offset": "0xc8", "artist_call": "0x8253daf8", "artist_target": "0x82531f40"},
        "startup_geometry_coff": trace(geometry, 0x8C, 0x98, 0x9C, 0x198, 0x19C, 0x1A8, 0x1E8, 0x21C, 0x224),
        "startup_geometry_artist": {hex(address): f"{word:08x}" for address, word in original_stores.items()},
    }


def verify_language(objects, h):
    raw, coff, words, relocation = (h[key] for key in ("x360rd", "Coff", "words", "relocation"))
    original, prior = prior_object(
        coff, WAVE / "r156-language-release", LANGUAGE, 1,
        "72e3c38e22208e222237f28e07aed1b627e999f62e56f57b368a2b5d909bfcba",
    )
    release, unload = "?Release@LanguageManager@", "?UnloadStringTable@LanguageManager@"
    same = same_bodies(objects[LANGUAGE], original, release, unload)
    body, refs = objects[LANGUAGE].body(release)
    assert set(refs) == {0xC}
    relocation(refs, 0xC, unload, 6)
    words(body, 0xC, 0x4BFFFFF5, 0x38600001)
    raw_bl(raw, 0x8286400C, 0x82862540)
    artist_body = raw.rd(0x82864000, len(body))
    assert body[:0xC] == artist_body[:0xC] and body[0x10:] == artist_body[0x10:]

    gui, gui_refs = objects[GUI].body("?Release@ViewModule@")
    relocation(gui_refs, 0x9C, release, 6)
    words(gui, 0x98, 0x387E7C0C, 0x4BFFFF65, 0x546B063E, 0x2B0B0000, 0x419A0060)
    raw_bl(raw, 0x82858604, 0x82864000)
    words(raw.rd(0x828585FC, 0x18), 0, 0x387E7C0C, 0x917F0000, 0x4800B9FD, 0x546B063E, 0x2B0B0000, 0x419A006C)
    return {
        "r156_object": prior,
        "equal_coff_bodies": same,
        "artist_release_body_hex": artist_body.hex(),
        "artist_unload_call": ["0x8286400c", "0x82862540"],
        "artist_gui_call": ["0x82858604", "0x82864000"],
        "coff_gui_call_offset": "0x9c",
        "coff_gui_return_test_offset": "0xa0",
        "comparison": "The complete Release body equals ARTIST with only its resolved REL24 displacement masked.",
    }


def verify_presentation(objects, h):
    raw, coff, words, relocation = (h[key] for key in ("x360rd", "Coff", "words", "relocation"))
    original, prior = prior_object(
        coff, WAVE / "r153-presentation-stream", PRESENTATION, 1,
        "0cd23216581bf40f195fecb6ba21d0e95425076abcff974acdfe103bfeff15ca",
    )
    obj = objects[PRESENTATION]
    attach_prefix = "?Attach@PresentationEffect@"
    play_prefix = "?Play@PresentationEffect@"
    params_prefix = "?GetCreateParams@PresentationEffect@"
    same = same_bodies(obj, original, attach_prefix, play_prefix, params_prefix)
    attach, attach_refs = obj.body(attach_prefix)
    play, play_refs = obj.body(play_prefix)
    getter, _ = obj.body(params_prefix)
    music = b"MusicVoiceSpec\0"
    wave = b"StereoWavVoiceSpec\0"
    music_literal = "??_C@_0P@FLFHBPI@MusicVoiceSpec?$AA@"
    wave_literal = "??_C@_0BD@LBJMKPLH@StereoWavVoiceSpec?$AA@"
    assert obj.literal(music_literal, len(music)) == music
    assert obj.literal(wave_literal, len(wave)) == wave
    for refs, hi, lo, symbol in ((attach_refs, 0xC0, 0xC4, music_literal), (play_refs, 0x124, 0x128, wave_literal)):
        relocation(refs, hi, symbol, 0x10)
        relocation(refs, lo, symbol, 0x11)
    relocation(attach_refs, 0xD0, "?MakeHash@Name@Playback@", 6)
    words(attach, 0xD8, 0x7C6A1B78)
    words(attach, 0xE4, 0x915F0284)
    words(getter, 0, 0x38630244, 0x4E800020)
    words(play, 0x14, 0xA17E0014, 0x2B0BFFFE, 0x409A0084)
    words(play, 0x7C, 0x915D0290, 0x897E0019, 0x997D02BC)
    relocation(play_refs, 0x88, "??0StreamRequest@", 6)
    relocation(play_refs, 0x94, "?PostStreamRequest@StreamingStateManager@", 6)
    words(play, 0x110, 0x2B0BFFFF, 0x91410090, 0x409A003C)
    relocation(play_refs, 0x134, "?MakeHash@Name@Playback@", 6)
    words(play, 0x13C, 0x9061007C)

    words(raw.rd(0x826F7920, 0x30), 0, 0x2B0BFFFE, 0x419A001C, 0x3D400000, 0x614AFFFF)
    assert raw.u32(0x826F7940) == 0x39600002 and raw.u32(0x826F794C) == 0x419A0090
    raw_bl(raw, 0x826F7960, 0x8269E418)
    raw_bl(raw, 0x8269E478, 0x82689A50)
    assert raw.u32(0x8269E480) == 0x907F0268
    raw_bl(raw, 0x8269E4FC, 0x826835D8)
    words(raw.rd(0x82683624, 0x2C), 0x14, 0x815E0004, 0x914B0004, 0x815E0008, 0x914B0008, 0x815E000C, 0x914B000C)
    assert raw.u32(0x8269E494) == 0xC00B3F40 and raw.u32(0x8269E4A0) == 0xD0010054
    assert raw.f32(0x82003F40) == 0.25
    words(play, 0x6C, 0x38A00006)
    tolerance = "__real@3dcccccd"
    relocation(play_refs, 0x78, tolerance, 0x11)
    assert obj.literal(tolerance, 4) == struct.pack(">f", 0.1)
    raw_bl(raw, 0x826F7A6C, 0x82689A50)
    return {
        "r153_object": prior,
        "equal_coff_bodies": same,
        "stream": {
            "splice": "0xfffe", "voice_spec": music[:-1].decode(),
            "artist_play_to_helper": ["0x826f7960", "0x8269e418"],
            "artist_literal": raw_string(raw, 0x8269E45C, 0x8269E464, music),
            "artist_store": "0x8269e480",
            "artist_post_stream_request": ["0x8269e4fc", "0x826835d8"],
            "coff_attach_spec_store_offset": "0xe4", "coff_post_stream_request_offset": "0x94",
            "request_abi_observation": "ARTIST passes a four-word request with f32 0.25 at +4. R148 passes reconstructed StreamRequest(this, 6, 0.1f).",
        },
        "resident_wave": {
            "splice": "0xffff", "voice_spec": wave[:-1].decode(),
            "artist_play_arm": "0x826f79dc",
            "artist_literal": raw_string(raw, 0x826F7A40, 0x826F7A4C, wave),
            "coff_play_spec_store_offset": "0x13c",
        },
        "comparison": "Splice and spec routing agree. R148 preconfigures the stream spec in Attach. ARTIST sets it in the stream-play helper. This verifies neither equivalent request parameters nor the subsequent streaming manager.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-only", action="store_true", help="Run all checks without creating a receipt.")
    args = parser.parse_args()
    assert __debug__, "Run without Python optimization to enable proof assertions."
    sys.dont_write_bytecode = True
    for path, digest in INPUT_PINS.items():
        assert sha_file(path) == digest, path
    helpers = runpy.run_path(str(WAVE / "r155-ice-menu-layouts/verify.py"))
    raw = helpers["x360rd"]
    assert raw.RAW and not raw.FLAT
    candidate = json.loads((PREFLIGHT / "CANDIDATE.json").read_bytes())
    objects, inputs = checked_artifacts(candidate, helpers["Coff"])
    proof = {
        "status": "pass",
        "scope": "Exact candidate source and six frozen compile-only PPC objects. Selected COFF and ARTIST witnesses.",
        "head": HEAD,
        "tree": TREE,
        "parent": BASE,
        "frozen_inputs_sha256": {str(path): digest for path, digest in INPUT_PINS.items()},
        "candidate_patches": candidate["groups"],
        "artist_raw": {"path": raw.RAW, "sha256": sha_file(Path(raw.RAW))},
        "sources_and_outputs": inputs,
        "ice": verify_ice(objects, helpers),
        "language": verify_language(objects, helpers),
        "presentation": verify_presentation(objects, helpers),
    }
    if args.verify_only:
        print("R148 preflight checks passed")
        return
    receipt = OUT / "R148_BOOT_PRESENTATION_PPC.json"
    with receipt.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(proof, stream, indent=2)
        stream.write("\n")
    print(receipt, sha_file(receipt))


if __name__ == "__main__":
    main()
