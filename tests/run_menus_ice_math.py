"""OWNERLIST 2026-09-27, lane L5 MENUS: the ICE angle maths behind the pause / crash-nav camera's lens.

The pause camera is an ICE take played by ICEWrapper. ICECameraMover::UpdateLens @0x8252E548 turns the take's lens length
into the camera FOV. It calls ConvertLensLengthToFovAngle, which is ATan(lens, flt_8207B1F8 == 0x417F5C2A), and then AngToDeg.
Three defects on the PC:
  - ICEMath::ATan @0x8252ABD8 was std::atan2(first, second). The console divides the SECOND argument by the FIRST (x = f1,
    y = f2), through vrefp, one fused Newton step, a fused y*r and XMVectorATan @0x821F0A70, then fixes the quadrant.
    With the old order a 50 mm lens came out 72.3 degrees instead of 17.7.
  - ICE::Angle::SetFromVecFloat @0x8252A948 multiplied by 180.0f / 3.1415927f, which folds to 0x42652EE0. The image's
    word is flt_8207A908 == 0x42652EE1.
  - ConvertLensLengthToFovAngle and ICEMath::Angles::AngToDeg were declared with no body.

Numeric: tests/MenusIceMath.cpp compiles the revision's bodies against its own ICEMath.hpp. The expected values come from
scratch/OWNERLIST_0927/L5/ice_model.py, an independent model of the same asm. A body the revision lacks is linked as a
stub that returns an impossible value, so its checks FAIL instead of the build failing.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_menus_ice_math.py [--rev <b5 rev>]
        [--src-root <dir holding src/...>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

ICEMATH_CPP = "src/SDKs/Packages/ICE/ICEMath.cpp"
ICEMATH_HPP = "src/SDKs/Packages/ICE/ICEMath.hpp"
NUMERIC_CHECKS = 33

SET_FROM_VEC_FLOAT = "void Angle::SetFromVecFloat("
ATAN = "Angle ATan("
CONVERT = "Angle ConvertLensLengthToFovAngle("
ANG_TO_DEG = "f32 AngToDeg("

STUB_CONVERT = ("Angle ConvertLensLengthToFovAngle(f32)\n{\n    return Angle(static_cast<u16>(0xFFFFu));"
                "   // [test stub: no body in this revision]\n}\n")
STUB_ANG_TO_DEG = ("f32 AngToDeg(Angle)\n{\n    return FromBits(0x7FC00000u);   // [test stub: no body in this revision]\n}\n")


class RootTree(Tree):
    """A b5 source tree rooted somewhere else (e.g. a lane's shadow mirror): <root>/src/..."""

    def __init__(self, root):
        super().__init__(None)
        self.root = Path(root)

    def read(self, relative):
        path = self.root / relative
        if not path.exists():
            return super().read(relative)
        return path.read_text(encoding="utf-8-sig")


def body(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return None


def anonymous_constants(source):
    """The anonymous-namespace block that holds the SetFromVecFloat factors (KF_RADIANS_TO_DEGREES ...), if any."""
    marker = source.find("KF_RADIANS_TO_DEGREES =")
    if marker < 0:
        return ""
    start = source.rfind("namespace", 0, marker)
    if start < 0:
        return ""
    return definition(source, source[start:source.index("{", start) + 1]) + "\n"


def ice_constants(source):
    """The ICE-scope f32 constants the bodies read (TWO_PI_ANGLE, TWO_PI, ...), as the revision defines them."""
    lines = []
    for name in ("TWO_PI_ANGLE", "TWO_PI_DEG", "TWO_PI", "MILE"):
        match = re.search(r"^const\s+f32\s+" + name + r"\s*=\s*([^;]+);", source, re.M)
        if match:
            lines.append(f"const f32 {name} = {match.group(1).strip()};")
    return "\n".join(lines) + "\n"


def numeric(tree):
    source = tree.read(ICEMATH_CPP)
    if not source:
        print("NUMERIC: ICEMath.cpp not found in this revision")
        return None
    set_from = body(source, SET_FROM_VEC_FLOAT)
    atan = body(source, ATAN)
    if set_from is None or atan is None:
        print("NUMERIC: SetFromVecFloat / ATan not found in this revision")
        return None
    convert = body(source, CONVERT)
    ang_to_deg = body(source, ANG_TO_DEG)
    missing = [name for name, text in (("ConvertLensLengthToFovAngle", convert), ("AngToDeg", ang_to_deg))
               if text is None]
    if missing:
        print("NUMERIC: no body in this revision for " + ", ".join(missing) + " (stubbed: their checks fail)")
    inc = ("namespace ICE\n{\n" + ice_constants(source) + anonymous_constants(source) + set_from + "\n"
           "namespace ICEMath\n{\n" + atan + "\n" + (convert or STUB_CONVERT) + "\n"
           "namespace Angles\n{\n" + (ang_to_deg or STUB_ANG_TO_DEG) + "\n}\n}\n}\n")
    shadow = {ICEMATH_HPP: tree.read(ICEMATH_HPP)}
    return compile_and_run(Path(__file__).with_name("MenusIceMath.cpp"), "menus_ice_math.inc", inc, "MenusIceMath",
                           shadow=shadow)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-root", help="read the b5 sources from <dir>/src/... (a shadow mirror)")
    args = parser.parse_args()
    tree = RootTree(args.src_root) if args.src_root else Tree(args.rev)
    return report("run_menus_ice_math", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
