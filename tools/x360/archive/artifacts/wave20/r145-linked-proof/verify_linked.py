"""Bind the composed R145 Apt PPC proofs to a sealed linked PE and map."""

import argparse
import json
import re
import runpy
import struct
import subprocess
from pathlib import Path

import pefile


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
BASE = "b956c18869bdde471088c9a640ea34fb41178f06"
TREE = "2242c25d9742f1f64dec98c6c529a90ecb8e0ca4"
PRIVATE = "5295d92753407372620b53c4674c60198b13bb97"
MANIFEST_SHA = "8ab5f9f6af0d4191cbad8972f0b2aea2ca208564be656671ea97dcbf9111b816"
COMPOSED_SHA = "17eca36bafb5179dbfb74e7b9d78d8e226cf39d45d91343803755ab4a3f13c48"
PREVIOUS_SHA = "bcccaf6d775764472f93c662b2dbb19caeceabdca7c04ecbcfa958ad957b81b1"

OLD_PATH = WAVE / "r143-linked-proof/verify_linked.py"
old = runpy.run_path(str(OLD_PATH))
sha = old["sha"]
assert sha(OLD_PATH.read_bytes()) == PREVIOUS_SHA
coff = old["coff_functions"]
prefixes = coff.__globals__["PREFIX"]
prefixes.update({
    "CihCtor": "??0AptCIH@@",
    "CihGetHasClass": "?GetHasClass@AptCIH@@",
    "CihSetHasClass": "?SetHasClass@AptCIH@@",
})

RETAINED = {
    stem: names for stem, names in old["FUNCTIONS"].items()
    if stem not in {"AptCIH", "AptDisplayList"}
}
CHANGED = {
    "AptAnimationTarget": ("AptAnimationTargetAddToRemList", "CleanRemList", "RunActions", "GetInRemList", "SetInRemList"),
    "AptCIH": ("CihCtor", "GetCIHState", "SetCIHState", "CihGetHasClass", "CihSetHasClass", "IsInCtor", "SetInCtor",
               "GetCreatedOnFrame", "SetCreatedOnFrame", "GetZombieCount", "IncZombieCount", "DecZombieCount",
               "SetDirtyState", "tick"),
    "AptCIHBehaviour": ("SetGeneralizedProcessDirtyState",),
    "AptDisplayList": ("AssociateInstToClass",),
    "AptGC": ("AptUpdateZombieVector",),
    "AptKey": RETAINED["AptKey"] + ("sMethod_addListener",),
    "AptLinker": ("ConvertToZombie", "Load", "Update"),
    "CgsAptAux": ("ConstructApt", "FreeConstantTable"),
    "AptLoader": ("CompleteLoad",),
}
OPTIONAL_INLINE = {"SetInCtor", "GetInRemList", "SetInRemList"}
FUNCTIONS = RETAINED | CHANGED
WORD = old["word"]
EXPECT = old["expect_words"]


def load(path, digest=None):
    data = path.read_bytes()
    if digest:
        assert sha(data) == digest, path
    return json.loads(data)


def sealed_link(link, head):
    result = load(link / "result.json")
    invocation = load(link / "invocation.json")
    seal_bytes = (link / "seal.json").read_bytes()
    seal = json.loads(seal_bytes)
    image = load(link / "image-verification.json")
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert image["head"] == seal["head"] == head, (image["head"], seal["head"], head)
    assert sha(seal_bytes) == invocation["sealed_sha256"]

    def output(name):
        [record] = [row for path, row in result["outputs"].items() if Path(path).name == name]
        data = (link / name).read_bytes()
        assert len(data) == record["size"] and sha(data) == record["sha256"], name
        return data

    xex, pe, map_data = [output(name) for name in ("native-game.exe", "native-game.pe", "native-game.map")]
    assert xex[:4] == b"XEX2" and sha(xex) == image["image_sha256"]
    p = pefile.PE(data=pe, fast_load=True)
    assert p.FILE_HEADER.Machine == 0x1F2

    def read(address, size):
        rva = address - p.OPTIONAL_HEADER.ImageBase
        section = next(s for s in p.sections if s.VirtualAddress <= rva and
                       rva + size <= s.VirtualAddress + s.SizeOfRawData)
        offset = section.PointerToRawData + rva - section.VirtualAddress
        return pe[offset:offset + size]

    [rsp_name] = [arg[1:] for arg in invocation["command"] if arg.startswith("@")]
    rsp_bytes = (link / Path(rsp_name).name).read_bytes()
    assert sha(rsp_bytes) == invocation["rsp_sha256"]
    rsp = rsp_bytes.decode().splitlines()
    sealed = {row["source"]: row for row in seal["selected"]}
    objects = {}
    for row in invocation["objects"]:
        stem = Path(row["source"]).stem
        if stem in FUNCTIONS:
            assert stem not in objects and sealed[row["source"]] == row
            path = Path(row["object"])
            assert rsp.count(f'"{path}"') == 1
            data = path.read_bytes()
            assert sha(data) == row["sha256"]
            objects[stem] = path, data, row
    hashes = {"seal": sha(seal_bytes), "response": sha(rsp_bytes), "xex": sha(xex), "pe": sha(pe), "map": sha(map_data)}
    return seal, objects, old["read_map"](map_data), read, hashes


