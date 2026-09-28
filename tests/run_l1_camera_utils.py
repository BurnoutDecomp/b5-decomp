"""L1 (owner's list 2026-09-28, item 2 "the camera is behind walls / below the map", piece 6): the two leaf helpers the
car-attached camera's FrustrumCollisionResolver needs, against the console's words run on emu64.

  XMVectorTan @0x821F0788 -- src/SDKs/XboxMath/XMVectorTan.h (NEW): the XDK tangent the resolver takes of the half field
      of view (CalculateFrustumLineTests @0x8220DE70, ProcessSceneQueryResults @0x82224404). Cody and Waite with the
      console's two-part pi/2 reduction, its rational N / D, the RAW vcmpbfp word as the near-zero select mask, and the
      guarded two-step reciprocals -- not std::tan.
  BrnDirector::Camera::Utils::ResolveLineTestNearestUsingDisplacementAndVector @0x8220CEB0 -- CameraUtils.cpp (NEW; the
      DWARF signature `bool (LineTestNearestPostBox&, Vector3, Vector3&, Vector3, VecFloat)`): the resolver's
      per-corner resolve. Did not exist on the PC.

Numeric: tests/L1CameraUtils.cpp compiles the revision's XMVectorTan.h and the revision's lane helpers + resolve body
(extracted from CameraUtils.cpp) and replays tests/L1CameraUtilsData.h, generated from the console's words on emu64 by
scratch/OWNERLIST_0927/L1/emu/gen_camutils_l1.py (seed 930): 564 tangents (the half fields of view, [-8, 8], the
quadrant boundaries +- an ulp, 0, +-0, 2^-12, huge, +-inf, NaN) and 532 resolves (test points on the plane, behind it,
around the minimum distance; vectors along / against the normal, parallel to the plane, tiny; unanswered boxes; NaN /
infinity lanes). A revision without the two bodies cannot build: every numeric check then fails.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l1_camera_utils.py [--rev <b5 rev>]
"""
import argparse
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

UTILS_CPP = "src/GameSource/Director/Camera/Utils/CameraUtils.cpp"
UTILS_H = "src/GameSource/Director/Camera/Utils/CameraUtils.h"
TAN_H = "src/SDKs/XboxMath/XMVectorTan.h"
HELPERS = "namespace\n{\n    Vector3 LaneVector(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)"
RESOLVE = "bool ResolveLineTestNearestUsingDisplacementAndVector(LineTestNearestPostBox& lPostBox"
NUMERIC_CHECKS = 4   # T1, D1..D3


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def numeric(tree):
    source = _read(tree, UTILS_CPP)
    header = _read(tree, UTILS_H)
    tan = _read(tree, TAN_H)
    if not tan:
        print("NUMERIC: cannot build -- no src/SDKs/XboxMath/XMVectorTan.h")
        return None
    try:
        helpers = definition(source, HELPERS)
        body = definition(source, RESOLVE)
    except ValueError:
        print("NUMERIC: cannot build -- CameraUtils.cpp has no ResolveLineTestNearestUsingDisplacementAndVector body")
        return None
    return compile_and_run(Path(__file__).with_name("L1CameraUtils.cpp"), "l1_camutils_bodies.inc",
                           helpers + "\n\n" + body + "\n", "L1CameraUtils", shadow={UTILS_H: header, TAN_H: tan})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l1_camera_utils", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
