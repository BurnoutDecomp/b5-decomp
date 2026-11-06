"""Verify the retained AEMS provider in a completed strict link. Requires pefile."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import pefile

from verify_profile_flow_regression import (
    LINK_BASE as BASE, LINK_TREE as TREE, R154_LINK, TARGET_MANIFEST_SHA, WAVE, target_pins,
)

BASE_RECEIPT_SHA256 = "f804e2f2103cf36ca52e92766031dadbb97397860bf0078950a3ac48b1d8dd7f"
PARSER = WAVE / "r137-intake/verify_netconn.py"
source = PARSER.read_text()
exec(source[source.index("def coff("):source.index("worker=")])

PLAYBACK = "CgsSoundPlaybackModule-46f08158.obj"
FACTORY = "CgsAemsFactory-223642e7.obj"
INTERFACE = "CgsAemsInterfaceImplementation-c3edfff6.obj"
ASSIGN = "??4?$Handle@VAemsFactory@Playback@CgsSound@@@Playback@CgsSound@@QAAAAU012@ABU012@@Z"
PREPARE = "?Prepare@Module@1Playback@CgsSound@@UAA_NPAUIResourceAllocator@rw@@PAVLinearMalloc@CgsMemory@@@Z"
RELEASE = "?Release@Module@1Playback@CgsSound@@UAA_NXZ"
DESTRUCT = "?Destruct@Module@1Playback@CgsSound@@UAAXXZ"
DUMP = "?DumpRegistries@Module@1Playback@CgsSound@@QAAXXZ"
ADD = "?AddRegistry@Module@1Playback@CgsSound@@QAAXAAURegistry@23@I@Z"
PLAYER_TABLE = "??_7AemsFactory@Playback@CgsSound@@6BIAemsSamplePlayerFactory@Snd9@@@"
PLAYBACK_TABLE = "??_7Module@0Playback@CgsSound@@6BModuleSingleBuffered@CgsModule@@@"
FACTORY_CTOR = "??0AemsFactory@Playback@CgsSound@@QAA@AAUEnvironment@12@ABUAemsFactorySpec@12@@Z"
AEMS_RELEASE = "?Release@AemsRWSampleFactory@Playback@CgsSound@@UAAXXZ"

def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))

def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def require(condition, description):
    if not condition:
        raise ValueError(description)

def section_signature(blob, parsed):
    """Compare native section payloads, flags, sizes and semantic relocations."""
    _, count, _, _, _, optional, _ = struct.unpack_from("<HHIIIHH", blob)
    sections = parsed[1]
    signature = []
    for i in range(count):
        name, _, _, size, raw, _, _, _, _, flags = struct.unpack_from("<8sIIIIIIHHI", blob, 20 + optional + 40 * i)
        _, relocations = sections[i + 1]
        data = blob[raw:raw + size] if raw else None
        signature.append((name, size, flags, data, relocations))
    return signature


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--out", type=Path, help="Receipt path; defaults to R155_AEMS_LINKED.json in link_dir")
    args = parser.parse_args()
    if not __debug__:
        parser.error("verification requires Python assertions")
    link = args.link_dir.resolve()
    out = args.out or link / "R155_AEMS_LINKED.json"
    manifest, seal, invocation, result, outputs = target_pins(link, args.expected_head)
    seal_path = link / "seal.json"
    base_receipt_path = WAVE / "r154-profile-regression/R154_AEMS_LINKED.json"
    require(sha(base_receipt_path) == BASE_RECEIPT_SHA256, "Published R154 AEMS receipt differs")
    base_receipt = load(base_receipt_path)
    require(base_receipt["ok"] and base_receipt["head"] == BASE and base_receipt["tree"] == manifest["base_tree"],
            "Published R154 AEMS receipt does not attest the expected base")
    require(base_receipt["seal_sha256"] == manifest["base_seal"]["sha256"], "Published R154 receipt belongs to another seal")
    for key, name in (("pe_sha256", "native-game.pe"), ("map_sha256", "native-game.map")):
        require(base_receipt[key] == sha(R154_LINK / name), f"Published R154 {name} differs")
    base_seal = load(R154_LINK / "seal.json")
    base_invocation = load(R154_LINK / "invocation.json")
    require(base_invocation["sealed_sha256"] == base_receipt["seal_sha256"], "R154 invocation seal differs")

    def source_blob(revision, source):
        return subprocess.check_output(
            ["git", "--no-optional-locks", "-C", seal["source_root"], "rev-parse", "--verify", f"{revision}:{source}"],
            text=True,
        ).strip()

    rsp = link / "r155-strict.rsp"
    require(sha(rsp) == invocation["rsp_sha256"], "Link response file differs")
    rsp_args = {line.strip().strip('"') for line in rsp.read_text().splitlines()}

    objects = {}
    object_receipts = {}
    for name in (PLAYBACK, FACTORY, INTERFACE):
        invoked = [row for row in invocation["objects"] if Path(row["object"]).name == name]
        sealed = [row for row in seal["selected"] if Path(row["object"]).name == name]
        require(len(invoked) == len(sealed) == 1 and invoked == sealed, f"{name}: selected binding differs")
        row = invoked[0]
        obj = Path(row["object"])
        require(row["object"] in rsp_args and sha(obj) == row["sha256"], f"{name}: selected object hash/path differs")
        old_selected = [item for item in base_seal["selected"] if Path(item["object"]).name == name]
        old_invoked = [item for item in base_invocation["objects"] if Path(item["object"]).name == name]
        require(len(old_selected) == len(old_invoked) == 1 and old_selected == old_invoked,
                f"{name}: R154 selected provider differs")
        old = old_selected[0]
        old_obj = Path(old["object"])
        pin = base_receipt["selected_objects"][name]
        require(old_obj == Path(pin["path"]) and sha(old_obj) == old["sha256"] == pin["sha256"],
                f"{name}: R154 object identity differs")
        source = row["source"]
        require(source == old["source"], f"{name}: source identity differs")
        blob_id = source_blob(BASE, source)
        require(blob_id == source_blob(args.expected_head, source), f"{name}: source blob differs from R154")
        before, after = old_obj.read_bytes(), obj.read_bytes()
        baseline, candidate = coff(before), coff(after)
        # Compare every AEMS witness at its named COFF symbol; Name ABI adds a separate section
        # and changes only the factory constructor among these selected bodies.
        witness_symbols = {PLAYBACK: [ASSIGN, PREPARE, RELEASE, DESTRUCT, DUMP, ADD, PLAYBACK_TABLE],
                           FACTORY: [PLAYER_TABLE, FACTORY_CTOR], INTERFACE: [AEMS_RELEASE]}[name]
        for symbol in witness_symbols:
            (old_code, old_relocs), old_start = find(symbol, *baseline)
            (new_code, new_relocs), new_start = find(symbol, *candidate)
            require(old_start == new_start == 0, f"{symbol}: unexpected symbol offset")
            if symbol == FACTORY_CTOR:
                require(sha256_bytes(old_code) == "858814e7cd7b219bc1891adc250e4171cc6933e907a779694cc3083d31e13de7" and sha256_bytes(new_code) == "858814e7cd7b219bc1891adc250e4171cc6933e907a779694cc3083d31e13de7", "factory constructor code differs")
                require(sha256_bytes(repr(old_relocs).encode()) == "9e944859fe71a308fb690b1b1847a59ddc9dd60309eedbef1c2db21290a7a545" and sha256_bytes(repr(new_relocs).encode()) == "9e944859fe71a308fb690b1b1847a59ddc9dd60309eedbef1c2db21290a7a545", "factory constructor relocations differ")
            else:
                require(old_code == new_code and old_relocs == new_relocs, f"{symbol}: witness changed")
        changed = []
        objects[name] = candidate
        object_receipts[name] = {
            "path": str(obj), "sha256": row["sha256"], "source": source, "source_blob": blob_id,
            "same_object_bytes_as_r154": row["sha256"] == old["sha256"],
            "changed_witnesses": changed,
        }

    image_path = link / "native-game.pe"
    map_path = link / "native-game.map"
    for path in (image_path, map_path):
        expected = [row for name, row in result["outputs"].items() if Path(name).name == path.name]
        require(len(expected) == 1 and expected[0]["sha256"] == sha(path), f"{path.name}: output hash differs")

    image = pefile.PE(str(image_path), fast_load=True)
    require(image.FILE_HEADER.Machine == 0x1F2, "PE is not Xbox 360 PPC")
    base = image.OPTIONAL_HEADER.ImageBase
    mapped = {}
    for line in map_path.read_text(errors="replace").splitlines():
        match = re.match(r"^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8})\s+(.+)$", line)
        if match:
            symbol, address, info = match.groups()
            mapped.setdefault(symbol, set()).add((int(address, 16), info.split()[-1]))

    def address(symbol, owner=None):
        entries = mapped.get(symbol, set())
        matches = {va for va, name in entries if owner is None or name == owner}
        require(len(matches) == 1, f"Map resolution for {symbol} ({owner}) is ambiguous or absent")
        return next(iter(matches))

    def read(va, size):
        rva = va - base
        sections = [s for s in image.sections if 0 <= rva - s.VirtualAddress <= s.SizeOfRawData - size]
        require(len(sections) == 1, f"Address 0x{va:08x} is outside the PE file-backed sections")
        section = sections[0]
        offset = section.PointerToRawData + rva - section.VirtualAddress
        return image.__data__[offset:offset + size]

    def word(va):
        return int.from_bytes(read(va, 4), "big")

    def body(owner, symbol):
        (data, relocations), start = find(symbol, *objects[owner])
        require(start == 0, f"{symbol}: unexpected COFF symbol offset")
        return data, {off: (kind, target) for off, kind, target in relocations if kind != 18}

    def compare(owner, symbol, start, size=None):
        data, relocs = body(owner, symbol)
        end = len(data) if size is None else start + size
        require(end <= len(data) and not any(start <= off < end for off in relocs), f"{symbol}: invalid byte window")
        require(read(address(symbol, owner) + start, end - start) == data[start:end], f"{symbol}: linked body mismatch at +0x{start:x}")

    def exact(owner, symbol, offset, expected):
        expected = bytes.fromhex(expected)
        compare(owner, symbol, offset, len(expected))
        require(read(address(symbol, owner) + offset, len(expected)) == expected, f"{symbol}: expected instruction sequence missing")

    edges = {}
    def branch(symbol, offset, target, target_owner=None):
        data, relocs = body(PLAYBACK, symbol)
        require(relocs.get(offset) == (6, target), f"{symbol}: unexpected REL24 at +0x{offset:x}")
        site = address(symbol, PLAYBACK) + offset
        opcode = word(site)
        original_opcode = int.from_bytes(data[offset:offset + 4], "big")
        require(opcode & 0xFC000003 == original_opcode & 0xFC000003 == 0x48000001, f"{symbol}: linked instruction is not relative bl")
        displacement = opcode & 0x03FFFFFC
        if displacement & 0x02000000:
            displacement -= 0x04000000
        destination = address(target, target_owner)
        require(site + displacement == destination, f"{symbol}: branch at 0x{site:x} misses {target}")
        edges[f"{symbol}+0x{offset:x}"] = {"site": hex(site), "target": target, "address": hex(destination)}

    def table(owner, symbol, slots):
        _, relocs = body(owner, symbol)
        va = address(symbol, owner)
        for offset, (target, target_owner) in slots.items():
            require(relocs.get(offset, (None, None))[1] == target, f"{symbol}: unexpected COFF slot +0x{offset:x}")
            require(word(va + offset) == address(target, target_owner), f"{symbol}: linked slot +0x{offset:x} is wrong")
        return hex(va)

    compare(PLAYBACK, ASSIGN, 0)
    exact(PLAYBACK, ASSIGN, 0x1C, """
        817e0000 2b0b0000 419a0010 814b0008 394a0001 914b0008
        807f0000 2b030000 419a0014 81630000 816b0004 7d6903a6 4e800421
        817e0000 7fe3fb78 917f0000
    """)
    exact(PLAYBACK, RELEASE, 0x188, "38810050 93e10050 387e2260 93fe225c")
    branch(RELEASE, 0x184, "?Release@Object@Playback@CgsSound@@QAAXXZ")
    branch(RELEASE, 0x198, ASSIGN, PLAYBACK)
    branch(RELEASE, 0x1A8, "?Release@Object@Playback@CgsSound@@QAAXXZ")
    branch(PREPARE, 0x274, "?Create@AemsFactory@Playback@CgsSound@@SA?AU?$Handle@VAemsFactory@Playback@CgsSound@@@23@AAUEnvironment@23@ABUAemsFactorySpec@23@@Z", FACTORY)
    exact(PLAYBACK, PREPARE, 0x278, "3bbe2260 38810058 7fa3eb78")
    branch(PREPARE, 0x284, ASSIGN, PLAYBACK)
    exact(PLAYBACK, PREPARE, 0x288, "80610058 2b030000 419a0014 81630000 816b0004 7d6903a6 4e800421")
    exact(PLAYBACK, DUMP, 0xAC, "83fe2260")
    exact(PLAYBACK, DUMP, 0xD0, "807f0060")
    branch(DUMP, 0xD4, "?Dump@Registry@Playback@CgsSound@@QAAXXZ")
    exact(PLAYBACK, ADD, 0xB8, "83df2260")
    exact(PLAYBACK, ADD, 0xE8, "83de0060")
    exact(PLAYBACK, ADD, 0x110, "7fc3f378 7fa4eb78")
    branch(ADD, 0x118, "??YRegistry@Playback@CgsSound@@QAAAAU012@AAU012@@Z")

    playback_table = table(PLAYBACK, PLAYBACK_TABLE, {8: (RELEASE, PLAYBACK), 12: (DESTRUCT, PLAYBACK), 0x44: (PREPARE, PLAYBACK)})
    aems_table = table(FACTORY, PLAYER_TABLE, {4: (AEMS_RELEASE, INTERFACE)})
    exact(INTERFACE, AEMS_RELEASE, 0, "4e800020")
    constructor = address(FACTORY_CTOR, FACTORY)
    _, ctor_relocs = body(FACTORY, FACTORY_CTOR)
    require(ctor_relocs.get(0x2C) == (16, PLAYER_TABLE) and ctor_relocs.get(0x34) == (17, PLAYER_TABLE), "AEMS constructor table relocations differ")
    hi, lo = word(constructor + 0x2C), word(constructor + 0x34)
    require(hi >> 16 == 0x3D60 and lo >> 16 == 0x396B, "AEMS constructor table address instructions differ")
    low = (lo & 0xFFFF) - (0x10000 if lo & 0x8000 else 0)
    require(((hi & 0xFFFF) * 0x10000 + low) & 0xFFFFFFFF == int(aems_table, 16), "AEMS constructor uses a different primary table")
    exact(FACTORY, FACTORY_CTOR, 0x40, "917f0000")

    receipt = {
        "ok": True, "head": seal["head"], "tree": TREE, "seal_sha256": sha(seal_path),
        "target_manifest_sha256": TARGET_MANIFEST_SHA, "xex_sha256": outputs["native-game.exe"],
        "base": BASE, "base_aems_receipt_sha256": BASE_RECEIPT_SHA256,
        "pe_sha256": sha(image_path), "map_sha256": sha(map_path), "selected_objects": object_receipts,
        "playback_vtable": playback_table, "aems_primary_vtable": aems_table,
        "typed_assign": hex(address(ASSIGN, PLAYBACK)), "aems_release": hex(address(AEMS_RELEASE, INTERFACE)),
        "checked_branches": edges,
        "scope": "Linked PE and map, selected PPC COFF and strict-link provenance"
    }
    with out.open("x", encoding="utf-8") as handle:
        json.dump(receipt, handle, indent=2)
        handle.write("\n")
    print(out)

if __name__ == "__main__":
    main()
