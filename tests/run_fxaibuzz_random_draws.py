"""FX-AIBUZZ reviewer-G item (a) (crash parity 2026-09-25): CgsNumeric::Random's bounded draws fuse their range map.

The console inlines RandomFloat(min, max) and RandomVector(min, max) everywhere. Their combine is ONE multiply-add,
which rounds once:
  RandomFloat   fsubs (max - min) ; fmadds D, range, t, min   CameraShake::Update @0x822213BC,
                BehaviourFixedCam @0x8222A0F8, ParticleModule @0x82281AD0, ClutchControl @0x826CDFC0 / @0x826CE07C
  RandomVector  vsubfp v6, v10, v12 @0x82221570 ; vmaddfp v12, v8, v12, v6 @0x822215CC (classic raw field order
                D, A, B, C = A * C + B: t * (max - min) + min, all four lanes)
The PC wrote `(max - min) * t + min`, which rounds the product first.

  1. WIRING -- RandomFloat returns std::fmaf(lfMax - lfMin, lfUnitValue, lfMin); RandomVector builds each of its four
     lanes with std::fmaf(lMax.c - lMin.c, lUnitValue.c, lMin.c) and no unfused lane remains.
  2. NUMERIC -- tests/FxAiBuzzRandomDraws.cpp runs the two extracted production bodies on the real Random layout:
     6 scalar and 12 vector-lane double-rounding counterexamples plus 2 scalar controls, each against the exactly
     rounded result, and the ring / seed / cursor every draw leaves behind.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaibuzz_random_draws.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, compile_and_run, definition, report

RANDOM_CPP = "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
RANDOM_H = "src/GameShared/GameClasses/Numeric/CgsRandom.h"
SCALAR = "f32 Random::RandomFloat(f32 lfMin, f32 lfMax)"
VECTOR = "rw::math::vpu::Vector3 Random::RandomVector(rw::math::vpu::Vector3 lMin,"
NUMERIC_CHECKS = 31


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(RANDOM_CPP).replace("\r\n", "\n")
    scalar = squash(body_or_empty(source, SCALAR))
    yield ("RandomFloat(min, max) returns std::fmaf(max - min, t, min) (fmadds @0x822213BC / 0x8222A0F8 / 0x82281AD0 / "
           "0x826CDFC0 / 0x826CE07C)", scalar.endswith("returnstd::fmaf(lfMax-lfMin,lfUnitValue,lfMin);}"))
    vector = squash(body_or_empty(source, VECTOR))
    lanes = all(f"lResult.{c}=std::fmaf(lMax.{c}-lMin.{c},lUnitValue.{c},lMin.{c});" in vector for c in "xyzw")
    yield ("RandomVector(min, max) fuses every lane: std::fmaf(max - min, t, min) (vmaddfp v12, v8, v12, v6 @0x822215CC)",
           lanes and "*lUnitValue." not in vector)


def numeric(tree):
    source = tree.read(RANDOM_CPP).replace("\r\n", "\n")
    try:
        bodies = [definition(source, SCALAR), definition(source, VECTOR)]
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxAiBuzzRandomDraws.cpp"), "fxaibuzz_random_draws.inc",
                           "\n".join(bodies) + "\n", "FxAiBuzzRandomDraws", shadow={RANDOM_H: tree.read(RANDOM_H)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxaibuzz_random_draws", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
