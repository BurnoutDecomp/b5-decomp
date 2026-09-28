"""L6 AIDRIVE (owner list 2026-09-27): "The enemies AIs always crash to the traffic or to walls".

THE AI TRAFFIC FEED WAS DEAD. TrafficEntityModule::StoreAISceneResultsForNextFrame @0x82728400 had no body,
and its console call -- PrePhysicsUpdate's running arm, `mr r4,r28 (lpInput) ; mr r3,r31 ; bl` @0x8274C804,
right after CleanUpCrashedVehiclePhysics @0x8274C7F8 -- was a LogMissingLeg park. It is the ONLY writer of
maStoredAITrafficData[].maTrafficEntityIDs / miNumTrafficIDs, so ConvertSceneResultsToTrafficDataForAI never
published a traffic entity, AIModule::SortTrafficIntoAICars never put a traffic car into any driver's
NearbyVehicles, and the steering fan's AvoidTraffic / AvoidOncomingTraffic rows (kfBias -100 / -400) never
saw one: every rival drove traffic-blind.

  1. WIRING -- PrePhysicsUpdate calls StoreAISceneResultsForNextFrame(lpInput) after
     CleanUpCrashedVehiclePhysics(lpOutput) (0x8274C7F8 -> 0x8274C804) and no longer logs it missing; the
     header declares it with the DWARF signature (const InputBuffer_PrePhysics*).
  2. NUMERIC -- tests/FxAiDriveTrafficFeed.cpp runs the extracted production body (+ its two id constants)
     on a fixture holding the module's real maStoredAITrafficData and a real InputBuffer_PrePhysics
     scene-result queue filled as the scene pass fills it.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaidrive_traffic_feed.py [--rev <b5 rev>]
    (--src-dir DIR reads BrnTrafficEntityModule.cpp/.h from DIR instead: the shadow gate before a copy-in)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, STRSTREAM_CPP, body_or_empty, code_only, compile_and_run, definition, report

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
MODULE_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
FIXTURE = "AITrafficFeedFixture"
BODY = "void TrafficEntityModule::StoreAISceneResultsForNextFrame("
NUMERIC_CHECKS = 14


class DirTree(Tree):
    """The module's two files from a directory (the shadow copy); everything else as the working tree."""

    def __init__(self, directory):
        super().__init__(None)
        self.directory = Path(directory)

    def read(self, relative):
        local = self.directory / Path(relative).name
        if relative in (MODULE_CPP, MODULE_H) and local.exists():
            return local.read_text(encoding="utf-8-sig")
        return super().read(relative)


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(MODULE_CPP).replace("\r\n", "\n")
    pre = squash(body_or_empty(source, "void TrafficEntityModule::PrePhysicsUpdate("))
    yield ("PrePhysicsUpdate's running arm calls StoreAISceneResultsForNextFrame(lpInput) right after "
           "CleanUpCrashedVehiclePhysics(lpOutput) (0x8274C7F8 -> 0x8274C804, r4 = r28 = lpInput)",
           "CreateBodiesForCrashingNetworkTraffic(lpOutput,&lCreatedBodies);CleanUpCrashedVehiclePhysics(lpOutput);"
           "StoreAISceneResultsForNextFrame(lpInput);}" in pre)
    yield ("PrePhysicsUpdate no longer parks it as a missing leg",
           "StoreAISceneResultsForNextFrame(in)" not in pre)
    header = squash(tree.read(MODULE_H).replace("\r\n", "\n"))
    yield ("BrnTrafficEntityModule.h declares StoreAISceneResultsForNextFrame(const BrnTrafficIO::"
           "InputBuffer_PrePhysics*) (DWARF BrnTrafficEntityModule.h:1759)",
           "voidStoreAISceneResultsForNextFrame(constBrnTrafficIO::InputBuffer_PrePhysics*lpInput);" in header)


def numeric(tree):
    source = tree.read(MODULE_CPP).replace("\r\n", "\n")
    try:
        body = definition(source, BODY).replace("TrafficEntityModule::StoreAISceneResultsForNextFrame",
                                                FIXTURE + "::StoreAISceneResultsForNextFrame", 1)
        base = re.search(r"const u32 KU_AI_FRUSTUM_QUERY_ID_BASE\s*=\s*\w+;", source).group(0)
        last = re.search(r"const u32 KU_AI_FRUSTUM_QUERY_ID_LAST\s*=[^;]*;", source).group(0)
    except (ValueError, AttributeError) as error:
        print("NUMERIC: cannot build -- production body / constants absent: " + str(error))
        return None
    text = "\n".join(["namespace BrnTraffic {", "namespace {", base, last, "}", body, "}"]) + "\n"
    return compile_and_run(Path(__file__).with_name("FxAiDriveTrafficFeed.cpp"), "fxaidrive_traffic_feed.inc",
                           text, "FxAiDriveTrafficFeed", extra_sources=(STRSTREAM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-dir", help="read BrnTrafficEntityModule.cpp/.h from this directory (shadow gate)")
    args = parser.parse_args()
    tree = DirTree(args.src_dir) if args.src_dir else Tree(args.rev)
    return report("run_fxaidrive_traffic_feed", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
