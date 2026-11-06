"""Verify R159 LiveUpdate bridge, selected PPC providers and linked vtable targets."""
import argparse
import json
from pathlib import Path
import runpy
import subprocess
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
GIT = "C:/Users/adri1/.codex/worktrees/bp360/debug_format_luna"
DONOR = WAVE / "r159-live-update"
HEAD = "dfa82054e6232064aaa9b9c4e8071dd760dc6dbc"
TREE = "878e29c6c2089a7df053f64732cfa47d13132bef"
DONOR_SHA = "413c36900e08fcd171a144214fd8709493a788c7cd7ad66e28ba67d55b92137d"
PRIOR = runpy.run_path(str(WAVE / "r158-boot-linked/verify_boot.py"))
IMAGE, COFF = PRIOR["IMAGE"], PRIOR["HELPER"]["Coff"]
digest, sha, word, load, pe_bytes, check_listing = (
    PRIOR[key] for key in ("digest", "sha", "word", "load", "pe_bytes", "check_listing")
)
MASKS = PRIOR["MASKS"]
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import u32

RESOURCE = "src/GameSource/Resource/"
SDK = "src/GameShared/GameClasses/System/Resource/"
DATA = RESOURCE + "BrnGameDataModule.cpp"
DATA_IO = RESOURCE + "BrnGameDataModuleIO.cpp"
LIVE = SDK + "CgsLiveUpdateModule.cpp"
LIVE_IO = SDK + "CgsLiveUpdateModuleIO_OutputBuffer.cpp"
VTABLE = "??_7LiveUpdateModule@CgsResource@@6B@"
L = "@LiveUpdateModule@CgsResource@@"
O = "@OutputBuffer@LiveUpdateIO@CgsResource@@"
C = "@GameDataModule@BrnResource@@"
D = "@OutputBuffer@GameDataIO@BrnResource@@"
FS_CONSTRUCT = "?Construct@FileSystemStatusInterface@ResourceIO@"
LOCK_W = "?LockForWrite@IOBuffer@"
LOCK_R = "?LockForRead@IOBuffer@"
UNLOCK_W = "?UnlockForWrite@IOBuffer@"
UNLOCK_R = "?UnlockForRead@IOBuffer@"
CREATE = "??$CreateIOBuffer@UOutputBuffer@LiveUpdateIO@"
DESTROY = "??$DestroyIOBuffer@UOutputBuffer@LiveUpdateIO@"

