"""Regression for PhysicalBodyPart::Construct @0x825B4178 (crash parity G28-D1 + G28-D2, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_part_construct.py [--pre-fix <b5 rev>]

G28-D1: Construct's first store zeroes mLocalJointPositionPlusRotation (0x825B41A8 stvx128 v127(0),
r31, r9=0x160); the tree never wrote it. G28-D2: mRigidBodyId is seeded whole from qword_82F2A3A8 ==
~0ull (0x825B41A0 ld / 0x825B41AC std); the tree left the two low sub-fields zero. The shipped
Construct is extracted and run on a poisoned real PhysicalBodyPart.
"""
import re
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, build_and_run, definition, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    tu = read(PHYS + "/BrnPhysicalBodyPart_Construct.cpp", rev)
    mass = re.search(r"static const f32 KF_PART_CONSTRUCT_MASS\s*=[^;]+;", tu).group(0)
    inc = "\n".join(["namespace BrnPhysics { namespace Deformation {",
                     mass,
                     definition(tu, "    void PhysicalBodyPart::Construct()"),
                     "} }"])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatPartConstruct.cpp"), {"methods.inc": inc},
                       "fxdeformlat_part_construct", open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
