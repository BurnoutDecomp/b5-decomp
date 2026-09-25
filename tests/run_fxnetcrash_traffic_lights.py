"""crash parity FX-NETCRASH (2026-09-25): the traffic lights receive the event countdown.

REVIEW-I's concern on b43b5c2b: HandleExternalRequests arm 47 (E_ACTION_SET_COUNTDOWN) calls
TrafficLightManager::SetCountdownValue(this + 0x53790, record[0]) at 0x8274BD98 on EVERY countdown change,
offline included, but the PC had that call behind a named gate. The manager's countdown members and the rest of
the trio were missing too:
  Construct         @0x82751708, called by TrafficEntityModule::Reset @0x8272D39C
  SetCountdownValue @0x82751750, called by HandleExternalRequests arm 47 @0x8274BD98..0x8274BDA0
  Update            @0x827517A8, called by PostPhysicsUpdate @0x8274EAF0..0x8274EB04 with mfSimTimeStep
The members (DWARF BrnTrafficLightManager.h:178..:180) sit at +0x12C0 / +0x12C4 / +0x12C8.

NUMERIC: the production trio (with its file-local constant and witness) from BrnTrafficLightManager.cpp, compiled
into FxNetcrashTrafficLights.cpp against the real header.
WIRING: the three call sites in BrnTrafficEntityModule.cpp, and the three retired gates.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_traffic_lights.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

LIGHTS_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.cpp"
MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
LIGHTS_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
UPDATE = "void TrafficLightManager::Update(f32 lfTimeDelta)"
NUMERIC_CHECKS = 17


def trio(source):
    """The anonymous namespace that holds KF_COUNTDOWN_RED_TIME, through the end of Update."""
    anchor = source.index("const f32 KF_COUNTDOWN_RED_TIME")
    start = source.rindex("namespace\n{", 0, anchor)
    update = definition(source, UPDATE)
    end = source.index(update) + len(update)
    return source[start:end]


def numeric(tree):
    source = tree.read(LIGHTS_CPP).replace("\r\n", "\n")
    try:
        # The block also carries ChangeLightState @0x827518E0 (bodied between SetCountdownValue and Update,
        # FX-NETCRASH UpdateEventStarts), which reaches the records through GetLightState @0x8274F9A0.
        block = trio(source) + "\n" + definition(source, "TrafficLightState* TrafficLightManager::GetLightState(u32 luInstance)")
    except ValueError as error:
        print("NUMERIC: cannot build -- the countdown trio is absent: " + str(error))
        return None
    shadow = {LIGHTS_H: tree.read(LIGHTS_H)}
    return compile_and_run(Path(__file__).with_name("FxNetcrashTrafficLights.cpp"), "light_bodies.inc", block,
                           "FxNetcrashTrafficLights", shadow=shadow, extra_sources=[STRSTREAM_CPP])


def body(module, signature):
    try:
        return code_only(definition(module, signature))
    except ValueError:
        return ""


def wiring(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    handler = body(module, "void TrafficEntityModule::HandleExternalRequests(")
    arm = ""
    at = handler.find("E_ACTION_SET_COUNTDOWN:")
    if at >= 0:
        arm = handler[at:handler.find("break;", at)]
    set_at = arm.find("mTrafficLightManager.SetCountdownValue(")
    online_at = arm.find("if (mbIsOnlineGameMode)")
    reset = body(module, "void TrafficEntityModule::Reset()")
    post = body(module, "void TrafficEntityModule::PostPhysicsUpdate(")
    return [
        ("arm 47 calls mTrafficLightManager.SetCountdownValue(record->miCountdownDisplay) FIRST, outside the "
         "online test (0x8274BD98..0x8274BDA0, then lbzx +0x717DC at 0x8274BDA4)",
         0 <= set_at < online_at and "miCountdownDisplay" in arm[set_at:online_at]),
        ("Reset calls mTrafficLightManager.Construct() right after mUsedHullRuntimeData's clear (0x8272D384..0x8272D39C)",
         re.search(r"mUsedHullRuntimeData\.Prepare\(\);\s*mTrafficLightManager\.Construct\(\);", reset) is not None),
        ("PostPhysicsUpdate calls mTrafficLightManager.Update(mfSimTimeStep) right after GenerateSceneUpdateEvents "
         "(0x8274EAF0..0x8274EB04)",
         re.search(r"GenerateSceneUpdateEvents\(lpOutput\);\s*mTrafficLightManager\.Update\(mfSimTimeStep\);", post)
         is not None),
        ("the three named gates are gone",
         "action 47 leg TrafficLightManager::SetCountdownValue" not in module
         and "Reset leg TrafficLightManager::Construct" not in module
         and "PostPhysicsUpdate RUNNING leg TrafficLightManager::Update" not in module),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_traffic_lights", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