CHECKS = {
    "embedded_vptr": (DATA, "??0GameDataModule@BrnResource@@", {}, {
        0x50: 0x3FDF0006, 0x58: 0x3BDE8500, 0x8C: 0x917E0000,
    }),
    "construct_dispatch": (DATA, "?Construct" + C, {}, {
        0x15C: 0x3C7F0006, 0x160: 0x7FA4EB78, 0x164: 0x38638500,
        0x168: 0x81630000, 0x16C: 0x816B0040, 0x170: 0x7D6903A6, 0x174: 0x4E800421,
    }),
    "update_bridge": (DATA, "?Update" + C, {
        0x5C: "?Update@GameTalk@",
        0x64: LOCK_W, 0x6C: LOCK_R, 0x74: FS_CONSTRUCT,
        0x88: "?SetAllocatorList" + D, 0xA0: CREATE, 0xC8: LOCK_R,
        0xD0: "?GetSt" + O, 0xDC: "?SetLiveUpdateStatus" + D, 0xE4: UNLOCK_W,
        0xF0: LOCK_W, 0xF8: "?GetOutputQueue" + O + "QBAPBV",
        0x104: "?GetResourceQueue@InputBuffer@ResourceIO@CgsResource@@QAAPAU",
        0x10C: "??$Append@$0CAA@$0BA@@?$VariableEventQueue@$0EAAA@$0BA@@",
        0x114: UNLOCK_R, 0x120: DESTROY, 0x128: "?GetRequestInterface@InputBuffer@GameDataIO@",
    }, {
        0x70: 0x387B0009, 0x80: 0x93FB0004, 0xA4: 0x3C7E0006,
        0xAC: 0x38638500, 0xB0: 0x7FA4EB78, 0xB4: 0x81630000,
        0xB8: 0x816B0044, 0xBC: 0x7D6903A6, 0xC0: 0x4E800421,
        0xD4: 0x7C641B78, 0xD8: 0x7F63DB78, 0xEC: 0x7F43D378,
        0xF4: 0x7FA3EB78, 0x100: 0x7F43D378, 0x108: 0x7F24CB78,
    }),
    "buffer_create": (DATA, CREATE, {
        0x4C: "?Alloc@IOBufferStack@", 0x74: "?Construct" + O,
    }, {0x44: 0x38800218}),
    "buffer_destroy": (DATA, DESTROY, {
        0x54: "?Destruct@IOBuffer@", 0x64: "?Free@IOBufferStack@",
    }, {0x58: 0x38A00218}),
    "game_data_output_construct": (DATA_IO, "?Construct" + D, {
        0x14: "?Construct@IOBuffer@", 0x1C: FS_CONSTRUCT,
    }, {0x18: 0x387F0009, 0x20: 0x39600000, 0x24: 0x917F0004}),
    "game_data_output_status": (DATA_IO, "?SetLiveUpdateStatus" + D, {}, {
        0x20: 0x556BEFFE, 0x4C: 0x897E0000, 0x50: 0x997F0008,
    }),
    "live_construct": (LIVE, "?Construct" + L, {
        0x48: "?Construct@ModuleSingleBuffered@", 0x68: "?Clear@BaseEventReceiverQueue@",
    }, {0x4C: 0x387F0228, 0x50: 0x39600200, 0x54: 0x39400010, 0x78: 0x93BF0440}),
    "live_update": (LIVE, "?Update" + L, {
        0x18: LOCK_W, 0x74: "?Get" + O, 0x90: "?SetFileName@BundleLoaderEvent@",
        0xBC: "?AddEvent@?$VariableEventQueue@$0CAA@$0BA@@CgsModule@@",
        0x1FC: "?SetStatus" + O, 0x204: UNLOCK_W,
    }, {0x9C: 0x38C00094, 0xA4: 0x38A00002}),
    "live_output_construct": (LIVE_IO, "?Construct" + O, {
        0x14: "?Construct@IOBuffer@", 0x1C: "?Construct@?$VariableEventQueue@$0CAA@$0BA@@",
    }, {0x18: 0x387F0004, 0x20: 0x39600000, 0x24: 0x997F0214}),
    "live_status_reader": (LIVE_IO, "?GetSt" + O, {}, {0x18: 0x556BE7FE, 0x44: 0x387F0214}),
    "live_queue_reader": (LIVE_IO, "?GetOutputQueue" + O + "QBAPBV", {}, {0x18: 0x556BE7FE, 0x44: 0x387F0004}),
    "live_status_writer": (LIVE_IO, "?SetStatus" + O, {}, {0x20: 0x556BEFFE, 0x50: 0x997F0214}),
}
ARTIST_WORDS = {
    0x82671CE4: 0x3C7F0006, 0x82671CEC: 0x38638500, 0x82671CF4: 0x816B0040,
    0x82671CFC: 0x4E800421, 0x82674818: 0x38720009, 0x82674828: 0x92720004,
    0x82674848: 0x3C7B0006, 0x82674850: 0x38638500, 0x8267485C: 0x816B0044,
    0x82674864: 0x4E800421, 0x826636F4: 0x38800218, 0x8266A9E8: 0x38A00218,
    0x828EEFB0: 0x387F0004, 0x828EEFC0: 0x997F0214, 0x826642C8: 0x387C0214,
    0x82664370: 0x387C0004, 0x829031C0: 0x38C00094, 0x829031C8: 0x38A00002,
}
ARTIST_VPTR = {
    0x827E3284: 0x3FFE0006, 0x827E328C: 0x3BFF8500,
    0x827E32B0: 0x3D60820D, 0x827E32B8: 0x396BE6D0, 0x827E32C0: 0x917F0000,
    0x82663ED0: 0x997C0008,
}
ARTIST_CALLS = {
    0x8267480C: 0x82204A18, 0x82674814: 0x82204C88, 0x8267481C: 0x82676600,
    0x8267482C: 0x826640E0, 0x82674844: 0x82663658, 0x8267486C: 0x82204C88,
    0x82674874: 0x82664230, 0x82674880: 0x82663E30, 0x82674888: 0x82204BB8,
    0x82674894: 0x82204A18, 0x8267489C: 0x826642D8, 0x826748A8: 0x82663B38,
    0x826748B0: 0x8266BE38, 0x826748B8: 0x82204D50, 0x826748C4: 0x8266A940,
    0x826748CC: 0x82663F90, 0x826636FC: 0x8286EDE8, 0x8266370C: 0x828EEF98,
    0x8266A9E4: 0x82216E60, 0x8266A9F4: 0x8286EF10, 0x829031E0: 0x8224B828,
    0x82903314: 0x828E63F0, 0x8290331C: 0x82204BB8,
}


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", GIT, *args])


