"""FX-GS (crash parity 2026-09-23, G11-D1/D2/D3): the HUD crash messages.

  HUDMessageLogic::PostWorldUpdate @0x8239D998 -- the case 0/10 arm (GenerateRaceModeMessages
      @0x82399C78) and the case 15 arm (DetectOnlineCrashes + RemoveCrashingMessagesForTakendownPlayers);
  GenerateRaceModeMessages -- the inlined player-checkpoint record (action 249), the crash split on
      the latched mode, the latch drop;
  DetectCrashes @0x82394418 (action 250), DetectOnlineCrashes @0x82394528 (the 1.5 s buffer in the
      ObjectPool<BufferedCrashingCar,8,s32> at +0x110), RemoveCrashingMessagesForTakendownPlayers
      @0x82366590; the pool's Clear in Construct @0x8236F530 / Prepare @0x82366478;
  the lifted post-world leg handing the crash queue, the takedown queue and the player index in.

Wiring: structural checks on the production sources. Numeric: tests/FxGsHudCrashes.cpp compiled
against the extracted production bodies (a revision that lacks them cannot build it: every numeric
check then counts as failed).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs_hud_crashes.py [--rev <b5 rev>]
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
MODE_WORLD_CPP = "src/GameSource/GameState/ModeManager/BrnModeManager_WorldTick.cpp"
MODE_MANAGER_H = "src/GameSource/GameState/ModeManager/BrnModeManager.h"
RCIF_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"
BODIES = [
    "void HUDMessageLogic::Construct()",
    "void HUDMessageLogic::Prepare()",
    "void HUDMessageLogic::PostWorldUpdate(",
    "void HUDMessageLogic::GenerateRaceModeMessages(",
    "void HUDMessageLogic::GeneratePlayerCheckpointMessage()",
    "void HUDMessageLogic::DetectCrashes(",
    "void HUDMessageLogic::DetectOnlineCrashes(",
    "void HUDMessageLogic::RemoveCrashingMessagesForTakendownPlayers(",
    "void HUDMessageLogic::GenerateOnlineTeamChangeMessages(",
]
RCIF_BODIES = [
    "EActiveRaceCarIndex RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex() const",
    "CgsID RCEntityActiveRaceCarOutputInterface::GetRivalId(EActiveRaceCarIndex leActiveRaceCarIndex) const",
    "bool RCEntityActiveRaceCarOutputInterface::IsCarInShowtime(EActiveRaceCarIndex leActiveRaceCarIndex) const",
    "bool RCEntityActiveRaceCarOutputInterface::IsRaceCarActive(EActiveRaceCarIndex leActiveRaceCarIndex) const",
]
NUMERIC_CHECKS = 38


def switch_arm(body, case_names):
    """The statements of the switch arm opened by the given case labels (up to its break)."""
    pattern = r"".join(r"case\s+GameStateModuleIO::" + name + r"\s*:\s*(?://[^\n]*)?\s*" for name in case_names)
    match = re.search(pattern + r"(.*?)break\s*;", body, re.S)
    return match.group(1) if match else ""


def wiring(tree):
    header = code_only(tree.read(HUD_H))
    yield ("D2 HUDMessageLogic carries `ObjectPool<BufferedCrashingCar, 8, s32> mBufferedCrashingCars` "
           "(DWARF :265, console +0x110)",
           re.search(r"ObjectPool\s*<\s*BufferedCrashingCar\s*,\s*8\s*,\s*s32\s*>\s+mBufferedCrashingCars\s*;", header)
           is not None)
    hud = tree.read(HUD_CPP)
    post = body_or_empty(hud, "void HUDMessageLogic::PostWorldUpdate(")
    race = switch_arm(post, ["E_MODE_OFFLINE_RACE", "E_MODE_ONLINE_RACE"])
    yield ("D1 PostWorldUpdate cases 0/10 -> GenerateRaceModeMessages(iface, scoring, crash queue, takedown queue, "
           "player, delta)  @0x8239DAB0..0x8239DACC",
           re.search(r"GenerateRaceModeMessages\s*\(\s*lpActiveRaceCarInterface\s*,\s*lpScoringSystem\s*,\s*"
                     r"lpRaceCarCrashQueue\s*,\s*lpTakedownQueue\s*,\s*lePlayerActiveRaceCarIndex\s*,\s*lfDelta\s*\)",
                     race) is not None)
    road = switch_arm(post, ["E_MODE_ONLINE_ROAD_RAGE"])
    road_calls = [road.find("GenerateOnlineBlueTeamEscapingMessage(lpScoringSystem, lfDelta, lePlayerActiveRaceCarIndex)"),
                  road.find("GenerateOnlineBlueTeamAreBehindYouMessage(lpScoringSystem, lfDelta, lePlayerActiveRaceCarIndex)"),
                  road.find("GenerateOnlineLeaderMilestoneMessage(lpScoringSystem, lfDelta, lePlayerActiveRaceCarIndex,")]
    yield ("D1 PostWorldUpdate case 11 calls the three Road Rage producers in ARTIST order and passes the progress byte",
           all(position >= 0 for position in road_calls) and road_calls == sorted(road_calls)
           and "lbIsGameModeInProgress" in road[road_calls[2]:])
    lobby = switch_arm(post, ["E_MODE_ONLINE_FREE_BURN_LOBBY"])
    detect = lobby.find("DetectOnlineCrashes(lpActiveRaceCarInterface, lpRaceCarCrashQueue, lfDelta)")
    remove = lobby.find("RemoveCrashingMessagesForTakendownPlayers(lpTakedownQueue)")
    yield ("D2/D3 PostWorldUpdate case 15 -> DetectOnlineCrashes then RemoveCrashingMessagesForTakendownPlayers  "
           "@0x8239DB50 / 0x8239DB5C", 0 <= detect < remove)
    world = body_or_empty(tree.read(MODE_WORLD_CPP), "ModeManager::PostWorldUpdate(")
    call = re.search(r"mHUDMessageLogic\.PostWorldUpdate\((.*?)\);", world, re.S)
    arguments = call.group(1) if call else ""
    yield ("D1 the selected ModeManager post-world caller carries the crash queue, takedown input, "
           "mode owner, vehicle output, player index and mode-progress byte in ARTIST order",
           re.fullmatch(r"\s*lpActiveRaceCarOutput\s*,\s*meCurrentGameModeType\s*,\s*this\s*,\s*&mScoringSystem\s*,\s*"
                        r"lpPostWorldInputBuffer->GetRaceCarCrashEventQueue\(\)\s*,\s*lpVehicleOutput\s*,\s*lpTakedownQueue\s*,\s*"
                        r"lfDelta\s*,\s*mePlayerActiveRaceCarIndex\s*,\s*lbIsGameModeInProgress\s*",
                        arguments) is not None)
    bhr = switch_arm(post, ["E_MODE_ONLINE_BURNING_HOME_RUN"])
    yield ("mode 13 carries the actual mode owner into the BHR generator",
           "GenerateBurningHomeRunMessages(lpModeManager, lpScoringSystem)" in bhr)
    yield ("the team-change generator remains after the mode switch",
           re.search(r"}\s*GenerateOnlineTeamChangeMessages\(lpVehicleOutputInterface,\s*"
                     r"lePlayerActiveRaceCarIndex\);\s*}\s*$", post) is not None)
    actions = code_only(tree.read(ACTIONS_H))
    yield ("D1 action ids 249 E_ACTION_HUD_MESSAGE_PLAYER_REACHES_CHECKPOINT / 250 E_ACTION_HUD_MESSAGE_X_CRASHES "
           "(DWARF 239/240, +10; translator case 250 @0x823EC0CC)",
           re.search(r"E_ACTION_HUD_MESSAGE_PLAYER_REACHES_CHECKPOINT\s*=\s*249\s*,", actions) is not None
           and re.search(r"E_ACTION_HUD_MESSAGE_X_CRASHES\s*=\s*250\s*,", actions) is not None)


def numeric(tree, output_dir=None, omit_team_tail=False):
    hud = tree.read(HUD_CPP)
    texts = []
    for signature in BODIES:
        try:
            texts.append(definition(hud, signature))
        except ValueError:
            print("NUMERIC: cannot build -- absent in this revision: " + signature)
            return None
    if omit_team_tail:
        call = "GenerateOnlineTeamChangeMessages(lpVehicleOutputInterface, lePlayerActiveRaceCarIndex);"
        assert texts[2].count(call) == 1, "Cannot isolate the production team-change tail"
        texts[2] = texts[2].replace(call, "", 1)
        print("NEGATIVE CONTROL: omit only the extracted PostWorld team-change tail")
    rcif = tree.read(RCIF_CPP)
    rcif_texts = [definition(rcif, signature) for signature in RCIF_BODIES]
    includes = re.findall(r"^#include [^\n]+", hud, re.M)
    constants = re.findall(r"^    const (?:f32|s32) K[FI]_[A-Z_]+\s*=[^;]+;", hud, re.M)
    inc = "\n".join(includes + ["namespace BrnGameState {", "namespace {"] + constants + ["}"] + texts + ["}",
                    "namespace BrnWorld { namespace RaceCarEntityModuleIO {"] + rcif_texts + ["} }"])
    shadow = {HUD_H: tree.read(HUD_H), ACTIONS_H: tree.read(ACTIONS_H), MODE_MANAGER_H: tree.read(MODE_MANAGER_H)}
    return compile_and_run(Path(__file__).with_name("FxGsHudCrashes.cpp"), "hud_crashes_methods.inc", inc,
                           "FxGsHudCrashes", shadow=shadow, extra_sources=(STRSTREAM_CPP,), output_dir=output_dir)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--output-dir", type=Path, help="retain generated sources, executable and execution receipts")
    parser.add_argument("--omit-team-tail", action="store_true", help="negative control: omit only the extracted team-change tail")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs_hud_crashes", list(wiring(tree)), numeric(tree, args.output_dir, args.omit_team_tail), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
