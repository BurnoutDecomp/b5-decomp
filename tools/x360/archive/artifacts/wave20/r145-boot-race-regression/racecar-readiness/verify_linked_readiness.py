"""Read a frozen strict link and its selected PPC COFF files. Do not write into it."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


HERE = Path(__file__).resolve().parent
REGRESSION_ROOT = HERE.parent
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
parser.add_argument("--expected-head", default=DEFAULT_HEAD)
parser.add_argument("--report", type=Path, help="write the successful linked receipt to a new file (never overwrites)")
args = parser.parse_args()
folder = args.link.resolve()
link_context = verify_link_context(folder, args.expected_head)
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
proof = {"scope": "consumers only" if args.consumers_only else "strict linked readiness",
         "link_context": link_context,
         "link_invocation_sha256": sha256(folder / "invocation.json"),
         "readiness_receipt_sha256": sha256(HERE / "RECEIPT.json"),
         "prior_r144_linked_receipt_sha256": TARGETS["previous_linked_receipts"]["R141_RACE_CAR_READY_LINKED.json"]["sha256"],
         "outputs_sha256": output_hashes, "functions": functions}
if args.report:
    report_path = args.report.resolve()
    assert report_path.parent.is_dir(), report_path.parent
    assert not report_path.exists(), f"refusing to overwrite existing report: {report_path}"
    report_path.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
print(json.dumps(proof, indent=2))