def tree(head):
    return git("rev-parse", f"{head}^{{tree}}").decode().strip()


def blob(head, source):
    return git("rev-parse", f"{head}:{source}").decode().strip()


def source_bytes(head, source):
    return git("show", f"{head}:{source}")


def canonical(code, refs):
    return b"".join((word(code, offset) & ~MASKS[refs[offset][0]] if offset in refs
                     else word(code, offset)).to_bytes(4, "big") for offset in range(0, len(code), 4))


def split_address(code, high, low):
    lo = word(code, low) & 0xFFFF
    return ((word(code, high) & 0xFFFF) << 16) + lo - (0x10000 if lo & 0x8000 else 0)


def inspect(label, spec, rows, head, linked=None):
    source, prefix, edges, words = spec
    row = rows[source]
    path = Path(row["object"])
    obj_data = path.read_bytes()
    object_sha = digest(obj_data)
    assert row.get("sha256", object_sha) == object_sha
    obj = COFF(obj_data)
    symbol, code, refs = obj.function(prefix)
    result = {
        "source": source, "git_blob": blob(head, source), "object": str(path), "object_sha256": object_sha,
        "symbol": symbol.name, "coff_body_sha256": digest(code),
        "coff_masked_body_sha256": digest(canonical(code, refs)),
        "words": {hex(offset): hex(expected) for offset, expected in words.items()}, "calls": {},
    }
    for offset, expected in words.items():
        assert offset not in refs and word(code, offset) == expected, (label, hex(offset), hex(word(code, offset)))
    if linked:
        image, entries, sources = linked
        src = sources[source.lower()]
        src_data = Path(src["path"]).read_bytes()
        assert digest(src_data) == src["sha256"]
        assert src_data.replace(b"\r\n", b"\n") == source_bytes(head, source).replace(b"\r\n", b"\n")
        [va] = {address for address, owner in entries[symbol.name] if owner.lower() == path.name.lower()}
        linked_code = pe_bytes(image, va, len(code))
        assert canonical(linked_code, refs) == canonical(code, refs), label
        result.update(linked_va=hex(va), linked_body_sha256=digest(linked_code), source_sha256=src["sha256"])
    else:
        listing = Path(row["assembly"])
        check_listing(listing, symbol.name, code, refs)
        result.update(listing=str(listing), listing_sha256=sha(listing))
    for offset, prefix in edges.items():
        kind, target = refs[offset]
        assert kind == 6 and target.name.startswith(prefix), (label, hex(offset), target.name)
        call = {"symbol": target.name}
        if linked:
            site = va + offset
            raw = image.word(site)
            assert raw >> 26 == 18 and raw & 1
            address = image.target(site, raw)
            owners = sorted({owner for destination, owner in entries[target.name] if destination == address})
            assert owners, (label, hex(site), target.name, hex(address))
            call.update(site_va=hex(site), target_va=hex(address), owners=owners)
        result["calls"][hex(offset)] = call
    if label == "embedded_vptr":
        assert refs[0x7C][0] == 16 and refs[0x84][0] == 17
        assert refs[0x7C][1] == refs[0x84][1] and refs[0x7C][1].name == VTABLE
        if linked:
            [table] = {address for address, owner in entries[VTABLE] if owner.lower() == path.name.lower()}
            assert split_address(linked_code, 0x7C, 0x84) == table
            result["installed_vtable_va"] = hex(table)
    if label == "update_bridge":
        assert refs[0x8C][0] == 16 and refs[0x98][0] == 17 and refs[0x8C][1] == refs[0x98][1]
        assert obj.literal(refs[0x8C][1], 11) == b"LiveUpdate\0"
        if linked:
            literal = split_address(linked_code, 0x8C, 0x98)
            assert pe_bytes(image, literal, 11) == b"LiveUpdate\0"
            result["literal_va"] = hex(literal)
    return result


