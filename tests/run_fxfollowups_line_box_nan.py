"""FX-FOLLOWUPS (crash parity 2026-09-25): BaseCollisionGenerator's two poly-soup LINE boxes are VMX min / max.

  CollideLineAgainstPolySoupListNearest @0x828131C0  vmaxfp128 / vminfp128 v127 (start), v123 (end) @0x82813318 / 0x82813320
  CollideLineAgainstPolySoupList        @0x82812AE0  the same pair @0x82812C20 / 0x82812C2C

A NaN lane in either operand gives a NaN box lane and +0 orders above -0; the C selects the boxes used before
answered the END for a NaN start lane and ordered the zeros by operand. tests/FxFollowupsLineBoxNan.cpp receives
each function's PRODUCTION box block (from `CgsGeometric::AxisAlignedBox lBox;` to its RunQuery line) and the
file-local VmxMaxFp / VmxMinFp when the revision has them.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_line_box_nan.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

GEN_CPP = "src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp"
NUMERIC_CHECKS = 8


def box_block(source, signature):
    body = definition(source, signature).replace("\r\n", "\n")
    start = body.index("CgsGeometric::AxisAlignedBox lBox;")
    end = body.index("RunQuery(lBox)", start)
    return body[start:body.rindex("\n", start, end) + 1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the source from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    source = Tree(args.rev).read(GEN_CPP)
    try:
        helpers = []
        for signature in ("inline f32 VmxMaxFp(", "inline f32 VmxMinFp("):
            try:
                helpers.append(definition(source, signature))
            except ValueError:
                pass   # a revision before the fix: the box block is the C selects
        inc = "\n".join([
            "#include <cmath>",
            "namespace {",
            *helpers,
            "}",
            "void NearestBox(const Vector3& lrStart, const Vector3& lrEnd, CgsGeometric::AxisAlignedBox& lrOut)",
            "{",
            box_block(source, "u16 BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest("),
            "    lrOut = lBox;",
            "}",
            "void AllHitsBox(const Vector3& lrStart, const Vector3& lrEnd, CgsGeometric::AxisAlignedBox& lrOut)",
            "{",
            box_block(source, "u16 BaseCollisionGenerator::CollideLineAgainstPolySoupList("),
            "    lrOut = lBox;",
            "}",
        ])
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return report("run_fxfollowups_line_box_nan", [], None, NUMERIC_CHECKS)
    numeric = compile_and_run(Path(__file__).with_name("FxFollowupsLineBoxNan.cpp"), "fxfu_linebox.inc", inc,
                              "FxFollowupsLineBoxNan")
    return report("run_fxfollowups_line_box_nan", [], numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
