"""FX-GATE (crash parity 2026-09-25): SteeringFan::GenerateFanVectors (X360 0x827792C0) builds its 17 rays the way
the console does.

  0x8277948C  fmadds f0, f0, f13, f28      angle = interp^3 * mfFanAngle + base, ONE rounding (ROUNDING_RULE 3)
  0x8277955C..0x827796BC                   the XDK's XMVectorSinCos, inlined -> XboxMath::XMVectorSinCos
  0x827796F4  vmaddfp (raw v13, v0, v13, v12)   mTarget    = unit * mfLookAheadRadius    + origin (rule 3)
  0x82779724  vmaddfp (raw v0, v0, v13, v12)    mHNGTarget = unit * mfLookAheadHNGRadius + origin (rule 3)

  1. WIRING -- the loop spells each of those, and no std::sin / std::cos is left in it.
  2. NUMERIC -- the loop body is EXTRACTED into a fixture and run on tests/FxGateFanVectorsData.h: 240 rows, each
     the console's own words 0x82779474..0x8277972C run on emu64 (scratch/CRASHPARITY_0922/fixes/FX-GATE.sincos/
     gen_fan_data.py), compared bit for bit on the unit direction and both targets.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_fan_vectors.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

FAN = "src/GameSource/World/AI/RacingLine/BrnAISteeringFan_Weightings.cpp"
SIGNATURE = "void SteeringFan::GenerateFanVectors(AICar* lpCar)"
START = "const f32 lfInterp"
END = "lfT += mfReciprocalSteps;"
NUMERIC_CHECKS = 240

EXPECT = [
    ("fmadds 0x8277948C", "constf32lfAngle=std::fmaf(lfInterp*lfInterp*lfInterp,mfFanAngle,lfBaseAngle);"),
    ("XMVectorSinCos 0x8277955C", "XboxMath::XMVectorSinCos(&lfSin,&lfCos,lfAngle);"),
    ("vmaddfp 0x827796F4 x", "mTarget[liStep].x=std::fmaf(lUnit.x,mfLookAheadRadius,mFanOrigin2D.x);"),
    ("vmaddfp 0x827796F4 y", "mTarget[liStep].y=std::fmaf(lUnit.y,mfLookAheadRadius,mFanOrigin2D.y);"),
    ("vmaddfp 0x82779724 x", "mHNGTarget[liStep].x=std::fmaf(lUnit.x,mfLookAheadHNGRadius,mFanOrigin2D.x);"),
    ("vmaddfp 0x82779724 y", "mHNGTarget[liStep].y=std::fmaf(lUnit.y,mfLookAheadHNGRadius,mFanOrigin2D.y);"),
]


def loop_body(tree):
    try:
        body = definition(tree.read(FAN).replace("\r\n", "\n"), SIGNATURE)
    except ValueError:
        return None
    s = body.find(START)
    e = body.find(END, s)
    if s < 0 or e < 0:
        return None
    return body[s:e]


def wiring(tree):
    body = loop_body(tree)
    flat = "" if body is None else re.sub(r"\s+", "", code_only(body))
    for label, text in EXPECT:
        yield (f"{label}: {text}", text in flat)
    yield ("no std::sin / std::cos left in the ray loop", body is not None and "std::sin(" not in flat
           and "std::cos(" not in flat)


def numeric(tree):
    body = loop_body(tree)
    if body is None:
        print("NUMERIC: cannot build -- the ray loop was not found")
        return None
    inc = ("struct FanFixture\n{\n"
           "    f32 mfFanAngle, mfLookAheadRadius, mfLookAheadHNGRadius;\n"
           "    Vector2 mFanOrigin2D;\n"
           "    Vector2 mUnitDirection[KI_FAN_STEPS], mTarget[KI_FAN_STEPS], mHNGTarget[KI_FAN_STEPS];\n"
           "    void Body(f32 lfT, f32 lfBaseAngle, s32 liStep)\n    {\n        " + body + "\n    }\n};\n")
    return compile_and_run(Path(__file__).with_name("FxGateFanVectors.cpp"), "fxgate_fan_vectors.inc", inc,
                           "FxGateFanVectors")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_fan_vectors", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