def verify_vtable(rows, prior, linked=None, checked=None):
    row = rows[DATA]
    path = Path(row["object"])
    obj = COFF(path.read_bytes())
    symbol = obj.definition(VTABLE)
    _, raw, _, count = obj.section(symbol.section)
    assert len(raw) == 0x48 and count == 18
    results = []
    if linked:
        image, entries, _ = linked
        table = int(checked["embedded_vptr"]["installed_vtable_va"], 16)
    for slot in prior["vtable"]:
        offset = slot["offset"]
        target = obj.vtable_slot(VTABLE, offset)
        assert target == slot["coff_symbol"], (offset, target)
        result = {**slot}
        if linked:
            address = image.word(table + offset)
            owners = sorted({owner for va, owner in entries[target] if va == address})
            assert owners, (offset, target, hex(address))
            if offset in (0x40, 0x44):
                label = "live_construct" if offset == 0x40 else "live_update"
                assert address == int(checked[label]["linked_va"], 16)
            result.update(linked_slot_va=hex(table + offset), linked_target_va=hex(address), owners=owners)
        results.append(result)
    return results


def donor_objects():
    prior_path = DONOR / "RECEIPT.json"
    assert sha(prior_path) == DONOR_SHA
    prior = load(prior_path)
    assert prior["commit"] == HEAD and prior["tree"] == tree(HEAD) == TREE
    compile_path = DONOR / "compile-receipt.json"
    assert sha(compile_path) == prior["compile_receipt_sha256"]
    report = load(compile_path)
    assert len(report["results"]) == prior["focused_ppc_compiles"] == 5
    for source, expected in report["input_sha256"].items():
        raw = source_bytes(HEAD, source)
        assert expected in {digest(raw), digest(raw.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))}, source
    rows = {}
    for item in report["results"]:
        assert item["exit_code"] == 0
        stem = Path(item["source"]).stem
        obj, asm = DONOR / (stem + ".obj"), DONOR / (stem + ".asm")
        assert sha(obj) == item["ppc_coff_sha256"] and sha(asm) == item["listing_sha256"]
        rows[item["source"]] = {"object": obj, "assembly": asm, "sha256": item["ppc_coff_sha256"]}
    return prior, rows


