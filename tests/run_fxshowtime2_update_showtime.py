"""FX-SHOWTIME2 (crash parity 2026-09-24): GameStateModule::UpdateShowtimeMode @0x82380EF8 and its
PreWorldUpdate call -- the pre-world half of the showtime "Cars Crashed" chain.

  The tree had NO body for it (hasbody), so nothing popped mShowtimePendingTrafficIndexStack: it filled
  to eight, no traffic-type request (116) was ever posted, CrashModeScoring::DealWithScoreForVehicleClass
  (the only writer of maiNumCarsCrashed) never ran and no VehicleHitAction (140) reached the director,
  crash play or the GUI. The console calls it unconditionally from PreWorldUpdate @0x823A5328
  (`bl` #65 @0x823A5888) between the event merge and ProcessGameEvents.

Numeric: tests/FxShowtime2UpdateShowtimeMode.cpp compiles the extracted production UpdateShowtimeMode,
ToggleShowtimeBehaviour and file constants against a stand-in module, with the REAL scorer bodies
(DealWithScoreForVehicleClass / GetVehicleScoreData / GetRecentCrash / GetNumCarsCrashed /
GetScoreMultiplier from BrnCrashModeScoring.cpp), the real Stack / EventQueue / VariableEventQueue and
the real action records, and checks the pop cadence, the request, the answer, the score, the 140 record
bytes, the missing-answer assert, the toggle and the in-frame leg order against the ARTIST asm.
Wiring: the PreWorldUpdate call at the console's position, the ClearData delay seed, the DWARF toggle
member, the two action ids, and the ProcessContacts witness no longer claiming "no consumer".

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxshowtime2_update_showtime.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, extract, compile_and_run, report, STRSTREAM_CPP

SHOWTIME_CPP = "src/GameSource/GameState/GameStateModule_Showtime.cpp"
SCORING_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"
PUMP_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
MODULE_H = "src/GameSource/GameState/BrnGameStateModule.h"
MODULE_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
NUMERIC_CHECKS = 45

METHODS = ["    void GameStateModule::ToggleShowtimeBehaviour()",
           "    void GameStateModule::UpdateShowtimeMode("]
SCORER = ["    void CrashModeScoring::DealWithScoreForVehicleClass(",
          "    void CrashModeScoring::GetVehicleScoreData(",
          "    CrashModeScoring::RecentCrash* CrashModeScoring::GetRecentCrash(",
          "    s32 CrashModeScoring::GetNumCarsCrashed() const",
          "    s32 CrashModeScoring::GetScoreMultiplier() const"]


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    try:
        pump = squash(definition(tree.read(PUMP_CPP), "void GameStateModule::PreWorldUpdateStuntBringUp("))
    except ValueError:
        pump = ""
    clear = pump.find("mGameEventCarryQueue.Clear();")
    call = pump.find("UpdateShowtimeMode(mpPreWorldInputBuffer,mpOutputBuffer,&mContactSpyInterface,"
                     "&mpTakedownCache->mTrafficTypeResponseQueue);")
    arms = pump.find("ProcessGameEventsCarCustomizationBringUp(")
    yield ("PreWorldUpdate calls UpdateShowtimeMode after the carry-queue Clear (#62) and before the "
           "ProcessGameEvents arms (#68) -- console `bl` #65 @0x823A5888, with the TrafficTypeResponse cache",
           0 <= clear < call < arms)
    header = squash(tree.read(MODULE_H))
    yield ("miShowtimePendingFrameDelay is seeded 1 (GameStateModule::ClearData @0x8236B550, li r10, 1)",
           "s32miShowtimePendingFrameDelay=1;" in header)
    yield ("the DWARF :860 toggle byte mbToggleShowtimeBehaviour exists (X360 +284512)",
           "boolmbToggleShowtimeBehaviour" in header)
    actions = squash(tree.read(ACTIONS_H))
    yield ("E_ACTION_TRAFFIC_TYPE_REQUEST == 116 and E_ACTION_TOGGLE_SHOWTIME_BEHAVIOUR == 138 (li r5, 0x74 / 0x8A)",
           "E_ACTION_TRAFFIC_TYPE_REQUEST=116," in actions and "E_ACTION_TOGGLE_SHOWTIME_BEHAVIOUR=138," in actions)
    module = tree.read(MODULE_CPP)
    yield ("ProcessContacts' [showtime-crash] witness no longer says the stack has no consumer",
           module != "" and "no consumer yet" not in module)


def constants(source):
    found = []
    for pattern in (r"const\s+u16\s+K_INVALID_VEHICLE_INDEX\s*=[^;]+;",
                    r"const\s+s32\s+KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES\s*=[^;]+;"):
        match = re.search(pattern, source)
        if match:
            found.append(match.group(0))
    try:
        found.append(definition(source, "bool ShowtimeScoreWitness()"))
    except ValueError:
        pass
    return found


def numeric(tree):
    source = tree.read(SHOWTIME_CPP)
    methods, missing = extract(tree, SHOWTIME_CPP, METHODS)
    scorer, scorer_missing = extract(tree, SCORING_CPP, SCORER)
    bonus = re.search(r"static\s+const\s+s32\s+KI_SCORE_BONUS_FOR_COMBO_CRASH\s*=[^;]+;", tree.read(SCORING_CPP))
    if bonus is None:
        scorer_missing.append("KI_SCORE_BONUS_FOR_COMBO_CRASH")
    else:
        scorer.insert(0, bonus.group(0))
    found = constants(source)
    missing += scorer_missing
    if len(found) != 3:
        missing.append("the K_INVALID_VEHICLE_INDEX / KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES / ShowtimeScoreWitness block")
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + ", ".join(missing))
        return None
    return compile_and_run(Path(__file__).with_name("FxShowtime2UpdateShowtimeMode.cpp"), "showtime_methods.inc",
                           "\n".join(methods) + "\n", "FxShowtime2UpdateShowtimeMode",
                           extra_sources=[STRSTREAM_CPP],
                           extra_files={"showtime_constants.inc": "\n".join(found) + "\n",
                                        "showtime_scoring_methods.inc": "\n".join(scorer) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxshowtime2_update_showtime", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
