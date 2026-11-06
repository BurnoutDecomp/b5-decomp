"""Read-only R159 preflight for the R157 CRT audio allocator initialization order.

Reads the exact R157 sealed link/source and the ARTIST IDA image raw. It never
compiles, links, launches, changes source, or modifies those inputs. --write
creates evidence files only in this directory.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import runpy
import struct

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
LINK = WAVE / "r157-link"
INTAKE = WAVE / "r157-intake"
MANIFEST = WAVE / "r157-batch/MANIFEST.json"
SOURCE_ROOT_EXPECTED = Path(r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake")
SOURCE = "src/GameSource/Sound/Module/BrnRootSoundModule.cpp"
HEAD = "e08c99a8a0902da7998408f8e324843ec86a3fae"
TREE = "41e8700b3dbef3308250b2da6c0be090e9563b0a"
BASE = "8157850d32d5d15c6187b22969e939f5ca7c201d"
BASE_TREE = "f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a"
SEAL_SHA = "c9136354e375d71c6f3b699d8f38eb380a47e6aae3be9d854ce7eae88474262a"
XEX_SHA = "e3e08607afc9128495061a5956c9985353b96e33bcdf103abe8b39b1bffe5684"
PE_SHA = "b4a46715e3efe619a41d1c3a3eca722d4b83f6b09dd96319ea6de54725494c2c"
MAP_SHA = "745fcf3b95284b7095fb5f0f6a54794417261d773a77b87b8e6372cb51cfb853"

R155_VERIFIER = WAVE / "r155-audio-regression/verify_linked.py"
X360RD = Path(r"D:\Reverse\BP-Decomp_Workflow\tools\re\x360rd.py")
ARTIST_EXPORT = Path(r"D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX")
ARTIST_RAW = Path(r"D:\Reverse\IDA_Files\artist_i64.raw")

ALLOCATORS = {
    "Rwac": {
        "variable": "gRwacTestBedAlloc",
        "symbol": "?gRwacTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgRwacTestBedAlloc@@YAXXZ",
        "function": 0x82C61A48,
        "artist_global": 0x82FFBF78,
    },
    "Logic": {
        "variable": "gLogicTestBedAlloc",
        "symbol": "?gLogicTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgLogicTestBedAlloc@@YAXXZ",
        "function": 0x82C61A88,
        "artist_global": 0x830060D0,
    },
    "Playback": {
        "variable": "gPlaybackTestBedAlloc",
        "symbol": "?gPlaybackTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgPlaybackTestBedAlloc@@YAXXZ",
        "function": 0x82C61AC8,
        "artist_global": 0x8300A6F8,
    },
    "Csis": {
        "variable": "gCsisTestBedAlloc",
        "symbol": "?gCsisTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
        "initializer": "??__EgCsisTestBedAlloc@@YAXXZ",
        "function": 0x82C61B08,
        "artist_global": 0x830086F0,
    },
}
DEFAULT_SEND = {
    "initializer": "??__EK_DefaultSendName@Playback@CgsSound@@YAXXZ",
    "function": 0x82C61A18,
}


def file_sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def pin(path):
    path = Path(path)
    return {"path": str(path), "sha256": file_sha(path)}


def make_helpers():
    r155 = runpy.run_path(str(R155_VERIFIER))
    r154 = runpy.run_path(str(r155["R154"]))
    r153 = runpy.run_path(str(r154["R153"]))
    r152 = runpy.run_path(str(r153["PRIOR_VERIFIER"]))
    inherited, voice, front, h, r151 = r152["helpers"]()
    return front, h


def u32_be(data, offset=0):
    return struct.unpack_from(">I", data, offset)[0]


def branch_target(pc, instruction):
    assert instruction >> 26 == 18 and instruction & 1
    displacement = instruction & 0x03FFFFFC
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return (pc + displacement) & 0xFFFFFFFF


def lis_addi_address(code, lis_offset, addi_offset):
    lis = u32_be(code, lis_offset)
    addi = u32_be(code, addi_offset)
    assert lis >> 26 == 15 and addi >> 26 == 14
    assert (lis >> 21) & 31 == (addi >> 16) & 31
    high = (lis & 0xFFFF) << 16
    low = addi & 0xFFFF
    if low & 0x8000:
        low -= 0x10000
    return (high + low) & 0xFFFFFFFF


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    front, h = make_helpers()
    seal_path = LINK / "seal.json"
    seal = front["load"](seal_path, SEAL_SHA)
    assert (seal["schema"], seal["head"]) == ("r157-native-link-seal-v1", HEAD)
    source_root = Path(seal["source_root"])
    assert source_root == SOURCE_ROOT_EXPECTED
    revisions = h.git(
        source_root, "rev-parse", HEAD, f"{HEAD}^{{tree}}", f"{HEAD}^"
    ).decode().split()
    assert revisions == [HEAD, TREE, BASE]

    manifest = front["load"](MANIFEST)
    assert (manifest["base"], manifest["base_tree"], manifest["projected_tree"]) == (
        BASE, BASE_TREE, TREE
    )
    commit_pin = next(
        row for row in seal["provenance"]
        if Path(row["path"]) == INTAKE / "commit-binding.json"
    )
    commit = front["from_pin"](commit_pin)
    assert (commit["head"], commit["base"]) == (HEAD, BASE)
    assert commit["manifest_sha256"] == file_sha(MANIFEST)

    result = front["load"](LINK / "result.json")
    image = front["load"](LINK / "image-verification.json")
    diagnostics = front["load"](LINK / "diagnostic-parsed.json")
    completion = front["load"](INTAKE / "completion.json")
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert image["head"] == HEAD
    assert diagnostics["unresolved_messages"] == 0
    assert diagnostics["unique_symbols"] == 0 and not diagnostics["duplicate_messages"]
    assert completion["status"] == "SEALED_NO_REGRESSIONS"
    assert (completion["requested"], completion["passed"], completion["failed"]) == (3710, 3710, 0)

    h.LINK = LINK
    read, entries, image_pins = h.load_image(result, image)
    assert (image_pins["xex_sha256"], image_pins["pe_sha256"], image_pins["map_sha256"]) == (
        XEX_SHA, PE_SHA, MAP_SHA
    )
    published = front["load"](LINK / "PUBLISHED_MATCH.json")
    assert published["xex_sha256"] == XEX_SHA

    source_path = source_root / SOURCE
    source_pin = next(
        row for row in seal["sources"]
        if h.norm(row["path"]) == h.norm(source_path)
    )
    source_bytes = front["pinned"](source_path, source_pin["sha256"])
    git_source = h.git(source_root, "show", f"{HEAD}:{SOURCE}")
    assert source_bytes.replace(b"\r\n", b"\n") == git_source.replace(b"\r\n", b"\n")
    source_text = git_source.decode(errors="replace")
    source_lines = source_text.splitlines()
    declaration_re = re.compile(
        r'^\s*CgsSound::TestBed::Allocator\s+g(\w+)\("([^"]+)",\s*0\);'
    )
    declarations = []
    accepted = {
        "RwacTestBedAlloc", "CsisTestBedAlloc",
        "PlaybackTestBedAlloc", "LogicTestBedAlloc",
    }
    for line_number, line in enumerate(source_lines, 1):
        match = declaration_re.match(line)
        if match and match.group(1) in accepted:
            declarations.append({
                "variable": "g" + match.group(1),
                "literal": match.group(2),
                "line": line_number,
            })
    current_source_order = [next(label for label, info in ALLOCATORS.items() if info["variable"] == row["variable"]) for row in declarations]
    assert current_source_order == ["Rwac", "Csis", "Playback", "Logic"]
    literal_by_owner = {
        next(label for label, info in ALLOCATORS.items() if info["variable"] == row["variable"]): row["literal"] for row in declarations
    }

    object_row = next(row for row in seal["selected"] if row["source"] == SOURCE)
    coff = h.Coff(front["pinned"](object_row["object"], object_row["sha256"]))
    max_section = max(sym.section for sym in coff.symbols.values() if sym.section > 0)
    sections = []
    for index in range(1, max_section + 1):
        try:
            name, data, start, count = coff.section(index)
        except Exception:
            continue
        if name == b".CRT$XCU":
            sections.append((index, name, data, start, count))
    assert len(sections) == 1
    crt_section_index, _, crt_data, crt_start, crt_count = sections[0]
    crt_relocations = []
    for offset, symbol_index, kind in coff.section_relocations(crt_start, crt_count):
        if kind == 2:
            crt_relocations.append({
                "offset": offset,
                "symbol": coff.symbols[symbol_index].name,
            })
    crt_relocations.sort(key=lambda row: row["offset"])
    crt_object_order = [row["symbol"] for row in crt_relocations]
    expected_current_coff = [
        "??__ENULLResourcePtr@CgsResource@@YAXXZ",
        DEFAULT_SEND["initializer"],
        ALLOCATORS["Rwac"]["initializer"],
        ALLOCATORS["Csis"]["initializer"],
        ALLOCATORS["Playback"]["initializer"],
        ALLOCATORS["Logic"]["initializer"],
    ]
    assert crt_object_order == expected_current_coff

    bss_offsets = {}
    for label, info in ALLOCATORS.items():
        symbol = next(sym for sym in coff.symbols.values()
                      if sym.name == info["symbol"])
        assert coff.section(symbol.section)[0] == b".bss"
        bss_offsets[label] = symbol.value
    current_global_addresses = {
        label: h.map_addr(entries, info["symbol"])
        for label, info in ALLOCATORS.items()
    }
    bss_bases = {
        current_global_addresses[label] - bss_offsets[label]
        for label in ALLOCATORS
    }
    assert len(bss_bases) == 1

    current_init_evidence = {}
    for label, info in ALLOCATORS.items():
        symbol = next(sym for sym in coff.symbols.values() if sym.name == info["initializer"])
        section_name, section_data, section_start, relocation_count = coff.section(symbol.section)
        assert section_name == b".text$yc" and symbol.value == 0
        code = section_data[symbol.value:symbol.value + 64]
        refs = {}
        for offset, symbol_index, kind in coff.section_relocations(section_start, relocation_count):
            if kind != 0x12:
                refs[offset] = (kind, coff.symbols[symbol_index])
        literal_kind, literal_symbol = refs[0x0C]
        global_kind, global_symbol = refs[0x18]
        assert literal_kind == 0x10 and global_kind == 0x10
        assert global_symbol.name == info["symbol"]
        current_name = coff.literal(literal_symbol, len(literal_by_owner[label]) + 1)
        assert current_name == literal_by_owner[label].encode("ascii") + b"\0"
        current_init_evidence[label] = {
            "initializer": symbol.name,
            "constructor_name": current_name[:-1].decode("ascii"),
            "allocator_symbol": global_symbol.name,
            "function_bytes_sha256": hashlib.sha256(code).hexdigest(),
        }

    map_start = h.map_addr(entries, "__xc_a")
    map_end = h.map_addr(entries, "__xc_z")
    assert map_end > map_start and (map_end - map_start) % 4 == 0
    crt_bytes = read(map_start, map_end - map_start)
    crt_words = struct.unpack(">" + "I" * ((map_end - map_start) // 4), crt_bytes)
    linked_symbols = {
        "default_send": DEFAULT_SEND["initializer"],
        **{label.lower(): info["initializer"] for label, info in ALLOCATORS.items()},
    }
    linked_targets = {
        label: h.map_addr(entries, symbol)
        for label, symbol in linked_symbols.items()
    }
    linked_indices = {}
    for label, target in linked_targets.items():
        matches = [index for index, value in enumerate(crt_words) if value == target]
        assert len(matches) == 1, (label, matches)
        linked_indices[label] = matches[0]
    current_link_order = [
        label for label, _ in sorted(linked_indices.items(), key=lambda row: row[1])
    ]
    assert current_link_order == ["default_send", "rwac", "csis", "playback", "logic"]

    artist_export_pins = {}
    expected_export_evidence = {
        0x82C61A18: 'Name::MakeHash("Send01"',
        0x82C61A88: '"Logic"',
        0x82C61AC8: '"Playback"',
        0x82C61B08: '"Csis"',
    }
    for address, evidence in expected_export_evidence.items():
        path = ARTIST_EXPORT / f"0x{address:08X}.json"
        export = json.loads(path.read_text(encoding="utf-8"))
        assert evidence in export["pseudocode"], (hex(address), evidence)
        artist_export_pins[hex(address)] = pin(path)

    spec = importlib.util.spec_from_file_location("r159_x360rd", X360RD)
    assert spec and spec.loader
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    assert Path(reader.RAW).resolve() == ARTIST_RAW.resolve()
    raw_hash = file_sha(ARTIST_RAW)

    artist_fragment_address = 0x82CD1DCC
    artist_fragment = reader.rd(artist_fragment_address, 20)
    artist_words = struct.unpack(">IIIII", artist_fragment)
    expected_artist_words = (
        DEFAULT_SEND["function"],
        ALLOCATORS["Rwac"]["function"],
        ALLOCATORS["Logic"]["function"],
        ALLOCATORS["Playback"]["function"],
        ALLOCATORS["Csis"]["function"],
    )
    assert artist_words == expected_artist_words

    artist_initializers = {}
    for label, info in ALLOCATORS.items():
        code = reader.rd(info["function"], 64)
        string_address = lis_addi_address(code, 0x0C, 0x14)
        global_address = lis_addi_address(code, 0x18, 0x1C)
        constructor = branch_target(
            info["function"] + 0x20, u32_be(code, 0x20)
        )
        atexit = branch_target(
            info["function"] + 0x2C, u32_be(code, 0x2C)
        )
        name = reader.rd(string_address, 16).split(b"\0", 1)[0].decode("ascii")
        assert global_address == info["artist_global"]
        assert constructor == 0x826ADE08
        assert atexit == 0x82C09120
        artist_initializers[label] = {
            "function": hex(info["function"]),
            "allocator_global": hex(global_address),
            "name_address": hex(string_address),
            "name": name,
            "constructor": hex(constructor),
            "atexit": hex(atexit),
            "function_bytes_sha256": hashlib.sha256(code).hexdigest(),
        }
    assert {label: row["name"] for label, row in artist_initializers.items()} == {
        "Rwac": "RWAC", "Logic": "Logic", "Playback": "Playback", "Csis": "Csis"
    }

    artist_default_export = json.loads(
        (ARTIST_EXPORT / "0x82C61A18.json").read_text(encoding="utf-8")
    )
    assert "dword_8300A6E0" in artist_default_export["pseudocode"]

    source_order_candidate = ["Rwac", "Logic", "Playback", "Csis"]
    exit_order_current = ["logic", "playback", "csis", "rwac"]
    exit_order_artist = ["csis", "playback", "logic", "rwac"]

    receipt = {
        "schema": "r159-crt-init-order-evidence-v1",
        "state": "source_order_candidate_substantiated_layout_gate_pending",
        "scope": "Static R157 source/link versus ARTIST CRT initializer fragment; no runtime or candidate build.",
        "r157": {
            "head": HEAD,
            "tree": TREE,
            "parent": BASE,
            "source_root": str(source_root),
            "seal": pin(seal_path),
            "xex_sha256": image_pins["xex_sha256"],
            "pe_sha256": image_pins["pe_sha256"],
            "map_sha256": image_pins["map_sha256"],
            "published_match": pin(LINK / "PUBLISHED_MATCH.json"),
            "completion": pin(INTAKE / "completion.json"),
            "completion_status": completion["status"],
            "native_requested_passed_failed": [
                completion["requested"], completion["passed"], completion["failed"]
            ],
            "source": {
                "path": SOURCE,
                "sha256": source_pin["sha256"],
                "git_blob": h.git(
                    source_root, "rev-parse", f"{HEAD}:{SOURCE}"
                ).decode().strip(),
                "selected_object": object_row,
                "declaration_order": declarations,
            },
            "link_crt": {
                "start": hex(map_start),
                "end_exclusive": hex(map_end),
                "entry_count": len(crt_words),
                "order": current_link_order,
                "slots": [
                    {
                        "label": label,
                        "symbol": symbol,
                        "target": hex(linked_targets[label]),
                        "index": linked_indices[label],
                        "entry_address": hex(map_start + 4 * linked_indices[label]),
                    }
                    for label, symbol in linked_symbols.items()
                ],
            },
            "coff": {
                "crt_section_index": crt_section_index,
                "crt_section_size": len(crt_data),
                "crt_initializers": crt_relocations,
                "allocator_bss_relative_offsets": {
                    label: hex(value) for label, value in bss_offsets.items()
                },
                "allocator_bss_addresses": {
                    label: hex(value) for label, value in current_global_addresses.items()
                },
                "common_bss_section_base": hex(next(iter(bss_bases))),
                "bss_storage_order": [
                    label for label, _ in sorted(
                        bss_offsets.items(), key=lambda row: row[1]
                    )
                ],
                "constructor_names": current_init_evidence,
            },
        },
        "artist": {
            "raw_image": {
                "reader": pin(X360RD),
                "idb_raw": {
                    **pin(ARTIST_RAW),
                    "bytes": ARTIST_RAW.stat().st_size,
                },
                "loaded_image_fragment_address": hex(artist_fragment_address),
                "fragment_bytes": artist_fragment.hex(),
                "fragment_order": [
                    "DefaultSendName", "Rwac", "Logic", "Playback", "Csis"
                ],
                "function_addresses": [hex(value) for value in artist_words],
                "initializer_bodies": artist_initializers,
                "direct_export_pins": artist_export_pins,
            },
            "allocator_global_addresses": {
                label: hex(info["artist_global"])
                for label, info in ALLOCATORS.items()
            },
        },
        "comparison": {
            "default_send_precedes_allocator_group_in_both": True,
            "r157_allocator_initializer_order": [
                "Rwac", "Csis", "Playback", "Logic"
            ],
            "artist_allocator_initializer_order": source_order_candidate,
            "source_order_candidate": source_order_candidate,
            "current_global_storage_order_r157": [
                "Rwac", "Logic", "Csis", "Playback"
            ],
            "artist_global_storage_order": [
                "Rwac", "Logic", "Csis", "Playback"
            ],
            "current_rwac_literal": literal_by_owner["Rwac"],
            "artist_rwac_literal": artist_initializers["Rwac"]["name"],
            "rwac_name_case_delta": literal_by_owner["Rwac"] != artist_initializers["Rwac"]["name"],
            "names_and_types_unchanged_by_candidate": (
                "A pure declaration permutation keeps each external g* symbol, "
                "Allocator type, constructor string argument, and null backer attached "
                "to its current definition. The existing Rwac/RWAC case delta remains."
            ),
        },
        "startup_consequences": {
            "construction": (
                "These dynamic global constructors run from the CRT table before game startup. "
                "Each calls Allocator(name, null), initializing bookkeeping and a mutex; "
                "no allocator initializer calls another allocator."
            ),
            "default_send": (
                "The Send01 Name::MakeHash initializer stays before the allocator group, "
                "matching the ARTIST table fragment."
            ),
            "teardown": (
                "Each allocator initializer registers its destructor with atexit. "
                f"The observed registration order implies current reverse teardown {exit_order_current}; "
                f"the ARTIST order implies {exit_order_artist}. The destructor body is empty, "
                "though the member mutex destructor still runs."
            ),
            "rwac_diagnostic_name": (
                "ARTIST passes the name RWAC while R157 source passes Rwac; the stored "
                "allocator name affects later dump/assert labels. An order-only move leaves "
                "this existing diagnostic-name delta unchanged."
            ),
        },
        "abi_verdict": {
            "source_level": (
                "The initializer sequence can be targeted by moving only the four global "
                "definitions into ARTIST order. Global names, types, constructor strings, "
                "backing arguments, and call sites can stay attached to their current definitions."
            ),
            "exact_storage_layout": (
                "Unverified after a declaration move. R157 currently places the .bss objects "
                "in ARTIST relative order, but no candidate COFF was built; moving definitions "
                "could change section offsets or linked addresses."
            ),
            "status": "not certified ABI-preserving until the post-move object/link layout is checked",
        },
        "limits": [
            "The ARTIST evidence is a contiguous loaded-image initializer-pointer fragment; stripped __xc_a/__xc_z boundary labels were not available in the export.",
            "No source was changed and no candidate compile, link, launch, or gameplay run was performed.",
            "This proves the current order and a minimal source-level candidate, not candidate binary equivalence or runtime impact.",
        ],
    }
    receipt["verifier"] = pin(Path(__file__))

    if args.write:
        out = HERE / "R159_CRT_ORDER_PREFLIGHT.json"
        with out.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
        markdown = f"""# R159 CRT initializer order

