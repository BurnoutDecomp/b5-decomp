import collections
import hashlib
import json
import pathlib
import re
import runpy

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
old = wave / "r41-link"
helpers = runpy.run_path(str(old / "prepare_seal.py"))
read, parse, archive, sha = (helpers[key] for key in ("read", "parse", "archive", "sha"))
base = read(old / "seal.json")
assert sha(old / "seal.json") == "b70317172b2308576abdd020b53c672fc8556f2effd7d78442d2f38c9b684120"
baseline = {row["source"]: row for row in base["selected"]}
report = here / "native-combined/compile-results.json"
compiled = read(report)
fresh = {row["source"]: row for row in compiled["results"]}
assert len(fresh) == compiled["requested"] == compiled["passed"] == 14
assert compiled["failed"] == 0 and fresh.keys() <= baseline.keys()
assert all(row["machine"] == 0x1f2 for row in fresh.values())

defined = {"strong", "comdat", "common", "absolute", "import"}
new_refs, sdk_refs = set(), collections.defaultdict(list)
for source, row in fresh.items():
    prior = {n for n, k, *_ in parse(pathlib.Path(baseline[source]["object"]).read_bytes()) if k == "undefined"}
    current = {n for n, k, *_ in parse(pathlib.Path(row["object"]).read_bytes()) if k == "undefined"}
    new_refs.update(current - prior)

providers = collections.defaultdict(set)
legacy = []
aliases = {}
for source, row in (baseline | fresh).items():
    for name, kind, *_ in parse(pathlib.Path(row["object"]).read_bytes()):
        if name in new_refs and kind in defined:
            providers[name].add(source)
        if (kind == "undefined" and re.match(r"\?gApt(?:KeyThis|ThisKey|KeyGlobal|KeySuper|KeyPrototype|KeyControllerKey|EmptyMethodName|ObjectClassName|StringClassName|SpriteClassKey|TypeName_(?:Undefined|Number|Boolean|String|Object|MovieClip|Null|Function))@@3BVEAStringC", name)):
            legacy.append((source, name))
assert not legacy, legacy
missing = new_refs - providers.keys()
for lib in base["libraries"]:
    if not missing:
        break
    path = pathlib.Path(lib["path"])
    assert sha(path) == lib["sha256"]
    for member, data in archive(path):
        symbols = parse(data)
        if symbols is None and len(data) >= 20 and data[:4] == b"\0\0\xff\xff":
            name = data[20:].split(b"\0")[0].decode("latin1")
            symbols = [(name, "import"), ("__imp_" + name, "import")]
        for name, kind, *_ in symbols or ():
            if name in missing and kind in defined:
                providers[name].add(f"{path.name}({member})")
                missing.remove(name)
assert not missing, missing

def asm(stem):
    row = next(x for x in fresh.values() if pathlib.PurePosixPath(x["source"]).stem == stem)
    return pathlib.Path(row["assembly"]).read_text(errors="replace")

glob = asm("AptGlobals")
for name, offset in re.findall(r"(\?gApt\w+@@3ABVEAStringC@@B) DD \?saConstant@StringPool@@2PAVEAStringC@@A\+(\d+)", glob):
    aliases[name] = int(offset) // 4
assert len(aliases) == 18, aliases
assert not re.search(r"\?\?__EgApt(?:Key|TypeName|ObjectClassName|StringClassName|SpriteClassKey)", glob)
init = asm("AptInit")
assert not any(name in init for name in ("gAptArrayClassName", "gAptDateClassName", "gAptTextFormatClassName",
                                        "gAptColorClassName", "gAptXMLClassName", "gAptErrorClassName"))
stack = asm("AptActionInterpreterStackOps")
assert ".CRT$" not in stack and "??__E" not in stack and "??__F" not in stack
call_method = stack.split("; Begin code for function: ?_FunctionAptActionCallMethod@", 1)[1]
call_method = call_method.split("; End code for function:", 1)[0]
assert len(re.findall(r"\bbl\s+_stricmp\b", call_method)) == 5

def pin(path):
    path = pathlib.Path(path)
    return {"path": str(path.resolve()), "sha256": sha(path)}
receipt = {
    "base": base["head"], "head": "c2e4cb76e633c8d2cbb3438cfa3260ae7bbd2e20",
    "tree": "d03fa15003fddfa3730cdee12b417168649e642b",
    "compile": pin(report), "targeted_passed": len(fresh),
    "startup_trace": pin(wave / "native-xenia-smoke/run-r41/startup-trace-functions.json"),
    "original_slot_base": "0x8324E580", "original_initializer": "0x82AE3630",
    "class_slot_order": [52, 31, 36, 38, 79, 48, 85, 39, 77],
    "native_alias_slots": aliases, "old_alias_refs_in_selected": legacy,
    "native_stack_stricmp_calls": 5, "stack_crt_initializers": 0,
    "added_reference_providers": {name: sorted(providers[name]) for name in sorted(new_refs)},
    "stack_receipt": pin(wave.parent / "wave42/apt-stack-literals-sol/RECEIPT.json"),
}
out = here / "DELIVERY.json"
with out.open("x", encoding="utf-8") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print("Apt targeted", len(fresh), "aliases", len(aliases), "new refs", len(new_refs), "receipt", sha(out))
