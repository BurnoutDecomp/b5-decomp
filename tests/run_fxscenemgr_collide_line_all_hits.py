"""FX-SCENEMGR (crash parity 2026-09-24, item 3b): BaseCollisionGenerator::CollideLineAgainstPolySoupList
(ARTIST 0x82812AE0) -- the all-hits line-vs-static-world driver under ProcessLineTestFine and
ProcessTriangleCollisionLineTests. Absent before; its two Geometric callees are still absent, so the body
traps at their call sites (the Nearest twin's precedent) -- this runner checks the driver around them.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxscenemgr_collide_line_all_hits.py [--pre-fix <b5 rev>]
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, code_mask, definition, optional_definition, pre_fix_rev, read

GENERATOR = "src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp"
LINE_CPP = REPO / "src/GameShared/GameClasses/Geometric/Primitives/CgsLine.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    source = read(GENERATOR, rev)
    body = optional_definition(source, "u16 BaseCollisionGenerator::CollideLineAgainstPolySoupList(")
    print(("found   " if body else "MISSING ") + "BaseCollisionGenerator::CollideLineAgainstPolySoupList")
    if not body:
        body = ("u16 BaseCollisionGenerator::CollideLineAgainstPolySoupList(const CgsGeometric::Line&, "
                "CgsGeometric::PolygonSoupListSpatialMap*, u16, u32, u16) { return 0; }")

    constant = re.search(r"^[^\n/]*\bKF_SHORT_LINE_LENGTH_SQ\s*=\s*[^;]+;", code_mask(source), re.M)
    helpers = source[constant.start():constant.end()].strip() + "\n" + \
        definition(source, "inline bool LeafOverlapsBoxXYZ(")
    pieces = {"fxsm_cla_helpers.inc": helpers, "fxsm_cla_body.inc": body}
    rc = build_and_run(REPO / "tests" / "FxScenemgrCollideLineAllHits.cpp", pieces, "fxsm_cla",
                       extra_sources=(LINE_CPP,))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
