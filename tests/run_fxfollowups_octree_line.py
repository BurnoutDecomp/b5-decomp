"""FX-FOLLOWUPS (crash parity 2026-09-25): the loose octree's LINE walk, which the director's camera
VisibilityTest line tests (entity flags 0x1E) reach through SceneManagerModule -> LooseOctree::LineTest.

  LooseOctree::LineTestOptimized @0x828CA5F8       a CGS_ASSERT(false) trap on PC before
  LooseOctree::LineTestRecursive @0x828BCF50        no PC body before
  CgsGeometric::TestLineSphere4 (CgsLineTests.cpp:172 / :305) and
  CgsGeometric::TestLineBoundingBoxAgainstAxisAlignedBox4 (:766), inlined into LineTestRecursive
  SpatialPartition::LineTestRecursiveFuncParams     DWARF CgsSpatialPartition.h:131-139 (was an opaque
                                                    `unsigned char maBytes[0x40]`, 779b479f)
  LooseOctree::TestLineAgainstNodeBoundingBox @0x828B0FC8 reads the block by name now

Numeric: tests/FxFollowupsOctreeLine.cpp compiles the PRODUCTION text of the octree bodies (with the
revision's own headers shadowed in), CgsLineTests.cpp and CgsSpatialPartition.cpp, plus the PRE-CHANGE
TestLineAgainstNodeBoundingBox from OLD_REV renamed to a free function, and checks the ARTIST decode: the
block's offsets and values, the short-segment SphereTest arm, the root gate, the four-slot batching and
its push order, the node / sub-tree masks, the per-axis child slab test and the chain count, the
line-vs-sphere lanes (per-lane length) and the slab lanes and asserts. A revision without the bodies
cannot build the numeric half: every numeric check then counts as failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_octree_line.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

PARTITIONS = "src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/"
OCT_CPP = PARTITIONS + "CgsLooseOctree.cpp"
OCT_H = PARTITIONS + "CgsLooseOctree.h"
SP_H = PARTITIONS + "CgsSpatialPartition.h"
SP_CPP = PARTITIONS + "CgsSpatialPartition.cpp"
LT_CPP = "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp"
LT_H = "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
# The last revision whose TestLineAgainstNodeBoundingBox read the opaque block by raw float offsets.
OLD_REV = "c451f0f0"
NUMERIC_CHECKS = 26


def wiring(tree):
    sp_h = code_only(tree.read(SP_H))
    oct_h = code_only(tree.read(OCT_H))
    lt_h = code_only(tree.read(LT_H))
    block = re.search(r"struct\s+alignas\(16\)\s+LineTestRecursiveFuncParams\s*\{(.*?)\};", sp_h, re.S)
    members = block.group(1) if block else ""
    return [
        ("CgsSpatialPartition.h: LineTestRecursiveFuncParams names the DWARF members in order "
         "(CgsSpatialPartition.h:131-139)",
         re.search(r"Vector3\s+mLineStart;\s*Vector3\s+mLineEnd;\s*Vector3\s+mLineDirection;\s*"
                   r"Vector3\s+mLineReciprocal;\s*VecFloat\s+mfLineLength;\s*u32\s+mx32EntityTypeFlags;\s*"
                   r"CoarseQueryResultBuffer<16384>\s*\*\s*mpResultBufferOut;", members) is not None),
        ("CgsLooseOctree.h declares LineTestRecursive(u16, const LineTestRecursiveFuncParams*) const "
         "(DWARF CgsLooseOctree.cpp:1654)",
         re.search(r"void\s+LineTestRecursive\s*\(\s*u16\s+\w+\s*,\s*const\s+SpatialPartition::"
                   r"LineTestRecursiveFuncParams\s*\*\s*\w+\s*\)\s*const\s*;", oct_h) is not None),
        ("CgsLineTests.h declares both TestLineSphere4 forms and TestLineBoundingBoxAgainstAxisAlignedBox4",
         len(re.findall(r"TestLineSphere4\s*\(", lt_h)) == 2
         and re.search(r"const\s+Vector4\s+TestLineBoundingBoxAgainstAxisAlignedBox4\s*\(", lt_h) is not None),
    ]


def constant(source, name):
    match = re.search(r"const\s+f32\s+" + name + r"\s*=\s*([^;]+);", source)
    if match is None:
        raise ValueError(name)
    return "const f32 %s = %s;" % (name, match.group(1).strip())


def numeric(tree):
    octree = tree.read(OCT_CPP)
    old = Tree(OLD_REV).read(OCT_CPP)
    try:
        oct_inc = "\n".join([
            "namespace {",
            constant(octree, "KF_LINE_TEST_MIN_LENGTH"),
            constant(octree, "KF_LINE_RECIPROCAL_EPSILON"),
            definition(octree, "inline f32 NewtonRaphsonReciprocalSqrt2("),
            definition(octree, "inline f32 NewtonRaphsonReciprocal2("),
            definition(octree, "inline void NoteOctreeLineTest("),
            definition(octree, "inline bool Mask4LaneSet("),
            "}",
            definition(octree, "LooseOctree::LooseOctree()"),
            definition(octree, "bool LooseOctree::TestLineAgainstNodeBoundingBox("),
            definition(octree, "void LooseOctree::LineTestRecursive("),
        ])
        opt_inc = definition(octree, "bool LooseOctree::LineTestOptimized(")
        old_body = definition(old, "bool LooseOctree::TestLineAgainstNodeBoundingBox(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    old_inc, renamed = re.subn(r"bool\s+LooseOctree::TestLineAgainstNodeBoundingBox\s*\(([^)]*)\)\s*const",
                               r"bool OldTestLineAgainstNodeBoundingBox(\1)", old_body)
    if renamed != 1 or "reinterpret_cast<const f32*>(lpParams)" not in old_inc:
        print("NUMERIC: cannot build -- the pre-change TestLineAgainstNodeBoundingBox at %s is not the "
              "raw-offset body" % OLD_REV)
        return None
    shadow = {path: tree.read(path) for path in (OCT_H, SP_H, LT_H)}
    # Stubs for the virtuals this test does not exercise that only later revisions declare.
    stubs = ""
    if re.search(r"virtual\s+bool\s+VolumeTest\s*\(", code_only(tree.read(OCT_H))):
        stubs += ("bool LooseOctree::VolumeTest(u32, const VolRef::Volume*, const Matrix44Affine*, "
                  "CoarseQueryResultBuffer<16384>*) { return false; }\n")
    with tempfile.TemporaryDirectory(prefix="brn_fxfu_octline_") as directory:
        sources = Path(directory)
        (sources / "CgsLineTests.cpp").write_text(tree.read(LT_CPP), encoding="utf-8")
        (sources / "CgsSpatialPartition.cpp").write_text(tree.read(SP_CPP), encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("FxFollowupsOctreeLine.cpp"), "fxfu_oct.inc", oct_inc,
                               "FxFollowupsOctreeLine", shadow=shadow,
                               extra_sources=(sources / "CgsLineTests.cpp", sources / "CgsSpatialPartition.cpp"),
                               extra_files={"fxfu_oct_opt.inc": opt_inc, "fxfu_oct_old.inc": old_inc,
                                            "fxfu_oct_stubs.inc": stubs})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxfollowups_octree_line", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
