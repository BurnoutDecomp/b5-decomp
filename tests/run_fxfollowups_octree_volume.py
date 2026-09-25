"""FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step C): the loose octree's VOLUME walk, the entity arm of the
scene manager's volume tests (SceneManagerModule::ProcessVolumeTestDeepest @0x828D4460 reaches it through the
partition's slot 8).

  LooseOctree::VolumeTest @0x828CA910            no PC declaration or body before
  LooseOctree::VolumeTestRecursive @0x828BDE28   no PC body before
  LooseOctree::Construct @0x828C99D8             the walk's state: two identity frames, a unit BoxVolume and a
                                                 unit SphereVolume (0x828C9E34..0x828CA09C) beside the query
  SpatialPartition::VolumeTestRecursiveFuncParams DWARF CgsSpatialPartition.h:171-176
  CoarseQueryResultBuffer::GetNumResultsAttempted @0x828ADCD8  its :348 batch assert (VolumeTest inlines it)

Numeric: tests/FxFollowupsOctreeVolume.cpp compiles the PRODUCTION text of the octree bodies (the revision's
headers shadowed in) and CgsSpatialPartition.cpp against rwcollision fixtures, and checks the ARTIST decode: the
Construct calls and frames, the parameter block, the monitor, the node box and entity sphere priming, the prune,
the sentinel-driven chain, the type filter, the child sub-tree masks and order, the pushes and the answer, and the
accessor's assert. A revision without the bodies cannot build the numeric half: every numeric check then counts as
failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_octree_volume.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, code_only, compile_and_run, report

SPM = "src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/"
PARTITIONS = SPM + "SpatialPartitions/"
OCT_CPP = PARTITIONS + "CgsLooseOctree.cpp"
OCT_H = PARTITIONS + "CgsLooseOctree.h"
SP_H = PARTITIONS + "CgsSpatialPartition.h"
SP_CPP = PARTITIONS + "CgsSpatialPartition.cpp"
QB_H = SPM + "CgsCoarseQueryResultBuffer.h"
# rw::BaseResourceDescriptor's out-of-line ctor (X360 @0x821F05C8), which Construct's descriptors use.
BRD_CPP = REPO / "vendor/renderware/src/rw/BaseResourceDescriptor.cpp"
NUMERIC_CHECKS = 18


def wiring(tree):
    sp_h = code_only(tree.read(SP_H))
    qb_h = code_only(tree.read(QB_H))
    block = re.search(r"struct\s+VolumeTestRecursiveFuncParams\s*\{(.*?)\};", sp_h, re.S)
    members = block.group(1) if block else ""
    accessor = re.search(r"GetNumResultsAttempted\s*\(\s*\)\s*const\s*\{(.*?)\}", qb_h, re.S)
    return [
        ("CgsSpatialPartition.h: VolumeTestRecursiveFuncParams names the DWARF members in order "
         "(CgsSpatialPartition.h:173-176)",
         re.search(r"const\s+VolRef::Volume\s*\*\s*mpVolume;\s*u32\s+mx32EntityTypeFlags;\s*"
                   r"CoarseQueryResultBuffer<16384>\s*\*\s*mpResultBufferOut;\s*"
                   r"const\s+Matrix44Affine\s*\*\s*mpTransform;", members) is not None),
        ("CgsSpatialPartition.h: SpatialPartition declares the pure virtual VolumeTest (slot 8, DWARF :284)",
         re.search(r"virtual\s+bool\s+VolumeTest\s*\(\s*u32\s+\w+\s*,\s*const\s+VolRef::Volume\s*\*\s*\w+\s*,\s*"
                   r"const\s+Matrix44Affine\s*\*\s*\w+\s*,\s*CoarseQueryResultBuffer<16384>\s*\*\s*\w+\s*\)\s*=\s*0\s*;",
                   sp_h) is not None),
        ("CgsCoarseQueryResultBuffer.h: GetNumResultsAttempted asserts mbInABatch before reading the count "
         "(0x828ADCD8, :348)",
         accessor is not None and re.search(r"CGS_ASSERT\s*\(\s*mbInABatch", accessor.group(1)) is not None),
    ]


def numeric(tree):
    octree = tree.read(OCT_CPP)
    try:
        perfmon = re.search(r"s32\s+LooseOctree::_miVolumeTestPerfMon\s*=\s*-1\s*;", octree)
        if perfmon is None:
            raise ValueError("LooseOctree::_miVolumeTestPerfMon")
        inc = "\n".join([
            "namespace {",
            definition(octree, "void* AllocFromResourceAllocator("),
            definition(octree, "void PrimeOctreeVolumeQuery("),
            "}",
            perfmon.group(0),
            definition(octree, "LooseOctree::LooseOctree()"),
            definition(octree, "void LooseOctree::Construct("),
            definition(octree, "void LooseOctree::AllocRecursive("),
            definition(octree, "bool LooseOctree::VolumeTest("),
            definition(octree, "void LooseOctree::VolumeTestRecursive("),
        ])
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    shadow = {path: tree.read(path) for path in (OCT_H, SP_H, QB_H)}
    with tempfile.TemporaryDirectory(prefix="brn_fxfu_octvol_") as directory:
        sources = Path(directory)
        (sources / "CgsSpatialPartition.cpp").write_text(tree.read(SP_CPP), encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("FxFollowupsOctreeVolume.cpp"), "fxfu_octvol.inc", inc,
                               "FxFollowupsOctreeVolume", shadow=shadow,
                               extra_sources=(sources / "CgsSpatialPartition.cpp", BRD_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxfollowups_octree_volume", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
