"""FX-TRAFFIC2 (crash parity 2026-09-24, G59-D3): the network-crashed traffic promotion.

  TrafficEntityModule::CreateBodiesForCrashingNetworkTraffic @0x8274B4B0 (no body before) + its
      PrePhysicsUpdate call at 0x8274C7EC, after SendEmergencyCrashEvents (0x8274C7DC) and before
      CleanUpCrashedVehiclePhysics (0x8274C7F8)
  TrafficEntityModule::IsPlayingOnlineGameMode (DWARF h:2221; inlined as `lbzx +0x717DC`)

  CGS_ASSERT(IsPlayingOnlineGameMode() || maNewCrashedNetworkVehicles.GetLength() == 0)   (no return)
  causer = (meLocalPlayerIndex << 10) | 0x01000000
  for each entry: SafeRequestMakeVehiclePhysical(entry, CRASHED, causer, CRASHING, Standard, out, bodies)
  Clear()

Wiring: PrePhysicsUpdate calls it after SendEmergencyCrashEvents and no longer logs it missing.
Numeric: tests/FxTraffic2NetworkCrashBodies.cpp compiled against the PRODUCTION bodies extracted
from the source (working tree, or --rev <b5 rev>), hosted on a fixture with the module's real
member types and a recorder for SafeRequestMakeVehiclePhysical.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_network_crash_bodies.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
MODULE_H = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
FIXTURE = "NetCrashFixture"
NUMERIC_CHECKS = 9
BODY = "void TrafficEntityModule::CreateBodiesForCrashingNetworkTraffic("
ONLINE = "bool IsPlayingOnlineGameMode() const {"
CONSTANTS = ("KU_RACE_CAR_PART_INDEX_SHIFT", "KU_RACE_CAR_OWNER_PACKED", "KU_NUM_BITS_FOR_ENTITY_NUM")


def wiring(tree):
    checks = []
    pre_physics = body_or_empty(tree.read(MODULE_CPP), "void TrafficEntityModule::PrePhysicsUpdate(")
    emergency = pre_physics.find("SendEmergencyCrashEvents( lpOutput, &lCreatedBodies );")
    network = pre_physics.find("CreateBodiesForCrashingNetworkTraffic( lpOutput, &lCreatedBodies );")
    clean_up = pre_physics.find("CleanUpCrashedVehiclePhysics( lpOutput );", max(emergency, 0))
    checks.append(("PrePhysicsUpdate calls CreateBodiesForCrashingNetworkTraffic(lpOutput, &lCreatedBodies) after "
                   "SendEmergencyCrashEvents, before CleanUpCrashedVehiclePhysics (0x8274C7DC -> 0x8274C7EC -> 0x8274C7F8)",
                   0 <= emergency < network < clean_up))
    checks.append(("PrePhysicsUpdate no longer logs CreateBodiesForCrashingNetworkTraffic as a missing leg",
                   '"CreateBodiesForCrashingNetworkTraffic @0x8274B4B0' not in pre_physics))
    return checks


def numeric(tree):
    try:
        module = tree.read(MODULE_CPP)
        body = definition(module, BODY).replace("TrafficEntityModule::", FIXTURE + "::", 1)
        online = definition(tree.read(MODULE_H), ONLINE).replace(
            "IsPlayingOnlineGameMode()", FIXTURE + "::IsPlayingOnlineGameMode()", 1)
        constants = [re.search(r"const u32 " + name + r"\s*=\s*\w+;", module).group(0) for name in CONSTANTS]
    except (ValueError, AttributeError) as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    parts = ["namespace BrnTraffic {", "namespace {"] + constants + ["}", "inline " + online, body, "}"]
    return compile_and_run(Path(__file__).with_name("FxTraffic2NetworkCrashBodies.cpp"), "network_crash_bodies.inc",
                           "\n".join(parts), "FxTraffic2NetworkCrashBodies")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic2_network_crash_bodies", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
