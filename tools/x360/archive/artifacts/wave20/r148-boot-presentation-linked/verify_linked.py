"""Bind the R148 boot and presentation preflight to the sealed PPC PE and XEX."""

import argparse
from collections import namedtuple
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import pefile


ROOT = Path(__file__).resolve().parent
LINK = ROOT.parent / "r148-link"
FROZEN = ROOT.parent / "r148-boot-presentation-proof/R148_BOOT_PRESENTATION_PPC.json"
FROZEN_SHA = "c555296c2291d829cc6199a882b4fe7a570585e514e9aed45043f297cc8992bf"
PRIVATE = "ebc86db664b0e6a4cd937bd5d89e7d26b9fa9976"
INTEGRATED = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
TREE = "292de86d70d0b034d502995355f6169a766a7d7e"
PARENT = "5509461903bd56c25d92208d84da7a37171a4fc4"
PRIOR_SEAL_SHA = "f3431bee73fb501ec8d9349f3f824e781123c15f5ae40b753910eb3ea7c165d5"
PROVIDER_SHA = "c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833"
GROUPS = {
    "ICEWidgetMenu": {
        "widget_construct": "?Construct@ICEWidgetMenu@", "widget_width": "?GetWidth@ICEWidgetMenu@",
        "widget_render": "?Render@ICEWidgetMenu@", "widget_draw_box": "?DrawBox@",
    },
    "ICEControllerMenus": {"menus": "?ConstructMenus@ICEController@"},
    "ICEController": {"controller": "?Construct@ICEController@"},
    "CgsLanguageManager": {"language_release": "?Release@LanguageManager@", "language_unload": "?UnloadStringTable@LanguageManager@"},
    "CgsGuiViewModule": {"gui_release": "?Release@ViewModule@"},
    "BrnPresentationEffect": {
        "attach": "?Attach@PresentationEffect@", "play": "?Play@PresentationEffect@",
        "get_create_params": "?GetCreateParams@PresentationEffect@",
    },
}
LITERALS = {
    "kaMenuStyles": 224, "KF_ICE_MENU_MARGIN": 4, "KF_ICE_MENU_TEXT_SIZE": 4,
    "KF_ICE_MENU_PAIR_A_X": 4, "KF_ICE_MENU_Y_SPACING": 4,
    "??_C@_0P@FLFHBPI@MusicVoiceSpec?$AA@": 15,
    "??_C@_0BD@LBJMKPLH@StereoWavVoiceSpec?$AA@": 19,
}
Symbol = namedtuple("Symbol", "name value section kind")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def pinned(path, digest):
    data = Path(path).read_bytes()
    assert sha(data) == digest, path
    return data


def record(path):
    return {"path": str(path), "sha256": file_sha(path)}


def read_json(path):
    return json.loads(Path(path).read_bytes())


def pinned_json(row, digest=None):
    if digest:
        assert row["sha256"] == digest
    return json.loads(pinned(row["path"], row["sha256"]))


def norm(path):
    return str(path).replace("\\", "/").lower()


def stable(name):
    return re.sub(r"\?A0x[0-9a-f]{8}@", "?anonymous@", name)


def git(root, *args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(root), *args])


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def sign(value, bits):
    top = 1 << (bits - 1)
    return (value ^ top) - top


