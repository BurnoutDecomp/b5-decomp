"""crash parity FX-TRAFFICLIGHTS (2026-09-25): the traffic's game-action dispatch is complete.

TrafficEntityModule::HandleExternalRequests @0x8274B660 ran eleven of the console's twenty-one action ids. Eight arms
and the post-loop tail were one named gate, and three of their callees had no body:
  13  EMPTY_TRAFFIC_POOL        (0x8274BD34) the IDLE -> EMPTYING / EMPTY -> FILLING half of the pool handshake
  30  STOP_MODE_INTRO           (0x8274BC84) an event that clears the traffic sweeps its start grid (30 m / 10 m, parked
                                             cars too); offline, the start-line protection ends
  73  CAR_SELECT_TRANSITION_IN  (0x8274C018) the junkyard car select widens the sim box (395 m, 64 cars within 400 m)
  75  CAR_SELECT_READY          (0x8274C000) the ONLINE car select (type 2) hides the traffic: HideAllTraffic @0x8273F418
  77  CAR_SELECT_EXIT           (0x8274C050) online: UnhideAllTraffic @0x8274A500; offline: clear 150 m around the exit
                                             spawn and restore Construct's box
  110 KILLZONE                  (0x8274BE64) FireKillZone @0x827343B8 per id (TrafficData::FindKillZone @0x827570A0)
  192 WAIT_FOR_STREAMING        (0x8274BC30) latch while starting up / tearing down, else answer StreamingCompleteEvent
                                             (module 0); PostPhysicsUpdate's two latched answers (0x8274E778, 0x8274EC94)
  244 HUD_MESSAGE_DIST_TO_FINISH(0x8274BF74) inside 1505 m of the finish the event density halves
  tail (0x8274C0D0) leaving Picture Paradise clears 90 m around the player (camera >= 100 m away, or player in front)

NUMERIC (FxTrafficLightsRequests.cpp): the PRODUCTION HandleExternalRequests, HideAllTraffic, UnhideAllTraffic,
FireKillZone and PostPhysicsUpdate's two latched-answer blocks, compiled as members of ReqFixture (the module's members
with their real types; recording doubles for KillAllTrafficInCylinder / RemoveVehicle / the other arms' callees),
with the production TrafficData::FindKillZone / GetKillZoneRegions (BrnTrafficData.cpp) and
HullRuntime::GetFirstParamInSection (BrnTrafficHullRuntime.cpp). Actions go through a real
VariableEventQueue<13312,16>, the answers into a real VariableEventQueue<1536,16>.
WIRING: the eight case labels, the tail after the queue loop, the retired gates, the emits, the enumerator, the
declaration.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtrafficlights_requests.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
DATA_CPP = "src/SharedClasses/Traffic/BrnTrafficData.cpp"
DATA_H = "src/SharedClasses/Traffic/BrnTrafficDataResourceType.h"
RUNTIME_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.cpp"
EVENTS_H = "src/GameSource/GameState/BrnGameEvents.h"
MODULE_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
FIXTURE = "ReqFixture"
NUMERIC_CHECKS = 62

MEMBER_BODIES = [
    "void TrafficEntityModule::HandleExternalRequests(",
    "void TrafficEntityModule::HideAllTraffic()",
    "void TrafficEntityModule::UnhideAllTraffic()",
    "void TrafficEntityModule::FireKillZone(",
]
LATCH_IF = "if (mbWaitingForStreaming && mStreamer.AreAllAssetsLoaded())"
LATCH_HEAD = "// 0x8274E744..0x8274E794"
LATCH_STARTUP = "// 0x8274EC74..0x8274ECB0"


def latched_answer(post, marker):
    at = post.find(marker)
    if at < 0:
        raise ValueError("PostPhysicsUpdate has no latched answer at " + marker)
    return definition(post[at:], LATCH_IF)


def numeric(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    data = tree.read(DATA_CPP).replace("\r\n", "\n")
    runtime = tree.read(RUNTIME_CPP).replace("\r\n", "\n")
    try:
        parts = [definition(module, signature).replace("TrafficEntityModule::", FIXTURE + "::", 1)
                 for signature in MEMBER_BODIES]
        post = definition(module, "void TrafficEntityModule::PostPhysicsUpdate(")
        parts.append("void " + FIXTURE + "::LatchedAnswerHead(FakeOutput* lpOutput)\n{\n"
                     + latched_answer(post, LATCH_HEAD) + "\n}")
        parts.append("void " + FIXTURE + "::LatchedAnswerStartup(FakeOutput* lpOutput)\n{\n"
                     + latched_answer(post, LATCH_STARTUP) + "\n}")
        others = "\n".join([
            definition(data, "const KillZone* TrafficData::FindKillZone("),
            definition(data, "const KillZoneRegion* TrafficData::GetKillZoneRegions("),
            definition(runtime, "u16 HullRuntime::GetFirstParamInSection("),
        ])
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    text = "\n".join(parts)
    text = text.replace("BrnTrafficIO::InputBuffer_PostPhysics::GameActionQueueStorage",
                        "FakeInput::GameActionQueueStorage")
    text = re.sub(r"const\s+BrnTrafficIO::InputBuffer_PostPhysics\*\s*lpInput", "const FakeInput* lpInput", text)
    text = re.sub(r"BrnTrafficIO::OutputBuffer_PostPhysics\*\s*lpOutput", "FakeOutput* lpOutput", text)
    shadow = {DATA_H: tree.read(DATA_H), EVENTS_H: tree.read(EVENTS_H), MODULE_H: tree.read(MODULE_H)}
    return compile_and_run(Path(__file__).with_name("FxTrafficLightsRequests.cpp"), "requests_bodies.inc", text,
                           "FxTrafficLightsRequests", shadow=shadow, extra_sources=[STRSTREAM_CPP],
                           extra_files={"requests_other_bodies.inc": others})


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    handler = body(module, "void TrafficEntityModule::HandleExternalRequests(")
    post = body(module, "void TrafficEntityModule::PostPhysicsUpdate(")
    events = code_only(tree.read(EVENTS_H).replace("\r\n", "\n"))
    data_h = code_only(tree.read(DATA_H).replace("\r\n", "\n"))
    labels = ["case KI_ACTION_EMPTY_TRAFFIC_POOL:", "E_ACTION_STOP_MODE_INTRO:", "case KI_ACTION_CAR_SELECT_TRANSITION_IN:",
              "E_ACTION_CAR_SELECT_READY:", "E_ACTION_CAR_SELECT_FINISHED:", "case KI_ACTION_KILLZONE:",
              "case KI_ACTION_WAIT_FOR_STREAMING:", "E_ACTION_HUD_MESSAGE_DIST_TO_FINISH:"]
    loop_end = handler.find("GetNextEvent(")
    tail = handler.find("E_FLAG_IS_PICTURE_PARADISE")
    return [
        ("HandleExternalRequests dispatches actions 13, 30, 73, 75, 77, 110, 192 and 244 (jump-table slots 0, 17, 60, "
         "62, 64, 97, 179, 231)", all(label in handler for label in labels)),
        ("the Picture Paradise tail runs after the queue loop (0x8274C0D0), not inside it",
         0 <= loop_end < tail),
        ("the gate that listed the unwired arms is retired",
         "Arms 13, 30, 73, 75, 77, 110, 192, 244 and the" not in module),
        ("PostPhysicsUpdate answers a latched wait at both sites (0x8274E778, 0x8274EC94) with module 0, and its gate "
         "is retired",
         post.count("E_MODULE_TRAFFIC_ENTITY") == 2
         and "StreamingCompleteEvent(E_MODULE_TRAFFIC_ENTITY) emit -- TWO" not in module),
        ("BrnGameEvents.h carries the DWARF enumerators E_MODULE_TRAFFIC_ENTITY = 0 and E_MODULE_GUI_SCREEN = 3",
         re.search(r"E_MODULE_TRAFFIC_ENTITY\s*=\s*0\b", events) is not None
         and re.search(r"E_MODULE_GUI_SCREEN\s*=\s*3\b", events) is not None),
        ("TrafficData declares FindKillZone (DWARF BrnTrafficData.h:80)", "FindKillZone(KillZoneId" in data_h),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtrafficlights_requests", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
