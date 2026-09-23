"""Regression for PhysicalBodyPart::GetJointRotationProportion @0x825C1B38 (crash parity G28-D3, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_joint_proportion.py [--pre-fix <b5 rev>]

The console computes rotation * refined-reciprocal(maxAngle) with no zero test (vrefp + two
vnmsubfp/vmaddfp Newton steps, 0x825C1BA8..0x825C1BBC): a zero max angle gives NaN. The tree's
invented `(m != 0) ? r/m : 0` returned 0. The shipped function and its Splat helper are extracted.
"""
import re
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, build_and_run, definition, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    tu = read(PHYS + "/BrnPhysicalBodyPart.cpp", rev)
    splat = re.search(r"inline Vector4 Splat\(f32 lfValue\)[^\n]*", tu).group(0)
    inc = "\n".join(["namespace BrnPhysics { namespace Deformation {",
                     "namespace {", splat, "}",
                     definition(tu, "    VecFloat PhysicalBodyPart::GetJointRotationProportion()"),
                     "} }"])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatJointProportion.cpp"), {"methods.inc": inc},
                       "fxdeformlat_joint_proportion", open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