class Coff:
    def __init__(self, data):
        self.data = data
        machine, _, _, table, count, optional, _ = struct.unpack_from("<HHIIIHH", data)
        assert machine == 0x1F2 and not optional
        self.symbols = {}
        index = 0
        while index < count:
            entry = table + 18 * index
            name, value, section, kind, _, aux = struct.unpack_from("<8sIhHBB", data, entry)
            if name[:4] == bytes(4):
                start = table + 18 * count + struct.unpack_from("<I", name, 4)[0]
                name = data[start:data.index(b"\0", start)]
            self.symbols[index] = Symbol(name.rstrip(b"\0").decode("ascii", errors="replace"), value, section, kind)
            index += 1 + aux

    def definition(self, prefix):
        [row] = [row for row in self.symbols.values() if row.name.startswith(prefix) and row.section > 0]
        return row

    def section(self, number):
        header = 20 + 40 * (number - 1)
        size, start, relocations = struct.unpack_from("<III", self.data, header + 16)
        count = struct.unpack_from("<H", self.data, header + 32)[0]
        name = self.data[header:header + 8].rstrip(b"\0")
        return name, self.data[start:start + size], relocations, count

    def function(self, prefix):
        symbol = self.definition(prefix)
        name, code, relocations, count = self.section(symbol.section)
        assert symbol.value == 0 and symbol.kind == 0x20 and name == b".text"
        refs = {}
        for index in range(count):
            offset, target, kind = struct.unpack_from("<IIH", self.data, relocations + 10 * index)
            if kind == 0x12:
                assert target == 0 and refs[offset][0] in (0x10, 0x11)
            else:
                assert offset not in refs and offset + 4 <= len(code)
                refs[offset] = kind, self.symbols[target]
        return symbol, code, refs

    def section_relocations(self, start, count):
        return struct.iter_unpack("<IIH", self.data[start:start + 10 * count])

    def vtable_slot(self, name, offset):
        symbol = self.definition(name)
        section, data, relocations, count = self.section(symbol.section)
        assert symbol.name == name and symbol.value == symbol.kind == 0 and section == b".rdata"
        assert u32(data, offset) == 0
        [target] = [
            (kind, self.symbols[index].name)
            for at, index, kind in self.section_relocations(relocations, count)
            if at == offset
        ]
        assert target[0] == 2
        return target[1]

    def literal(self, symbol, size):
        name, data, start, count = self.section(symbol.section)
        assert name == b".rdata" and symbol.value + size <= len(data)
        assert all(at + 4 <= symbol.value or at >= symbol.value + size for at, _, _ in self.section_relocations(start, count))
        return data[symbol.value:symbol.value + size]


def map_symbols(data):
    rows = {}
    for line in data.decode(errors="replace").splitlines():
        match = re.match(r"\s+[\da-fA-F]{4}:[\da-fA-F]{8}\s+(\S+)\s+([\da-fA-F]{8})\s+(.+)", line)
        if match:
            rows.setdefault(match[1], set()).add((int(match[2], 16), match[3].split()[-1]))
    return rows


def map_addr(rows, name, owner=None):
    [address] = {addr for addr, module in rows[name] if owner is None or module == owner}
    return address


def ref_signature(symbol, refs):
    return {
        at: (kind, stable(target.name), target.value if symbol.section == target.section else None)
        for at, (kind, target) in refs.items()
    }


def local_data_destinations(read, coff, section, code, refs, entries, linked, section_bases):
    found = {}
    for at, (kind, symbol) in refs.items():
        if kind != 0x11 or symbol.name in entries or symbol.section <= 0 or symbol.section == section:
            continue
        name, section_data, start, count = coff.section(symbol.section)
        assert name == b".rdata", symbol
        register = u32(code, at) >> 16 & 31
        high = max(
            hi for hi, pair in refs.items()
            if hi < at and pair == (0x10, symbol) and u32(code, hi) >> 21 & 31 == register
        )
        high_word = u32(linked, high)
        assert high_word >> 26 == 15 and high_word >> 16 & 31 == 0
        address = (((high_word & 0xFFFF) << 16) + sign(u32(linked, at) & 0xFFFF, 16)) & 0xFFFFFFFF
        base = address - symbol.value
        assert section_bases.setdefault(symbol.section, base) == base
        expected = bytearray(section_data)
        for offset, target, relocation in coff.section_relocations(start, count):
            assert relocation == 2 and u32(section_data, offset) == 0
            struct.pack_into(">I", expected, offset, map_addr(entries, coff.symbols[target].name))
        assert read(base, len(expected)) == expected, symbol
        assert found.setdefault(symbol, address) == address
    return found


