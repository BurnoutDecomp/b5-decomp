"""FX-GS (crash parity 2026-09-23, G11-D4): the online team modes (11 / 13).

  ModeManager::HandleOnlineTeamModes @0x8234C750, ::HandleOnlineTeamTakedowns @0x823440B8,
  ::HandleOnlineTeamCheckForModeFinished @0x82328910, ::HandleOnlineBurningHomeRunCheckForModeFinished
  @0x82328A70, GameStateModule::GetNetworkPlayerID @0x823639C0; actions 165/166/167; the pre-world
  call after DetectModeStarts (console 0x823A5C14).

Wiring: structural checks on the production sources. Numeric: tests/FxGsOnlineTeam.cpp compiled
against the extracted production bodies, re-homed onto small ModeManager / ScoringSystem fixtures
(a revision that lacks the bodies cannot build it: every numeric check then counts as failed).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs_online_team.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

UPDATE_MODE = "src/GameSource/GameState/ModeManager/BrnModeManager_UpdateMode.cpp"
NETWORK_ID = "src/GameSource/GameState/GameStateModule_GetActiveRaceCarIndexFromNetworkPlayer.cpp"
GUI00 = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
MODE_MANAGER_H = "src/GameSource/GameState/ModeManager/BrnModeManager.h"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
SHARED_IO_H = "src/GameSource/GameState/BrnGameStateSharedIO.h"
BODIES = [
    "void ModeManager::HandleOnlineTeamModes(",
    "void ModeManager::HandleOnlineTeamTakedowns(",
    "void ModeManager::HandleOnlineTeamCheckForModeFinished()",
    "void ModeManager::HandleOnlineBurningHomeRunCheckForModeFinished()",
]
HELPER = "inline GameStateModuleIO::PlayerFinishedModeEvent\nMakePlayerFinishedModeEvent("
NETWORK_ID_BODY = "BrnNetwork::NetworkPlayerID GameStateModule::GetNetworkPlayerID("
NUMERIC_CHECKS = 33


def wiring(tree):
    header = code_only(tree.read(MODE_MANAGER_H))
    yield ("D4 ModeManager declares the four DWARF members (:483/:724/:727/:730)",
           all(name in header for name in ("void HandleOnlineTeamModes(", "void HandleOnlineTeamTakedowns(",
                                           "void HandleOnlineTeamCheckForModeFinished()",
                                           "void HandleOnlineBurningHomeRunCheckForModeFinished()")))
    gui = code_only(tree.read(GUI00))
    detect = gui.find("DetectModeStarts(mpPreWorldInputBuffer, mpOutputBuffer, lfGameTimestep);")
    call = gui.find("mModeManager.HandleOnlineTeamModes(")
    yield ("D4 the pre-world pump calls HandleOnlineTeamModes after DetectModeStarts, with the takedown cache's queue "
           "(console 0x823A5BE4 < 0x823A5C14, r6 = gsm+249936)",
           0 <= detect < call and "mpTakedownCache->mTakedownEventQueue" in gui[call:call + 300])
    actions = code_only(tree.read(ACTIONS_H))
    yield ("D4 action ids 165 PLAYER_ELIMINATED / 166 TRAITOROUS_TAKEDOWN / 167 SWITCH_BURNING_HOME_RUN_RUNNER "
           "(DWARF 157/158/159, +8)",
           all(re.search(pattern, actions) for pattern in (r"E_ACTION_PLAYER_ELIMINATED\s*=\s*165\s*,",
                                                           r"E_ACTION_TRAITOROUS_TAKEDOWN\s*=\s*166\s*,",
                                                           r"E_ACTION_SWITCH_BURNING_HOME_RUN_RUNNER\s*=\s*167\s*,")))


def numeric(tree):
    update = tree.read(UPDATE_MODE)
    network = tree.read(NETWORK_ID)
    texts = []
    try:
        texts.append(definition(update, HELPER))
        for signature in BODIES:
            texts.append(definition(update, signature))
        texts.append(definition(network, NETWORK_ID_BODY))
    except ValueError as error:
        print("NUMERIC: cannot build -- absent in this revision: " + str(error))
        return None
    body = "\n".join(texts)
    body = body.replace("ModeManager::", "ModeFixture::")
    body = re.sub(r"\bScoringSystem\b", "ScoringFixture", body)
    inc = "namespace BrnGameState {\n" + body + "\n}\n"
    shadow = {ACTIONS_H: tree.read(ACTIONS_H), SHARED_IO_H: tree.read(SHARED_IO_H)}
    return compile_and_run(Path(__file__).with_name("FxGsOnlineTeam.cpp"), "online_team_methods.inc", inc,
                           "FxGsOnlineTeam", shadow=shadow, extra_sources=(STRSTREAM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs_online_team", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
