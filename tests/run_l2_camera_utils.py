"""L2 CAMCOLLIDE (owner's list 2026-09-28): the two camera utils CollisionPolicyAttachedToVehicle's scene-query pair
calls, against the ARTIST words run on emu64.

  BrnDirector::Camera::Utils::ApplyPitchAboutPointRads                @0x822183E0
  BrnDirector::Camera::Utils::ResolveLineTestNearestUsingNormalStrict @0x8220CD58
                                      -- both in src/GameSource/Director/Camera/Utils/CameraUtils.cpp

On the PC ApplyPitchAboutPointRads was declaration-only (and declared with a guessed `Matrix44Affine (Vector3,
VecFloat)` signature; the DWARF's is `void (Matrix44Affine&, Vector3, VecFloat)`), and
ResolveLineTestNearestUsingNormalStrict did not exist -- so the car-attached camera policy's auto-elevate and its
pull-in-front-of-the-wall could not be written.

Numeric: tests/L2CameraUtils.cpp compiles the revision's two bodies (and the lane helpers above them, extracted from
CameraUtils.cpp) against the revision's CameraUtils.h and replays tests/L2CameraUtilsData.h, generated from the
console's words on emu64 by scratch/OWNERLIST_0927/L2/emu/gen_camutils.py (seed 928): 260 pitch rows (camera
transforms about a pivot, elevations in [-1.4, 1.4] plus 0, +-pi/2, +-3, 7; a pivot straight below / above), every
lane of the four rows bit for bit, with the console's look-at fed in (its two arguments checked); 424 resolve rows
(hits with and without an intersection, lengths around the minimum distance; 40 rows whose normal . (position - hit)
rounds differently as vmsum3fp128's one rounding and as a sequential float sum, with the minimum distance between
the two; NaN / infinity lanes).
A revision without the bodies does not build (every numeric check counts as failed). Mutants (scratch
mutate_p7a.py): unfused vmaddfp, the pitch's sine sign, lFlatForward's -0 lane, the 0x8220CE34 NaN polarity and a
sequential dot are all caught; the two survivors are equivalent (the X rotation's first-row w lane is never read by
the next product, and the look-at's inverse translation is 0 in any order because its eye is the origin).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l2_camera_utils.py [--rev <b5 rev>]
"""
import argparse
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

UTILS_CPP = "src/GameSource/Director/Camera/Utils/CameraUtils.cpp"
UTILS_H = "src/GameSource/Director/Camera/Utils/CameraUtils.h"
MARKER = "// ApplyPitchAboutPointRads @0x822183E0 and ResolveLineTestNearestUsingNormalStrict @0x8220CD58 -- BODIED"
LAST = "bool ResolveLineTestNearestUsingNormalStrict(LineTestNearestPostBox& lPostBox"
NUMERIC_CHECKS = 5   # A1..A3, S1, S2


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def numeric(tree):
    source = _read(tree, UTILS_CPP)
    header = _read(tree, UTILS_H)
    if MARKER not in source:
        print("NUMERIC: cannot build -- CameraUtils.cpp has no ApplyPitchAboutPointRads / "
              "ResolveLineTestNearestUsingNormalStrict bodies")
        return None
    start = source.index(MARKER)
    try:
        last = definition(source, LAST)
    except ValueError:
        print("NUMERIC: cannot build -- no ResolveLineTestNearestUsingNormalStrict body")
        return None
    end = source.index(last) + len(last)
    bodies = source[start:end] + "\n"
    return compile_and_run(Path(__file__).with_name("L2CameraUtils.cpp"), "l2_camutils_bodies.inc", bodies,
                           "L2CameraUtils", shadow={UTILS_H: header})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l2_camera_utils", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