def verify_body(read, entries, coff, owner, selected, frozen, section_bases):
    symbol, code, refs = selected
    original, original_code, original_refs = frozen
    assert stable(symbol.name) == stable(original.name) and code == original_code, symbol.name
    assert ref_signature(symbol, refs) == ref_signature(original, original_refs), symbol.name
    address = map_addr(entries, symbol.name, owner)
    linked = read(address, len(code), executable=True)
    data_targets = local_data_destinations(read, coff, symbol.section, code, refs, entries, linked, section_bases)
    expected = bytearray(code)
    bindings = {}
    for at, (kind, target) in refs.items():
        location = address + at
        if symbol.section == target.section:
            assert target.name.startswith("$LN")
            dest = address + target.value
        elif target.name in entries:
            dest = map_addr(entries, target.name)
        else:
            dest = data_targets[target]
        word = u32(code, at)
        if kind == 0x06:
            assert word & 0xFC000002 == 0x48000000
            word = word & 0xFC000003 | ((dest - location) & 0x03FFFFFC)
            assert location + sign(word & 0x03FFFFFC, 26) == dest
        elif kind in (0x10, 0x11):
            assert word & 0xFFFF == 0
            word |= (((dest + 0x8000) >> 16) if kind == 0x10 else dest) & 0xFFFF
        else:
            assert kind == 0x02 and word == 0
            word = dest
        assert u32(linked, at) == word, (symbol.name, hex(at), target.name, hex(word), hex(u32(linked, at)))
        struct.pack_into(">I", expected, at, word)
        bindings[at] = {"symbol": target.name, "address": dest, "kind": kind}
        size = LITERALS.get(target.name, 4 if target.name.startswith("__real@") else None)
        if size is not None:
            assert target.section > 0 and coff.literal(target, size) == read(dest, size), target
    assert linked == expected, symbol.name
    return {"symbol": symbol.name, "address": address, "size": len(linked), "sha256": sha(linked), "bindings": bindings}