Static comparison of the exact R157 link at {HEAD} (tree {TREE}) against ARTIST's loaded-image initializer-pointer fragment.

| Evidence | ARTIST | R157 |
| --- | --- | --- |
| Send01 initializer | Immediately before allocators | Immediately before allocators |
| Allocator construction order | Rwac → Logic → Playback → Csis | Rwac → Csis → Playback → Logic |
| Global storage order | Rwac → Logic → Csis → Playback | Rwac → Logic → Csis → Playback |

The current object has one .CRT$XCU relocation section whose allocator entries follow the source definitions. A minimal source-order candidate is Rwac, Logic, Playback, Csis; it keeps the current global names, allocator types, constructor arguments, and references. The current .bss placement matches ARTIST, but the effect of moving those definitions on section offsets is unverified without a candidate compile/link, so exact native storage ABI preservation is not certified.

The Rwac diagnostic label is a separate existing delta: ARTIST initializes it with RWAC; R157 source uses Rwac. An order-only move preserves the current source literal. Initialization also registers atexit callbacks, so reordering changes their reverse teardown sequence. The allocator destructor body is empty, though its mutex member still destructs.

No source, build, link, runtime, or UI changes were made. See R159_CRT_ORDER_PREFLIGHT.json for exact pointers, addresses, hashes, and COFF section evidence.
"""
        md = HERE / "PREFLIGHT.md"
        with md.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(markdown)
        print(pin(out))
        print(pin(md))

    print(
        "PASS: R157 Rwac/Csis/Playback/Logic -> ARTIST Rwac/Logic/Playback/Csis; "
        "the source-order candidate is substantiated, exact post-move layout is unverified"
    )


if __name__ == "__main__":
    main()