import collections
import json
import pathlib
import runpy

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
old = wave / "r40-link-repaired"
helpers = runpy.run_path(str(old / "prepare_seal.py"))
read, parse, archive, sha = (helpers[key] for key in ("read", "parse", "archive", "sha"))
seal = read(old / "seal.json")
assert sha(old / "seal.json") == "fed9eb3508f52daea6d1d046a692ccd66cde8aaa4764d2c1fb6ecddceae85193"
xhv = helpers["SDK"] / "lib/xbox/xhv.lib"
assert sha(xhv) == "d4cff97c603e15bce167559bca2e7d02f6676c8a958f0e66b37e5c873497485e"
def_types = {"strong", "comdat", "common", "absolute", "import"}
members = collections.defaultdict(list)
archive_rows = list(archive(xhv))
unparsed = []
for member, payload in archive_rows:
    rows = parse(payload)
    if rows is None:
        unparsed.append(member)
        continue
    for name, kind, *_ in rows:
        if kind in def_types:
            members[name].append((member, kind))
overlap = collections.defaultdict(list)
for row in seal["selected"]:
    path = pathlib.Path(row["object"])
    for name, kind, *_ in parse(path.read_bytes()):
        if kind in def_types and name in members:
            assert sha(path) == row["sha256"]
            overlap[name].append((row["source"], kind))
result = {
    "head": seal["head"],
    "archive_sha256": sha(xhv),
    "selected_objects": len(seal["selected"]),
    "archive_members": len(archive_rows),
    "unparsed_archive_members": unparsed,
    "overlap": {name: {"archive": members[name], "selected": owners} for name, owners in sorted(overlap.items())},
    "nonconstant_overlap": sorted(name for name in overlap if not name.startswith(("__real@", "__xmm@"))),
}
out = here / "R40_XHV_FULL_ARCHIVE_OVERLAP.json"
with out.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print("overlap", len(overlap), "nonconstant", result["nonconstant_overlap"], "unparsed", len(unparsed), "sha", sha(out))
