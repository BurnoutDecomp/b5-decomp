"""FX-SHOWTIME2 (crash parity 2026-09-24): the CASE-52 / CASE-53 arms of GameStateModule::ProcessGameEvents
@0x823A0A18 -- the showtime bounce relay (event 52 -> action 144, event 53 -> action 145).

  The tree had neither arm: VehicleManager::ProcessAftertouchEvents @0x82633DE8 has posted world events 52
  (the recent-bounce report, 32 bytes) and 53 (the extra-spin latch, 1 byte) all along, and they reach the
  pre-world pump's merged queue (BridgePhysicsToOutput leg 5 -> the carry queue), but no arm turned them into
  actions, so MainDirector (ShowTimeInfo), RaceCarEntityModule (CrashPlayManager::OnBounce -- the bounce
  spend) and the GUI translator (402) never saw a bounce. The console's arms (0x823A3D74..0x823A3E20) build a
  48-byte JustBouncedAction from the event and the crash scorer, post it as 144, call CrashModeScoring::
  DealWithPlayerBounced (an empty body on both consoles), and post a 1-byte 145 for event 53.

Numeric: tests/FxShowtime2Bounce.cpp compiles the extracted production ProcessGameEventsShowtimeBounceBringUp
(GameStateModule_gUI_00.cpp) and the production GetCurrentComboCount / GetNumCarsCrashed
(BrnCrashModeScoring.cpp) against a stand-in module, the real event / action records and the real queues, and
checks the 144 record byte for byte, the 145, the DealWithPlayerBounced arguments and position, that no other
event is relayed, the walk order, and that the queue is not cleared.
Wiring: the pump calls the arm function in the same walk (after the case-31 arm, before the pause arms), the
header declares it, the event / action ids, and the production DealWithPlayerBounced body is empty (the
console's lone `blr` @0x8284CB38; PS3 0x1CD04C).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxshowtime2_bounce.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, extract, compile_and_run, report, STRSTREAM_CPP

PUMP_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
SCORING_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"
MODULE_H = "src/GameSource/GameState/BrnGameStateModule.h"
EVENTS_H = "src/GameSource/GameState/BrnGameEvents.h"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
NUMERIC_CHECKS = 31

METHODS = ["void GameStateModule::ProcessGameEventsShowtimeBounceBringUp("]
SCORER = ["    s32 CrashModeScoring::GetNumCarsCrashed() const",
          "    s32 CrashModeScoring::GetCurrentComboCount() const"]


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    try:
        pump = squash(definition(tree.read(PUMP_CPP), "void GameStateModule::PreWorldUpdateStuntBringUp("))
    except ValueError:
        pump = ""
    impact = pump.find("ProcessGameEventsVehicleImpactBringUp(&lGameEventQueue,lpActionQueue);")
    bounce = pump.find("ProcessGameEventsShowtimeBounceBringUp(&lGameEventQueue,lpActionQueue);")
    pause = pump.find("ProcessGameEventsPauseBringUp(&lGameEventQueue,lpActionQueue);")
    yield ("PreWorldUpdate walks the merged queue through the case-52/53 arms, beside the case-31 arm and "
           "before the pause arms (the console's one ProcessGameEvents pass)",
           0 <= impact < bounce < pause)
    header = squash(tree.read(MODULE_H))
    yield ("BrnGameStateModule.h declares ProcessGameEventsShowtimeBounceBringUp(VariableEventQueue<1536,16>*, "
           "GameActionQueue*)",
           "voidProcessGameEventsShowtimeBounceBringUp(constCgsModule::VariableEventQueue<1536,16>*" in header)
    events = squash(tree.read(EVENTS_H))
    yield ("E_EVENT_JUST_BOUNCED == 52 and E_EVENT_JUST_APPLIED_EXTRA_SPIN == 53 (li r5, 0x34 / 0x35 "
           "@0x82633EB4 / 0x82633EE4)",
           "E_EVENT_JUST_BOUNCED=52," in events and "E_EVENT_JUST_APPLIED_EXTRA_SPIN=53," in events)
    actions = squash(tree.read(ACTIONS_H))
    yield ("E_ACTION_JUST_BOUNCED == 144 and E_ACTION_JUST_APPLIED_EXTRA_SPIN == 145 (li r5, 0x90 / 0x91)",
           "E_ACTION_JUST_BOUNCED=144," in actions and "E_ACTION_JUST_APPLIED_EXTRA_SPIN=145," in actions)
    try:
        body = squash(definition(tree.read(SCORING_CPP),
                                 "void CrashModeScoring::DealWithPlayerBounced("))
    except ValueError:
        body = ""
    body = body[body.find("{"):] if "{" in body else ""
    yield ("CrashModeScoring::DealWithPlayerBounced exists and does nothing (the console's callee is a lone "
           "`blr` @0x8284CB38; PS3 0x1CD04C)",
           body == "{(void)lbOnCar;(void)lbWasGoodImpact;(void)lidImpactEntityId;}")


def numeric(tree):
    methods, missing = extract(tree, PUMP_CPP, METHODS)
    scorer, scorer_missing = extract(tree, SCORING_CPP, SCORER)
    missing += scorer_missing
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + ", ".join(missing))
        return None
    return compile_and_run(Path(__file__).with_name("FxShowtime2Bounce.cpp"), "bounce_methods.inc",
                           "\n".join(methods) + "\n", "FxShowtime2Bounce",
                           extra_sources=[STRSTREAM_CPP],
                           extra_files={"bounce_scoring_methods.inc": "\n".join(scorer) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxshowtime2_bounce", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
