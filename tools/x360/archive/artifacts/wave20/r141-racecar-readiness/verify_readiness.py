import hashlib
import json
import re
import subprocess
from pathlib import Path


root = Path(__file__).resolve().parent
base = "bdbd812370b68b9d3fd1b3b4f27f6fa3d7632704"
source = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def procedure(asm, name):
    match = re.search(rf"^({re.escape(name)}\S+) PROC NEAR[^\n]*\n(.*?)^\1 ENDP",
                      asm, re.M | re.S)
    assert match, name
    return match[2]


def assert_in_order(text, markers):
    position = 0
    for marker in markers:
        match = re.search(marker, text[position:], re.M)
        assert match, marker
        position += match.end()


receipt = {"base": base, "runtime": "not run", "native": {}, "evidence": {}}
listings = {}
for variant, count in (("baseline", 3), ("candidate", 6)):
    path = root / variant / "compile-results.json"
    report = json.loads(path.read_text())
    assert report["passed"] == count and report["failed"] == 0
    assert all(item["machine"] == 0x1F2 for item in report["results"])
    receipt["native"][variant] = {
        "compile_report_sha256": sha256(path),
        "object_sha256": {item["source"]: sha256(Path(item["object"])) for item in report["results"]},
    }
    listings[variant] = {
        Path(item["source"]).stem: Path(item["assembly"]).read_text(encoding="utf-8", errors="replace")
        for item in report["results"]
    }

before = procedure(listings["baseline"]["BrnRaceCarEntityModule"], "?UpdateStreaming@RaceCarEntityModule")
racecar = listings["candidate"]["BrnRaceCarEntityModule"]
after = procedure(racecar, "?UpdateStreaming@RaceCarEntityModule")
assert "bl           ?IsRaceCarLoaded@RaceCarStreamer" not in before
assert "bl           ?OnRaceCarResourcesLoaded@RaceCarEntityModule" not in before
assert_in_order(after, [
    r"000cc\s+897b0740\s+lbz\s+r11,740h\(r27\)",
    r"000d0\s+2b0b0001\s+cmplwi\s+cr6,r11,1",
    r"000e0\s+48000001\s+bl\s+\?IsRaceCarLoaded@RaceCarStreamer",
    r"000ec\s+419a006c\s+beq\s+cr6,\$LN11@UpdateStre",
    r"000f4\s+48000001\s+bl\s+\?IsPlayer@ActiveRaceCar",
    r"00100\s+409a001c\s+bne\s+cr6,\$LN12@UpdateStre",
    r"00104\s+38800000\s+li\s+r4,0",
    r"0010c\s+48000001\s+bl\s+\?IsOnRaceStartState@ActiveRaceCar",
    r"00118\s+409a0040\s+bne\s+cr6,\$LN11@UpdateStre",
    r"bl\s+\?GetSceneInputInterface@OutputBuffer_PreScene",
    r"bl\s+\?GetRaceCarAIInterface@OutputBuffer_PreScene",
    r"bl\s+\?GetVehicleInputInterface@OutputBuffer_PreScene",
    r"0013c\s+7c651b78\s+mr\s+r5,r3",
    r"00140\s+7fe7fb78\s+mr\s+r7,r31",
    r"00144\s+7fc6f378\s+mr\s+r6,r30",
    r"00148\s+7fa4eb78\s+mr\s+r4,r29",
    r"00150\s+48000001\s+bl\s+\?OnRaceCarResourcesLoaded@RaceCarEntityModule",
    r"^\$LN11@UpdateStre:",
    r"00158\s+7e95a378\s+mr\s+r21,r20",
    r"0020c\s+9aa32861\s+stb\s+r21,2861h\(r3\)",
    r"00214\s+419a002c\s+beq\s+cr6,\$LN1@UpdateStre",
    r"0021c\s+396b8348",
    r"00230\s+9a8b0000",
    r"00238\s+614a8371",
    r"0023c\s+7d7c51ae",
])
assert re.search(r"li\s+r20,0\s*$", after, re.M)
loaded = procedure(racecar, "?OnRaceCarResourcesLoaded@RaceCarEntityModule")
assert_in_order(loaded, [r"ori\s+r10,r11,34676", r"ori\s+r11,r11,34672", r"ori\s+r9,r9,34656",
                         r"vmulfp128", r"bl\s+\?GetGraphicsResource", r"bl\s+\?GetPhysicsResource",
                         r"bl\s+\?OnResourcesLoaded", r"bl\s+\?SetupCarColour", r"bl\s+\?PlaceRaceCarOnLoad",
                         r"stb\s+r11,775h\(r28\)", r"stb\s+r11,776h\(r28\)"])
