"""L3 RACEINTRO (2026-09-27): the race intro's camera below the map -- MainDirector::CalcTrafficLightSpace @0x8221A3A8.

The owner's report: "The races intro doesn't show the traffic light going green, the camera is below the map".
An offline race's countdown take, Race_Event_Start (guid 574883), is authored in three intervals whose eye / look spaces
are [SCENE, TRAFFIC_LIGHT, HEADING]. The TRAFFIC_LIGHT space (eICE_TRAFFIC_LIGHT_SPACE, 5) is GameState::
mTrafficLightSpace, and its only producer is CalcTrafficLightSpace: the amber corona transform of the event junction's
light nearest the player and in front of it. ProcessInputQueue calls it on the countdown push (cases 30 / 47, bl
0x82237B08 / 0x82237B98). On the PC it was a GATE comment with a thirteen-int phantom declaration, so the space stayed at
GameState::Clear's identity and the take's middle interval put the eye at the world origin, under the map -- on the old
exe 7fdddf649d45 and on the owner's d65db9997047 alike (scratch/bugtest/runs/l3_raceintro_cam_ab/A_*, B_*).

The body needed the collection's methods on TrafficData::mTrafficLights, which the BL-1 ODR fork made impossible (two
TrafficLightCollection definitions, no TU could hold both): the fork is merged into the DWARF-path header.

NUMERIC (FxRaceIntroTrafficLightSpace.cpp): the production CalcTrafficLightSpace (+ its constant and its default-off
witness) over the real JunctionLogicBox / TrafficLightCollection / ConsoleVpu::Dot3, against the console's own words run
on emu64 (FxRaceIntroTrafficLightSpaceData.h, 56 cases: no box, behind / abeam / tie / NaN / rewrite / empty
controllers, the map not loaded, an instance out of range, 8 controllers, three pairs where ROUNDING_RULE 1 (vmsum3fp128)
and a sequential f32 sum order the two lights differently, 40 random junctions): per case the lights chosen call by call,
the 16 words of the space, and the tripwire counts; plus GetPlayer's two calls.
WIRING: the countdown arm's call, the retired gate, the declaration, the BL-1 merge.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxraceintro_trafficlight_space.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP, REPO

DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
DIRECTOR_H = "src/GameSource/Director/BrnMainDirector.h"
COLLECTION_H = "src/SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"
RETIRED_H = "src/SharedClasses/Traffic/BrnTrafficLightCollection.h"
TRAFFICDATA_H = "src/SharedClasses/Traffic/BrnTrafficDataResourceType.h"
JUNCTION_H = "src/SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"
CONSTANTS_H = "src/SharedClasses/Traffic/BrnTrafficSharedConstants.h"
CONSOLEVPU_H = "src/GameSource/Director/Camera/Utils/BrnConsoleVpu.h"
NUMERIC_CHECKS = 56 * 3 + 2


def numeric(tree):
    director = tree.read(DIRECTOR_CPP).replace("\r\n", "\n")
    try:
        constant = re.search(r"const f32 KF_TRAFFIC_LIGHT_SPACE_NO_LIGHT_YET\s*=[^;]+;", director)
        if constant is None:
            raise ValueError("KF_TRAFFIC_LIGHT_SPACE_NO_LIGHT_YET is absent")
        parts = ["namespace {",
                 constant[0],
                 definition(director, "void BrnDiag_ReportTrafficLightSpace("),
                 "}",
                 definition(director, "void MainDirector::CalcTrafficLightSpace(const DirectorInputOutput* lpIO)")]
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    shadow = {}
    for header in (COLLECTION_H, JUNCTION_H, CONSTANTS_H, CONSOLEVPU_H):
        text = tree.read(header)
        if text:
            shadow[header] = text
    return compile_and_run(Path(__file__).with_name("FxRaceIntroTrafficLightSpace.cpp"), "tlspace_bodies.inc",
                           "\n".join(parts), "FxRaceIntroTrafficLightSpace", shadow=shadow,
                           extra_sources=[STRSTREAM_CPP])


def exists(tree, relative):
    """Whether a src file exists in the revision (the working tree: on disk)."""
    if tree.rev is None:
        return (REPO / relative).exists()
    return bool(tree.read(relative))


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    director = tree.read(DIRECTOR_CPP).replace("\r\n", "\n")
    header = code_only(tree.read(DIRECTOR_H))
    queue = body(director, "void MainDirector::ProcessInputQueue(const DirectorInputOutput* lpIO)")
    collection = code_only(tree.read(COLLECTION_H))
    trafficdata = code_only(tree.read(TRAFFICDATA_H))
    arm = re.search(r"case 30:\s*case 47:\s*\{(.*?)\n\s*break;\s*\}", queue, re.S)
    return [
        ("ProcessInputQueue's countdown arm (cases 30 / 47) calls CalcTrafficLightSpace(lpIO) right after the "
         "COUNTDOWN push (bl 0x8221A3A8 at 0x82237B08 / 0x82237B98)",
         arm is not None and re.search(r"SetCurrent\(GameState::E_EVENT_STATE_COUNTDOWN\);\s*"
                                       r"CalcTrafficLightSpace\(lpIO\);\s*$", arm[1]) is not None),
        ("the CalcTrafficLightSpace gate comment is retired",
         "GATE: CalcTrafficLightSpace" not in director),
        ("BrnMainDirector.h declares the console's two-argument signature (this, lpIO), not the 13-int phantom",
         re.search(r"void CalcTrafficLightSpace\(const DirectorInputOutput\* lpIO\);", header) is not None
         and "liArg14" not in header),
        ("BL-1 merged: one TrafficLightCollection -- TrafficData includes the DWARF-path header, the second definition "
         "is gone, and the header takes ETrafficLightState from BrnTrafficSharedConstants.h",
         '#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"' in trafficdata
         and not exists(tree, RETIRED_H)
         and re.search(r"enum\s+ETrafficLightState", collection) is None
         and '#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"' in collection),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxraceintro_trafficlight_space", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
