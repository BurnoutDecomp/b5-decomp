"""crash parity FX-TRAFFICLIGHTS (2026-09-25): the traffic lights cycle.

TrafficEntityModule::UpdateJunctions @0x82723EA0 had no body: UpdateDecisionFrame's call at 0x8274E5EC was a named
gate, so on the PC no junction ever changed phase. No stop line went red outside an event start, no light left the
state Construct gave it, and free-roam traffic never stopped at a light. Its callee TrafficLightManager::UpdateHull
@0x827517F8 (every light instance of a hull through TrafficLightRuntimeState::Update @0x827515D8: an AMBER light
running down to RED) had no body either, and RecalculateActiveHulls' stop-line release walk (0x8274D4B0..0x8274D88C:
a departed hull's lights let go of the stop lines they hold in hulls still active) was a gate.

NUMERIC (FxTrafficLightsCycle.cpp): the production UpdateJunctions and release-walk block (BrnTrafficEntityModule.cpp),
HullRuntime::SetStoplineRed (BrnTrafficHullRuntime.cpp), the JunctionLogicBox accessors (the header), and in their own
TUs TrafficLightManager::GetLightState / Construct / ChangeLightState (BrnTrafficLightManager.cpp) and the whole
BrnTrafficLightRuntimeState.cpp (Update + UpdateHull), against the real types:
  the first frame after Prepare, the fused phase time (fmadds 0x82724244; a case where the unfused sum differs),
  600 frames against the console's arithmetic, the two deltas, the event-start hold, a NaN time, UpdateHull's range,
  the release walk, the accessors and their tripwires, and the callee Update.
WIRING: the UpdateDecisionFrame call's position and the release walk's; the two retired gates.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtrafficlights_cycle.py [--rev <b5 rev>]
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
STATE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightRuntimeState.cpp"
HULL_RUNTIME_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.cpp"
HULL_RUNTIME_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"
JUNCTION_H = "src/SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"
WALK_MARKER = "// ---- the stop-line release walk, 0x8274D4B0..0x8274D88C"
NUMERIC_CHECKS = 30


def constant(source, name):
    match = re.search(r"const f32 " + name + r"\s*=[^;]+;", source)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match[0]


def release_walk(module):
    body = definition(module, "void TrafficEntityModule::RecalculateActiveHulls(")
    at = body.find(WALK_MARKER)
    if at < 0:
        raise ValueError("the stop-line release walk is absent from RecalculateActiveHulls")
    return definition(body[at:], "for (u32 luOld = 0;")


def numeric(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    lights = tree.read(LIGHTS_CPP).replace("\r\n", "\n")
    runtime = tree.read(HULL_RUNTIME_CPP).replace("\r\n", "\n")
    state = tree.read(STATE_CPP).replace("\r\n", "\n")
    try:
        # TU 1 (the fixture): UpdateJunctions, the release walk and SetStoplineRed, over Hull / JunctionLogicBox /
        # HullRuntime (the Hull headers bring BrnTrafficSharedConstants.h's ETrafficLightState).
        parts = [definition(module, "void TrafficEntityModule::UpdateJunctions()"),
                 "void TrafficEntityModule::ReleaseWalk(ActiveHullSet* lpOutOldHulls)\n{\n"
                 + release_walk(module) + "\n}",
                 definition(runtime, "void HullRuntime::SetStoplineRed(u32 luStopline, bool lbRed)")]
        # TU 2: the light manager's bodies with their .cpp's ETrafficLightState home (BrnTrafficLightCollection.h).
        lights_tu = "\n".join([
            '#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"',
            '#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"',
            '#include "GameShared/GameClasses/Core/CgsAssert.h"',
            "#include <cstddef>",
            "namespace BrnTraffic {",
            "namespace {",
            constant(lights, "KF_AMBER_TIME"),
            "}",
            definition(lights, "TrafficLightState* TrafficLightManager::GetLightState(u32 luInstance)"),
            definition(lights, "void TrafficLightManager::Construct()"),
            definition(lights, "void TrafficLightManager::ChangeLightState("),
            "}"]) + "\n"
        # TU 3: the whole BrnTrafficLightRuntimeState.cpp (TrafficLightRuntimeState::Update + UpdateHull).
        definition(state, "void TrafficLightManager::UpdateHull(")
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    shadow = {LIGHTS_H: tree.read(LIGHTS_H), JUNCTION_H: tree.read(JUNCTION_H),
              HULL_RUNTIME_H: tree.read(HULL_RUNTIME_H)}
    with tempfile.TemporaryDirectory(prefix="brn_traffic_lights_cycle_") as directory:
        lights_path = Path(directory) / "CycleLights.cpp"
        lights_path.write_text(lights_tu, encoding="utf-8")
        state_path = Path(directory) / "CycleRuntimeState.cpp"
        state_path.write_text(state, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("FxTrafficLightsCycle.cpp"), "cycle_bodies.inc",
                               "\n".join(parts), "FxTrafficLightsCycle", shadow=shadow,
                               extra_sources=[STRSTREAM_CPP, lights_path, state_path])


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def ordered(text, *needles):
    at = -1
    for needle in needles:
        found = text.find(needle, at + 1)
        if found < 0:
            return False
        at = found
    return True


def wiring(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    decision = body(module, "void TrafficEntityModule::UpdateDecisionFrame(")
    recalc = body(module, "void TrafficEntityModule::RecalculateActiveHulls(")
    return [
        ("UpdateDecisionFrame calls UpdateJunctions() after the showtime spawn and before UpdateParams (0x8274E5EC)",
         re.search(r"SpawnShowtimeTraffic\(\);\s*\}\s*UpdateJunctions\(\);", decision) is not None
         and ordered(decision, "UpdateJunctions();", "UpdateParams(lpInput);")),
        ("the UpdateDecisionFrame gate for UpdateJunctions is retired",
         "UpdateDecisionFrame leg UpdateJunctions" not in module),
        ("RecalculateActiveHulls: the stop-line release walk after the HullRuntime allocate loop and before "
         "RebuildGeneratorList (0x8274D4B0..0x8274D88C, SetStoplineRed(id, false) @0x8274D82C)",
         ordered(recalc, "Prepare(GetHull(luHull), luHull);", "mActiveHulls.Contains(luStopLineHull)",
                 "SetStoplineRed(lpLight->mauStopLineIds[luStopLine], false);", "RebuildGeneratorList();")),
        ("the RecalculateActiveHulls light-manager gate is retired",
         "RecalculateActiveHulls light-manager legs" not in module),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtrafficlights_cycle", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
