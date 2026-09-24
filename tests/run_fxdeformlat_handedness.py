"""Regression for BodyPartBBoxSpec::HackCheckHandedness @0x825E6EA0 / BBoxPointSkinData::HackSwapHandedness
@0x825E6DB8 (crash parity G16-D1; prepared by FX-DEFORM-LAT, landed by FX-XLANE -- the two sources live in
src/SharedClasses/Physics/Deformation/).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_handedness.py [--pre-fix <b5 rev>] [--src <dir>]
--src <dir> reads BrnBodyPartBBoxSpec.cpp / BrnBBoxPointSkinData.cpp from <dir> instead of the tree.
The pre-fix stubs (b5 fd9d189f) fail 5/8 checks; the reconstruction passes 8/8.
"""
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import build_and_run, definition, pre_fix_rev, read

SHARED = "src/SharedClasses/Physics/Deformation"


def main():
    rev = pre_fix_rev(sys.argv)
    if "--src" in sys.argv:
        src = Path(sys.argv[sys.argv.index("--src") + 1])
        spec = (src / "BrnBodyPartBBoxSpec.cpp").read_text(encoding="utf-8-sig").replace("\r\n", "\n")
        skin = (src / "BrnBBoxPointSkinData.cpp").read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    else:
        spec = read(SHARED + "/BrnBodyPartBBoxSpec.cpp", rev)
        skin = read(SHARED + "/BrnBBoxPointSkinData.cpp", rev)
    inc = "\n".join(["namespace BrnPhysics { namespace Deformation {",
                     definition(skin, "void BBoxPointSkinData::HackSwapHandedness("),
                     definition(spec, "void BodyPartBBoxSpec::HackCheckHandedness()"),
                     "} }"])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatHandedness.cpp"), {"methods.inc": inc},
                       "fxdeformlat_handedness")
    sys.exit(rc)


if __name__ == "__main__":
    main()
