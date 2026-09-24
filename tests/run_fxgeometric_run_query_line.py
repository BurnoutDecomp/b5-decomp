"""FX-GEOMETRIC (crash parity 2026-09-24): PolygonSoupListSpatialMap::RunQuery(const Line&) (ARTIST 0x82843E98,
X360 `sub_82843E98`, PS3 mangle @0xB64574) -- the segment leaf gather of CollideLineAgainstPolySoupList's long arm.
Absent before; with --pre-fix it is stood in by `return 0` (the driver's trap delivered no leaves at all).

The production body is extracted from CgsPolygonSoupListSpatialMap_Query.cpp into FxGeometricRunQueryLine.cpp; the
real CgsLineTests.cpp (TestLineStartEndAxisAlignedBox, the per-node test the console inlines) is compiled alongside.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgeometric_run_query_line.py [--pre-fix <b5 rev>]
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, optional_definition, pre_fix_rev, read

QUERY = "src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap_Query.cpp"
LINE_TESTS = REPO / "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    body = optional_definition(read(QUERY, rev), "s32 PolygonSoupListSpatialMap::RunQuery(const Line& lrLine)")
    print(("found   " if body else "MISSING ") + "PolygonSoupListSpatialMap::RunQuery(const Line&)"
          + (f" (at {rev})" if rev else ""))
    if not body:
        body = "s32 PolygonSoupListSpatialMap::RunQuery(const Line&) { return 0; }   // absent (pre-fix)"
    rc = build_and_run(REPO / "tests" / "FxGeometricRunQueryLine.cpp", {"fxg_rql_body.inc": body}, "fxg_rql",
                       extra_sources=(LINE_TESTS,))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
