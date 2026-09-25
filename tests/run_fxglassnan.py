"""FX-GLASSNAN (crash parity wave 5, 2026-09-26): every 'Glass_shattering' LION effect spawned its particles at NaN.

BrnEffects::BrnEffectsGlassManager::FireGlassEffect @0x82295D50 caches the shatter in its car's frame,
mLocalTransform = lEffectTransform * Inverse(lVehicleTransform), and ::UpdateVehicleEffectPositions @0x8228D208 re-seats
the effect every frame as mLocalTransform * the car's transform. The console INLINES the affine rw::math::vpu::Inverse
and operator* (0x82295E70..0x82295FC0; 0x8228D3D8..0x8228D478 / 0x8228D5DC..0x8228D67C): they read the x, y, z lanes
only. The PC called the general 4x4 Inverse @0x825B2628 (on the console: the shadow map and three debug components
only). A Matrix44Affine's w column is (0,0,0,0) -- the console's own SetIdentity inlined in Reset @0x8228F298 writes it,
and so do the PC's producers -- so its 4x4 determinant is 0, and every entry came out 0 * inf = NaN.

  1. WIRING -- FireGlassEffect takes the AFFINE inverse and product (no general 4x4 Inverse, no determinant, no NaN
     guard the console lacks); the re-seat uses the same product in both of its arms.
  2. NUMERIC -- tests/FxGlassNan.cpp compiles the PRODUCTION BrnEffectsGlassManager.cpp (and the PC's own
     SimpleVehiclePhysics::GetGraphicsVehicleTransform for the producer chain) onto a fixture, links the vendor
     Matrix44Operation.cpp (the 4x4 inverse the pre-fix body calls), and compares the results bit for bit with the
     console's own outputs in tests/FxGlassNanData.h (FireGlassEffect / UpdateVehicleEffectPositions run whole on
     emu64 by FX-GLASSNAN's gen_glassnan_data.py): 26 fire cases x 4 checks, 3 re-seat cases x 4 checks, 3
     producer-chain checks. The live case is tests/FxGlassNanLive.ps1.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxglassnan.py [--rev <b5 rev>]
                                                                                        [--root <shadow tree root>]
(--root reads any file present under <root>/... in place of the working tree's: the local gate for an edit that has
not reached the shared tree yet.)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

GLASS_CPP = "src/GameSource/Effects/BrnEffectsGlassManager.cpp"
GLASS_H = "src/GameSource/Effects/BrnEffectsGlassManager.h"
SIMPLE_CPP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp"
MATRIX44_CPP = "vendor/renderware/src/rw/math/vpu/Matrix44Operation.cpp"
PRODUCER = ("static Vector3 RotateCOMOffsetToWorld(", "static bool IsTransformValid(",
            "Matrix44Affine SimpleVehiclePhysics::GetGraphicsVehicleTransform() const")

FIRE_SIGNATURE = "void BrnEffectsGlassManager::FireGlassEffect("
UPDATE_SIGNATURE = "void BrnEffectsGlassManager::UpdateVehicleEffectPositions("
MATHS = ("Matrix44Affine InverseAffine(", "Matrix44Affine MultiplyAffine(", "Vector3 RotateRow(", "f32 RefinedRecip(",
         "f32 Dot3(", "Vector3 CrossPermuted(")

# 26 fire cases x 4 + 3 re-seat cases x 4 + 3 producer-chain checks (see FxGlassNan.cpp)
NUMERIC_CHECKS = 26 * 4 + 3 * 4 + 3


class RootTree(Tree):
    """The working tree (or --rev) with an optional shadow root whose files take precedence."""

    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig")
        try:
            return super().read(relative)
        except FileNotFoundError:
            return ""


def normalised(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def without_includes(text):
    """The file with its #include lines dropped: the fixture supplies every type the bodies name."""
    return "\n".join(line for line in text.split("\n") if not re.match(r"\s*#\s*include\b", line)) + "\n"


def wiring(tree):
    glass = normalised(tree, GLASS_CPP)
    code = code_only(glass)
    try:
        fire = code_only(definition(glass, FIRE_SIGNATURE))
        update = code_only(definition(glass, UPDATE_SIGNATURE))
    except ValueError:
        fire = update = ""
    yield ("FireGlassEffect caches the shatter with the AFFINE inverse and product (0x82295E70..0x82295FC0): it no "
           "longer calls the general 4x4 rw::math::vpu::Inverse @0x825B2628, and the file no longer declares it",
           fire != "" and "InverseAffine(lVehicleTransform)" in fire
           and re.search(r"MultiplyAffine\(\s*lEffectTransform\s*,", fire) is not None
           and "Matrix44" not in fire.replace("Matrix44Affine", "")
           and re.search(r"Matrix44\s+Inverse\s*\(", code) is None and "lDeterminant" not in fire)
    yield ("UpdateVehicleEffectPositions re-seats with the same product in both arms (0x8228D3D8 / 0x8228D5DC)",
           update.count("MultiplyAffine(lrEffect.mLocalTransform,") == 2 and " * lr" not in update)
    maths = []
    for signature in MATHS:
        try:
            maths.append(code_only(definition(glass, signature)))
        except ValueError:
            maths = []
            break
    yield ("no NaN / singular-matrix guard the console lacks: FireGlassEffect, the inverse and the product test "
           "neither the determinant nor a lane for NaN",
           fire != "" and not re.search(r"isnan|isfinite|_finite|fpclassify|== *0\.0f|!= *0\.0f", fire)
           and len(maths) == len(MATHS)
           and not any(re.search(r"isnan|isfinite|_finite|fpclassify|== *0\.0f|!= *0\.0f", body) for body in maths))


def numeric(tree):
    glass = normalised(tree, GLASS_CPP)
    simple = normalised(tree, SIMPLE_CPP)
    if not glass:
        print("NUMERIC: cannot build -- no " + GLASS_CPP)
        return None
    producer = []
    for signature in PRODUCER:
        try:
            producer.append(definition(simple, signature))
        except ValueError:
            print("NUMERIC: cannot build -- no " + signature)
            return None
    shadow = {GLASS_H: tree.read(GLASS_H)} if tree.read(GLASS_H) else {}
    # The vendor 4x4 inverse the pre-fix body calls is linked from the working tree (the file is not part of this fix,
    # so HEAD~1's copy is the same text).
    return compile_and_run(Path(__file__).with_name("FxGlassNan.cpp"), "fxglassnan_body.inc", without_includes(glass),
                           "FxGlassNan", shadow=shadow, extra_sources=(REPO / MATRIX44_CPP,),
                           extra_files={"fxglassnan_producer.inc": "\n".join(producer) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxglassnan", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
