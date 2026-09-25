"""FX-OCTREE (crash parity 2026-09-25): the loose octree's ADAPTIVE DEPTH against the console's own words.

  LooseOctree::Update @0x828D0180               RemoveNodes -> AddNodes -> UpdateRecursive when the root is flagged
                                                (the PC ran UpdateRecursive alone)
  AdaptiveDepthUpdateRemoveNodesRecursive @0x828CA4F8 / AdaptiveDepthUpdateAddNodesRecursive @0x828CA360
  SplitAndPropogateRecursive @0x828BBBD0 / MergeSubTreeRecursive @0x828BC340   (no PC body before)
  IndexedPool<LooseOctreeNodeAllocation, u16>   mFreeNodeGroupPool: Construct's two carves, Pop (sub_828B8DC0),
                                                PushIndex (sub_828AE988); AllocRecursive @0x828BB4A0 takes the static
                                                tree's groups from it (the PC counted them out in order)

Numeric: tests/FxOctreeAdaptive.cpp compiles the revision's WHOLE CgsLooseOctree.cpp (its headers shadowed in) with
CgsSpatialPartition.cpp, CgsLineTests.cpp and CgsStrStream.cpp, replays the op lists of tests/FxOctreeAdaptiveData.h
(recorded from the console's words on emu64 by scratch/CRASHPARITY_0922/fxoctree_emu/gen_octree_data.py) through the
production bodies, and compares every CHECK: the tree reachable from the root (indices, topology, chain order,
counts, flags, type masks, the vectors' bits), the node-group pool and every entity's owning node. A revision whose
octree has no pool gets a fallback accessor set that fails the pool checks.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxoctree_adaptive.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, code_only, compile_and_run, report

SPM = "src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/"
PARTITIONS = SPM + "SpatialPartitions/"
OCT_CPP = PARTITIONS + "CgsLooseOctree.cpp"
OCT_H = PARTITIONS + "CgsLooseOctree.h"
SP_H = PARTITIONS + "CgsSpatialPartition.h"
SP_CPP = PARTITIONS + "CgsSpatialPartition.cpp"
QB_H = SPM + "CgsCoarseQueryResultBuffer.h"
SPM_H = SPM + "CgsSpatialPartitionManager.h"
POOL_H = "src/GameShared/GameClasses/Containers/CgsIndexedPool.h"
LT_CPP = "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp"
LT_H = "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
BRD_CPP = REPO / "vendor/renderware/src/rw/BaseResourceDescriptor.cpp"
DATA_H = Path(__file__).with_name("FxOctreeAdaptiveData.h")

POOL_REAL = """
static bool PoolPresent() { return true; }
static u32  PoolUsed(LooseOctree* lpTree)     { return lpTree->mFreeNodeGroupPool.msNumAllocated; }
static u32  PoolFree(LooseOctree* lpTree)     { return lpTree->mFreeNodeGroupPool.msNumFree; }
static u32  PoolCapacity(LooseOctree* lpTree) { return lpTree->mFreeNodeGroupPool.msCapacity; }
static u32  PoolFreeIndex(LooseOctree* lpTree, u32 luSlot) { return lpTree->mFreeNodeGroupPool.mpFreeIndices[luSlot]; }
static void PoolDrainOne(LooseOctree* lpTree) { lpTree->mFreeNodeGroupPool.Pop(); }
"""
POOL_ABSENT = """
// This revision's LooseOctree has no mFreeNodeGroupPool: every pool check fails, DRAIN does nothing.
static bool PoolPresent() { return false; }
static u32  PoolUsed(LooseOctree*)     { return 0xFFFFu; }
static u32  PoolFree(LooseOctree*)     { return 0xFFFFu; }
static u32  PoolCapacity(LooseOctree*) { return 0xFFFFu; }
static u32  PoolFreeIndex(LooseOctree*, u32) { return 0xFFFFu; }
static void PoolDrainOne(LooseOctree*) {}
"""


def numeric_total():
    text = DATA_H.read_text(encoding="utf-8")
    block = text.split("kaChecks[] = {", 1)[1].split("};", 1)[0]
    return 2 + 7 * block.count("{")


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    octree = tree.read(OCT_CPP)
    pool_h = code_only(tree.read(POOL_H))
    update = body(octree, "void LooseOctree::Update()")
    order = [update.find(name) for name in ("AdaptiveDepthUpdateRemoveNodesRecursive(0, 0)",
                                             "AdaptiveDepthUpdateAddNodesRecursive(0, 0)", "UpdateRecursive(0)")]
    alloc = body(octree, "void LooseOctree::AllocRecursive(")
    merge = body(octree, "void LooseOctree::MergeSubTreeRecursive(")
    split = body(octree, "void LooseOctree::SplitAndPropogateRecursive(")
    return [
        ("LooseOctree::Update runs RemoveNodes(0, 0), AddNodes(0, 0), UpdateRecursive(0) in that order "
         "(0x828D01B8 / 0x828D01C8 / 0x828D01D4)",
         all(p >= 0 for p in order) and order == sorted(order)),
        ("CgsLooseOctree.cpp: the stale 'FLAG (deferred, not a divergence in the results)' banner is gone",
         "not a divergence in the results" not in octree),
        ("AllocRecursive takes the static tree's groups from mFreeNodeGroupPool.Pop() (sub_828B8DC0), no private "
         "group counter",
         "mFreeNodeGroupPool.Pop()" in alloc and "miNumStaticNodes" not in alloc and "muNumNodeGroups" not in alloc),
        ("SplitAndPropogateRecursive pops a group; MergeSubTreeRecursive pushes one back by index (sub_828AE988)",
         "mFreeNodeGroupPool.Pop()" in split and "mFreeNodeGroupPool.PushIndex(" in merge),
        ("CgsIndexedPool.h: GetPoolSize / GetNumFree / GetNumUsed (DWARF CgsIndexedPool.h:171 / :187 / :203)",
         all(re.search(r"IndexType\s+%s\s*\(\s*\)" % name, pool_h) for name in ("GetPoolSize", "GetNumFree",
                                                                                   "GetNumUsed"))),
    ]


def numeric(tree):
    header = tree.read(OCT_H)
    if not header or not tree.read(OCT_CPP):
        print("NUMERIC: cannot build -- no octree at this revision")
        return None
    pool = POOL_REAL if re.search(r"NodeGroupPool\s+mFreeNodeGroupPool\s*;", code_only(header)) else POOL_ABSENT
    shadow = {path: tree.read(path) for path in (OCT_H, SP_H, QB_H, SPM_H, POOL_H, LT_H) if tree.read(path)}
    with tempfile.TemporaryDirectory(prefix="brn_fxoct_adaptive_") as directory:
        sources = Path(directory)
        for relative in (OCT_CPP, SP_CPP, LT_CPP):
            (sources / Path(relative).name).write_text(tree.read(relative), encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("FxOctreeAdaptive.cpp"), "fxoct_pool.inc", pool,
                               "FxOctreeAdaptive", shadow=shadow,
                               extra_flags=f'/I"{DATA_H.parent}"',
                               extra_sources=(sources / "CgsLooseOctree.cpp", sources / "CgsSpatialPartition.cpp",
                                              sources / "CgsLineTests.cpp", STRSTREAM_CPP, BRD_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxoctree_adaptive", wiring(tree), numeric(tree), numeric_total())


if __name__ == "__main__":
    sys.exit(main())
