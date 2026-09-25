"""FX-GATE (crash parity 2026-09-25): BehaviourAftertouchCrash::Update builds its four rotations with the console's
sine and cosine, through file-local builders; the shared vendor MakeRotationX/Y/Z are left alone.

  random start  0x822283A0..0x822284E0  inlined XMVectorSinCos -> RotationYAxisZ   (sin, 0, cos)
  orbit         0x82228724 bl XMMatrixRotationY @0x82203560 -> XMScalarSinCos -> XMMatrixRotationY
  pitch         0x82228EF8..0x822290BC  inlined XMVectorSinCos -> RotationX        rows (1,0,0) / (0,c,s) / (0,-s,c)
  roll          0x822292DC..0x82229448  inlined XMVectorSinCos -> RotationZ        rows (c,s,0) / (-s,c,0) / (0,0,1)

  1. WIRING -- the four Update sites call the builders; no vendor MakeRotation is left in Update.
  2. NUMERIC -- the builders are EXTRACTED and compared bit for bit with tests/FxGateAftertouchSinCosData.h: 150
     angles per site, each the console's own words run on emu64 (scratch/CRASHPARITY_0922/fixes/FX-GATE.sincos/
     gen_atc_data.py).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_aftertouch_sincos.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

ATC = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp"
UPDATE = "bool BehaviourAftertouchCrash::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)"
BUILDERS = ["Vector3 RotationYAxisZ(f32 lfAngleRads)", "Matrix44Affine RotationX(f32 lfAngleRads)",
            "Matrix44Affine RotationZ(f32 lfAngleRads)", "Matrix44Affine XMMatrixRotationY(f32 lfAngleRads)"]
CALLS = [
    ("random start 0x822283A0", "mManualCameraDirection=RotationYAxisZ(lrSharedInfo.mpRandom->RandomFloat());"),
    ("orbit bl XMMatrixRotationY 0x82228724", "XMMatrixRotationY(lOrbitStick.x*KF_CAMERA_X_ROTATION_SPEED)"),
    ("pitch 0x82228EF8", "RotationX(lrParameters.mfPitch*KF_DEGS_TO_RADS)"),
    ("roll 0x822292DC", "RotationZ(mfRollAngleRads)"),
]
NUMERIC_CHECKS = 600


def update_body(tree):
    try:
        return code_only(definition(tree.read(ATC).replace("\r\n", "\n"), UPDATE))
    except ValueError:
        return None


def wiring(tree):
    body = update_body(tree)
    flat = "" if body is None else re.sub(r"\s+", "", body)
    for label, call in CALLS:
        # the builder's own name, not a vendor MakeRotation* that ends with it
        found = re.search(r"(?<![\w:])" + re.escape(call), flat) is not None
        yield (f"{label}: {call}", found)
    yield ("no vendor MakeRotationX/Y/Z left in Update", body is not None and "MakeRotation" not in flat)


def numeric(tree):
    source = tree.read(ATC).replace("\r\n", "\n")
    parts = []
    for signature in BUILDERS:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            print(f"NUMERIC: cannot build -- `{signature}` is not in the source")
            return None
    return compile_and_run(Path(__file__).with_name("FxGateAftertouchSinCos.cpp"), "fxgate_aftertouch_sincos.inc",
                           "\n\n".join(parts), "FxGateAftertouchSinCos")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_aftertouch_sincos", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
