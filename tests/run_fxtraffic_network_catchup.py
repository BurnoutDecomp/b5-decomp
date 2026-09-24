"""FX-TRAFFIC (crash parity 2026-09-23, G34-D1): PhysicalTrafficManager::UpdateNetworkTrafficVehicle
@0x8261CBD0 -- the traffic twin of VehicleManager::UpdateNetworkCatchup.

  assert id valid (:560) ; idx = extrwi(id,14,8) ; GetTrafficVehicle(idx) ; lbz 0x32 < COUNT (h:382) ;
  FULL only ; GetFullTraffic (0x825C0148) ; GetTrafficDriver(idx) (0x825B4900) ;
  StartCatchupInterpolation(full, event+0x10, v1 [full+0x50], v2 [full+0x60], li r6 0) @0x8261CC8C.
Before the fix it was a LogOnce gate (0 calls).

Numeric: tests/FxTrafficNetworkCatchup.cpp compiled against the PRODUCTION body (and the production
GetTrafficVehicle / GetTrafficDriver / GetFullTrafficPhysics) on a fixture with the real member types;
StartCatchupInterpolation is a recorder.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic_network_catchup.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

MANAGER_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.cpp"
EVENTS_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_TrafficEvents.cpp"
FIXTURE = "NetworkCatchupFixture"
NUMERIC_CHECKS = 9

REQUIRED = [
    (MANAGER_CPP, "PhysicalTrafficVehicle* PhysicalTrafficManager::GetTrafficVehicle(s32"),
    (MANAGER_CPP, "VehicleDriver* PhysicalTrafficManager::GetTrafficDriver(s32"),
    (MANAGER_CPP, "TrafficPhysics* PhysicalTrafficVehicle::GetFullTrafficPhysics()"),
    (EVENTS_CPP, "void PhysicalTrafficManager::UpdateNetworkTrafficVehicle("),
]


def numeric(tree):
    parts = ["namespace BrnPhysics { namespace Vehicle {"]
    for relative, signature in REQUIRED:
        try:
            parts.append(definition(tree.read(relative), signature).replace("PhysicalTrafficManager::", FIXTURE + "::"))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature.strip())
            return None
    parts.append("} }")
    return compile_and_run(Path(__file__).with_name("FxTrafficNetworkCatchup.cpp"), "network_catchup.inc",
                           "\n".join(parts), "FxTrafficNetworkCatchup")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtraffic_network_catchup", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
