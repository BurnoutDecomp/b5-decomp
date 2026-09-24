"""FX-TAILS-A item 3 (crash parity 2026-09-24): no invented null test around the embedded TakedownManager.

The console embeds TakedownManager at gsm+0x238 and calls it with no test anywhere (REVIEW-E on a5619c29):
  OnModeFinish @0x82390EE0       `addi r3, r30, 0x238 ; bl ClearAllTakedowns` @0x82390F2C (r4 re-fetched
                                 GetGameActionQueue @0x82390F24), between the camera-off AddEvent (0x82390F1C)
                                 and the drive-thru re-open stores (0x82390F3C / 0x82390F40)
  PreWorldUpdate @0x823A5328     `addi r3, r31, 0x238 ; bl TakedownManager::Update` @0x823A59F0
  OnModeEnd @0x823767E0          `addi r3, r31, 0x238 ; bl ClearRaceCarData` @0x82376804
  UpdateCurrentMode @0x82350EC8  `lwz r11, 0x6D58(r31) ; addi r3, r11, 0x238 ; bl IsInTakedownCamera` @0x823513DC
On PC the manager is heap-allocated by ConstructTakedownBringUp, i.e. always set before any caller runs, so the
`mpTakedownManager != 0` tests were invented arms that could only pass. A null manager is not reachable, so there
is no numeric side: this runner is structural (the bodies, comments stripped).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_takedown_guards.py [--rev <b5 rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, report

TD_CPP = "src/GameSource/GameState/GameStateModule_gTD_00.cpp"


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def unguarded(body):
    return body != "" and "mpTakedownManager!=0" not in body and "mpTakedownManager==0" not in body


def wiring(tree):
    source = tree.read(TD_CPP).replace("\r\n", "\n")
    finish = squash(body_or_empty(source, "void GameStateModule::OnModeFinish("))
    order = [finish.find(s) for s in ("AddEvent(", "mpTakedownManager->ClearAllTakedowns(lpActionQueue);",
                                      "mDriveThruManager.DriveThroughsCanNowOpenAgain();")]
    yield ("OnModeFinish: camera-off AddEvent, ClearAllTakedowns, drive-thrus re-open -- in the console's order",
           all(o >= 0 for o in order) and order == sorted(order))
    yield ("OnModeFinish calls ClearAllTakedowns with no manager test (0x82390F2C)", unguarded(finish))
    leg = squash(body_or_empty(source, "void GameStateModule::TakedownPreWorldLeg("))
    tick = leg.find("mpTakedownManager->Update(")
    yield ("PreWorldUpdate's takedown leg ticks the manager with no test (0x823A59F0)",
           tick >= 0 and "if(mpTakedownManager!=0){mpTakedownManager->Update(" not in leg)
    clear = squash(body_or_empty(source, "void GameStateModule::ClearTakedownRaceCarData()"))
    yield ("ClearTakedownRaceCarData calls ClearRaceCarData with no test (OnModeEnd 0x82376804)",
           unguarded(clear) and clear.endswith("{mpTakedownManager->ClearRaceCarData();}"))
    camera = squash(body_or_empty(source, "bool GameStateModule::IsInTakedownCamera() const"))
    yield ("IsInTakedownCamera asks the manager with no test (UpdateCurrentMode 0x823513DC)",
           unguarded(camera) and camera.endswith("{returnmpTakedownManager->IsInTakedownCamera();}"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtailsa_takedown_guards", list(wiring(Tree(args.rev))), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
