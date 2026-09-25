"""FX-GATE (crash parity 2026-09-25): VehiclePhysics builds the steered wheel direction R(Up, angle) * At the way
the console does, at both sites: the inlined XMVectorSinCos (XboxMath::XMVectorSinCos), then the Rodrigues rows and
the product with At in the console's roundings (the file-local SteeredDirection).

  VehiclePhysics::SetWheelVelocities  0x825FD4A4..0x825FD530  (the wheel spin re-seed)
  VehiclePhysics::UpdateWheels        0x8261E6F0..0x8261E79C  (mSteeringDirection +0x10E0, every tick)
  each a*b + c row term is ONE vmaddfp (ROUNDING_RULE 3); each a*b - c is vmulfp128 then vsubfp (rule 4);
  direction = row2 * At.z + (row1 * At.y + row0 * At.x): vmulfp128 then two vmaddfp.

  1. WIRING -- both sites call SteeredDirection(lvUp, lvAt, lfSin, lfCos); the builder spells each fused row term
     and the fused At cascade.
  2. NUMERIC -- each site's span (angle -> direction) is EXTRACTED with the builder and compared bit for bit with
     tests/FxGateSteeredDirectionData.h: 300 rows, each the console's own words of BOTH blocks run on emu64
     (scratch/CRASHPARITY_0922/fixes/FX-GATE.sincos/gen_steerdir_data.py).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_steered_direction.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

VP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
HELPER = "inline Vector3 SteeredDirection("
SITES = [
    ("SetWheelVelocities", "void VehiclePhysics::SetWheelVelocities(Vector3 lvVelocity)",
     "const Vector3 lvSteeredDirection", "lvSteeredDirection",
     "constVector3lvSteeredDirection=SteeredDirection(lvUp,lvAt,lfSin,lfCos);"),
    ("UpdateWheels", "void VehiclePhysics::UpdateWheels(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep)",
     "mSteeringDirection =", "mSteeringDirection",
     "mSteeringDirection=SteeredDirection(lvUp,lvAt,lfSin,lfCos);"),
]
HELPER_TERMS = [
    "std::fmaf(lfTx,lrUp.x,lfCos)", "std::fmaf(lfTx,lrUp.y,lfSz)", "lfTx*lrUp.z-lfSy",
    "lfTy*lrUp.x-lfSz", "std::fmaf(lfTy,lrUp.y,lfCos)", "std::fmaf(lfTy,lrUp.z,lfSx)",
    "std::fmaf(lfTz,lrUp.x,lfSy)", "lfTz*lrUp.y-lfSx", "std::fmaf(lfTz,lrUp.z,lfCos)",
    "std::fmaf(lvRow2.x,lrAt.z,std::fmaf(lvRow1.x,lrAt.y,lvRow0.x*lrAt.x))",
    "std::fmaf(lvRow2.y,lrAt.z,std::fmaf(lvRow1.y,lrAt.y,lvRow0.y*lrAt.x))",
    "std::fmaf(lvRow2.z,lrAt.z,std::fmaf(lvRow1.z,lrAt.y,lvRow0.z*lrAt.x))",
]
NUMERIC_CHECKS = 600


def source(tree):
    return tree.read(VP).replace("\r\n", "\n")


def span(tree, signature, anchor):
    try:
        body = definition(source(tree), signature)
    except ValueError:
        return None
    s = body.find("const f32 lfSteerAngle")
    a = body.find(anchor, s)
    if s < 0 or a < 0:
        return None
    e = body.find(";", a)
    return None if e < 0 else code_only(body[s:e + 1])


def helper(tree):
    text = source(tree)
    return definition(text, HELPER) if HELPER in text else None


def wiring(tree):
    for name, signature, anchor, _, call in SITES:
        text = span(tree, signature, anchor)
        flat = "" if text is None else re.sub(r"\s+", "", text)
        yield (f"{name}: {call}", call in flat)
    h = helper(tree)
    flat = "" if h is None else re.sub(r"\s+", "", code_only(h))
    for term in HELPER_TERMS:
        yield (f"SteeredDirection: {term}", term in flat)


def numeric(tree):
    parts = []
    h = helper(tree)
    if h is not None:
        parts.append(h)
    for name, signature, anchor, result, _ in SITES:
        text = span(tree, signature, anchor)
        if text is None:
            print(f"NUMERIC: cannot build -- {name}'s steering span was not found")
            return None
        member = "    Vector3 mSteeringDirection;\n" if result == "mSteeringDirection" else ""
        parts.append(f"static void K_{name}(f32 lfAngle, const Vector3& lvUp, const Vector3& lvAt, Vector3* lpOut)\n{{\n"
                     "    SteeringVector mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount = { lfAngle, 0.0f, 0.0f, 0.0f };\n"
                     f"{member}    {text}\n    *lpOut = {result};\n}}\n")
    return compile_and_run(Path(__file__).with_name("FxGateSteeredDirection.cpp"), "fxgate_steered_direction.inc",
                           "\n".join(parts), "FxGateSteeredDirection")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_steered_direction", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
