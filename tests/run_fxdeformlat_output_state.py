"""Regression for DeformableObject::OutputState @0x825C1EA8 (crash parity G17-D1, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_output_state.py [--pre-fix <b5 rev>]

The console merges the WHEEL's position Y lane into the local tag point (vrlimi128 v127,v0,4,0
@0x825C2214; PS3 vperm<0,5,2,3> @0x6F4190); the tree merged the W lane. The shipped OutputState,
the TU's anonymous-namespace constants and StreamedDeformationSpec::GetWheelSpec are extracted
and run against the real types.
"""
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, build_and_run, definition, anonymous_namespace, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    glass = read(PHYS + "/BrnDeformableObject_GlassState.cpp", rev)
    spec = read(PHYS + "/BrnStreamedDeformationSpec.cpp", rev)
    start = glass.index("namespace Deformation")
    inc = "\n".join([
        '#include "rw/math/vpu/vector3_operation.h"',
        '#include "rw/math/vpu/matrix44affine_operation.h"',
        "namespace BrnPhysics { namespace Deformation {",
        "namespace vpu = rw::math::vpu;",
        anonymous_namespace(glass, start),
        definition(spec, "    const WheelSpec* StreamedDeformationSpec::GetWheelSpec("),
        definition(glass, "    void DeformableObject::OutputState("),
        "} }",
    ])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatOutputState.cpp"), {"methods.inc": inc},
                       "fxdeformlat_output_state", open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
