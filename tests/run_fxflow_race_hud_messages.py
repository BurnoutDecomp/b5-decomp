"""FX-FLOW (crash parity 2026-09-24, G11-D1 remainder, producer half): HUDMessageLogic's five race-arm
generators and their place in GenerateRaceModeMessages @0x82399C78.

  0x82399CA4 GenerateLeaderMessages @0x82394110          -> action 245 (24)
  0x82399CB0 GenerateFinisherMessage @0x82394258         -> action 247 (8)
  0x82399CF8 GenerateRivalCheckpointMessage @0x82394338  -> action 248 (24)
  0x82399D44 GenerateFirstOrLastMessage @0x82395760      -> actions 242 / 243 (16)
  0x82399D54 GenerateDistanceToFinishMessage @0x82395A88 -> action 244 (8)
FX-GS (a7cee49f) left the five as named legs; the PC race arm posted only 249 and 250, so none of the
leader-split, rival-finisher, took-the-lead or distance messages were ever produced.

Wiring: GenerateRaceModeMessages calls the five at the console's positions.
Numeric: tests/FxFlowRaceHudMessages.cpp drives the extracted production bodies through the real
HUDMessageLogic and RCEntityActiveRaceCarOutputInterface (accessor bodies extracted), with the two
out-of-line scorer queries scripted.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_race_hud_messages.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, code_only, compile_and_run, report, STRSTREAM_CPP

HUD_CPP = "src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.cpp"
HUD_H = "src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
SCORING_H = "src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
MODE_MANAGER_H = "src/GameSource/GameState/ModeManager/BrnModeManager.h"
RCIF_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"
BODIES = [
    "void HUDMessageLogic::Construct()",
    "void HUDMessageLogic::Prepare()",
    "void HUDMessageLogic::GenerateRaceModeMessages(",
    "void HUDMessageLogic::GeneratePlayerCheckpointMessage()",
    "void HUDMessageLogic::GenerateLeaderMessages(",
    "void HUDMessageLogic::GenerateFinisherMessage(",
    "void HUDMessageLogic::GenerateRivalCheckpointMessage(",
    "void HUDMessageLogic::GenerateFirstOrLastMessage(",
    "void HUDMessageLogic::GenerateDistanceToFinishMessage(",
    "void HUDMessageLogic::DetectCrashes(",
    "void HUDMessageLogic::DetectOnlineCrashes(",
    "void HUDMessageLogic::RemoveCrashingMessagesForTakendownPlayers(",
]
RCIF_BODIES = [
    "EActiveRaceCarIndex RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex() const",
    "CgsID RCEntityActiveRaceCarOutputInterface::GetRivalId(EActiveRaceCarIndex leActiveRaceCarIndex) const",
    "bool RCEntityActiveRaceCarOutputInterface::IsCarInShowtime(EActiveRaceCarIndex leActiveRaceCarIndex) const",
    "bool RCEntityActiveRaceCarOutputInterface::IsRaceCarActive(EActiveRaceCarIndex leActiveRaceCarIndex) const",
    "bool RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive() const",
    "RCEntityActiveRaceCarOutputInterface::GetRaceCarState(EActiveRaceCarIndex leActiveRaceCarIndex) const",
]
NUMERIC_CHECKS = 25


def wiring(tree):
    race = re.sub(r"\s+", "", code_only(body_or_empty(tree.read(HUD_CPP), "void HUDMessageLogic::GenerateRaceModeMessages(")))
    order = [
        "GenerateLeaderMessages(lpActiveRaceCarInterface,lpScoringSystem);",
        "GenerateFinisherMessage(lpActiveRaceCarInterface);",
        "GeneratePlayerCheckpointMessage();",
        "GenerateRivalCheckpointMessage(lpActiveRaceCarInterface,lpScoringSystem);",
        "DetectCrashes(lpActiveRaceCarInterface,lpRaceCarCrashQueue);",
        "GenerateFirstOrLastMessage(lpScoringSystem,lfTimeStep,lePlayerRaceCarIndex,lpActiveRaceCarInterface);",
        "GenerateDistanceToFinishMessage(lpScoringSystem,lePlayerRaceCarIndex);",
        "mbPlayerHasJustTriggeredCheckpoint=false;",
    ]
    positions = [race.find(call) for call in order]
    yield ("GenerateRaceModeMessages: leader, finisher, player checkpoint, rival checkpoint, crash split, "
           "first-or-last, distance, latch drop (0x82399CA4 .. 0x82399D5C)",
           all(position >= 0 for position in positions) and positions == sorted(positions))


def numeric(tree):
    hud = tree.read(HUD_CPP)
    texts = []
    for signature in BODIES:
        try:
            texts.append(definition(hud, signature))
        except ValueError:
            print("NUMERIC: cannot build -- absent in this revision: " + signature)
            return None
    rcif = tree.read(RCIF_CPP)
    rcif_texts = []
    for signature in RCIF_BODIES:
        start = rcif.find(signature)
        if start < 0:
            print("NUMERIC: cannot build -- accessor absent: " + signature)
            return None
        # GetRaceCarState's return type sits on the line above its signature.
        line_start = rcif.rfind("\n", 0, start) + 1
        previous = rcif.rfind("\n", 0, line_start - 1) + 1
        head = rcif[previous:line_start] if signature.startswith("RCEntity") else ""
        rcif_texts.append(head + definition(rcif, signature))
    includes = re.findall(r"^#include [^\n]+", hud, re.M)
    constants = re.findall(r"^    const (?:f32|s32) K[FI]_[A-Z_]+\s*=[^;]+;", hud, re.M)
    inc = "\n".join(includes + ["namespace BrnGameState {", "namespace {"] + constants + ["}"] + texts + ["}",
                    "namespace BrnWorld { namespace RaceCarEntityModuleIO {"] + rcif_texts + ["} }"])
    shadow = {HUD_H: tree.read(HUD_H), ACTIONS_H: tree.read(ACTIONS_H), SCORING_H: tree.read(SCORING_H),
              MODE_MANAGER_H: tree.read(MODE_MANAGER_H)}
    return compile_and_run(Path(__file__).with_name("FxFlowRaceHudMessages.cpp"), "race_hud_methods.inc", inc,
                           "FxFlowRaceHudMessages", shadow=shadow, extra_sources=(STRSTREAM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_race_hud_messages", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
