"""FX-FOLLOWUPS item 2 (crash parity 2026-09-25): the WORLD arm of SceneManagerModule::ProcessVolumeTestDeepest
@0x828D4460 -- the director's 0.1 m camera sphere (failure reason 0) asks whether the camera is inside the static world.

  CgsGeometric::TestSpherePolygonSoup @0x828455E8                      an export hole, no PC body before
  BaseCollisionGenerator::TestSphereAgainstPolySoupList @0x82812950     an export hole, no PC body before

Numeric: tests/FxFollowupsSphereSoup.cpp compiles the PRODUCTION text of both (plus UnpackPolygonSoupVertices,
TestSphereTriangle4SOA and the driver's LeafOverlapsBoxXYZ) with CgsPolygonSoup.cpp / CgsSphere.cpp alongside, and
checks the ARTIST decode: the soup walk's lane / vertex order (pinned through the console's own
TestSphereTriangle4SOA minimum-cascade bug), the v126 / v127 answers, and the driver's Prepare(1, tagA, tagB), box,
leaf order, touching-counts overlap, local sphere copy, count 1 / 0 and index return. A revision without the bodies
cannot build the numeric half: every numeric check then counts as failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_sphere_soup.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, code_only, compile_and_run, report

SOUP_CPP = "src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.cpp"
SOUP_H = "src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
GEN_CPP = "src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp"
GEN_H = "src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"
EXTRA_SOURCES = (
    REPO / "src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.cpp",   # GetPolygon / GetVertex
    REPO / "src/GameShared/GameClasses/Geometric/Primitives/CgsSphere.cpp",                    # GetPosition / GetRadius
)
NUMERIC_CHECKS = 35


def wiring(tree):
    soup_h = code_only(tree.read(SOUP_H))
    gen_h = code_only(tree.read(GEN_H))
    return [
        ("CgsPolygonSoupTests.h declares TestSpherePolygonSoup(const PolygonSoup&, const Sphere&) -> MaskScalar "
         "(DWARF CgsPolygonSoupTests.cpp:629)",
         re.search(r"rw::math::vpu::MaskScalar\s+TestSpherePolygonSoup\s*\(\s*const\s+PolygonSoup\s*&\s*\w+\s*,"
                   r"\s*const\s+Sphere\s*&\s*\w+\s*\)\s*;", soup_h) is not None),
        ("BaseCollisionGenerator declares TestSphereAgainstPolySoupList(const Sphere*, map*, u32, u16) -> u16 "
         "(DWARF CgsCollisionGenerator.h:72)",
         re.search(r"u16\s+TestSphereAgainstPolySoupList\s*\(\s*const\s+CgsGeometric::Sphere\s*\*\s*\w+\s*,"
                   r"\s*CgsGeometric::PolygonSoupListSpatialMap\s*\*\s*\w+\s*,\s*u32\s+\w+\s*,\s*u16\s+\w+\s*\)\s*;",
                   gen_h) is not None),
    ]


def numeric(tree):
    soup = tree.read(SOUP_CPP)
    gen = tree.read(GEN_CPP)
    constant = re.search(r"const\s+s32\s+KI_MAX_POLYGON_SOUP_VERTICES\s*=\s*(\d+)\s*;", soup)
    try:
        if constant is None:
            raise ValueError("KI_MAX_POLYGON_SOUP_VERTICES")
        soup_inc = "\n".join([
            "namespace { const s32 KI_MAX_POLYGON_SOUP_VERTICES = %s; }" % constant.group(1),
            definition(soup, "void UnpackPolygonSoupVertices("),
            definition(soup, "Triangle4::Mask4 TestSphereTriangle4SOA("),
            "namespace {",
            definition(soup, "struct SphereSoupLane") + ";",
            definition(soup, "Triangle4::Mask4 TestSphereLanes("),
            definition(soup, "inline bool SphereSoupLaneHit("),
            definition(soup, "inline rw::math::vpu::MaskScalar SphereSoupAnswer("),
            "}",
            definition(soup, "rw::math::vpu::MaskScalar TestSpherePolygonSoup("),
        ])
        list_inc = "\n".join([
            definition(gen, "inline bool LeafOverlapsBoxXYZ("),
            definition(gen, "u16 BaseCollisionGenerator::TestSphereAgainstPolySoupList("),
        ])
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxFollowupsSphereSoup.cpp"), "fxfu_soup.inc", soup_inc,
                           "FxFollowupsSphereSoup", extra_sources=EXTRA_SOURCES,
                           extra_files={"fxfu_list.inc": list_inc})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxfollowups_sphere_soup", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
