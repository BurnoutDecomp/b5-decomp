"""FX-GS2 (crash parity 2026-09-23, G12-D11 part 1): CgsNumeric::Random::SetSeed and the par-rival seed.

  SetSeed (DWARF CgsRandom.h:58) has no X360 symbol; five inline expansions agree
  (OnRoundStart @0x8236D290, OnRoundEnd @0x8236D4D0, SelectionHistory::Randomize @0x826C5900,
  PaybackComponent::Construct @0x8242E3A8, OnlineStuntRunMode::Start @0x82339E70):
  idx = 0 ; ring[0] = F(hi32(seed)) ; one LCG step ; 7 x AddRandomFloatToBuffer ; idx wraps to 0.
  StreetManager::SetupParRivals @0x8233F560 draws from r27 = 0xB5E330D02EC654DA (0x8233F5D8..E8),
  which is the state Construct() leaves -- so its source is a bare Construct().

Numeric: tests/FxGs2Random.cpp compiled against the revision's SetSeed / RandomUInt / RandomInt
(CgsRandom.cpp) and SetupParRivals' seeding statements (extracted from its body), checked against an
oracle written from the console's constants.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs2_random.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

RANDOM_CPP = "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
PAR_RIVALS_CPP = "src/GameSource/GameState/StreetData/BrnGameStateStreetManager_SetupParRivals.cpp"
NUMERIC_CHECKS = 11


def seeding_statements(par_rivals):
    """SetupParRivals' own Random set-up: the statements that follow `CgsNumeric::Random lRandom;`
    up to the next blank line, plus the revision's seed constant when it has one."""
    body = definition(par_rivals, "void StreetManager::SetupParRivals(")
    start = body.index("CgsNumeric::Random lRandom;") + len("CgsNumeric::Random lRandom;")
    block = body[start:].split("\n\n", 1)[0]
    statements = code_only(block).strip()
    constant = re.search(r"const u64 KU_PAR_RIVAL_SELECTION_SEED\s*=\s*[^;]+;", par_rivals)
    return (constant.group(0) if constant else ""), statements


def wiring(tree):
    par_rivals = tree.read(PAR_RIVALS_CPP)
    try:
        _, statements = seeding_statements(par_rivals)
    except ValueError:
        statements = ""
    yield ("SetupParRivals seeds with a bare Construct() -- no SetSeed (0x8233F5D8..E8 is Construct's own state)",
           "lRandom.Construct()" in statements and "SetSeed" not in statements)


def numeric(tree):
    random = tree.read(RANDOM_CPP)
    multiplier = re.search(r"static const u64 KU_RANDOM_LCG_MULTIPLIER\s*=\s*[^;]+;", random)
    try:
        bodies = [definition(random, "    void Random::SetSeed("),
                  definition(random, "    u32 Random::RandomUInt()"),
                  definition(random, "    s32 Random::RandomInt(")]
        constant, statements = seeding_statements(tree.read(PAR_RIVALS_CPP))
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    if multiplier is None:
        print("NUMERIC: cannot build -- CgsRandom.cpp's KU_RANDOM_LCG_MULTIPLIER is absent")
        return None
    inc = ("namespace CgsNumeric {\n" + multiplier.group(0) + "\n" + "\n".join(bodies) + "\n}\n"
           + "namespace ParRivalsSeed {\n" + constant + "\n"
           + "static void Seed(CgsNumeric::Random& lRandom)\n{\n" + statements + "\n}\n}\n")
    return compile_and_run(Path(__file__).with_name("FxGs2Random.cpp"), "random_methods.inc", inc,
                           "FxGs2Random", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs2_random", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
