"""FX-TRAFFIC2 (crash parity 2026-09-24, G59-D2): the cab/trailer other-half crash queue drain.

  TrafficEntityModule::SendEmergencyCrashEvents @0x82747BB8 (no body before) + its PrePhysicsUpdate
      call at 0x8274C7DC, between SendPhysicalRequests (0x8274C7CC) and
      CreateBodiesForCrashingNetworkTraffic / CleanUpCrashedVehiclePhysics (0x8274C7EC / 0x8274C7F8)

  for each maEmergencyCrashingVehicles entry: skip if in lpCreatedBodies, dead or IsCrashing;
  physical -> [IsRecoveringFromSlam -> RecordTrafficVehicleIsPhysical(v, victim, causer, 0, 0, 0)]
              SetTrafficCrashing(victim);
  else     -> MakeVehiclePhysical(v, out, bodies, causer, CRASHING, Standard);
  then Clear().

Wiring: PrePhysicsUpdate calls it right after SendPhysicalRequests and no longer logs it missing.
Numeric: tests/FxTraffic2EmergencyCrash.cpp compiled against the PRODUCTION bodies extracted from
the source (working tree, or --rev <b5 rev>), hosted on a fixture with the module's real member
types and recorders for the callees.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_emergency_crash.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
FIXTURE = "EmergencyFixture"
NUMERIC_CHECKS = 10
BODY = "void TrafficEntityModule::SendEmergencyCrashEvents("


def wiring(tree):
    checks = []
    pre_physics = body_or_empty(tree.read(MODULE_CPP), "void TrafficEntityModule::PrePhysicsUpdate(")
    send_physical = pre_physics.find("SendPhysicalRequests( lpOutput, &lCreatedBodies );")
    emergency = pre_physics.find("SendEmergencyCrashEvents( lpOutput, &lCreatedBodies );")
    clean_up = pre_physics.find("CleanUpCrashedVehiclePhysics( lpOutput );", max(send_physical, 0))
    checks.append(("PrePhysicsUpdate calls SendEmergencyCrashEvents(lpOutput, &lCreatedBodies) right after "
                   "SendPhysicalRequests, before CleanUpCrashedVehiclePhysics (0x8274C7CC -> 0x8274C7DC -> 0x8274C7F8)",
                   0 <= send_physical < emergency < clean_up))
    checks.append(("PrePhysicsUpdate no longer logs SendEmergencyCrashEvents as a missing leg",
                   '"SendEmergencyCrashEvents @0x82747BB8' not in pre_physics))
    return checks


def numeric(tree):
    try:
        module = tree.read(MODULE_CPP)
        body = definition(module, BODY).replace("TrafficEntityModule::", FIXTURE + "::", 1)
        unpack = definition(module, "    inline u32 EntityIndexOf(EntityId lId)")
        shift = re.search(r"const u32 KU_ENTITY_INDEX_SHIFT\s*=\s*\w+;", module).group(0)
        mask = re.search(r"const u32 KU_ENTITY_INDEX_MASK\s*=\s*\w+;", module).group(0)
        vehicle = tree.read(VEHICLE_CPP)
        crashing = definition(vehicle, "bool Vehicle::IsCrashing() const")
        recovering = definition(vehicle, "bool Vehicle::IsRecoveringFromSlam() const")
    except (ValueError, AttributeError) as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    parts = ["namespace BrnTraffic {", "namespace {", shift, mask, unpack, "}", crashing, recovering, body, "}"]
    return compile_and_run(Path(__file__).with_name("FxTraffic2EmergencyCrash.cpp"), "emergency_crash.inc",
                           "\n".join(parts), "FxTraffic2EmergencyCrash")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic2_emergency_crash", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
