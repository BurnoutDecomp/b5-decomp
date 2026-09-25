"""crash parity FX-NETCRASH (2026-09-25): an event start sets up the lights at its start line, and arm 34.

TrafficEntityModule::UpdateEventStarts @0x82743B80 (called by PostPhysicsUpdate every frame, 0x8274EE90) had no
body, so mbNeedToSetUpLightsForEventStart -- stored by HandlePrepareForModeAction for every mode that clears the
traffic -- was never consumed: the start line's stop lines never went red and its lights were never changed
(TrafficLightManager::ChangeLightState @0x827518E0 had no body either), and online the participants' grid slots
were never cleared of traffic. Its absence also kept HandleExternalRequests arm 34 (E_ACTION_START_PLAYING_MODE,
0x8274BDF0: SetCountdownValue(0) + the .cpp 5995 tripwire on that flag) unwired.

NUMERIC: the production UpdateEventStarts (+ its two file constants), ChangeLightState + GetLightState (+
KF_AMBER_TIME) and HullRuntime::SetStoplineRed, compiled into FxNetcrashEventStarts.cpp against the real types.
WIRING: the PostPhysicsUpdate call before GenerateNetworkUpdateEvents; arm 34.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_event_starts.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
LIGHTS_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.cpp"
LIGHTS_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
RUNTIME_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.cpp"
JUNCTION_H = "src/SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"
NUMERIC_CHECKS = 18


def constant(source, name):
    match = re.search(r"const f32 " + name + r"\s*=[^;]+;", source)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match[0]


def numeric(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    lights = tree.read(LIGHTS_CPP).replace("\r\n", "\n")
    runtime = tree.read(RUNTIME_CPP).replace("\r\n", "\n")
    try:
        # TU 1 (the fixture): UpdateEventStarts + SetStoplineRed, over Hull / JunctionLogicBox / HullRuntime.
        parts = ["namespace {",
                 constant(module, "KF_EVENT_START_GRID_CLEAR_RADIUS"),
                 constant(module, "KF_EVENT_START_GRID_CLEAR_HEIGHT"),
                 "}",
                 definition(module, "void TrafficEntityModule::UpdateEventStarts()"),
                 definition(runtime, "void HullRuntime::SetStoplineRed(u32 luStopline, bool lbRed)")]
        # TU 2: the light manager's bodies with the enum home their .cpp uses (BrnTrafficLightCollection.h). The
        # Hull headers bring the other ETrafficLightState home (BrnTrafficSharedConstants.h, the BL-1 conflict),
        # so the two cannot share a TU -- exactly as in the game, where they live in different .cpp files.
        lights_tu = "\n".join([
            '#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"',
            '#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"',
            '#include "GameShared/GameClasses/Core/CgsAssert.h"',
            "namespace BrnTraffic {",
            "namespace {",
            constant(lights, "KF_AMBER_TIME"),
            "}",
            definition(lights, "TrafficLightState* TrafficLightManager::GetLightState(u32 luInstance)"),
            definition(lights, "void TrafficLightManager::Construct()"),
            definition(lights, "void TrafficLightManager::ChangeLightState("),
            "}"]) + "\n"
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    shadow = {LIGHTS_H: tree.read(LIGHTS_H), JUNCTION_H: tree.read(JUNCTION_H)}
    with tempfile.TemporaryDirectory(prefix="brn_event_starts_") as directory:
        lights_path = Path(directory) / "EventStartsLights.cpp"
        lights_path.write_text(lights_tu, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("FxNetcrashEventStarts.cpp"), "event_starts_bodies.inc",
                               "\n".join(parts), "FxNetcrashEventStarts", shadow=shadow,
                               extra_sources=[STRSTREAM_CPP, lights_path])


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    post = body(module, "void TrafficEntityModule::PostPhysicsUpdate(")
    handler = body(module, "void TrafficEntityModule::HandleExternalRequests(")
    at = handler.find("E_ACTION_START_PLAYING_MODE:")
    arm = handler[at:handler.find("break;", at)] if at >= 0 else ""
    return [
        ("PostPhysicsUpdate calls UpdateEventStarts() right before GenerateNetworkUpdateEvents (0x8274EE8C..0x8274EEA0)",
         re.search(r"UpdateEventStarts\(\);\s*GenerateNetworkUpdateEvents\(lpInput,\s*lpOutput\);", post) is not None),
        ("arm 34 (E_ACTION_START_PLAYING_MODE, 0x8274BDF0): SetCountdownValue(0), then the lights tripwire",
         "mTrafficLightManager.SetCountdownValue(0);" in arm
         and "!mbNeedToSetUpLightsForEventStart || mbDEBUGTurnTrafficOff" in handler[at:at + 1200]),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_event_starts", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