def compare_obj(data, frozen, names):
    a, b = coff(data, names), coff(frozen, names)
    for method in names:
        name, code, refs = a[method]
        old_name, old_code, old_refs = b[method]
        assert old["stable_name"](name) == old["stable_name"](old_name) and code == old_code, method
        assert old["canonical_bindings"](refs) == old["canonical_bindings"](old_refs), method
    return a


def bind(objects, frozen, names_by_object, entries, read):
    linked, proofs, compiled_only = {}, {}, {}
    for stem, methods in names_by_object.items():
        path, data, _ = objects[stem]
        for method, (symbol, code, refs) in compare_obj(data, frozen[stem], methods).items():
            if method in OPTIONAL_INLINE and symbol not in entries:
                compiled_only[method] = {"symbol": symbol, "bytes": len(code), "sha256": sha(code),
                                         "reason": "selected COFF verified; no standalone symbol in map"}
                continue
            address = old["map_address"](entries, symbol, None if method in OPTIONAL_INLINE else path.name)
            linked[method], proofs[method] = old["verify_function"](entries, read, address, code, refs)
            proofs[method]["symbol"] = symbol
    return linked, proofs, compiled_only


def check_new_paths(linked, proofs, compiled_only, composed, entries, read):
    # Bytes and relocations of each named body match the frozen composed COFF.
    # These checks also identify the address-sensitive field and callback witnesses.
    ci = runpy.run_path(str(WAVE / "r145-cih-proof/verify.py"))
    for method in composed["cih_cases"]:
        if method in compiled_only:
            continue
        key = {"GetHasClass": "CihGetHasClass", "SetHasClass": "CihSetHasClass"}.get(method, method)
        code = struct.unpack(f">{len(linked[key]) // 4}I", linked[key])
        refs = {int(off, 16) // 4: bound["symbol"] for off, bound in proofs[key]["bindings"].items()}
        assert ci["verify_leaf"](method, code, refs, candidate=True) == composed["cih_cases"][method]
    # Branch call sites are the same as the frozen bit/constructor/reaper proof.
    EXPECT(linked["tick"], 0x10, 0x815C000C, 0x554B018C)
    EXPECT(linked["AptAnimationTargetAddToRemList"], 0x10, 0x817E000C, 0x556B37FE)
    EXPECT(linked["AptAnimationTargetAddToRemList"], 0x84, 0x817E000C, 0x57EA103A, 0x656B0400, 0x917E000C)
    EXPECT(linked["CleanRemList"], 0x7C, 0x817F000C, 0x556B0044)
    EXPECT(linked["CihCtor"], 0x6C, 0x3D800B80, 0x817F000C, 0x38800001, 0x618C007F)
    EXPECT(linked["CihCtor"], 0x98, 0x656BFFFC, 0x917F0010)
    EXPECT(linked["CihCtor"], 0xB0, 0x556B0146)
    for off, callee in (("0x58", "GetCIHState"), ("0x74", "GetZombieCount"), ("0xd8", "SetCIHState")):
        binding = proofs["AptUpdateZombieVector"]["bindings"][off]
        assert binding["target"] == proofs[callee]["address"]
    EXPECT(linked["AptUpdateZombieVector"], 0x78, 0x7C6B0734, 0x2F0B0000, 0x409A009C)

    construct = linked["ConstructApt"]
    register = proofs["ConstructApt"]["bindings"]["0x200"]
    assert register["symbol"] == proofs["FreeConstantTable"]["symbol"]
    assert register["target"] == proofs["FreeConstantTable"]["address"]
    high = [row for row in proofs["ConstructApt"]["bindings"].values()
            if row["type"] == "0x10" and row["symbol"] == register["symbol"]]
    assert high and all(row["target"] == register["target"] for row in high)
    EXPECT(construct, 0x204, 0x914B0028)
    assert linked["FreeConstantTable"] == bytes.fromhex("4e800020")

    complete = linked["CompleteLoad"]
    EXPECT(complete, 0x1C, 0x2B1E0000, 0x419A007C)
    EXPECT(complete, 0x7C, 0x7D7E5850, 0x917F0014)
    EXPECT(complete, 0x90, 0x816B0028, 0x7D6903A6, 0x4E800421, 0x39600000)
    call_refs = proofs["CompleteLoad"]["bindings"]
    for offset, kind in (("0x84", "0x10"), ("0x8c", "0x11")):
        assert call_refs[offset]["symbol"] == "?gAptFuncs@@3UAptUserFunctions@@A" and call_refs[offset]["type"] == kind
    assert call_refs["0x84"]["target"] == call_refs["0x8c"]["target"]
    assert call_refs["0xa8"]["symbol"].startswith("?Dispose@?$AptSharedPtr@UAptFile@@@@")
    return {"verified_leaf_cases": {name: count for name, count in composed["cih_cases"].items()
                                    if name not in compiled_only}, "dirty_get_inlined": "tick+0x10",
            "removal_get_set_inlined": ["AptAnimationTargetAddToRemList+0x10", "AptAnimationTargetAddToRemList+0x84"],
            "cih_compound_checks": composed["cih_consumers"],
            "constant_table": {"slot": "0x28", "registration": register, "dispatch_order":
                               ["restore at +0x80", "callback at +0x98", "Dispose at +0xa8"],
                               "null_branch": "+0x20 skips the callback"}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path, nargs="?")
    parser.add_argument("--expected-head")
    parser.add_argument("--self-test", action="store_true", help="bind retained providers in R144 and inspect R145 COFF")
    args = parser.parse_args()
    manifest = load(WAVE / "r145-batch/MANIFEST.json", MANIFEST_SHA)
    assert (manifest["base"], manifest["candidate"], manifest["projected_tree"]) == (BASE, PRIVATE, TREE)
    composed = load(WAVE / "r145-composed-proof/R145_COMPOSED_COFF.json", COMPOSED_SHA)
    assert (composed["head"], composed["tree"], composed["parent"]) == (PRIVATE, TREE, BASE)
    report = load(WAVE / "r145-composed-preflight/compile-results.json", composed["compile_report_sha256"])
    preflight = {Path(row["source"]).stem: Path(row["object"]) for row in report["results"]}
    assert report["passed"] == 11 and report["failed"] == 0
    _, base_objects, base_entries, base_read, _ = sealed_link(WAVE / "r144-link", BASE)

    frozen = {stem: data for stem, (_, data, _) in base_objects.items() if stem in FUNCTIONS}
    for stem, path in preflight.items():
        frozen[stem] = path.read_bytes()
        assert sha(frozen[stem]) == composed["objects_sha256"][stem]
    if args.self_test:
        retained = {stem: names for stem, names in RETAINED.items() if stem != "AptKey"}
        # R144 already links the retained paths and corrected CIH constructor bracket.
        retained |= {"AptKey": RETAINED["AptKey"], "AptCIH": ("IsInCtor", "SetInCtor"),
                     "AptDisplayList": ("AssociateInstToClass",)}
        linked, proofs, only = bind(base_objects, {s: row[1] for s, row in base_objects.items()}, retained,
                                   base_entries, base_read)
        old["verify_dispatch"](linked, proofs, base_entries)
        old["verify_paths"](linked, proofs, base_entries, base_read)
        old["verify_class_paths"](linked, proofs)
        parsed = sum(len(coff(frozen[stem], names)) for stem, names in CHANGED.items())
        print(f"R144 linked retained: {len(proofs)}; compiled-only: {len(only)}; R145 changed COFF parsed: {parsed}")
        return

    if not args.link_dir or not re.fullmatch(r"[0-9a-f]{40}", args.expected_head or ""):
        parser.error("provide link_dir and --expected-head, or --self-test")
    seal, objects, entries, read, hashes = sealed_link(args.link_dir.resolve(), args.expected_head)
    git = ["git", "--no-optional-locks", "-C", seal["source_root"]]
    revision = subprocess.check_output([*git, "rev-parse", f"{args.expected_head}^{{tree}}", f"{args.expected_head}^"],
                                      text=True).splitlines()
    assert revision == [TREE, BASE], revision
    diff = subprocess.check_output([*git, "diff", "--name-only", BASE, args.expected_head], text=True).splitlines()
    assert set(diff) == {row["path"] for row in manifest["final_paths"]}
    linked, proofs, only = bind(objects, frozen, FUNCTIONS, entries, read)
    slots = old["verify_dispatch"](linked, proofs, entries)
    legacy = old["verify_paths"](linked, proofs, entries, read)
    classes = old["verify_class_paths"](linked, proofs)
    native = check_new_paths(linked, proofs, only, composed, entries, read)
    receipt = {
        "schema": "r145-apt-linked-v1", "head": args.expected_head, "tree": TREE, "base": BASE,
        "manifest_sha256": MANIFEST_SHA, "composed_coff_sha256": COMPOSED_SHA, "link_sha256": hashes,
        "selected_objects": {stem: row for stem, (_, _, row) in objects.items()},
        "dispatch_slots": slots, "retained_paths": legacy, "class_paths": classes,
        "native_paths": native, "functions": proofs, "compiled_only": only, "runtime": "not run",
    }
    out = ROOT / "R145_APT_LINKED.json"
    with out.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(out, sha(out.read_bytes()))


if __name__ == "__main__":
    main()
