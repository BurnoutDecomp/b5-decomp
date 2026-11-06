"""Read a frozen strict link and its selected PPC COFF files. Do not write into it."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


HERE = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r149-boot-race-regression\racecar-readiness")
REGRESSION_ROOT = HERE.parent
assert hashlib.sha256((REGRESSION_ROOT / "link_context.py").read_bytes()).hexdigest() == "a3d7f1b7f2a043462992e6a3e3edecdb2f405dff0aa63ee8919aff8d684316e3"
assert hashlib.sha256((REGRESSION_ROOT / "TARGETS.json").read_bytes()).hexdigest() == "fd8b0bb14402a1487fda4c749d588a4e804fde634623e1db9ae727cc07c23088"
sys.path.insert(0, str(REGRESSION_ROOT))
from link_context import DEFAULT_HEAD, TARGETS, sha as link_sha, verify_link_context
RACECAR = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def unpack_words(code):
    return [word for word, in struct.iter_unpack(">I", code)]


def coff_function(path, wanted):
    data = path.read_bytes()
    machine, count, _, symbols, entries, optional, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional == 0
    strings = symbols + entries * 18
    index = 0
    while index < entries:
        offset = symbols + index * 18
        raw_name = data[offset:offset + 8]
        value, section, _, _, aux = struct.unpack_from("<IhHBB", data, offset + 8)
        if raw_name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", raw_name, 4)[0]
            name = data[start:data.index(0, start)].decode()
        else:
            name = raw_name.rstrip(bytes(1)).decode()
        if name == wanted and section > 0:
            assert section <= count and value == 0, (wanted, section, value)
            _, _, _, size, raw, relocation, _, rel_count, _, _ = struct.unpack_from(
                "<8sIIIIIIHHI", data, 20 + 40 * (section - 1))
            words = unpack_words(data[raw:raw + size])
            rels = [(off, kind) for off, _, kind in struct.iter_unpack(
                "<IIH", data[relocation:relocation + rel_count * 10])]
            return words, rels
        index += 1 + aux
    raise ValueError(wanted)


def branch_target(pc, word):
    assert word >> 26 == 18 and not (word & 2), hex(word)
    disp = word & 0x03FFFFFC
    return pc + disp - (0x04000000 if disp & 0x02000000 else 0)


def assert_subsequence(actual, expected):
    position = 0
    for value in expected:
        position = actual.index(value, position) + 1


parser = argparse.ArgumentParser()
parser.add_argument("link", type=Path)
parser.add_argument("--consumers-only", action="store_true", help="exercise unchanged consumers against R140")
parser.add_argument("--expected-head", required=True)
parser.add_argument("--seal-sha", required=True)
parser.add_argument("--xex-sha", required=True)
parser.add_argument("--pe-sha", required=True)
parser.add_argument("--map-sha", required=True)
parser.add_argument("--report", type=Path, help="write the successful linked receipt to a new file (never overwrites)")
args = parser.parse_args()
assert not args.consumers_only, "R151 requires all 14 readiness functions"
folder = args.link.resolve()
link_context = verify_link_context(folder, args.expected_head)
# Pin the frozen source tree and targeted compiled objects before accepting a link.
binding_path = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r151-batch\TARGETED_OBJECT_BINDING.json")
assert sha256(binding_path) == "60b3f9ff9bb6c8aa0577056972bd165512f46917d9e734a8caf7e538abe7214b"
binding = json.loads(binding_path.read_text())
assert (binding["head"], binding["tree"]) == ("19368322ef39f735c81948fae621ab875669717c", "e5a7b8962ff39181fd7ccc77f8708851418bbd44")
assert sha256(folder / "seal.json") == args.seal_sha.lower()
for filename, digest in {"native-game.exe": args.xex_sha, "native-game.pe": args.pe_sha, "native-game.map": args.map_sha}.items():
    assert sha256(folder / filename) == digest.lower()
assert args.expected_head == "bcc3e02a41cdb7bba938b91d7ab638c46962540d"
seal = json.loads((folder / "seal.json").read_text())
import subprocess
root = Path(seal["source_root"])
tree = subprocess.check_output(["git", "rev-parse", args.expected_head + "^{tree}"], cwd=root, text=True).strip()
assert tree == "610a39be8529a22eeb2d463669c53ede9e46a2fe", tree
for row in binding["results"]:
    for artifact in row["artifacts"].values():
        assert sha256(Path(artifact["path"])) == artifact["sha256"]

receipt = json.loads((HERE / "RECEIPT.json").read_text())
race_receipt_witness = TARGETS["r141_supporting_witnesses"]["racecar_receipt"]["sha256"]
candidate_witness = TARGETS["r141_supporting_witnesses"]["racecar_candidate_object"]["sha256"]
assert link_sha(HERE / "RECEIPT.json") == race_receipt_witness
assert link_sha(HERE / "candidate/BrnRaceCarEntityModule-7a7e1565.obj") == candidate_witness
result = json.loads((folder / "result.json").read_text())
invocation = json.loads((folder / "invocation.json").read_text())
assert result["exit_code"] == 0 and result["all_post_checks_pass"]
output_hashes = {Path(path).name: entry["sha256"] for path, entry in result["outputs"].items()}
for name in ("native-game.exe", "native-game.pe", "native-game.map"):
    assert sha256(folder / name) == output_hashes[name], name
selected = {row["source"]: row for row in invocation["objects"]}
objects_by_name = {Path(row["object"]).name: row for row in selected.values()}
# Compare full function instructions and semantic relocations, not whole-object
# hashes: assert strings embed the compiler checkout path in three functions.
import runpy
reader = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r148-boot-presentation-linked\verify_linked.py")
assert sha256(reader) == "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba"
h = runpy.run_path(str(reader))
path_exceptions = {
    "?UpdateOutputInterfaces@RaceCarEntityModule": {0x134:"BrnRaceCarEntityModule.h", 0x138:"BrnRaceCarEntityModule.h"},
    "?UpdateStreaming@RaceCarEntityModule": {0x7c:"BrnRaceCarEntityModule.h", 0x88:"BrnRaceCarEntityModule.h"},
    "?PreSceneUpdate@RaceCarEntityModule": {0x20c:"BrnRaceCarEntityModuleIO.h", 0x214:"BrnRaceCarEntityModuleIO.h", 0x230:"BrnRaceCarEntityModule.h", 0x238:"BrnRaceCarEntityModule.h"},
}
object_proofs = {}
for row in binding["results"]:
    source = row["source"]
    if not source.endswith(("/BrnRaceCarEntityModule.cpp", "/BrnRCEntityActiveRaceCarOutputInterface.cpp")):
        continue
    linked_obj = selected[source]
    assert sha256(Path(linked_obj["object"])) == linked_obj["sha256"].lower()
    blob = subprocess.check_output(["git", "rev-parse", args.expected_head + ":" + source], cwd=root, text=True).strip()
    assert blob == row["git_blob"]
    source_record = [x for x in seal["sources"] if x["path"].replace("\\", "/").lower().endswith("/" + source.lower())]
    assert len(source_record) == 1 and sha256(Path(source_record[0]["path"])) == source_record[0]["sha256"].lower() == row["source_sha256"]
    current = h["Coff"](Path(linked_obj["object"]).read_bytes())
    frozen = h["Coff"](Path(row["artifacts"]["object"]["path"]).read_bytes())
    prefixes = (["?UpdateOutputInterfaces@RaceCarEntityModule", "?UpdateStreaming@RaceCarEntityModule", "?OnRaceCarResourcesLoaded@RaceCarEntityModule", "?PreSceneUpdate@RaceCarEntityModule", "?PostPhysicsUpdate@RaceCarEntityModule", "?SendGameEvents@RaceCarEntityModule"] if "SharedIO" not in source else ["?SetWorldMap2D@RCEntityActiveRaceCarOutputInterface"])
    method_proofs = {}
    for prefix in prefixes:
        symbol, code, refs = current.function(prefix)
        old_symbol, old_code, old_refs = frozen.function(prefix)
        assert symbol.name == old_symbol.name and code == old_code
        actual = h["ref_signature"](symbol, refs)
        expected = h["ref_signature"](old_symbol, old_refs)
        assert actual.keys() == expected.keys()
        allowed = path_exceptions.get(prefix, {})
        differences = {off for off in actual if actual[off] != expected[off]}
        assert differences == set(allowed), (prefix, differences, allowed)
        for off, filename in allowed.items():
            for coff, ref, rootname in ((current, refs[off], "wave4-intake"), (frozen, old_refs[off], "debug_format_luna")):
                kind, target = ref
                assert kind in (0x10, 0x11) and kind == refs[off][0] == old_refs[off][0]
                _, data, _, _ = coff.section(target.section)
                literal = data[target.value:data.index(b"\0", target.value)+1].replace(b"\\", b"/")
                assert literal.endswith(("/" + rootname + "/src/GameSource/World/EntityModules/RaceCarEntityModule/" + filename).encode() + b"\0"), literal
        method_proofs[prefix] = {"instruction_sha256": hashlib.sha256(code).hexdigest(), "relocation_count": len(refs), "assert_path_relocation_offsets": [hex(x) for x in sorted(allowed)]}
    object_proofs[source] = {"git_blob":blob,"source_sha256":row["source_sha256"],"selected_object_sha256":linked_obj["sha256"],"targeted_object_sha256":row["artifacts"]["object"]["sha256"],"functions":method_proofs}
assert len(object_proofs) == 2
assert sha256(Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r152-replay-linked\RECEIPT.json")) == "038c0e458169c3af2d4668a15f396afc049065830d385e333ba1f58aa1c112d5"
assert len(selected) == len(invocation["objects"])
assert len(objects_by_name) == len(selected)

mapping = {}
for line in (folder / "native-game.map").read_text(errors="replace").splitlines():
    match = re.match(r"\s+\w+:\w+\s+(\S+)\s+([0-9A-Fa-f]{8})\s+f\b.*?\s+(\S+\.obj)\s*$", line)
    if match:
        symbol, address, owner = match.groups()
        mapping[symbol] = (int(address, 16), owner)

pe = (folder / "native-game.pe").read_bytes()
header = struct.unpack_from("<I", pe, 0x3C)[0]
assert pe[header:header + 4] == b"PE\0\0"
machine, count, _, _, _, optional, _ = struct.unpack_from("<HHIIIHH", pe, header + 4)
base = struct.unpack_from("<I", pe, header + 52)[0]
assert machine == 0x1F2 and base == 0x82000000
sections = [struct.unpack_from("<8sIIII", pe, header + 24 + optional + 40 * i)[2:]
            for i in range(count)]


def mapped(prefix):
    if prefix in mapping:
        return prefix
    matches = [name for name in mapping if name.startswith(prefix)]
    assert len(matches) == 1, (prefix, matches)
    return matches[0]


def pe_words(va, count):
    rva = va - base
    start, size, raw = next(row for row in sections if row[0] <= rva < row[0] + row[1])
    assert rva + count * 4 <= start + size
    offset = raw + rva - start
    return unpack_words(pe[offset:offset + count * 4])


functions = {}


def verify(prefix, fixed=None, calls=()):
    name = mapped(prefix)
    va, owner = mapping[name]
    row = objects_by_name[owner]
    obj = Path(row["object"])
    assert sha256(obj) == row["sha256"], obj
    native, rels = coff_function(obj, name)
    linked = pe_words(va, len(native))
    masks = {2: 0xFFFFFFFF, 6: 0x03FFFFFC, 0x10: 0xFFFF, 0x11: 0xFFFF, 0x12: 0}
    masked = {}
    for offset, kind in rels:
        masked[offset] = masked.get(offset, 0) | masks[kind]
    for i, (a, b) in enumerate(zip(native, linked)):
        offset = i * 4
        assert not ((a ^ b) & ~masked.get(offset, 0)), (
            name, hex(offset), hex(a), hex(b), masked.get(offset))
    fixed = fixed or {}
    for offset, word in fixed.items():
        assert offset not in masked and native[offset // 4] == linked[offset // 4] == word, (name, offset)
    actual_branches = {offset: branch_target(va + offset, linked[offset // 4])
                       for offset, kind in rels if kind == 6}
    expected = [mapping[mapped(call)][0] for call in calls]
    assert_subsequence(list(actual_branches.values()), expected)
    functions[name] = {"va": hex(va), "bytes": len(native) * 4, "object_sha256": row["sha256"],
                       "fixed_words": {hex(k): hex(v) for k, v in fixed.items()},
                       "checked_calls": {call: hex(target) for call, target in zip(calls, expected)}}
    return linked, actual_branches


if not args.consumers_only:
    official = Path(selected[RACECAR]["object"])
    independent = HERE / "candidate/BrnRaceCarEntityModule-7a7e1565.obj"
    assert sha256(independent) == receipt["native"]["candidate"]["object_sha256"][RACECAR]
    for prefix in ("?UpdateStreaming@RaceCarEntityModule", "?OnRaceCarResourcesLoaded@RaceCarEntityModule"):
        name = mapped(prefix)
        assert coff_function(official, name) == coff_function(independent, name), name
    verify("?UpdateStreaming@RaceCarEntityModule", {
        0xCC: 0x897B0740, 0xD0: 0x2B0B0001, 0xEC: 0x419A006C, 0x104: 0x38800000,
        0x118: 0x409A0040, 0x13C: 0x7C651B78, 0x140: 0x7FE7FB78, 0x144: 0x7FC6F378,
        0x148: 0x7FA4EB78, 0x158: 0x7E95A378, 0x20C: 0x9AA32861, 0x214: 0x419A002C,
        0x21C: 0x396B8348, 0x230: 0x9A8B0000, 0x238: 0x614A8371, 0x23C: 0x7D7C51AE,
    }, ("?IsRaceCarLoaded@RaceCarStreamer", "?IsPlayer@ActiveRaceCar", "?IsOnRaceStartState@ActiveRaceCar",
        "?GetSceneInputInterface@OutputBuffer_PreScene@RaceCarEntityModuleIO@BrnWorld@@QA",
        "?GetRaceCarAIInterface@OutputBuffer_PreScene@RaceCarEntityModuleIO@BrnWorld@@QA",
        "?GetVehicleInputInterface@OutputBuffer_PreScene@RaceCarEntityModuleIO@BrnWorld@@QA",
        "?OnRaceCarResourcesLoaded@RaceCarEntityModule"))
    verify("?OnRaceCarResourcesLoaded@RaceCarEntityModule", {
        0x138: 0x616A8774, 0x168: 0x616B8770, 0x170: 0x61298760, 0x188: 0x17ED009C,
        0x1F4: 0x997C0775, 0x1F8: 0x997C0776,
    }, ("?GetGraphicsResource@RaceCarStreamer", "?GetPhysicsResource@RaceCarStreamer", "?OnResourcesLoaded@ActiveRaceCar",
        "?SetupCarColour@RaceCarEntityModule", "?PlaceRaceCarOnLoad@RaceCarEntityModule"))

# Exactly documented assertion source-line shifts in the rebuilt owner, all other
# instruction words and relocation positions must equal the independent R141 object.
line_shifts = {
    "?PreSceneUpdate@RaceCarEntityModule": {0x38:(0x38a012c0,0x38a012c3), 0x5c:(0x38a012c1,0x38a012c4), 0x2d8:(0x38a01363,0x38a01366)},
    "?PostPhysicsUpdate@RaceCarEntityModule": {0x3c:(0x38a01888,0x38a0188b), 0x60:(0x38a01889,0x38a0188c)},
    "?SendGameEvents@RaceCarEntityModule": {},
}
for prefix, allowed in line_shifts.items():
    name = mapped(prefix)
    before, old_rels = coff_function(independent, name)
    after, new_rels = coff_function(official, name)
    assert len(before) == len(after) and old_rels == new_rels
    diffs = {i*4:(a,b) for i,(a,b) in enumerate(zip(before,after)) if a != b}
    assert diffs == allowed, (name, diffs)

# Composed PPC map publish: two calls with preserved argument registers and
# module+0x18300 source, then the global-pointer assertion.
setter = "?SetWorldMap2D@RCEntityActiveRaceCarOutputInterface@RaceCarEntityModuleIO@BrnWorld@@"
caller, branches = verify("?UpdateOutputInterfaces@RaceCarEntityModule", {
    0x1c:0x7c7b1b78, 0x20:0x7c9d2378, 0x24:0x3ffb0002,
    0x28:0x7cba2b78, 0x2c:0x3bff8300, 0x30:0x7fa3eb78,
    0x38:0x7fe4fb78, 0x40:0x7cde3378, 0x4c:0x7fe4fb78,
    0x50:0x7fc3f378, 0x5c:0x2b1a0000,
}, (setter, setter, "?BeginAssert@Assert@CgsDev"))
assert branches[0x48] == branches[0x54] == mapping[mapped(setter)][0]
assert branches[0x6c] == mapping[mapped("?BeginAssert@Assert@CgsDev")][0]
setter_words, setter_branches = verify(setter, {0x00:0x7c8b2378, 0x04:0x394328b0, 0x08:0x39200006,
    0x0c:0x7d2903a6, 0x10:0xe92b0000, 0x14:0x396b0008,
    0x18:0xf92a0000, 0x1c:0x394a0008, 0x20:0x4200fff0, 0x24:0x4e800020})
assert len(setter_words) == 10 and not setter_branches

verify("?PreSceneUpdate@RaceCarEntityModule", calls=("?UpdateStreaming@RaceCarEntityModule",))
verify("?PostPhysicsUpdate@RaceCarEntityModule", calls=("?SendGameEvents@RaceCarEntityModule",))
game_event_add = "?AddEvent@?$VariableEventQueue@$0GAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z"
game_event_append = "??$Append@$0GAA@$0BA@@?$VariableEventQueue@$0GAA@$0BA@@CgsModule@@QAA_NABV01@@Z"
send_words, send_branches = verify("?SendGameEvents@RaceCarEntityModule", calls=(
    "?GetGameEventQueue@OutputBuffer_PostPhysics@RaceCarEntityModuleIO@BrnWorld@@QA", game_event_add))
assert_subsequence(send_words, [0x3BFF8371, 0x897F0000, 0x39600001, 0x38A00009, 0x38C00010, 0x997F0000])
assert send_words[0x84 // 4:0x8C // 4] == [0x38A00009, 0x38C00010]
assert send_branches[0x8C] == mapping[game_event_add][0]
verify("?BridgeRaceCarModuleToWorldModule_PreScene@WorldModule@BrnWorld", calls=(
    "?GetActiveRaceCarOutputInterface@OutputBuffer_PreScene@RaceCarEntityModuleIO@BrnWorld@@QB",
    "?SetActiveRaceCarInterface@InputBuffer_PreScene@WorldEntityIO"))
copy_words, _ = verify("?SetActiveRaceCarInterface@InputBuffer_PreScene@WorldEntityIO", calls=("memcpy",))
assert any(word >> 26 == 14 and word & 0xFFFF == 0x28F0 for word in copy_words)
verify("?BridgeRaceCarEntityInfoToOutput_PostPhysics@WorldModule@@", calls=(
    "?GetGameEventQueue@OutputBuffer_PostPhysics@RaceCarEntityModuleIO@BrnWorld@@QB",
    "?GetGameEventQueue@UpdateOutputBuffer@BrnWorldIO@@QA", game_event_append))
verify("?BridgeWorldToGameState@BrnGameModule", calls=("?GetGameEventQueue@UpdateOutputBuffer@BrnWorldIO@@QB",
    "?GetGameEventQueue@PostWorldInputBuffer@GameStateModuleIO@BrnGameState@@QA", game_event_append))
verify("?PostWorldUpdate@GameStateModule", calls=("?GetGameEventQueue@PostWorldInputBuffer@GameStateModuleIO@BrnGameState@@QB",
    game_event_append))
verify("?PreWorldUpdate@GameStateModule", calls=(game_event_append,
    "?GetGameEventQueue@PreWorldInputBuffer@GameStateModuleIO@BrnGameState@@QB", "?ProcessGameEvents@GameStateModule"))
verify("?ProcessGameEvents@GameStateModule", calls=("?ProcessGameEvents_Group1@GameStateModule",))
verify("?ProcessGameEvents_Group1@GameStateModule", calls=("?ProcessStreamingCompleteEvent@GameStateModule",))
verify("?ProcessStreamingCompleteEvent@GameStateModule", calls=("?FinishStreaming@GameStateModule",))
# Carry every R152 function and canonical relocation against its actual R153
# selected owner. This tolerates no body or relocation differences, even when
# a transitive rebuild changes the whole-object hash.
prior = json.loads(Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r152-replay-linked\RECEIPT.json").read_text())
old_inv = json.loads(Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r152-link\invocation.json").read_text())
old_by_sha = {x["sha256"].lower(): x for x in old_inv["objects"]}
assert set(functions) == set(prior["functions"]) and len(functions) == 16
carry = {}
for name, now in functions.items():
    old = old_by_sha[prior["functions"][name]["object_sha256"].lower()]
    va, owner = mapping[name]
    current = objects_by_name[owner]
    assert old["source"] == current["source"], name
    old_path, new_path = Path(old["object"]), Path(current["object"])
    assert sha256(old_path) == old["sha256"].lower() and sha256(new_path) == current["sha256"].lower()
    a, b = h["Coff"](old_path.read_bytes()), h["Coff"](new_path.read_bytes())
    sa, ca, ra = a.function(name)
    sb, cb, rb = b.function(name)
    assert ca == cb and h["ref_signature"](sa, ra) == h["ref_signature"](sb, rb), name
    oldblob = subprocess.check_output(["git", "rev-parse", "5159415953b6b1c2e9cf9c82bfa67dfa5d5e2ed9:" + old["source"]], cwd=root, text=True).strip()
    newblob = subprocess.check_output(["git", "rev-parse", args.expected_head + ":" + old["source"]], cwd=root, text=True).strip()
    assert oldblob == newblob, old["source"]
    carry[name] = {"source": old["source"], "git_blob": newblob, "r152_object_sha256":old["sha256"], "r153_object_sha256":current["sha256"], "instruction_sha256":hashlib.sha256(cb).hexdigest(), "canonical_relocations":len(rb)}
proof = {"scope": "R153 strict linked replay map and 14 readiness functions",
         "r152_receipt_sha256": sha256(Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r152-replay-linked\RECEIPT.json")),
         "targeted_binding_sha256": sha256(binding_path), "frozen_tree": tree, "rebuilt_owner_comparison": object_proofs, "r152_function_carry": carry,
         "link_context": link_context,
         "link_invocation_sha256": sha256(folder / "invocation.json"),
         "readiness_receipt_sha256": sha256(HERE / "RECEIPT.json"),
         "prior_r148_linked_receipt_sha256": TARGETS["previous_linked_receipts"]["R141_RACE_CAR_READY_LINKED.json"]["sha256"],
         "outputs_sha256": output_hashes, "functions": functions}
if args.report:
    report_path = args.report.resolve()
    assert report_path.parent.is_dir(), report_path.parent
    assert not report_path.exists(), f"refusing to overwrite existing report: {report_path}"
    report_path.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
print(json.dumps(proof, indent=2))
