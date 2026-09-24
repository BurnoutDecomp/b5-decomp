"""FX-TRAFFIC2 (crash parity 2026-09-24, G32-D1): PhysicalTrafficManager::ValidateTrafficContact
@0x825CACB8 accepts every world contact of a traffic car with a detached wheel.

  0x825CAE98  lbValid = !IsContactBelowWheelPlane(mPointOnB, 0.4)
  0x825CAEAC  lbz r11, 0x715(body)   (SimpleVehiclePhysics::mbAnyWheelsDetatched, DWARF h:373;
                                      accessor AreAnyWheelsDetatched() h:324)
  0x825CAEB4  bne -> 0x825CAF4C li r3, 1
Before the fix the arm was a GATE comment, so a wheel-less upright car's road contacts were dropped.

Numeric: tests/FxTraffic2WheelsDetached.cpp compiled against the PRODUCTION ValidateTrafficContact,
GetTrafficVehicle and SimpleVehiclePhysics::IsContactBelowWheelPlane (working tree, or --rev <b5 rev>)
on a fixture with the manager's real member types.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_wheels_detached.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

MANAGER_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.cpp"
SVP_CPP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp"
FIXTURE = "ContactFixture"
NUMERIC_CHECKS = 7


def numeric(tree):
    try:
        manager = tree.read(MANAGER_CPP)
        get_vehicle = definition(manager, "PhysicalTrafficVehicle* PhysicalTrafficManager::GetTrafficVehicle(s32 liVehicle)")
        validate = definition(manager, "bool PhysicalTrafficManager::ValidateTrafficContact(")
        below = definition(tree.read(SVP_CPP),
                           "    bool SimpleVehiclePhysics::IsContactBelowWheelPlane(Vector3 lvContactPoint, VecFloat lvfThreshold) const")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    parts = ["namespace BrnPhysics { namespace Vehicle {",
             get_vehicle.replace("PhysicalTrafficManager::", FIXTURE + "::"),
             validate.replace("PhysicalTrafficManager::", FIXTURE + "::"),
             below,
             "} }"]
    return compile_and_run(Path(__file__).with_name("FxTraffic2WheelsDetached.cpp"), "wheels_detached.inc",
                           "\n".join(parts), "FxTraffic2WheelsDetached",
                           extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtraffic2_wheels_detached", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
