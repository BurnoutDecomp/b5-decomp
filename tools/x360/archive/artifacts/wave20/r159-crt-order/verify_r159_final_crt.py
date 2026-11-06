"""Bind the frozen R159 CRT allocator order to a pinned final PE/XEX/map.

Read-only verifier: it consumes the final strict-link artifacts and the existing
R159 candidate COFF receipt. --write only adds R159_FINAL_LINKED.json and
FINAL_LINK.md here. It never builds, links, launches, or changes source.
"""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
R155_VERIFIER = WAVE / "r155-audio-regression/verify_linked.py"
PREFLIGHT = HERE / "R159_CRT_ORDER_EVIDENCE.json"
SOURCE = "src/GameSource/Sound/Module/BrnRootSoundModule.cpp"
ROOT_LINK_PINS = WAVE / "r159-link/ROOT_LINK_PINS.json"
ROOT_LINK_PINS_SHA = "0079e9f5142a38ce932b2e0197bf3ce1c2fb1eb01745b98648342ddb48154b8e"
DONOR_COMMIT = "655e9d73490a49d4c4487e3852d869035d2cc5c1"
DONOR_TREE = "dc10bfea0cabd8380bce54d410beefb23305799e"
ALLOCATORS = {
    "Rwac": {
        "global": "?gRwacTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgRwacTestBedAlloc@@YAXXZ",
        "callback": "??__FgRwacTestBedAlloc@@YAXXZ",
        "name": "RWAC",
    },
    "Logic": {
        "global": "?gLogicTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgLogicTestBedAlloc@@YAXXZ",
        "callback": "??__FgLogicTestBedAlloc@@YAXXZ",
        "name": "Logic",
    },
    "Playback": {
        "global": "?gPlaybackTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgPlaybackTestBedAlloc@@YAXXZ",
        "callback": "??__FgPlaybackTestBedAlloc@@YAXXZ",
        "name": "Playback",
    },
    "Csis": {
        "global": "?gCsisTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgCsisTestBedAlloc@@YAXXZ",
        "callback": "??__FgCsisTestBedAlloc@@YAXXZ",
        "name": "Csis",
    },
}
DEFAULT_SEND = {
    "initializer": "??__EK_DefaultSendName@Playback@CgsSound@@YAXXZ",
    "global": "?K_DefaultSendName@Playback@CgsSound@@3UName@12@B",
    "hash": "?MakeHash@Name@Playback@CgsSound@@SAIPBD@Z",
    "literal": "Send01",
}
CTOR = "??0Allocator@TestBed@CgsSound@@QAA@PBDPAUIResourceAllocator@rw@@@Z"
ATEXIT = "atexit"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_pin(path):
    path = Path(path)
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data), "sha256": sha(data)}


def make_helpers():
    r155 = runpy.run_path(str(R155_VERIFIER))
    r154 = runpy.run_path(str(r155["R154"]))
    r153 = runpy.run_path(str(r154["R153"]))
    r152 = runpy.run_path(str(r153["PRIOR_VERIFIER"]))
    inherited, voice, front, h, r151 = r152["helpers"]()
    return front, h


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def lis_addi(code, lis_offset, addi_offset):
    first, second = u32(code, lis_offset), u32(code, addi_offset)
    assert first >> 26 == 15 and second >> 26 == 14
    assert (first >> 21) & 31 == (second >> 16) & 31
    low = second & 0xFFFF
    if low & 0x8000:
        low -= 0x10000
    return (((first & 0xFFFF) << 16) + low) & 0xFFFFFFFF


def lis_stw(code, lis_offset, stw_offset):
    first, second = u32(code, lis_offset), u32(code, stw_offset)
    assert first >> 26 == 15 and second >> 26 == 36
    assert (first >> 21) & 31 == (second >> 16) & 31
    low = second & 0xFFFF
    if low & 0x8000:
        low -= 0x10000
    return (((first & 0xFFFF) << 16) + low) & 0xFFFFFFFF


def branch_target(pc, instruction):
    assert instruction >> 26 == 18 and instruction & 1
    displacement = instruction & 0x03FFFFFC
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return (pc + displacement) & 0xFFFFFFFF


def resolve(h, entries, name):
    return h.map_addr(entries, name)


