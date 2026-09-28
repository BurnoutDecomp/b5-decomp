"""L2 CAMCOLLIDE (owner's list 2026-09-27): the NaN branch polarities of the volume line kernels and of the two
primitive tests they call, against the ARTIST words run on emu64.

  SphereVolume::LineSegIntersect      @0x82BA82C8   0x82BA83BC
  BoxVolume::LineSegIntersect         @0x82BA9478   0x82BA96C4 / 96DC / 96F0 / 9704 / 99E8 / 99FC / 9ACC
  CapsuleVolume::LineSegIntersect     @0x82BAFCF8   0x82BAFEF0 / FF04 / FFA4 / FFC4, 0x82BB0078
  rwcSphereLineSegIntersect           @0x82BA81D8   0x82BA8288 (the discriminant)
  rwcCylinderLineSegIntersect         @0x82BAF8A0   0x82BAF95C (the discriminant)
                                      -- all in src/vendor/renderware/collision/LineSegIntersect.cpp

REVIEW-K DELTA 2 (scratch/CRASHPARITY_0922/REVIEW_K.md): 623c92bf spelled the FALL-THROUGH of each console `ble` /
`bge` as `!(a <= b)` / `!(a >= b)` -- TRUE for a NaN -- where `ble` (bc 4,gt) and `bge` (bc 4,lt) are TAKEN on an
unordered compare, so the fall-through is the ORDERED `a > b` / `a < b`; and where the console's `bge` is the TAKEN
hit arm, `x >= y` must be `!(x < y)`. The two discriminant tests of the wave-2 primitives had the same misreading
(`!(disc >= 0)` -> return 0, where `bge` carries a NaN disc on).

Numeric: tests/L2LineKernelNan.cpp compiles the revision's LineSegIntersect.cpp beside it (the revision's headers
shadowed in) and replays tests/L2LineKernelNanData.h, generated from the console's words on emu64 by
scratch/OWNERLIST_0927/L2/emu/gen_linekernel_nan.py (seed 927, 5803 rows): the 1299 finite rows bit for bit (F1 the
three kernels, F2 the two primitives called directly), and every NaN / infinity row, grouped by the branch whose NaN
arm it reaches (the generator records, per row, which of the 15 sites ran ORDERED and which UNORDERED). Four of the 15
NaN arms cannot be reached through the kernels' inputs:
  box 0x82BA99E8 / 0x82BA99FC / 0x82BA9ACC -- a NaN in any input of the edge arm's cap or plane events also feeds
      rwcCylinderLineSegIntersect's crosses with the edge axis (0 * NaN), so the cylinder returns -1 and the kernel
      returns before the events;
  capsule 0x82BAFFC4 -- a NaN event num needs a NaN z (then delta.z is NaN and 0x82BAFFA4 skips the event) or a NaN
      hh (then the start is classified as the barrel and the barrel event takes the cylinder hit).
Those four are fixed from the words alone; the test prints them as NOTE lines. The rows need the console's rounding of
the two primitives (run_l2_line_kernel_rounding.py) to match bit for bit.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l2_line_kernel_nan.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

C = "src/vendor/renderware/collision/"
LSI_CPP = C + "LineSegIntersect.cpp"
HEADERS = [C + "CollisionVolume.hpp", C + "CapsuleVolume.hpp", C + "LineSegIntersect.hpp", C + "LineSegKernelMath.hpp"]
KERNELS = ["RwBool SphereVolume::LineSegIntersect(", "RwBool BoxVolume::LineSegIntersect(",
           "RwBool CapsuleVolume::LineSegIntersect("]
NUMERIC_CHECKS = 14   # F1 + F2 + the 11 NaN arms the inputs can reach + O


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def numeric(tree):
    lsi_text = _read(tree, LSI_CPP)
    for signature in KERNELS:
        try:
            definition(lsi_text, signature)
        except ValueError:
            print("NUMERIC: cannot build -- LineSegIntersect.cpp has no body for " + signature.strip())
            return None
    shadow = {}
    for path in HEADERS:
        text = _read(tree, path)
        if not text:
            print("NUMERIC: cannot build -- " + path + " is absent")
            return None
        shadow[path] = text
    with tempfile.TemporaryDirectory(prefix="brn_l2_lkn_") as directory:
        lsi = Path(directory) / "LineSegIntersect.cpp"
        lsi.write_text(lsi_text, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("L2LineKernelNan.cpp"), "l2_lkn_unused.inc",
                               "// not included: the kernels come from LineSegIntersect.cpp\n",
                               "L2LineKernelNan", shadow=shadow, extra_sources=(lsi,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l2_line_kernel_nan", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
