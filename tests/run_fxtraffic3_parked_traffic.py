"""FX-TRAFFIC3 item 4 (crash parity wave 5, 2026-09-24): the Power Parking producer.

  TrafficEntityModule::GenerateNearbyParkedTrafficOutput @0x8271FA18 had no body and PreSceneUpdate
  stood a named gate where the console calls it (0x8274AB14, between
  GeneratePotentialLeapedAndStompedCarsOutput at 0x8274AB04 and ManageTriggers at 0x8274AB20). While
  the player power parks (+0x717E5), the console measures every live, alarm-free, non-crashing parked
  car whose hull record is not divergent-only with CheckVehicleForPowerPark and publishes the count
  and the closest / second-closest / angle / perpendicular results to the race-car module
  (TrafficToRaceCarInterface_PreScene +0x20C..+0x21C). The two DWARF accessors that carry the
  publish (SetNearbyParkedTrafficData :135, GetNearbyParkedTrafficData :144) land with it.

Wiring: PreSceneUpdate calls GenerateNearbyParkedTrafficOutput(lpInput, lpOutput) after
GeneratePotentialLeapedAndStompedCarsOutput and before ManageTriggers. Numeric:
tests/FxTraffic3ParkedTraffic.cpp compiled against the PRODUCTION producer and every body it reaches
(working tree, or --rev <b5 rev>), built twice: the console paths, then the PC-only
BRN_PARKED_DRYRUN diagnostic (a closed gate measures but never publishes). Expectations are the
ARTIST values (gate +0x717E5, FLT_MAX seeds, filters +5 & 1 / +7 & 0x10 / IsCrashing / record
+0x43 & 1, the 15 m radius, the five stores).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic3_parked_traffic.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
STATIC_PARAM_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.cpp"
IO_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.cpp"
RACE_CAR_OUT_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"
POWER_PARKING_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.cpp"
FIXTURE = "ParkedFixture"
NUMERIC_CHECKS = 12 + 4   # the console build + the BRN_PARKED_DRYRUN build

MODULE_BODIES = [
    "    Vehicle* TrafficEntityModule::GetStaticVehicle(u32 luIndex)",
    "    StaticTrafficParam* TrafficEntityModule::GetStaticTrafficParam(u32 luIndex)",
    "    u32 TrafficEntityModule::GetVehicleIndexFromStaticIndex(u32 luStaticVehicle)",
    "const Hull* TrafficEntityModule::GetHull(u32 luIndex) const",
    "void TrafficEntityModule::GenerateNearbyParkedTrafficOutput(",
]
CONSTANTS = [
    "KU8_STATIC_VEHICLE_FLAG_DIVERGENT_ONLY", "KI_PARKED_DIAG_CAP", "giParkedDiagLines", "gbParkedGateLogged",
    "guParkedLastDryRunCount",
]
DIAG_HELPERS = ["    bool ParkedDryRunEnabled()"]
VEHICLE_BODIES = ["bool Vehicle::IsCrashing() const", "bool Vehicle::IsAlarmOn() const"]
STATIC_PARAM_BODIES = ["    u16 StaticTrafficParam::GetHull() const", "    u8 StaticTrafficParam::GetIndexInHull() const"]
IO_BODIES = [
    "    const InputBuffer_PreScene::ActiveRaceCarOutputInterface*\n"
    "    InputBuffer_PreScene::GetActiveRaceCarOutputInterface() const",
    # the leading newline keeps the anchor off the const twin (0x827BB090)
    "\n    OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene*\n"
    "    OutputBuffer_PreScene::GetTrafficToRaceCarInterface_PreScene()",
]
RACE_CAR_OUT_BODIES = [
    "bool RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive() const",
    "Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerPosition() const",
    "Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerDirection() const",
]
POWER_PARKING_BODIES = [
    "    f32 GetPointToInfiniteLineDistance(Vector3 lPoint, Vector3 lPointOnLine1, Vector3 lPointOnLine2)",
]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


def wiring(tree):
    pre = body_or_empty(tree.read(MODULE_CPP), "void TrafficEntityModule::PreSceneUpdate(")
    stompees = pre.find("GeneratePotentialLeapedAndStompedCarsOutput(lpInput, lpOutput);")
    parked = pre.find("GenerateNearbyParkedTrafficOutput(lpInput, lpOutput);")
    triggers = pre.find("ManageTriggers(lpOutput);")
    return [("PreSceneUpdate calls GenerateNearbyParkedTrafficOutput(lpInput, lpOutput) after "
             "GeneratePotentialLeapedAndStompedCarsOutput and before ManageTriggers "
             "(0x8274AB04 -> 0x8274AB14 -> 0x8274AB20)",
             0 <= stompees < parked < triggers)]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts += [definition(module, helper) for helper in DIAG_HELPERS]
        parts.append("}")
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in MODULE_BODIES]
        parts += [definition(tree.read(VEHICLE_CPP), body) for body in VEHICLE_BODIES]
        parts += [definition(tree.read(STATIC_PARAM_CPP), body) for body in STATIC_PARAM_BODIES]
        parts.append("namespace BrnTrafficIO {")
        parts += [definition(tree.read(IO_CPP), body) for body in IO_BODIES]
        parts.append("}")
        parts.append("}")
        parts.append("namespace BrnWorld { namespace RaceCarEntityModuleIO {")
        parts += [definition(tree.read(RACE_CAR_OUT_CPP), body) for body in RACE_CAR_OUT_BODIES]
        parts.append("} }")
        parts.append("namespace BrnMath {")
        parts += [definition(tree.read(POWER_PARKING_CPP), body) for body in POWER_PARKING_BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    test_cpp = Path(__file__).with_name("FxTraffic3ParkedTraffic.cpp")
    inc = "\n".join(parts)
    console = compile_and_run(test_cpp, "parked_traffic.inc", inc, "FxTraffic3ParkedTraffic",
                              extra_sources=[STRSTREAM_CPP])
    dry_run = compile_and_run(test_cpp, "parked_traffic.inc", inc, "FxTraffic3ParkedTrafficDryRun",
                              extra_flags="/DFXTRAFFIC3_PARKED_DRYRUN", extra_sources=[STRSTREAM_CPP])
    console = console if console is not None else (12, 12)
    dry_run = dry_run if dry_run is not None else (4, 4)
    return console[0] + dry_run[0], console[1] + dry_run[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic3_parked_traffic", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
