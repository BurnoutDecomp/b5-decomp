"""FX-SHOWTIME2 (crash parity 2026-09-24): AchievementManagerBase::OnShowTimeMultiplier.

  DWARF BrnGameStateAchievementManagerBase.h:153 declares it; the tree had no body. The console
  inlines it at the tail of GameStateModule::UpdateShowtimeMode @0x82380EF8 (0x8238112C..0x82381178):
  IsAchievementEarnt(13) (slot 1), then `cmpwi r29, 0xA ; blt`, then AchievementEarnt(13) (slot 0),
  with r29 = CrashModeScoring::miScoreMultiplier. PS3 twin 0x23F824: id 21, `> 9`.

Numeric: tests/FxShowtime2Achievement.cpp compiles the extracted production body (+ its two
constants) against the real AchievementManagerBase and a recording leaf.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxshowtime2_achievement.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, extract, code_only, body_or_empty, compile_and_run, report

BASE_CPP = "src/GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.cpp"
SIGNATURE = "void AchievementManagerBase::OnShowTimeMultiplier("
NUMERIC_CHECKS = 8


def constants(source):
    found = []
    for pattern in (r"const\s+EAchievement\s+E_CONSOLE_ACHIEVEMENT_SHOWTIME_MULTIPLIER\s*=[^;]+;",
                    r"const\s+s32\s+KI_SHOWTIME_MULTIPLIER_FOR_ACHIEVEMENT\s*=[^;]+;"):
        match = re.search(pattern, source)
        if match:
            found.append(match.group(0))
    return found


def wiring(tree):
    source = tree.read(BASE_CPP)
    body = body_or_empty(source, SIGNATURE)
    yield ("AchievementManagerBase::OnShowTimeMultiplier has a body (the DWARF :153 hook)", body != "")
    found = " ".join(constants(source))
    yield ("its id is the console's 13 (li r4, 0xD @0x82381134/0x82381168) and its threshold 10 "
           "(cmpwi r29, 0xA @0x8238115C)",
           re.search(r"static_cast<EAchievement>\(13\)", found) is not None
           and re.search(r"KI_SHOWTIME_MULTIPLIER_FOR_ACHIEVEMENT\s*=\s*10\s*;", found) is not None)


def numeric(tree):
    source = tree.read(BASE_CPP)
    texts, missing = extract(tree, BASE_CPP, [SIGNATURE])
    found = constants(source)
    if missing or len(found) != 2:
        print("NUMERIC: cannot build -- production body or constants absent: " + ", ".join(missing))
        return None
    return compile_and_run(Path(__file__).with_name("FxShowtime2Achievement.cpp"),
                           "achievement_methods.inc", "\n".join(texts) + "\n", "FxShowtime2Achievement",
                           extra_files={"achievement_constants.inc": "\n".join(found) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxshowtime2_achievement", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
