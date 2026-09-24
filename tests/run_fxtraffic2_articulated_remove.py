"""FX-TRAFFIC2 (crash parity 2026-09-24, G34-D3): removing one half of an articulated pair.

  PhysicalTrafficManager::RemoveTrafficVehicle @0x8261CC98 -- the HasNonBrokenJoint arm
      0x8261CDF4..0x8261CFBC (was a logged gate): find the other half through the joint pool, clear
      its type/state/joint to NONE/NONE/-1, and ArticulatedJointPool::RemoveJoint the joint
  ArticulatedJointPool::RemoveJoint @0x825D8248 (no body before): FlagJointToBeRemoved(i, packed id)
      then mUsedJoints.UnSetBit(i)

Wiring: the pool header declares RemoveJoint; the arm in RemoveTrafficVehicle no longer logs a gate.
Numeric: tests/FxTraffic2ArticulatedRemove.cpp compiled against the PRODUCTION bodies extracted from
the source (working tree, or --rev <b5 rev>) on a fixture with the manager's real members. The
pre-fix RemoveTrafficVehicle still compiles here (its gate macro is stubbed), so the old body FAILS
the numbers rather than failing to build.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_articulated_remove.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report

MANAGER_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.cpp"
REMOVE_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_Remove.cpp"
IO_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.cpp"
POOL_H = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h"
POOL_CPP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.cpp"
JOINT_CPP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.cpp"
FIXTURE = "RemoveFixture"
NUMERIC_CHECKS = 10


def wiring(tree):
    checks = []
    header = code_only(tree.read(POOL_H))
    checks.append(("ArticulatedJointPool declares RemoveJoint(ArticulatedJointCreateBuffer*, s32) (DWARF :98)",
                   re.search(r"void\s+RemoveJoint\(\s*ArticulatedJointCreateBuffer\*\s*\w+\s*,\s*s32\s+\w+\s*\)\s*;",
                             header) is not None))
    remove = body_or_empty(tree.read(REMOVE_CPP), "    void PhysicalTrafficManager::RemoveTrafficVehicle(")
    checks.append(("RemoveTrafficVehicle's HasNonBrokenJoint arm calls mArticulatedJointPool.RemoveJoint and no "
                   "longer logs the articulated-teardown gate (0x8261CFBC)",
                   "mArticulatedJointPool.RemoveJoint(mpArticulatedJointCreateBuffer, liJointIndex);" in remove
                   and "BRN_T3_REMOVE_GATE" not in remove))
    return checks


def numeric(tree):
    try:
        manager = tree.read(MANAGER_CPP)
        pool = tree.read(POOL_CPP)
        parts = [
            "namespace BrnPhysics { namespace Vehicle {",
            definition(manager, "PhysicalTrafficVehicle* PhysicalTrafficManager::GetTrafficVehicle(s32 liVehicle)")
            .replace("PhysicalTrafficManager::", FIXTURE + "::", 1),
            definition(manager, "PhysicalTrafficVehicle::EArticulatedVehicleType "
                                "PhysicalTrafficVehicle::GetArticulatedVehicleType() const"),
            definition(manager, "bool PhysicalTrafficVehicle::HasNonBrokenJoint() const"),
            definition(pool, "ArticulatedJoint* ArticulatedJointPool::GetJoint(s32 liJointIndex)"),
            definition(pool, "bool ArticulatedJointPool::IsJointInUse(s32 liJointIndex) const"),
            definition(pool, "    ArticulatedJointId ArticulatedJoint::GetJointId() const"),
            definition(pool, "    u16 ArticulatedJointId::GetJointPoolIndex() const"),
            definition(pool, "    u16 ArticulatedJointId::GetTrailerVehicleIndex() const"),
            definition(pool, "    s32 ArticulatedJointPool::GetIndexOfOtherHalf("),
            definition(tree.read(JOINT_CPP), "    EntityId ArticulatedJointId::GetCabEntityId() const"),
            definition(tree.read(IO_CPP), "    void ArticulatedJointCreateBuffer::FlagJointToBeRemoved("),
        ]
        # RemoveJoint is new with the fix; the pre-fix RemoveTrafficVehicle never calls it.
        if "void ArticulatedJointPool::RemoveJoint(" in pool:
            parts.append(definition(pool, "void ArticulatedJointPool::RemoveJoint("))
        parts.append(definition(tree.read(REMOVE_CPP), "    void PhysicalTrafficManager::RemoveTrafficVehicle(")
                     .replace("PhysicalTrafficManager::", FIXTURE + "::", 1))
        parts.append("} }")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic2ArticulatedRemove.cpp"), "articulated_remove.inc",
                           "\n".join(parts), "FxTraffic2ArticulatedRemove")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic2_articulated_remove", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
