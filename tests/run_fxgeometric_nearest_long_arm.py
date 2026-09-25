"""FX-GEOMETRIC (crash parity 2026-09-24): BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest (ARTIST
0x828131C0) -- its long arm (a line of 20 m or more, 0x82813440..0x82813918) was a named trap; it now runs
PolygonSoupListSpatialMap::RunQuery(const Line&), the inlined TestLineStartEndAxisAlignedBox per leaf and the
short arm's nearest test + strictly-nearer copy. The nearest kernel is a recording fake; the real CgsLineTests.cpp
and CgsLine.cpp are compiled alongside.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgeometric_nearest_long_arm.py [--pre-fix <b5 rev>]
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, code_mask, definition, optional_definition, pre_fix_rev, read

GENERATOR = "src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp"
LINE_CPP = REPO / "src/GameShared/GameClasses/Geometric/Primitives/CgsLine.cpp"
LINE_TESTS_CPP = REPO / "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp"


def constant(source, name):
    match = re.search(r"^[^\n/]*\b" + name + r"\s*=\s*[^;]+;", code_mask(source), re.M)
    if not match:
        raise ValueError("constant not found: " + name)
    return source[match.start():match.end()].strip()


def main():
    rev = pre_fix_rev(sys.argv)
    source = read(GENERATOR, rev)
    body = definition(source, "u16 BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest(")
    # The line box's VMX max / min (2026-09-25, FX-FOLLOWUPS) -- present from that revision on.
    vmx = [text for text in (optional_definition(source, "inline f32 VmxMaxFp("),
                             optional_definition(source, "inline f32 VmxMinFp(")) if text]
    helpers = "\n".join([constant(source, "KF_SHORT_LINE_LENGTH_SQ"), constant(source, "KF_LINE_PARAM_NO_HIT"),
                         definition(source, "inline bool LeafOverlapsBoxXYZ(")] + vmx)
    pieces = {"fxg_nla_helpers.inc": helpers, "fxg_nla_body.inc": body}
    rc = build_and_run(REPO / "tests" / "FxGeometricNearestLongArm.cpp", pieces, "fxg_nla",
                       extra_sources=(LINE_CPP, LINE_TESTS_CPP))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
