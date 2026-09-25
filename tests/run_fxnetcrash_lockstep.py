"""crash parity FX-NETCRASH (2026-09-25), online traffic lockstep: RecalculateActiveHulls sorts mActiveHulls.

The sort itself landed upstream in b5 ca4ac341 (Niaz, network wave 3). This runner is the regression pin for it:
the tree before that commit FAILS (ca4ac341~1: 5/11), and the tree after it passes (11/11).

Online, every machine must run the same traffic simulation, decision frame for decision frame. The hull set is
lockstepped (online hull set pieces 1-3), but without the sort the ORDER of that set is not. On PC each machine is
its own active race car 0, so the union of the players' hull lists comes out in a different order on each machine.
The console sorts it:
  RecalculateActiveHulls @0x8274C870. After the union loop (0x8274CA00..0x8274CA94) it asserts the set
  (CgsSet.h:227 constructed, :274/:275 GetItem(0)). A non-empty set then goes through
  std::_Sort<ushort *,int>(first, first + muLength, muLength) @0x8274CB98, and the two SetDifferences follow
  (0x8274CBA8 / 0x8274CBB8).
SetDifference keeps its first operand's order. So the new hulls FillNewHull fills, the old hulls
KillOutOfAreaTraffic empties, and RebuildGeneratorList's generator list all follow that order, and each of them
draws mRand hull by hull. Without the sort, two machines draw the same random numbers for DIFFERENT hulls, and
the traffic diverges.

NUMERIC: the production statements from the previous-set snapshot to the second SetDifference are compiled into
FxNetcrashLockstep.cpp over the real containers. Two machines hold the same two players in opposite slots.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_lockstep.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
NUMERIC_CHECKS = 8
FIRST = "const ActiveHullSet lPreviousActiveHulls = mActiveHulls;"
LAST = "lpOutOldHulls->SetDifference(lPreviousActiveHulls, mActiveHulls);"


def recalc_block(module):
    body = definition(module, "void TrafficEntityModule::RecalculateActiveHulls(")
    start = body.index(FIRST)
    end = body.index(LAST, start) + len(LAST)
    return body[start:end]


def numeric(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    try:
        block = recalc_block(module)
    except ValueError as error:
        print("NUMERIC: cannot build -- production statements absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxNetcrashLockstep.cpp"), "recalc_block.inc", block,
                           "FxNetcrashLockstep")


def wiring(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    try:
        block = code_only(recalc_block(module))
    except ValueError:
        block = ""
    union_at = block.find("mActiveHulls.Insert(luRaceCarHull);")
    sort_match = re.search(r"std::sort\(\s*(\w+)\s*,\s*\1\s*\+\s*mActiveHulls\.GetLength\(\)\s*\)", block)
    sort_at = sort_match.start() if sort_match else -1
    diff_at = block.find("lpOutNewHulls->SetDifference(mActiveHulls, lPreviousActiveHulls);")
    try:
        generators = code_only(definition(module, "void TrafficEntityModule::RebuildGeneratorList()"))
    except ValueError:
        generators = ""
    try:
        reset = code_only(definition(module, "void TrafficEntityModule::Reset()"))
    except ValueError:
        reset = ""
    return [
        ("RecalculateActiveHulls sorts mActiveHulls after the union and before the SetDifferences "
         "(std::_Sort @0x8274CB98)", 0 <= union_at < sort_at < diff_at),
        ("the stale 'std::_Sort ... has no Sort' gate is gone", "has no Sort" not in block),
        ("RebuildGeneratorList walks mActiveHulls in its (now sorted) order",
         "mActiveHulls[luActiveHull]" in generators),
        # Reset @0x8272CDA0: 0x8272CEA8 `ori r8, r8, 0x13F8` + 0x8272CECC `stfsx f31(=flt_82001CC0 0.0f),
        # r30, r8` clears the decision accumulator (+0x713F8, UpdateTimers' mfSimTimeSinceLastDecision);
        # the step at +0x713FC is not stored. POPULATING's SpawnNewTraffic ticks every generator by the
        # accumulator before UpdateTimers first runs, so a stale per-machine value re-phases the generators.
        ("Reset clears mfSimTimeSinceLastDecision (+0x713F8, 0x8272CECC) and leaves mfSimTimeStep alone",
         re.search(r"\bmfSimTimeSinceLastDecision\s*=\s*0\.0f\s*;", reset) is not None
         and re.search(r"\bmfSimTimeStep\s*=", reset) is None),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_lockstep", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