assert re.search(r"bl\s+\?UpdateStreaming@RaceCarEntityModule", procedure(racecar, "?PreSceneUpdate@RaceCarEntityModule"))
assert re.search(r"bl\s+\?SendGameEvents@RaceCarEntityModule", procedure(racecar, "?PostPhysicsUpdate@RaceCarEntityModule"))
send = procedure(racecar, "?SendGameEvents@RaceCarEntityModule")
assert_in_order(send, [r"addi\s+r31,r31,-31887", r"lbz\s+r11,0\(r31\)", r"li\s+r11,1",
                       r"li\s+r5,9", r"li\s+r6,16", r"bl\s+\?AddEvent", r"stb\s+r11,0\(r31\)"])
candidate = listings["candidate"]
for unit, markers in {
    "WorldBridgeRaceCarToWorldModule": [r"bl\s+\?GetActiveRaceCarOutputInterface@OutputBuffer_PreScene",
                                       r"bl\s+\?SetActiveRaceCarInterface@InputBuffer_PreScene"],
    "BrnRCEntityActiveRaceCarOutputInterface": [r"lbz\s+r8,2861h\(r17\)", r"stb\s+r8,2861h\(r22\)"],
    "WorldBridgeEntityModulesToOutput": [r"bl\s+\?GetGameEventQueue@OutputBuffer_PostPhysics@RaceCarEntityModuleIO"],
    "GameBridgeWorldToX": [r"bl\s+\?GetGameEventQueue@UpdateOutputBuffer@BrnWorldIO"],
    "GameStateModule_wW_01": [r"bl\s+\?GetGameEventQueue@PostWorldInputBuffer"],
}.items():
    assert_in_order(candidate[unit], markers)

receipt["evidence"] = {
    "ARTIST": {
        "event8": "823A2758..823A2784",
        "wait_countdown_action192": "82390134..82390140, 823A58B8..823A5900",
        "active_car_resource_and_start_line_gate": "822FF098..822FF144",
        "readiness_and_completion_latch": "822FF204..822FF240",
        "finish_resources_debug_velocity_and_arguments": "822FED24..822FEDC4",
        "pre_scene_caller": "8230D928",
        "streaming_complete_publisher": "822F6C24..822F6C98",
    },
    "candidate": {
        "UpdateStreaming": "COFF +0xCC state; +0xE0, +0xF4, +0x10C predicates; +0x150 resource completion",
        "readiness": "both hold branches target +0x158, setting r21=0; +0x20C writes r21 to output+0x2861; +0x214 gates completion",
        "completion": "UpdateStreaming +0x230..0x23C clears module+0x18348 and sets +0x18371; SendGameEvents +0x44..0x94 reads +0x18371, posts event 9 module 1 (16 bytes) and clears it",
        "pre_scene_bit_consumer": "WorldBridgeRaceCarToWorldModule forwards the pre-scene active-car output into WorldEntityIO; native interface assignment copies +0x2861",
        "module_reply_consumer": "PostPhysicsUpdate calls SendGameEvents; world post-physics bridge appends race-car event queue; Game bridge appends the world queue into GameState PostWorld input; GameState carries that queue into the next PreWorld event walk",
    },
}
head = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
if head != base:
    assert subprocess.check_output(["git", "rev-parse", "HEAD^"], text=True).strip() == base
    paths = subprocess.check_output(["git", "diff-tree", "--no-commit-id", "--name-only", "-r", head], text=True).splitlines()
    assert set(paths) == {source + ".h", source + ".cpp"}
    receipt["candidate_commit"] = head
receipt["candidate_source_sha256"] = {source + ext: sha256(Path(source + ext)) for ext in (".h", ".cpp")}
(root / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print("Verified native active-car readiness, World bit transfer and module-1 completion reply")
