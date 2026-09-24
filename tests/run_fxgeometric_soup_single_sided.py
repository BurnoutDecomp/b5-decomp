"""FX-GEOMETRIC (crash parity 2026-09-24): CgsGeometric::IntersectLinePolygonSoupSingleSided (ARTIST 0x8283C598), the
all-hits single-sided line-vs-soup kernel under BaseCollisionGenerator::CollideLineAgainstPolySoupList. Absent before
(the driver trapped at both call sites); with --pre-fix the absent body is stood in by a `return 0` stub, which is
what the driver's trap delivered (an empty list).

The production CgsPolygonSoupTests_LineNearest.cpp (kernel + the 4-wide IntersectLinePolySoupTriangleSingleSided4)
and the production UnpackPolygonSoupVertices are compiled into FxGeometricSoupSingleSided.cpp; CgsPolygonSoup.cpp
supplies GetPolygon / GetVertex.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgeometric_soup_single_sided.py [--pre-fix <b5 rev>]
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, definition, pre_fix_rev, read

KERNEL = "src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests_LineNearest.cpp"
UNPACK = "src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.cpp"
SOUP_CPP = REPO / "src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.cpp"

ABSENT_STUB = """
namespace CgsGeometric {
s32 IntersectLinePolygonSoupSingleSided(const PolygonSoup&, const Vector3&, const Vector3&,
                                        PolySoupLineNearestResult*, s32) { return 0; }   // absent (pre-fix)
}
"""


def main():
    rev = pre_fix_rev(sys.argv)
    kernel = read(KERNEL, rev)
    present = "s32 IntersectLinePolygonSoupSingleSided(" in kernel
    print(("found   " if present else "MISSING ") + "CgsGeometric::IntersectLinePolygonSoupSingleSided"
          + (f" (at {rev})" if rev else ""))
    if not present:
        kernel += ABSENT_STUB
    unpack = definition(read(UNPACK, rev), "void UnpackPolygonSoupVertices(")
    rc = build_and_run(REPO / "tests" / "FxGeometricSoupSingleSided.cpp",
                       {"fxg_soup_kernel.inc": kernel, "fxg_soup_unpack.inc": unpack}, "fxg_soup",
                       extra_sources=(SOUP_CPP,))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
