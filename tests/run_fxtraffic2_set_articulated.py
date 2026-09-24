"""FX-TRAFFIC2 (crash parity 2026-09-24, G39-D2 + G39-D3): PhysicalTrafficVehicle::SetArticulated @0x825F3B68.

  G39-D2  the hitch point: mArticulationPointLocal (+0) = InverseOfMatrixWithOrthonormal3x3(spec +0x610)
          applied to the first generic locator of type 29 (CAB) / 28 (TRAILER); "Failed to find
          articulation tag point" (:463) when absent, index still used. Before the fix nothing in the
          tree ever wrote mArticulationPointLocal.
  G39-D3  a FULL car's body +0x1050 lane w = KF_ARTICULATED_SOLVE_PENETRATION_WEIGHT_FACTOR (0.5,
          unk_8208FACC), lanes x/y/z untouched; a SIMPLE car's body untouched. Before the fix the lane
          kept Prepare's 1.0.

Numeric: tests/FxTraffic2SetArticulated.cpp compiled against the PRODUCTION SetArticulated,
GetFullTrafficPhysics and LocatorPointSpecList::GetLocatorXf (working tree, or --rev <b5 rev>), on
a real StreamedDeformationSpec whose locators sit below 4 GB. The pre-fix SetArticulated compiles
here, so the old body FAILS the numbers rather than failing to build.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_set_articulated.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

MANAGER_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.cpp"
SPEC_CPP = "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.cpp"
NUMERIC_CHECKS = 10
WEIGHT = r"const f32 KF_ARTICULATED_SOLVE_PENETRATION_WEIGHT_FACTOR\s*=\s*[^;]+;"


def numeric(tree):
    try:
        manager = tree.read(MANAGER_CPP)
        parts = [
            "namespace BrnPhysics { namespace Deformation {",
            definition(tree.read(SPEC_CPP), "    const Matrix44Affine* LocatorPointSpecList::GetLocatorXf(u32 luTag) const"),
            "} }",
            "namespace BrnPhysics { namespace Vehicle {",
        ]
        # The weight constant is new with G39-D3; the pre-fix SetArticulated never names it.
        weight = re.search(WEIGHT, manager)
        if weight:
            parts.append(weight.group(0))
        parts += [
            definition(manager, "TrafficPhysics* PhysicalTrafficVehicle::GetFullTrafficPhysics()"),
            definition(manager, "void PhysicalTrafficVehicle::SetArticulated("),
            "} }",
        ]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic2SetArticulated.cpp"), "set_articulated.inc",
                           "\n".join(parts), "FxTraffic2SetArticulated")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtraffic2_set_articulated", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
