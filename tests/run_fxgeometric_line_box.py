"""FX-GEOMETRIC (crash parity 2026-09-24): CgsGeometric::TestLineStartEndAxisAlignedBox (ARTIST 0x82812498), the
slab test inlined per node / per leaf by PolygonSoupListSpatialMap::RunQuery(const Line&) @0x82843E98 and the long
arm of BaseCollisionGenerator::CollideLineAgainstPolySoupList @0x82812AE0.

The whole production CgsLineTests.cpp is compiled into the harness (FxGeometricLineBox.cpp), so the checks run the
shipped body. The pre-fix body lacked the console's END-INSIDE term (0x82812880) and used a bare 1/d reciprocal
(the console refines vrefp128 three times, so an infinite direction lane gives NaN, never the 0 that trips the
"Line reciprocal X is 0" assert).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgeometric_line_box.py [--pre-fix <b5 rev>]
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, pre_fix_rev, read

LINE_TESTS = "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    source = read(LINE_TESTS, rev)
    if "bool TestLineStartEndAxisAlignedBox(" not in source:
        print("MISSING CgsGeometric::TestLineStartEndAxisAlignedBox")
        sys.exit(1)
    print("found   CgsGeometric::TestLineStartEndAxisAlignedBox" + (f" (at {rev})" if rev else ""))
    rc = build_and_run(REPO / "tests" / "FxGeometricLineBox.cpp", {"fxg_linebox_source.inc": source}, "fxg_linebox")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
