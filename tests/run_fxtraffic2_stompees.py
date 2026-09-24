"""FX-TRAFFIC2 (crash parity 2026-09-24, CHAIN-STOMPEES parts a + b): the Showtime leap/stomp producer.

  TrafficEntityModule::GeneratePotentialLeapedAndStompedCarsOutput @0x8271F298  (no body before;
      the ONLY caller of AddPotentialStompee @0x82706028) + its PreSceneUpdate call at 0x8274AB04
  TrafficToRaceCarInterface_PreScene::GetPotentialStompees (DWARF :122) / ClearStompees (:126)
  OutputBuffer_PreScene::AddPotentialScoree @0x8271D2E8 (no body before)

Wiring: PreSceneUpdate calls the producer right after GenerateNearMissOutput and before the state
switch; the interface header carries GetPotentialStompees returning mPotentialStompees with the
count written through the pointer.
Numeric: tests/FxTraffic2Stompees.cpp compiled against the PRODUCTION bodies extracted from the
source (working tree, or --rev <b5 rev>), hosted on a fixture with the module's real member types.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_stompees.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report, REPO

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
MODULE_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
IO_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.cpp"
T2RC_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.cpp"
T2RC_H = "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
MATH_CPP = "src/GameSource/Math/BrnMathUtils.cpp"
FIXTURE = "StompFixture"
NUMERIC_CHECKS = 39
CONSTANTS_BLOCK = "namespace   // the leap/stomp producer's file-scope constants"
PRODUCER = "void TrafficEntityModule::GeneratePotentialLeapedAndStompedCarsOutput("
FIRST_UNUSED = "ShowtimeVehicleInfo* GetFirstUnusedShowtimeVehicleInfo(u32& luInfoIndex)"


def wiring(tree):
    checks = []
    module = tree.read(MODULE_CPP)
    pre_scene = body_or_empty(module, "void TrafficEntityModule::PreSceneUpdate(")
    near_miss = pre_scene.find("GenerateNearMissOutput(lpInput, lpOutput);")
    producer = pre_scene.find("GeneratePotentialLeapedAndStompedCarsOutput(lpInput, lpOutput);")
    switch = pre_scene.find("switch (meState)")
    checks.append(("PreSceneUpdate calls GeneratePotentialLeapedAndStompedCarsOutput after GenerateNearMissOutput "
                   "and before the state switch (0x8274AAF4 -> 0x8274AB04)",
                   near_miss >= 0 and near_miss < producer < switch))
    header = code_only(tree.read(T2RC_H))
    accessor = re.search(r"const\s+VehicleStompingData\*\s+GetPotentialStompees\(\s*s32\*\s*(\w+)\s*\)\s*const\s*\{"
                         r"\s*\*\1\s*=\s*miPotentialStompeeCount;\s*return\s+mPotentialStompees;\s*\}", header)
    checks.append(("TrafficToRaceCarInterface_PreScene::GetPotentialStompees(s32*) const (DWARF :122): count out, "
                   "records back (the inlined shape at 0x822BD62C/0x822BD638)", accessor is not None))
    clear = re.search(r"void\s+ClearStompees\(\)\s*\{\s*miPotentialStompeeCount\s*=\s*0;\s*\}", header)
    checks.append(("TrafficToRaceCarInterface_PreScene::ClearStompees() (DWARF :126) zeroes the count only",
                   clear is not None))
    return checks


def numeric(tree):
    parts = []
    try:
        module = tree.read(MODULE_CPP)
        constants = definition(module, CONSTANTS_BLOCK)
        producer = definition(module, PRODUCER).replace("TrafficEntityModule::", FIXTURE + "::", 1)
        first_unused = definition(tree.read(MODULE_H), FIRST_UNUSED).replace(
            "GetFirstUnusedShowtimeVehicleInfo(", FIXTURE + "::GetFirstUnusedShowtimeVehicleInfo(", 1)
        scoree = definition(tree.read(IO_CPP), "    void OutputBuffer_PreScene::AddPotentialScoree(")
        stompee = definition(tree.read(T2RC_CPP), "    void TrafficToRaceCarInterface_PreScene::AddPotentialStompee(")
        velocity = definition(tree.read(VEHICLE_CPP), "Vector3 Vehicle::GetLinearVelocity() const")
        magnitude = definition(tree.read(MATH_CPP), "    f32 Magnitude2D(Vector3 lVector)")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    parts += ["namespace BrnMath {", magnitude, "}"]
    parts += ["namespace BrnTraffic {", "namespace BrnTrafficIO {", scoree, stompee, "}", velocity,
              constants, first_unused, producer, "}"]
    return compile_and_run(Path(__file__).with_name("FxTraffic2Stompees.cpp"), "stompees.inc", "\n".join(parts),
                           "FxTraffic2Stompees",
                           extra_sources=[REPO / "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.cpp"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic2_stompees", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
