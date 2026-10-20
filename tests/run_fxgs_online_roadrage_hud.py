"""FX-GS source-only follow-on: the three Road Rage HUD producer bodies.

The runner extracts the real HUDMessageLogic methods from source, rehomes their receiver and
ScoringSystem onto small fixture classes, and describes ARTIST-derived numeric cases. The fixture
is paired with CP09: its first-team-not-1 case is expected to expose the frozen CP02 predicate if
CP09 has not yet been composed. This runner was not executed during recovery.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs_online_roadrage_hud.py [--rev <b5 rev>]
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
SHARED_IO_H = "src/GameSource/GameState/BrnGameStateSharedIO.h"
BODIES = [
    "void HUDMessageLogic::GenerateOnlineBlueTeamEscapingMessage(",
    "void HUDMessageLogic::GenerateOnlineBlueTeamAreBehindYouMessage(",
    "void HUDMessageLogic::GenerateOnlineLeaderMilestoneMessage(",
]
NUMERIC_CHECKS = 19


def switch_arm(body, name):
    pattern = r"case\s+GameStateModuleIO::" + name + r"\s*:\s*(?://[^\n]*)?\s*(.*?)break\s*;"
    match = re.search(pattern, body, re.S)
    return match.group(1) if match else ""


def wiring(tree):
    hud = tree.read(HUD_CPP)
    post = body_or_empty(hud, "void HUDMessageLogic::PostWorldUpdate(")
    road = switch_arm(post, "E_MODE_ONLINE_ROAD_RAGE")
    calls = [
        road.find("GenerateOnlineBlueTeamEscapingMessage(lpScoringSystem, lfDelta, lePlayerActiveRaceCarIndex)"),
        road.find("GenerateOnlineBlueTeamAreBehindYouMessage(lpScoringSystem, lfDelta, lePlayerActiveRaceCarIndex)"),
        road.find("GenerateOnlineLeaderMilestoneMessage(lpScoringSystem, lfDelta, lePlayerActiveRaceCarIndex,"),
    ]
    yield ("D1 case 11 calls all three generators in ARTIST order and passes the progress byte",
           all(i >= 0 for i in calls) and calls == sorted(calls)
           and "lbIsGameModeInProgress" in road[calls[2]:])

    header = code_only(tree.read(HUD_H))
    yield ("D1 PostWorldUpdate declares the mode-progress input consumed by the third generator",
           re.search(r"void\s+PostWorldUpdate\s*\([^;]*bool\s+lbIsGameModeInProgress", header, re.S) is not None)

    actions = code_only(tree.read(ACTIONS_H))
    checks = (
        r"E_ACTION_BLUE_TEAM_IS_ESCAPING\s*=\s*162\s*,",
        r"E_ACTION_BLUE_TEAM_IS_BEHIND_YOU\s*=\s*163\s*,",
        r"E_ACTION_LEADER_REACHED_BOUNDARY\s*=\s*164\s*,",
        r"static_assert\(sizeof\(BlueTeamIsEscapingAction\)\s*==\s*1",
        r"static_assert\(sizeof\(BlueTeamIsBehindYouAction\)\s*==\s*1",
        r"static_assert\(sizeof\(LeaderReachedBoundaryAction\)\s*==\s*12",
    )
    yield ("D1 producer action ids and real 1/1/12-byte record types remain pinned",
           all(re.search(pattern, actions) is not None for pattern in checks))


def numeric(tree, legacy_team_predicate=False):
    hud = tree.read(HUD_CPP)
    bodies = []
    for signature in BODIES:
        try:
            bodies.append(definition(hud, signature))
        except ValueError as error:
            print("NUMERIC: cannot build -- absent in this revision: " + str(error))
            return None

    body = "\n".join(bodies)
    if legacy_team_predicate:
        predicate = "leCandidateTeam != GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM ||"
        if body.count(predicate) != 1:
            raise ValueError("Cannot isolate the production team predicate for the negative control")
        body = body.replace(predicate, "leCandidateTeam == GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM &&", 1)
        print("NEGATIVE CONTROL: restore the old team predicate in the extracted body only")
    body = body.replace("HUDMessageLogic::", "HUDFixture::")
    body = re.sub(r"\bScoringSystem\b", "ScoringFixture", body)
    body = re.sub(r"\bCarData\b", "CarFixture", body)
    includes = re.findall(r"^#include [^\n]+", hud, re.M)
    constants = re.findall(r"^    const (?:f32|s32) K[FI]_[A-Z_]+\s*=[^;]+;", hud, re.M)
    inc = "\n".join(includes + ["namespace BrnGameState {", "namespace {"] + constants + ["}", body, "}"])
    shadow = {HUD_H: tree.read(HUD_H), ACTIONS_H: tree.read(ACTIONS_H), SHARED_IO_H: tree.read(SHARED_IO_H)}
    result = compile_and_run(Path(__file__).with_name("FxGsOnlineRoadRageHud.cpp"),
                           "online_roadrage_hud_methods.inc", inc,
                           "FxGsOnlineRoadRageHud", shadow=shadow, extra_sources=(STRSTREAM_CPP,))
    if result is not None and result[0] != NUMERIC_CHECKS:
        print("NUMERIC: unexpected executed check count: " + str(result[0]))
        return None
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--legacy-team-predicate", action="store_true",
                        help="negative control: restore only the old team-selection predicate")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs_online_roadrage_hud", list(wiring(tree)),
                  numeric(tree, args.legacy_team_predicate), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
