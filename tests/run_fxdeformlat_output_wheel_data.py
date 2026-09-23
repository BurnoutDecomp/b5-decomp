"""Regression for DeformableObject::OutputWheelData @0x82608E28 (crash parity G18-D1 + G18-D2, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_output_wheel_data.py [--pre-fix <b5 rev>]

G18-D1: the console's live arm publishes the wheel's body-point velocity (wheel+0xA0 -> entry+0x40)
and its spin (row0 * splat(mIntegrationVariables.x) -> entry+0x50); the tree left both at zero.
G18-D2: the console's live arm never touches the running entity-sphere size (v127); the tree folded
|wheel - car| into it. The shipped OutputWheelData and the TU's anonymous-namespace constants are
extracted and run against the real types (real WheelPhysicalStates::operator= and CgsStrStream).
"""
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, REPO, build_and_run, definition, anonymous_namespace, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    glass = read(PHYS + "/BrnDeformableObject_GlassState.cpp", rev)
    start = glass.index("namespace Deformation")
    inc = "\n".join([
        '#include "rw/math/vpu/vector3_operation.h"',
        '#include "rw/math/vpu/matrix44affine_operation.h"',
        "#include <cstdlib>",
        "namespace BrnPhysics { namespace Deformation {",
        "namespace vpu = rw::math::vpu;",
        anonymous_namespace(glass, start),
        definition(glass, "    void DeformableObject::OutputWheelData("),
        "} }",
    ])
    extra = [REPO / "src/GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.cpp",
             REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]
    rc = build_and_run(Path(__file__).with_name("FxDeformLatOutputWheelData.cpp"), {"methods.inc": inc},
                       "fxdeformlat_output_wheel_data", extra_sources=extra, open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
