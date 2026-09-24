"""FX-CRASHSND (crash parity 2026-09-24): the collision-sound bin choice, SelectBin, against ARTIST.

CollisionStateManager::SelectBin<crashbinlist, crashbin> @0x826A97E8 (propscrash @0x826A8828) vs the
PC SelectCollisionBin:
  * 0x826A9808..0x826A9820  a second material of exactly 1 selects no bin (the PC searched on);
  * 0x826AA38C..0x826AA4F0  a bin with no sample of the chosen size is abandoned for the NEXT bin
                            (the PC fell back to a smaller size in the same bin -- a different
                            sample for many impacts);
  * 0x826AA324..0x826AA34C  the normalisation has no MAX == MIN guard (two fsel's clamp to [0,1];
                            a degenerate bin normalises to 1.0, the PC forced 0.0);
  * 0x826AA2B4 / 0x826AA320 miBinIndex and mBinKey are stored before the impulse / size tests;
                            mNormalizedImpulse only on acceptance (0x826AA75C).

Numeric: tests/FxCrashSndSelectBin.cpp compiles the PRODUCTION SelectCollisionBin template (and the
BinLookupCache class it walks) against fixture bins. --rev reads a b5 revision; --file reads the
collision manager .cpp from a path (for checking a candidate before it is in the tree).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd_select_bin.py [--rev <rev>] [--file <path>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

MANAGER_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
CACHE_H = "src/GameSource/Sound/Collision/BrnBinLookupCache.h"
SELECT = "void CollisionStateManager::SelectCollisionBin("
NUMERIC_CHECKS = 25


def wiring(manager):
    select = code_only(definition(manager, SELECT)) if SELECT in manager else ""
    head = select[:select.find("for (")] if "for (" in select else ""
    return [
        ("SelectCollisionBin returns before any bin when the second material is 1 (0x826A9810)",
         re.search(r"if\s*\(\s*lrOutput\s*\.\s*maMaterial\s*\[\s*1\s*\]\s*==\s*1\s*\)\s*return\s*;", head)
         is not None),
        ("no MAX == MIN guard on the normalisation (0x826AA324..0x826AA34C: fsel clamp only)",
         select != "" and "lfDenominator > 0.0f" not in select),
    ]


def numeric(tree, manager):
    try:
        klass = definition(tree.read(CACHE_H), "class BinLookupCache") + ";"
        body = "template <typename ListType, typename BinType>\n" + definition(manager, SELECT)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    body = body.replace("CollisionStateManager::SelectCollisionBin", "SelectBinFixture::SelectCollisionBin")
    return compile_and_run(Path(__file__).with_name("FxCrashSndSelectBin.cpp"), "fxcrashsnd_selectbin_body.inc",
                           body, "FxCrashSndSelectBin",
                           extra_files={"fxcrashsnd_selectbin_cache_class.inc": klass})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    parser.add_argument("--file", help="read BrnCollisionStateManager.cpp from this path instead")
    args = parser.parse_args()
    tree = Tree(args.rev)
    manager = Path(args.file).read_text(encoding="utf-8-sig") if args.file else tree.read(MANAGER_CPP)
    return report("run_fxcrashsnd_select_bin", wiring(manager), numeric(tree, manager), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
