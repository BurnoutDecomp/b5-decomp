"""FX-GATE (crash parity 2026-09-25): VehiclePhysics takes the steering angle's sine and cosine from the console's
inlined XMVectorSinCos, XboxMath::XMVectorSinCos (e13f4b71), not from std::sin / std::cos.

  VehiclePhysics::SetWheelVelocities  0x825FD2F4..0x825FD4B0  (sine v10, cosine v11)
  VehiclePhysics::UpdateWheels        0x8261E524..0x8261E710  (sine v10, cosine v11; mSteeringDirection, every tick)

  1. WIRING -- each function's SinCos span calls XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfSteerAngle) and has no
     std::sin / std::cos.
  2. NUMERIC -- each span is EXTRACTED into a kernel and compared bit for bit with tests/FxGateSteerSinCosData.h:
     400 angles, each the console's own words of BOTH blocks run on emu64 (scratch/CRASHPARITY_0922/fixes/
     FX-GATE.sincos/gen_vp_data.py).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_steer_sincos.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

VP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
SITES = [
    ("SetWheelVelocities", "void VehiclePhysics::SetWheelVelocities(Vector3 lvVelocity)", "0x825FD2F4"),
    ("UpdateWheels", "void VehiclePhysics::UpdateWheels(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep)",
     "0x8261E524"),
]
START = "const f32 lfSteerAngle"
# The span ends where the rotation starts: `const f32 lfOneMinusCos` up to d6efe5e1; since the Rodrigues follow-up,
# the SteeredDirection call (`const Vector3 lvSteeredDirection` / `mSteeringDirection =`).
ENDS = ("const f32 lfOneMinusCos", "const Vector3 lvSteeredDirection", "mSteeringDirection =")
CALL = "XboxMath::XMVectorSinCos(&lfSin,&lfCos,lfSteerAngle);"
NUMERIC_CHECKS = 800


def span(tree, signature):
    try:
        body = definition(tree.read(VP).replace("\r\n", "\n"), signature)
    except ValueError:
        return None
    s = body.find(START)
    if s < 0:
        return None
    ends = [e for e in (body.find(end, s) for end in ENDS) if e >= 0]
    if not ends:
        return None
    return code_only(body[s:min(ends)])


def wiring(tree):
    for name, signature, address in SITES:
        text = span(tree, signature)
        flat = "" if text is None else re.sub(r"\s+", "", text)
        yield (f"{name}: the inlined XMVectorSinCos {address} -> {CALL}", CALL in flat)
        yield (f"{name}: no std::sin / std::cos in the span", text is not None and "std::sin(" not in flat
               and "std::cos(" not in flat)


def numeric(tree):
    parts = []
    for name, signature, _ in SITES:
        text = span(tree, signature)
        if text is None:
            print(f"NUMERIC: cannot build -- {name}'s SinCos span was not found")
            return None
        parts.append(f"static void K_{name}(f32 lfAngle, f32* lpfSin, f32* lpfCos)\n{{\n"
                     "    SteeringVector mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount = { lfAngle, 0.0f, 0.0f, 0.0f };\n"
                     f"    {text}\n    *lpfSin = lfSin;\n    *lpfCos = lfCos;\n}}\n")
    return compile_and_run(Path(__file__).with_name("FxGateSteerSinCos.cpp"), "fxgate_steer_sincos.inc",
                           "\n".join(parts), "FxGateSteerSinCos")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_steer_sincos", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
