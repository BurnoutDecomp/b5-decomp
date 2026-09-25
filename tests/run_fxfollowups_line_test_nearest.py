"""FX-FOLLOWUPS (crash parity 2026-09-25, stage a): the fine module's nearest-line test and the rwcollision line walk.

  FineIntersectionTestModule::ComputeLineTestNearest @0x828C8CC8   a LOUD trap before
  VolumeLineQuery::AddPrimitiveRef @0x82BB3230, AddVolumeRef @0x82BB3300, GetIntersections @0x82BB3470 (an export
  hole), GetAllIntersections @0x82BB3820; InitQuery / Finished (header inlines)   no bodies before (GetIntersections
  was an `int ... { return 0; }` link stub in AptRenderLinkStubs.cpp)

tests/FxFollowupsLineTestNearest.cpp compiles the revision's line-walk region of VolumeQuery.cpp (between its BEGIN /
END markers) and its ComputeLineTestNearest against the revision's VolumeQuery.hpp / LineSegIntersect.hpp, the real
VolRef.cpp, fixture rwcollision descriptors whose lineSegIntersect slot answers from a script, and fakes of the
entity / volume managers. It checks: AddPrimitiveRef / AddVolumeRef (fill, transform copy, capacity, the aggregate
split), GetAllIntersections (results set, budget), the walk (disabled inputs, LIFO test order, the result record, v =
the last consumed input, the unwritten tag-bit byte, m_endClipVal, resumable batches, the ignored AddVolumeRef answer
on the input path), the two PC traps (a primitive slot with no host body, an aggregate), and ComputeLineTestNearest
(the up-front writes, the nearest over batches / instances / entities with FLT_MAX never reset, strict less and NaN,
position / normal / tags, the InitQuery priming, the EQUALITY exclude rule with and without parts, the type-flag gate).
A revision without the region or the body cannot build: every numeric check then counts as failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_line_test_nearest.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, REPO

COLLISION = "src/vendor/renderware/collision/"
VOLUME_QUERY_CPP = COLLISION + "VolumeQuery.cpp"
VOLUME_QUERY_HPP = COLLISION + "VolumeQuery.hpp"
LINE_SEG_HPP = COLLISION + "LineSegIntersect.hpp"
FINE_SQ1 = "src/GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule_wSQ1.cpp"
BEGIN = "// ---- BEGIN VolumeLineQuery line walk"
END = "// ---- END VolumeLineQuery line walk ----"
NUMERIC_CHECKS = 25


def walk_region(source):
    start = source.index(BEGIN)
    end = source.index(END, start)
    return source[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    try:
        region = walk_region(tree.read(VOLUME_QUERY_CPP))
        print("found   the VolumeLineQuery line-walk region")
        body = definition(tree.read(FINE_SQ1), "void FineIntersectionTestModule::ComputeLineTestNearest(")
        if "CGS_ASSERT(false" in body:
            raise ValueError("ComputeLineTestNearest is the LOUD trap at this revision")
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return report("run_fxfollowups_line_test_nearest", [], None, NUMERIC_CHECKS)
    shadow = {VOLUME_QUERY_HPP: tree.read(VOLUME_QUERY_HPP), LINE_SEG_HPP: tree.read(LINE_SEG_HPP)}
    numeric = compile_and_run(Path(__file__).with_name("FxFollowupsLineTestNearest.cpp"), "fxfu_cltn_body.inc",
                              body, "FxFollowupsLineTestNearest", shadow=shadow,
                              extra_sources=[REPO / (COLLISION + "VolRef.cpp")],
                              extra_files={"fxfu_vlq_walk.inc": region})
    return report("run_fxfollowups_line_test_nearest", [], numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
