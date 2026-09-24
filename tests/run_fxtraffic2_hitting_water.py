"""FX-TRAFFIC2 (crash parity 2026-09-24, G35-D1): PhysicalTrafficManager::CheckForTrafficHittingWater
@0x8261DDF0 -- a physical traffic car whose down-ray hits water within 0.25 m of its lowest point is
reported removed and released, every frame (ProcessTrafficMaintenanceEvents 0x82649B08).

  walk mUsedTrafficVehicles ; AGTR (+0x570) valid ; surface = (tag low halfword >> 4) & 0x3F ;
  KAB_SURFACE_IS_WATER[surface] ; GetSimpleVehicleBox ; lowest = w.y - |x.y*dx| - |y.y*dy| - |z.y*dz|
  -> mavfLowestPointWorldSpace[i] ; hit iff AGTR.y >= lowest - KVF_RESET_ON_WATER_HEIGHT (0.25) ;
  AddTrafficRemovedEvent(maTrafficEntityIDs[i], mePhysicalTrafficState) ; RemoveTrafficVehicle((u8)i, .., false)
Before the fix the function was a BRN_MAINTENANCE_GATE (nothing ever removed for water).

Numeric: tests/FxTraffic2HittingWater.cpp compiled against the PRODUCTION CheckForTrafficHittingWater
and GetTrafficVehicle (working tree, or --rev <b5 rev>) on a fixture with the manager's real members.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_hitting_water.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

MANAGER_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.cpp"
MAINTENANCE_CPP = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_MaintenanceEvents.cpp"
FIXTURE = "WaterFixture"
NUMERIC_CHECKS = 11


def numeric(tree):
    try:
        get_vehicle = definition(tree.read(MANAGER_CPP),
                                 "PhysicalTrafficVehicle* PhysicalTrafficManager::GetTrafficVehicle(s32 liVehicle)")
        water = definition(tree.read(MAINTENANCE_CPP), "    void PhysicalTrafficManager::CheckForTrafficHittingWater(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    parts = ["namespace BrnPhysics { namespace Vehicle {",
             get_vehicle.replace("PhysicalTrafficManager::", FIXTURE + "::"),
             water.replace("PhysicalTrafficManager::", FIXTURE + "::"),
             "} }"]
    return compile_and_run(Path(__file__).with_name("FxTraffic2HittingWater.cpp"), "hitting_water.inc",
                           "\n".join(parts), "FxTraffic2HittingWater", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtraffic2_hitting_water", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
