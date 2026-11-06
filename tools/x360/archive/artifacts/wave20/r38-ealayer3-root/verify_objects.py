import collections
import json
import pathlib
import re
import runpy
import subprocess

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
helpers = runpy.run_path(str(wave / "r38-link/prepare_seal.py"))
read, parse, archive, sha = (helpers[key] for key in ("read", "parse", "archive", "sha"))

base = "5ea596e049a8eaff6253c1513e54d9208ef2751e"
head = "0c3ab487c25377a8baac8e72fc337413857509df"
assert subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=work).decode().strip() == head
assert subprocess.check_output(["git", "rev-parse", "HEAD^"], cwd=work).decode().strip() == base
paths = subprocess.check_output(["git", "diff", "--name-only", base, head], cwd=work).decode().splitlines()
assert len(paths) == 6
patch = here / "candidate.patch"
with patch.open("xb") as stream:
    stream.write(subprocess.check_output(["git", "diff", "--binary", "--full-index", base, head], cwd=work))

compile_report = here / "native2/compile-results.json"
compiled = read(compile_report)
assert compiled["requested"] == compiled["passed"] == 4 and compiled["failed"] == 0
by_file = {pathlib.Path(row["source"]).stem: row for row in compiled["results"]}
assert all(row["machine"] == 0x1F2 for row in by_file.values())
obj = pathlib.Path(by_file["EaLayer3Dec"]["object"]).read_bytes()
raw_tables = read(here / "tables-native.json")
gain = bytes.fromhex(raw_tables["gain"]["bytes"])[:256 * 4]
tables = [(hex(raw_tables["gain"]["address"]), gain)]
tables += [(hex(row["address"]), bytes.fromhex(row["bytes"])) for row in raw_tables["huffman"]]
data_proof = {}
for address, native_bytes in tables:
    assert obj.count(native_bytes) == 1, address
    data_proof[address] = {"size": len(native_bytes), "object_offset": obj.find(native_bytes)}

symbol = "?CreateInstance@EaLayer3DecBase@core@audio@rw@@QAAHE@Z"
legacy = "?CreateInstance@EaLayer3DecBase@core@audio@rw@@SAHHE@Z"
defined = {"strong", "comdat", "common"}
assert any(name == symbol and kind in defined for name, kind, *_ in parse(obj))
for name, immediate in (("EaLayer31Dec", "38800000"), ("EaLayer32PcmDec", "38800001")):
    listing = pathlib.Path(by_file[name]["assembly"]).read_text(errors="replace")
    insns = re.findall(r"^\s*([0-9a-f]{5})\t([0-9a-f]{8})\t(.+)$", listing, re.M)
    assert [(offset, word) for offset, word, _ in insns] == [("00000", immediate), ("00004", "48000000")]
    assert insns[1][2].strip() == "b            " + symbol
    names = {n for n, k, *_ in parse(pathlib.Path(by_file[name]["object"]).read_bytes()) if k == "undefined"}
    assert symbol in names and legacy not in names

baseline = read(wave / "r38-link/seal.json")
actual = {row["source"]: row for row in baseline["selected"]}
actual.update({row["source"]: row for row in by_file.values()})
refs = {n for n, k, *_ in parse(obj) if k == "undefined"}
providers = collections.defaultdict(set)
for path, row in actual.items():
    object_path = pathlib.Path(row["object"])
    for name, kind, *_ in parse(object_path.read_bytes()):
        if name in refs and kind in defined:
            if "sha256" in row:
                assert sha(object_path) == row["sha256"]
            providers[name].add(path)
missing = refs - providers.keys()
for lib in baseline["libraries"]:
    if not missing:
        break
    library = pathlib.Path(lib["path"])
    assert sha(library) == lib["sha256"]
    for member, contents in archive(library):
        for name, kind, *_ in parse(contents) or []:
            if name in missing and kind in defined:
                providers[name].add(f"{library.name}({member})")
                missing.remove(name)
assert not missing, missing

def pin(p):
    p = pathlib.Path(p)
    return {"path": str(p.resolve()), "sha256": sha(p)}

result = {
    "base": base,
    "head": head,
    "paths": paths,
    "patch": pin(patch),
    "compile_report": pin(compile_report),
    "command": pin(here / "command2.json"),
    "raw_artifacts": [pin(here / name) for name in ("core.txt", "tables-native.json", "extract_tables.py", "emit_tables.py")],
    "native_artifacts": [{key: pin(row[key]) for key in ("object", "assembly", "log")} for row in by_file.values()],
    "matched_data_bytes": data_proof,
    "calls": {name: sorted(providers[name]) for name in sorted(refs)},
    "old_missing_abi": legacy,
    "typed_abi": symbol,
    "limits": [
        "Allocation and core lifetime only. Decoder registration, DecodeEvent, and target audio are unverified.",
        "The original allocation order and unchecked null allocation for a nonzero channel count are preserved.",
        "Decoder/core size asserts describe the Xbox 360 ABI.",
    ],
}
with (here / "DELIVERY.json").open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(pin(here / "DELIVERY.json"))
