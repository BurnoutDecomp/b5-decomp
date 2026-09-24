"""Regression for the three rw::collision guard epsilons (crash parity H2-D3 / H2-D4 / H2-D5, FX-XLANE).

The rows were an inferred 1.0e-12f; the image says (each a .bss splat written by a CRT dyn-init thunk):
  unk_8327EEA0 = splat(flt_821801B4 = 0x34000000 FLT_EPSILON)  thunk 0x82C73BD0  -> FeatureEdge ctor
  unk_8327EFD0 = splat(flt_82180A28 = 0x34000000 FLT_EPSILON)  thunk 0x82C73E10  -> Feature::BuildEdgePlanes
  unk_8327EFB0 = splat(flt_82180894 = 0x00800000 FLT_MIN)      thunk 0x82C73DA8  -> RimToEdge / RimToRim

FeatureEdge.cpp and Feature.cpp are compiled whole from the revision under test; SeparatingDirection.cpp's
helper namespace, its KF_RIM_* constants and the production RimRadialDirection are extracted into a
generated TU that exports one wrapper.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxxlane_rw_epsilons.py [--pre-fix <b5 rev>]
The inferred 1e-12 rows (b5 before the fix, e.g. 24ba8ece) fail 4/7 checks; the image values pass 7/7.
"""
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import anonymous_namespace, build_and_run, definition, pre_fix_rev, read

COLL = "src/vendor/renderware/collision/"


def main():
    rev = pre_fix_rev(sys.argv)
    edge = read(COLL + "FeatureEdge.cpp", rev)
    feature = read(COLL + "Feature.cpp", rev)
    sep = read(COLL + "SeparatingDirection.cpp", rev)

    constants = "\n".join(re.findall(r"^static const f32 KF_RIM_[A-Z_]+\s*=\s*[^;]+;", sep, re.M))
    rim = "\n".join([
        '#include "vendor/renderware/collision/GPInstance.hpp"',
        "#include <cmath>",
        "namespace rw { namespace collision {",
        anonymous_namespace(sep),
        constants,
        "namespace {",
        definition(sep, "    inline Vec4 RimRadialDirection("),
        "}",
        "Vec4 FxXlaneRimRadialDirection(const Vec4& arV) { return RimRadialDirection(arV); }",
        "} }",
    ])
    pieces = {"fe.cpp": edge, "fb.cpp": feature, "rim.cpp": rim}
    rc = build_and_run(Path(__file__).with_name("FxXlaneRwEpsilons.cpp"), pieces, "fxxlane_rw_epsilons",
                       extra_sources=["fe.cpp", "fb.cpp", "rim.cpp"])
    sys.exit(rc)


if __name__ == "__main__":
    main()
