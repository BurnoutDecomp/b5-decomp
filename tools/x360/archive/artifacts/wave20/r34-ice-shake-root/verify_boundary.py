import hashlib
import json
import pathlib
import runpy
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
TOOLS = runpy.run_path(str(HERE.parent / "r32-link/prepare_seal.py"))
read, parse, sha = (TOOLS[key] for key in ("read", "parse", "sha"))
seal_path = pathlib.Path(sys.argv[1])
seal = read(seal_path)
objects = seal["selected"] + seal["excluded"]
assert len(objects) == len({row["source"] for row in objects}) == 3695
source = "src/GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp"
owner, = [row for row in seal["selected"] if row["source"] == source]
get_shake = "?GetShakeTakes@DirectorResourceManager@BrnDirector@@QBAPAUICEGroup@ICE@@XZ"
get_group = "?GetShakeGroup@ICEWrapper@BrnDirector@@QAAPAUICEGroup@ICE@@XZ"
targets = (get_shake, get_group)
matches = {}
archive_matches = {}


def scan_file(row):
    path = pathlib.Path(row["object"] if "object" in row else row["path"])
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == row["sha256"], path
    return data


owner_data = scan_file(owner)
owner_symbols = parse(owner_data)
assert owner_symbols is not None
definitions = {value[0]: value for value in owner_symbols if value[1] == "comdat"}
assert get_shake in definitions and all(value[1] in ("comdat", "undefined") for value in owner_symbols)
assert {row[0] for row in owner_symbols if row[1] == "undefined"} == {get_group, "__savegprlr_28", "__restgprlr_28"}
assert definitions[get_shake][3] == 1
assert all(value[3] == 2 for name, value in definitions.items() if name != get_shake)


def sections(data):
    machine, count, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2
    return [struct.unpack_from("<8sIIIIIIHHI", data, 20 + optional_size + i * 40) for i in range(count)]


def directive_bytes(data):
    return [data[item[4]:item[4] + item[3]] for item in sections(data) if item[0] == b".drectve"]


owner_sections = sections(owner_data)
assert not any(item[0].startswith(b".CRT") for item in owner_sections)
owner_directives = directive_bytes(owner_data)
fallback = {}
external_refs = set()
same_directives = None
selected = {row["source"] for row in seal["selected"]}
for row in objects:
    data = scan_file(row)
    parsed = parse(data)
    assert parsed is not None, row
    relevant = [value for value in parsed if value[0] in targets]
    if relevant:
        matches[row["source"]] = relevant
    if row["source"] == source:
        continue
    for name, kind, _, selection, value in parsed:
        if kind == "weak" and value in definitions:
            external_refs.add(value)
        if name not in definitions:
            continue
        if kind == "undefined":
            external_refs.add(name)
        elif kind == "comdat" and selection == definitions[name][3] and row["source"] in selected:
            fallback.setdefault(name, row["source"])
    if same_directives is None and row["source"] in selected and directive_bytes(data) == owner_directives:
        same_directives = row["source"]

unbacked = sorted(set(definitions) - fallback.keys())
for row in seal["libraries"]:
    data = scan_file(row)
    possible = [symbol for symbol in set(targets).union(unbacked) if symbol.encode() in data]
    if possible:
        archive_matches[row["path"]] = possible

assert list(matches) == [source], matches
assert not archive_matches, archive_matches
assert not external_refs - fallback.keys(), sorted(external_refs - fallback.keys())
assert same_directives or not owner_directives
report = {"head": seal["head"], "seal_sha256": sha(seal_path), "objects_screened": len(objects),
          "archives_screened": len(seal["libraries"]), "source": source, "object": owner,
          "symbols": matches, "outside_shake_references": [], "owner_comdats": len(definitions),
          "fallbacks": {name: fallback[name] for name in sorted(external_refs)},
          "comdats_without_retained_alternative_or_external_reference": unbacked,
          "same_directives": same_directives}
output = HERE / f"{seal['head'][:9]}-COFF_BOUNDARY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(report, stream, indent=2)
    stream.write("\n")
print("No outside callers or providers", sha(output), output)