def composed_objects(args, donor_rows):
    assert args.composed_tree and tree(args.composed_head) == args.composed_tree
    assert args.composed_sha256 and sha(args.composed_report) == args.composed_sha256
    report = load(args.composed_report)
    assert report["requested"] == report["passed"] and report["failed"] == 0
    rows = {item["source"]: item for item in report["results"]}
    assert all(row["machine"] == 0x1F2 and row["passed"] and row["returncode"] == 0 for row in rows.values())
    for source, *_ in CHECKS.values():
        if source not in rows:
            assert blob(args.composed_head, source) == blob(HEAD, source), source
            rows[source] = donor_rows[source]
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--composed-head")
    parser.add_argument("--composed-tree")
    parser.add_argument("--composed-report", type=Path)
    parser.add_argument("--composed-sha256")
    parser.add_argument("--link-head")
    parser.add_argument("--completion-path", type=Path, default=WAVE / "r159-intake/completion.json")
    for pin in ("seal", "invocation", "result", "pe", "map", "xex", "image-verification", "completion"):
        parser.add_argument(f"--{pin}-sha256")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    donor, donor_rows = donor_objects()
    for row in donor["vtable"]:
        assert u32(0x820CE6D0 + row["offset"]) == int(row["artist_target"], 16)
    for address, expected in (ARTIST_WORDS | ARTIST_VPTR).items():
        assert u32(address) == expected, hex(address)
    for address, expected in ARTIST_CALLS.items():
        raw = u32(address)
        assert raw >> 26 == 18 and raw & 1 and IMAGE.target(None, address, raw) == expected

    rows = composed_objects(args, donor_rows) if args.composed_head else donor_rows
    head = args.composed_head or HEAD
    checked = {name: inspect(name, spec, rows, head) for name, spec in CHECKS.items()}
    receipt = {
        "head": head, "tree": tree(head), "donor_head": HEAD, "donor_receipt_sha256": DONOR_SHA,
        "artist_vtable": "0x820CE6D0",
        "artist_words": {hex(address): hex(value) for address, value in ARTIST_WORDS.items()},
        "artist_calls": {hex(address): hex(target) for address, target in ARTIST_CALLS.items()},
        "functions": checked, "vtable": verify_vtable(rows, donor),
    }
    if args.composed_head:
        receipt["composed_report"] = {"path": str(args.composed_report), "sha256": args.composed_sha256}
        receipt["artist_vptr_words"] = {hex(address): hex(value) for address, value in ARTIST_VPTR.items()}
    if args.link_head:
        assert args.composed_head and tree(args.link_head) == args.composed_tree
        link = WAVE / "r159-link"
        files = {
            "seal": link / "seal.json", "invocation": link / "invocation.json", "result": link / "result.json",
            "pe": link / "native-game.pe", "map": link / "native-game.map", "xex": link / "native-game.exe",
            "image_verification": link / "image-verification.json",
            "completion": args.completion_path,
        }
        pins = {key: getattr(args, key + "_sha256") for key in files}
        for key, path in files.items():
            assert pins[key] and sha(path) == pins[key], key
        seal, invocation, result = (load(files[key]) for key in ("seal", "invocation", "result"))
        completion, image_check = (load(files[key]) for key in ("completion", "image_verification"))
        assert seal["head"] == completion["head"] == image_check["head"] == args.link_head
        assert invocation["sealed_sha256"] == pins["seal"] and invocation["objects"] == seal["selected"]
        assert result["exit_code"] == 0 and result["all_post_checks_pass"]
        outputs = {Path(path).name: row for path, row in result["outputs"].items()}
        for key in ("pe", "map", "xex"):
            assert outputs[files[key].name]["sha256"] == pins[key]
        assert image_check["magic"] == "XEX2" and image_check["image_sha256"] == pins["xex"]
        assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0

        rows = {row["source"]: row for row in invocation["objects"]}
        entries = PRIOR["HELPER"]["map_symbols"](files["map"].read_bytes())
        image = IMAGE(files["pe"], files["map"])
        root = Path(seal["source_root"])
        sources = {Path(row["path"]).relative_to(root).as_posix().lower(): row for row in seal["sources"]
                   if Path(row["path"]).is_relative_to(root)}
        linked = image, entries, sources
        final = {name: inspect(name, spec, rows, args.link_head, linked) for name, spec in CHECKS.items()}
        for name, before in checked.items():
            assert final[name]["coff_masked_body_sha256"] == before["coff_masked_body_sha256"], name
            assert final[name]["git_blob"] == before["git_blob"], name
        receipt["linked"] = {"head": args.link_head, "tree": tree(args.link_head), "pins": pins}
        receipt["functions"] = final
        receipt["vtable"] = verify_vtable(rows, donor, linked, final)
    receipt["scope"] = "Static GameData LiveUpdate dispatch, buffer lifetime, and request bridge. Runtime LiveUpdate remains unverified."
    document = json.dumps(receipt, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == document
    else:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(receipt["functions"]), "functions", len(receipt["vtable"]), "vtable slots", sha(args.output))


if __name__ == "__main__":
    main()


