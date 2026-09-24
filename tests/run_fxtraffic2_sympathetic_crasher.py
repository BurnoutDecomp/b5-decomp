"""FX-TRAFFIC2 (crash parity 2026-09-24, G58-D1): the Showtime sympathetic-crasher producer.

  TrafficEntityModule::GenerateSympatheticCrasherOutput @0x82715C30 (no body before) + its
      PreSceneUpdate call at 0x8274AAE4, the first of the four output producers
  Vehicle::IsSympatheticCrasher (DWARF BrnTrafficVehicle.h:387; inlined at 0x82715C80 as the raw
      `lwz 0x40 ; cmpwi -1` -- no IsValid assert)

  if (!mbPlayingShowtimeMode) return;                                   lbzx +0x717DD
  for i < 400: SetSympatheticCrasher(i, alive && IsCrashing() && target != -1)

Wiring: PreSceneUpdate calls the producer before GenerateNearMissOutput; the Vehicle header carries
the raw target predicate.
Numeric: tests/FxTraffic2SympatheticCrasher.cpp compiled against the PRODUCTION bodies extracted
from the source (working tree, or --rev <b5 rev>), hosted on a fixture with the module's real
member types.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_sympathetic_crasher.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
T2RC_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.cpp"
FIXTURE = "SympFixture"
NUMERIC_CHECKS = 12
PRODUCER = "void TrafficEntityModule::GenerateSympatheticCrasherOutput("


def wiring(tree):
    checks = []
    module = tree.read(MODULE_CPP)
    pre_scene = body_or_empty(module, "void TrafficEntityModule::PreSceneUpdate(")
    producer = pre_scene.find("GenerateSympatheticCrasherOutput(lpInput, lpOutput);")
    near_miss = pre_scene.find("GenerateNearMissOutput(lpInput, lpOutput);")
    checks.append(("PreSceneUpdate calls GenerateSympatheticCrasherOutput first, before GenerateNearMissOutput "
                   "(0x8274AAE4 -> 0x8274AAF4)", producer >= 0 and producer < near_miss))
    header = code_only(tree.read(VEHICLE_H))
    predicate = re.search(r"bool\s+IsSympatheticCrasher\(\)\s*const\s*\{\s*return\s+mSympCrashTarget\.muValue\s*"
                          r"!=\s*0xFFFFFFFFu?\s*;\s*\}", header)
    checks.append(("Vehicle::IsSympatheticCrasher() const (DWARF h:387) is the raw target != -1 read "
                   "(0x82715C80 `lwz 0x40 ; cmpwi -1`, no IsValid assert)", predicate is not None))
    return checks


def numeric(tree):
    try:
        producer = definition(tree.read(MODULE_CPP), PRODUCER).replace("TrafficEntityModule::", FIXTURE + "::", 1)
        crashing = definition(tree.read(VEHICLE_CPP), "bool Vehicle::IsCrashing() const")
        setter = definition(tree.read(T2RC_CPP), "    void TrafficToRaceCarInterface_PreScene::SetSympatheticCrasher(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    parts = ["namespace BrnTraffic {", "namespace BrnTrafficIO {", setter, "}", crashing, producer, "}"]
    return compile_and_run(Path(__file__).with_name("FxTraffic2SympatheticCrasher.cpp"), "sympathetic_crasher.inc",
                           "\n".join(parts), "FxTraffic2SympatheticCrasher")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic2_sympathetic_crasher", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