def coff_section(coff, wanted):
    max_section = max(symbol.section for symbol in coff.symbols.values() if symbol.section > 0)
    found = []
    for index in range(1, max_section + 1):
        try:
            name, data, start, count = coff.section(index)
        except Exception:
            continue
        if name == wanted:
            found.append((index, data, start, count))
    assert len(found) == 1, (wanted, len(found))
    return found[0]


def section_relocations(coff, index):
    name, data, start, count = coff.section(index)
    rows = []
    for offset, symbol_index, kind in coff.section_relocations(start, count):
        if kind != 0x12:
            rows.append((offset, kind, coff.symbols[symbol_index]))
    return name, data, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--link-dir", type=Path, default=WAVE / "r159-link")
    parser.add_argument("--head", required=True)
    parser.add_argument("--tree", required=True)
    parser.add_argument("--seal-sha", required=True)
    parser.add_argument("--xex-sha", required=True)
    parser.add_argument("--pe-sha", required=True)
    parser.add_argument("--map-sha", required=True)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    front, h = make_helpers()
    link = args.link_dir
    root_pin_receipt = front["load"](ROOT_LINK_PINS, ROOT_LINK_PINS_SHA)
    assert (root_pin_receipt["head"], root_pin_receipt["tree"]) == (args.head, args.tree)
    root_pins = root_pin_receipt["pins"]
    seal_path = link / "seal.json"
    seal = front["load"](seal_path, args.seal_sha)
    assert seal["schema"] == "r159-native-link-seal-v1"
    assert seal["head"] == args.head
    assert root_pins["seal"] == {"path": str(seal_path), "sha256": args.seal_sha}
    root = Path(seal["source_root"])
    root_tree, parent = h.git(
        root, "rev-parse", f"{args.head}^{{tree}}", f"{args.head}^"
    ).decode().split()
    assert root_tree == args.tree

    result_path = link / "result.json"
    image_path = link / "image-verification.json"
    invocation_path = link / "invocation.json"
    result = front["load"](result_path)
    image = front["load"](image_path)
    invocation = front["load"](invocation_path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert image["head"] == args.head
    assert invocation["sealed_sha256"] == args.seal_sha
    for key, path in (("result", result_path), ("image", image_path),
                      ("invocation", invocation_path),
                      ("pe", link / "native-game.pe"), ("map", link / "native-game.map"),
                      ("xex", link / "native-game.exe")):
        assert Path(root_pins[key]["path"]) == path
        assert file_pin(path)["sha256"] == root_pins[key]["sha256"]
    completion_pin = root_pins["completion"]
    completion_path = Path(completion_pin["path"])
    completion = front["load"](completion_path, completion_pin["sha256"])
    assert completion["head"] == args.head
    assert completion["status"] == "SEALED_NO_REGRESSIONS"
    assert (completion["requested"], completion["passed"], completion["failed"]) == (3710, 3710, 0)
    release_path = link / "ROOT_RELEASE.json"
    if "release" in invocation:
        assert Path(invocation["release"]) == release_path
        release = front["load"](release_path, invocation["release_sha256"])
        assert release["sealed_sha256"] == args.seal_sha

    diagnostics_path = link / "diagnostic-parsed.json"
    diagnostics = front["load"](diagnostics_path)
    assert Path(root_pins["diagnostics"]["path"]) == diagnostics_path
    assert file_pin(diagnostics_path)["sha256"] == root_pins["diagnostics"]["sha256"]
    assert diagnostics["unresolved_messages"] == 0
    assert diagnostics["unique_symbols"] == 0 and not diagnostics["duplicate_messages"]

    h.LINK = link
    read, entries, image_pins = h.load_image(result, image)
    assert (image_pins["xex_sha256"], image_pins["pe_sha256"], image_pins["map_sha256"]) == (
        args.xex_sha, args.pe_sha, args.map_sha
    )

    # Tie the final composition back to the committed four-declaration donor.
    donor = json.loads(PREFLIGHT.read_text(encoding="utf-8"))["candidate"]
    assert donor["commit"] == DONOR_COMMIT and donor["tree"] == DONOR_TREE
    final_source_blob = h.git(root, "rev-parse", f"{args.head}:{SOURCE}").decode().strip()
    assert final_source_blob == donor["committed_source_blob"]

    final_object = next(row for row in seal["selected"] if row["source"] == SOURCE)
    final_coff = h.Coff(front["pinned"](final_object["object"], final_object["sha256"]))
    crt_section_index, crt_section_data, crt_section_start, crt_section_count = \
        coff_section(final_coff, b".CRT$XCU")
    crt_relocs = [
        {"offset": offset, "symbol": symbol.name, "kind": kind}
        for offset, kind, symbol in section_relocations(final_coff, crt_section_index)[2]
        if kind == 2
    ]
    crt_relocs.sort(key=lambda row: row["offset"])
    expected_symbols = [
        "??__ENULLResourcePtr@CgsResource@@YAXXZ",
        DEFAULT_SEND["initializer"],
        ALLOCATORS["Rwac"]["initializer"],
        ALLOCATORS["Logic"]["initializer"],
        ALLOCATORS["Playback"]["initializer"],
        ALLOCATORS["Csis"]["initializer"],
    ]
    assert [row["symbol"] for row in crt_relocs] == expected_symbols
    assert len(crt_section_data) == 24
    final_bss_offsets = {}
    for label, info in ALLOCATORS.items():
        symbol = next(sym for sym in final_coff.symbols.values() if sym.name == info["global"])
        assert final_coff.section(symbol.section)[0] == b".bss"
        final_bss_offsets[label] = symbol.value

    # Resolve each CRT pointer to its final PE body address.
    crt_start = resolve(h, entries, "__xc_a")
    crt_end = resolve(h, entries, "__xc_z")
    assert crt_end > crt_start and (crt_end - crt_start) % 4 == 0
    crt_words = struct.unpack(">" + "I" * ((crt_end - crt_start) // 4),
                              read(crt_start, crt_end - crt_start))
    symbols = {
        "Send01": DEFAULT_SEND["initializer"],
        **{label: info["initializer"] for label, info in ALLOCATORS.items()},
    }
    targets = {label: resolve(h, entries, symbol) for label, symbol in symbols.items()}
    indices = {}
    for label, target in targets.items():
        matches = [i for i, value in enumerate(crt_words) if value == target]
        assert len(matches) == 1, (label, matches)
        indices[label] = matches[0]
    final_order = [label for label, _ in sorted(indices.items(), key=lambda item: item[1])]
    assert final_order == ["Send01", "Rwac", "Logic", "Playback", "Csis"]

    # Verify the final PE's arguments and callback handoff at the linked call sites.
    name_make_hash = resolve(h, entries, DEFAULT_SEND["hash"])
    send_addr = targets["Send01"]
    send_code = read(send_addr, 48, executable=True)
    send_string_addr = lis_addi(send_code, 0x0C, 0x10)
    send_global_addr = lis_stw(send_code, 0x18, 0x1C)
    assert read(send_string_addr, len(DEFAULT_SEND["literal"]) + 1) == b"Send01\0"
    assert branch_target(send_addr + 0x14, u32(send_code, 0x14)) == name_make_hash
    assert send_global_addr == resolve(h, entries, DEFAULT_SEND["global"])

    ctor_addr = resolve(h, entries, CTOR)
    atexit_addr = resolve(h, entries, ATEXIT)
    constructor_calls = {}
    storage = {}
    for label, info in ALLOCATORS.items():
        fn_addr = targets[label]
        code = read(fn_addr, 64, executable=True)
        assert u32(code, 0x10) == 0x38A00000  # li r5, 0 (null backing allocator)
        name_addr = lis_addi(code, 0x0C, 0x14)
        object_addr = lis_addi(code, 0x18, 0x1C)
        callback_addr = lis_addi(code, 0x24, 0x28)
        ctor_call = branch_target(fn_addr + 0x20, u32(code, 0x20))
        exit_call = branch_target(fn_addr + 0x2C, u32(code, 0x2C))
        assert read(name_addr, len(info["name"]) + 1) == info["name"].encode() + b"\0"
        assert object_addr == resolve(h, entries, info["global"])
        assert callback_addr == resolve(h, entries, info["callback"])
        assert ctor_call == ctor_addr and exit_call == atexit_addr
        constructor_calls[label] = {
            "initializer": info["initializer"],
            "initializer_address": hex(fn_addr),
            "name": info["name"],
            "name_address": hex(name_addr),
            "this_global": hex(object_addr),
            "backing_allocator_r5": 0,
            "constructor_target": hex(ctor_call),
            "atexit_callback_argument_r3": hex(callback_addr),
            "atexit_callback_symbol": info["callback"],
            "atexit_target": hex(exit_call),
            "constructor_window_sha256": sha(code[0x0C:0x30]),
        }
        storage[label] = {
            "symbol": info["global"],
            "absolute_address": hex(object_addr),
            "object_bss_relative_offset": hex(final_bss_offsets[label]),
        }

    address_order = [
        label for label, _ in sorted(
            storage.items(), key=lambda item: int(item[1]["absolute_address"], 16)
        )
    ]
    receipt = {
        "schema": "r159-final-linked-crt-check-v1",
        "state": "pass",
        "link": {
            "head": args.head,
            "tree": args.tree,
            "parent": parent,
            "root_link_pins": file_pin(ROOT_LINK_PINS),
            "seal": file_pin(seal_path),
            "image": image_pins,
            "intake_completion": {
                "pin": completion_pin,
                "status": completion["status"],
                "requested": completion["requested"],
                "passed": completion["passed"],
                "failed": completion["failed"],
            },
            "result": file_pin(result_path),
            "image_verification": file_pin(image_path),
            "invocation": file_pin(invocation_path),
            "diagnostics": file_pin(diagnostics_path),
            "selected_object": {
                **final_object,
                "actual_sha256": sha(front["pinned"](final_object["object"], final_object["sha256"])),
            },
            "boundary": seal.get("boundary"),
            "source_inventory": seal.get("source_inventory"),
        },
        "donor": {
            "commit": DONOR_COMMIT,
            "tree": DONOR_TREE,
            "final_source_blob": final_source_blob,
            "candidate_preflight": file_pin(PREFLIGHT),
        },
        "crt": {
            "section_start": hex(crt_start),
            "section_end_exclusive": hex(crt_end),
            "entry_count": len(crt_words),
            "allocator_order": final_order[1:],
            "ordered_initializers": final_order,
            "initializer_slots": {
                label: {
                    "index": indices[label],
                    "entry_address": hex(crt_start + 4 * indices[label]),
                    "target": hex(targets[label]),
                    "symbol": symbols[label],
                }
                for label in final_order
            },
            "selected_object_section_index": crt_section_index,
            "selected_object_section_size": len(crt_section_data),
            "selected_object_relocations": crt_relocs,
        },
        "send01_initializer": {
            "symbol": DEFAULT_SEND["initializer"],
            "address": hex(send_addr),
            "string": DEFAULT_SEND["literal"],
            "string_address": hex(send_string_addr),
            "make_hash_target": hex(name_make_hash),
            "storage_symbol": DEFAULT_SEND["global"],
            "storage_address": hex(send_global_addr),
        },
        "allocator_constructor_arguments_and_callbacks": constructor_calls,
        "final_storage_layout": {
            "addresses": storage,
            "ascending_address_order": address_order,
            "bss_relative_offsets": {
                label: hex(offset) for label, offset in final_bss_offsets.items()
            },
            "note": (
                "These are final link map addresses, reported separately from CRT execution order. "
                "Object-relative offsets come from the final selected COFF."
            ),
        },
        "limits": [
            "PE/map evidence covers linked initializer pointers and the constructor/atexit call windows only; no raw full-body equivalence claim is made.",
            "No runtime startup or gameplay behavior is established by this static link receipt.",
        ],
    }
    receipt["verifier"] = file_pin(Path(__file__))

    if args.write:
        out = HERE / "R159_FINAL_LINKED.json"
        markdown = f"""# R159 final linked CRT check

The pinned final R159 image orders the initializers Send01, Rwac, Logic, Playback, Csis. Each allocator initializer passes its exact name, its final global object address, and a null backing allocator to the mapped Allocator constructor, then registers its matching destructor wrapper with atexit.

Final global addresses are listed separately from CRT order. This receipt inspects the final PE, map, XEX, selected COFF, and pinned seal. It makes no full-body or runtime claim.
"""
        out.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        (HERE / "FINAL_LINK.md").write_text(markdown, encoding="utf-8")
        print(json.dumps({
            "receipt": file_pin(out),
            "crt_order": final_order,
            "storage_order": address_order,
            "allocator_callbacks": len(constructor_calls),
        }, indent=2))

    print(
        f"PASS {args.head}: final CRT order {final_order}; "
        f"{len(constructor_calls)} allocator ctor and callback windows"
    )


if __name__ == "__main__":
    main()