def load_seal(head, frozen):
    result = read_json(LINK / "result.json")
    image = read_json(LINK / "image-verification.json")
    invocation = read_json(LINK / "invocation.json")
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] is True
    seal_path = LINK / "seal.json"
    assert Path(invocation["sealed"]) == seal_path
    seal = json.loads(pinned(seal_path, invocation["sealed_sha256"]))
    assert seal["schema"] == "r148-native-link-seal-v1" and seal["head"] == image["head"] == head
    release = json.loads(pinned(invocation["release"], invocation["release_sha256"]))
    assert Path(invocation["release"]) == LINK / "ROOT_RELEASE.json"
    assert release == {"action": "one strict R148 native link", "sealed_sha256": invocation["sealed_sha256"]}
    assert invocation["objects"] == seal["selected"] and invocation["excluded"] == seal["excluded"]
    assert invocation["sources"] == seal["sources"] and invocation["libraries"] == seal["libraries"]
    selected = seal["selected"]
    assert len(selected) == 3589 and len(seal["excluded"]) == 121
    assert len({norm(row["source"]) for row in selected + seal["excluded"]}) == 3710
    selection = sorted((norm(row["source"]), norm(row["object"]), row["sha256"]) for row in selected)
    digest = sha(json.dumps(selection, separators=(",", ":")).encode())
    assert digest == seal["selected_objects_sha256"]
    rsp = LINK / "r148-strict.rsp"
    assert invocation["command"][1:] == ["@" + str(rsp)]
    args = pinned(rsp, invocation["rsp_sha256"]).decode().splitlines()
    assert [arg for arg in args if arg.lower().endswith('.obj"')] == [f'"{row["object"]}"' for row in selected]
    assert file_sha(LINK / "link.log") == result["log_sha256"]
    diagnostic = read_json(LINK / "diagnostic-parsed.json")
    assert diagnostic["unique_symbols"] == diagnostic["unresolved_messages"] == 0 and not diagnostic["duplicate_messages"]

    prior_seal = pinned_json(seal["prior_successful_seal"], PRIOR_SEAL_SHA)
    cohort = pinned_json(seal["provider_selection"], PROVIDER_SHA)["sources"]
    assert prior_seal["head"] == PARENT and len(cohort) == 15
    assert cohort == seal["provider_cohort"] == prior_seal["provider_cohort"]
    excluded = {norm(row["source"]): row for row in seal["excluded"]}
    assert set(excluded) == {norm(row["source"]) for row in prior_seal["excluded"]}
    for row in cohort:
        current = excluded[norm(row["source"])]
        assert norm(row["object"]) == norm(current["object"]) and row["sha256"] == current["sha256"]
        assert file_sha(current["object"]) == current["sha256"]
    boundary = pinned_json(seal["boundary"])
    assert boundary["head"] == head and boundary["schema"] == "r148-link-boundary-v1"
    assert boundary["selected_objects_sha256"] == digest and boundary["no_excluded_cohort_outside_references"]

    source_root = Path(seal["source_root"])
    for revision in (head, PRIVATE):
        assert git(source_root, "rev-parse", f"{revision}^{{tree}}", f"{revision}^").decode().split() == [TREE, PARENT]
    commit_path = ROOT.parent / "r148-intake/commit-binding.json"
    commit_pin = next(row for row in seal["provenance"] if Path(row["path"]) == commit_path)
    commit = pinned_json(commit_pin)
    assert commit["head"] == head and commit["base"] == PARENT
    manifest = json.loads(pinned(ROOT.parent / "r148-batch/MANIFEST.json", commit["manifest_sha256"]))
    assert manifest["base"] == PARENT and manifest["projected_tree"] == TREE
    diff = git(source_root, "diff", "--name-only", PARENT, head).decode().splitlines()
    assert sorted(diff) == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    inventory = pinned_json(seal["source_inventory"])
    assert inventory["head"] == head and inventory["files"] == seal["sources"]
    report = pinned_json(seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    proof_rows = frozen["sources_and_outputs"]
    source_hashes = {norm(row["path"]): row["sha256"] for row in seal["sources"]}
    report_rows = {row["source"]: row for row in report["results"]}
    selected_rows = {row["source"]: row for row in selected}
    objects = {}
    for source, proof in proof_rows.items():
        blob = git(source_root, "show", f"{head}:{source}")
        assert sha(blob) == proof["git_blob_sha256"], source
        checkout = (source_root / source).read_bytes()
        assert sha(checkout) == source_hashes[norm(source_root / source)], source
        assert checkout.replace(b"\r\n", b"\n") == blob.replace(b"\r\n", b"\n"), source
        row, artifact = selected_rows[source], report_rows[source]
        assert norm(row["object"]) == norm(artifact["object"]) and artifact["passed"] and artifact["machine"] == 0x1F2
        path = Path(row["object"])
        old_obj = proof["outputs"]["object"]
        assert path.name == Path(old_obj["path"]).name
        objects[Path(source).stem] = (path, pinned(path, row["sha256"]), pinned(old_obj["path"], old_obj["sha256"]), row, old_obj)
    pins = {
        "manifest_sha256": commit["manifest_sha256"], "seal_sha256": invocation["sealed_sha256"],
        "commit_binding_sha256": commit_pin["sha256"], "release_sha256": invocation["release_sha256"],
        "result_sha256": file_sha(LINK / "result.json"), "image_verification_sha256": file_sha(LINK / "image-verification.json"),
        "response_sha256": invocation["rsp_sha256"], "selected_objects_sha256": digest,
        "boundary_sha256": seal["boundary"]["sha256"], "provider_selection_sha256": PROVIDER_SHA,
        "prior_successful_seal_sha256": PRIOR_SEAL_SHA, "inherited_provider_cohort_count": len(cohort),
    }
    return result, image, objects, pins


def load_image(result, image):
    def verified(name):
        path = LINK / name
        row = result["outputs"][str(path)]
        data = pinned(path, row["sha256"])
        assert len(data) == row["size"]
        return data

    exe, pe, mapping = (verified(name) for name in ("native-game.exe", "native-game.pe", "native-game.map"))
    assert Path(image["image"]) == LINK / "native-game.exe"
    assert len(exe) == image["bytes"] and sha(exe) == image["image_sha256"]
    assert exe[:4] == b"XEX2" and image["magic"] == "XEX2"
    dump = pinned(LINK / "xex-dump.log", image["xex_dump_sha256"]).decode(errors="replace")
    assert all(marker in dump for marker in ("entry point", "xboxkrnl.exe", "XHV 2.0.6534.1"))

    parsed = pefile.PE(data=pe, fast_load=True)
    assert parsed.FILE_HEADER.Machine == 0x1F2
    base = parsed.OPTIONAL_HEADER.ImageBase
    entry = base + parsed.OPTIONAL_HEADER.AddressOfEntryPoint
    count = u32(exe, 20)
    assert 24 + count * 8 <= u32(exe, 8) < len(exe)
    xex_options = dict(struct.iter_unpack(">II", exe[24:24 + count * 8]))
    assert len(xex_options) == count and xex_options[0x10201] == base and xex_options[0x10100] == entry
    assert re.search(rf"\b{entry:08X} entry point", dump)
    assert re.search(rf"\b{base:08X} image base address", dump)

    def read(address, size, executable=False):
        rva = address - base
        [section] = [row for row in parsed.sections if row.VirtualAddress <= rva and
                     rva + size <= row.VirtualAddress + row.SizeOfRawData]
        assert bool(section.Characteristics & 0x20000000) == executable, hex(address)
        start = section.PointerToRawData + rva - section.VirtualAddress
        data = pe[start:start + size]
        assert len(data) == size
        return data

    pins = {
        "xex_sha256": sha(exe), "pe_sha256": sha(pe), "map_sha256": sha(mapping),
        "xex_dump_sha256": image["xex_dump_sha256"], "base": base, "entry": entry,
    }
    entries = map_symbols(mapping)
    assert map_addr(entries, "mainCRTStartup") == entry
    assert re.search(rf"Preferred load address is {base:08X}", mapping.decode(errors="replace"))
    return read, entries, pins


def verify_presentation_vtables(read, entries, path, selected, frozen, functions):
    current, prior = Coff(selected), Coff(frozen)
    tables = {
        "stream_user_get_create_params": ("??_7PresentationEffect@Logic@BrnSound@@6B@", 0, "get_create_params"),
        "effect_object_attach": ("??_7PresentationEffect@Logic@BrnSound@@6BEffectObject@1CgsSound@@@", 0x14, "attach"),
    }
    result = {}
    for label, (table, offset, method) in tables.items():
        target = current.vtable_slot(table, offset)
        assert target == prior.vtable_slot(table, offset) == functions[method]["symbol"]
        slot = map_addr(entries, table, path.name) + offset
        address = u32(read(slot, 4), 0)
        assert address == functions[method]["address"]
        result[label] = {"table": table, "offset": offset, "slot": slot, "target": address}
    return result


def verify_witnesses(functions, read, frozen):
    def target(name, offset, prefix):
        row = functions[name]["bindings"][offset]
        assert row["symbol"].startswith(prefix), row
        return row["address"]

    def code_words(name, checks):
        address = functions[name]["address"]
        for offset, expected in checks.items():
            assert u32(read(address + offset, 4, executable=True), 0) == expected, (name, hex(offset))

    style = target("widget_construct", 0x14, "kaMenuStyles")
    data = read(style, 224)
    assert sha(data) == frozen["ice"]["style_table"]["sha256"]
    assert target("widget_width", 0xC, "KF_ICE_MENU_MARGIN") + 0x38 == style
    assert target("widget_render", 0x60, "KF_ICE_MENU_TEXT_SIZE") + 0x30 == style
    assert target("controller", 0xC8, "?ConstructMenus@ICEController@") == functions["menus"]["address"]
    for offset in (0x40, 0x64, 0xBC, 0xE0, 0x104, 0x128, 0x14C):
        assert target("menus", offset, "?Construct@ICEWidgetMenu@") == functions["widget_construct"]["address"]
    y = target("menus", 0x34, "KF_ICE_MENU_PAIR_A_X") + 0x28
    assert read(y, 4) == struct.pack(">f", 25.0)
    code_words("menus", {int(off, 16): int(word, 16) for off, word in frozen["ice"]["startup_geometry_coff"].items()})
    assert target("language_release", 0xC, "?UnloadStringTable@LanguageManager@") == functions["language_unload"]["address"]
    assert target("gui_release", 0x9C, "?Release@LanguageManager@") == functions["language_release"]["address"]
    code_words("language_release", {0x10: 0x38600001, 0x20: 0x4E800020})
    code_words("gui_release", {0x98: 0x387E7C0C, 0xA0: 0x546B063E, 0xA4: 0x2B0B0000, 0xA8: 0x419A0060})
    stream = target("attach", 0xC4, "??_C@_0P@FLFHBPI@MusicVoiceSpec")
    wav = target("play", 0x128, "??_C@_0BD@LBJMKPLH@StereoWavVoiceSpec")
    assert read(stream, 15) == b"MusicVoiceSpec\0" and read(wav, 19) == b"StereoWavVoiceSpec\0"
    name_hash = target("attach", 0xD0, "?MakeHash@Name@Playback@")
    assert target("play", 0x134, "?MakeHash@Name@Playback@") == name_hash
    post = target("play", 0x94, "?PostStreamRequest@StreamingStateManager@")
    code_words("attach", {0xD8: 0x7C6A1B78, 0xE4: 0x915F0284})
    code_words("get_create_params", {0: 0x38630244, 4: 0x4E800020})
    code_words("play", {
        0x18: 0x2B0BFFFE, 0x1C: 0x409A0084, 0x6C: 0x38A00006,
        0x7C: 0x915D0290, 0x84: 0x997D02BC, 0x110: 0x2B0BFFFF,
        0x118: 0x409A003C, 0x13C: 0x9061007C,
    })
    return {
        "ice": {"linked_table": style, "bytes": len(data), "sha256": sha(data),
                "artist": frozen["ice"]["style_table"]["artist"], "copy_y_spacing": 25.0},
        "language": {"release": functions["language_release"]["address"], "unload": functions["language_unload"]["address"],
                     "gui_release": functions["gui_release"]["address"], "gui_return_check_offset": 0xA0},
        "presentation": {
            "stream_splice": 0xFFFE, "music_voice_spec": stream, "stream_spec_field": 0x284,
            "post_stream_request": post, "resident_wav_splice": 0xFFFF, "stereo_wav_voice_spec": wav,
            "artist_stream_literal": frozen["presentation"]["stream"]["artist_literal"],
            "artist_wav_literal": frozen["presentation"]["resident_wave"]["artist_literal"],
            "scope": frozen["presentation"]["comparison"],
            "request_abi_difference": frozen["presentation"]["stream"]["request_abi_observation"],
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    assert __debug__ and args.expected_head == INTEGRATED
    frozen = json.loads(pinned(FROZEN, FROZEN_SHA))
    assert (frozen["status"], frozen["head"], frozen["tree"], frozen["parent"]) == ("pass", PRIVATE, TREE, PARENT)
    result, image, objects, seal_pins = load_seal(args.expected_head, frozen)
    read, entries, image_pins = load_image(result, image)
    assert set(objects) == set(GROUPS)
    functions, providers = {}, {}
    for stem, methods in GROUPS.items():
        path, selected, prior, row, old_obj = objects[stem]
        coff, preflight = Coff(selected), Coff(prior)
        section_bases = {}
        for key, prefix in methods.items():
            functions[key] = verify_body(
                read, entries, coff, path.name, coff.function(prefix), preflight.function(prefix), section_bases,
            )
        providers[row["source"]] = {"selected": row, "preflight": old_obj}
    path, selected, prior, _, _ = objects["BrnPresentationEffect"]
    vtables = verify_presentation_vtables(read, entries, path, selected, prior, functions)
    receipt = {
        "schema": "r148-boot-presentation-linked-v1", "status": "pass",
        "head": args.expected_head, "tree": TREE, "parent": PARENT,
        "preflight_head": PRIVATE, "preflight_proof_sha256": FROZEN_SHA,
        "seal": seal_pins, "image": image_pins, "providers": providers,
        "functions": functions, "presentation_vtables": vtables, "witnesses": verify_witnesses(functions, read, frozen),
        "scope": "Sealed XEX provenance, selected COFF versus linked PPC PE, and frozen raw ARTIST witnesses.",
    }
    if args.verify_only:
        print("R148 linked boot and presentation checks passed")
        return
    output = ROOT / "R148_BOOT_PRESENTATION_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, file_sha(output))


if __name__ == "__main__":
    main()
