"""FX-DIRECTOR2 (crash parity 2026-09-25): BehaviourInterpolate's inlined rw::math::fpu::IsZero, by NaN polarity.

  BehaviourInterpolate::PostCollisionUpdate @0x82252AB8 guards its parametric time with
  "!rw::math::fpu::IsZero(mfDuration)" (:145). The console's inlined IsZero (0x82252B84..0x82252BB0) is
  `fcmpu x, +EPS ; bgt -> 0 ; li 1 ; fcmpu x, -EPS ; bge -> keep 1 ; 0` (flt_82001770 / flt_82002514), so a NaN falls
  through the first branch, takes the second and is ZERO -- the assert fires for a NaN duration. The PC's local copy
  read `x > EPS ? false : x >= -EPS`, which answers false for NaN (FX-GATE's NaN sweep).

Numeric: tests/FxDirector2InterpolateIsZero.cpp runs the revision's own helper and epsilon, extracted from
BrnBehaviourInterpolate.cpp: NaN of both signs, the signed zeros, the closed band edges, one ulp outside, the denormals,
1 / -1 / the infinities. Wiring: PostCollisionUpdate's :145 assert goes through that helper on mfDuration.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_interpolate_iszero.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

INTERPOLATE_CPP = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.cpp"
HELPER = "    bool IsZero(f32 lfValue)"
POST_COLLISION = "BehaviourInterpolate::PostCollisionUpdate(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)"
EPSILON = re.compile(r"const f32 KF_IS_ZERO_EPSILON = [^;]*;")
NUMERIC_CHECKS = 8


def wiring(tree):
    source = tree.read(INTERPOLATE_CPP)
    try:
        post = re.sub(r"\s+", "", code_only(definition(source, POST_COLLISION)))
    except ValueError:
        post = ""
    yield ("PostCollisionUpdate's :145 tripwire tests mfDuration through the file's inlined IsZero (0x82252B84..0x82252BD4)",
           'CGS_ASSERT(!IsZero(mfDuration),"!rw::math::fpu::IsZero(mfDuration)");' in post)


def numeric(tree):
    source = tree.read(INTERPOLATE_CPP)
    epsilon = EPSILON.search(code_only(source))
    try:
        helper = definition(source, HELPER)
    except ValueError:
        helper = None
    if epsilon is None or helper is None:
        print("NUMERIC: cannot build -- the revision has no local IsZero / KF_IS_ZERO_EPSILON in BrnBehaviourInterpolate.cpp")
        return None
    return compile_and_run(Path(__file__).with_name("FxDirector2InterpolateIsZero.cpp"), "interpolate_iszero.inc",
                           epsilon.group(0) + "\n" + helper + "\n", "FxDirector2InterpolateIsZero")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_interpolate_iszero", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
