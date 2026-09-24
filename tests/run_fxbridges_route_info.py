"""FX-BRIDGES (crash parity 2026-09-24) CC-11: the checkpoint-distance route pair.

A race with more than one landmark asks the AI route planner for the length of every checkpoint-to-checkpoint leg
(ModeManager::UpdateCheckpointDistanceRequests @0x823279B8 -> GameStateModule::SendRouteRequestAction @0x82381DC8 ->
action 50), and the answers come back through BridgeWorldToGameState @0x823E5368 leg 10 as game event 174, which
ProcessGameEvents' case 174 hands to ModeManager::HandleCheckpointDistanceResponse @0x8231E6C8 -- the only path by
which ScoringSystem::SetCheckpointDistances / ProcessFinishDistances ever run. On the PC every piece of that loop was
missing (the request was built but never sent, event 174 had no type, no leg 10, no arm, no handler), so a race never
learnt its checkpoint-to-finish distances.

Numeric (two fixtures, summed):
  FxBridgesRouteInfoGameState.cpp -- the PRODUCTION SendRouteRequestAction, its nearest-section helper (0x8267A588)
                                     and HandleCheckpointDistanceResponse against the real event / action records;
  FxBridgesRouteInfoBridge.cpp    -- the PRODUCTION leg-10 helper against the real RouteResponse queue and the real
                                     VariableEventQueue<1536,16>.
A revision without the bodies cannot build them: every numeric check then counts as failed.
Wiring: the request producer calls SendRouteRequestAction as E_OWNER_MODE_MANAGER; BridgeWorldToGameState runs leg 10;
the one-feed post-world seam in BrnGameModule.cpp builds its game-event queue from legs 2 + 10; the case-174 arm is
drained before the ModeManager tick.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_route_info.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report, STRSTREAM_CPP

GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
MODE_CPP = "src/GameSource/GameState/ModeManager/BrnModeManager_UpdateMode.cpp"
DRAIN_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToX.cpp"
MODULE_CPP = "src/GameSource/Game/BrnGameModule.cpp"
EVENTS_H = "src/GameSource/GameState/BrnGameEvents.h"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
NUMERIC_GAMESTATE = 15
NUMERIC_BRIDGE = 5


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def body(source, signature):
    try:
        text = definition(source, signature)
    except ValueError:
        return None
    return text[text.index("{"):] + "\n"


def whole(source, signature):
    try:
        return definition(source, signature) + "\n"
    except ValueError:
        return None


def wiring(tree):
    mode = squash(body(tree.read(MODE_CPP), "void ModeManager::UpdateCheckpointDistanceRequests(") or "")
    yield ("UpdateCheckpointDistanceRequests sends the pair as E_OWNER_MODE_MANAGER (0x82327B30 `li r6, 2`; 0x82327B88)",
           "mpGameStateModule->SendRouteRequestAction(&lRouteRequest,lpGameActionQueue,BrnAI::RouteMapModuleIO::E_OWNER_MODE_MANAGER);" in mode)
    bridge = squash(body(tree.read(BRIDGE_CPP), "void BrnGameModule::BridgeWorldToGameState(") or "")
    yield ("BridgeWorldToGameState runs leg 10 onto the post-world game-event queue (0x823E5494..0x823E554C)",
           "BridgeWorldToGameState_RouteInfo(lpGameStateInput->GetGameEventQueue(),lpWorldOutput);" in bridge)
    module = squash(tree.read(MODULE_CPP))
    seam = module.find("lPostWorldGameEventQueue.Append(*lpcWorldOutput->GetGameEventQueue());")
    leg10 = module.find("BrnGame::BridgeWorldToGameState_RouteInfo(&lPostWorldGameEventQueue,lpcWorldOutput);")
    feed = module.find("&lPostWorldGameEventQueue,", max(leg10, 0))
    yield ("the one-feed post-world seam builds its game-event queue from legs 2 then 10 and feeds it",
           0 <= seam < leg10 < feed)
    drain = squash(tree.read(DRAIN_CPP))
    arm = drain.find("ProcessGameEventsModeManagerRouteInfoBringUp(&lGameEventQueue);")
    tick = drain.find("mModeManager.PreWorldUpdate(")
    yield ("the case-174 arm is drained before the ModeManager tick (ProcessGameEvents precedes it)", 0 <= arm < tick)


def numeric_gamestate(tree):
    gsm = tree.read(GSM_CPP)
    nearest = whole(gsm, "u16 FindNearestAISectionWithoutPointMap(")
    send = body(gsm, "void GameStateModule::SendRouteRequestAction(")
    handle = body(tree.read(MODE_CPP), "void ModeManager::HandleCheckpointDistanceResponse(")
    missing = [name for name, text in (("FindNearestAISectionWithoutPointMap", nearest),
                                       ("GameStateModule::SendRouteRequestAction", send),
                                       ("ModeManager::HandleCheckpointDistanceResponse", handle)) if text is None]
    if missing:
        print("NUMERIC (game state): missing bodies: " + ", ".join(missing))
        return None
    shadow = {EVENTS_H: tree.read(EVENTS_H), ACTIONS_H: tree.read(ACTIONS_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxBridgesRouteInfoGameState.cpp"),
                           "fxbridges_route_info_send.inc", send, "FxBridgesRouteInfoGameState",
                           shadow=shadow, extra_sources=[STRSTREAM_CPP],
                           extra_files={"fxbridges_route_info_nearest.inc": nearest,
                                        "fxbridges_route_info_handle.inc": handle})


def numeric_bridge(tree):
    leg10 = body(tree.read(BRIDGE_CPP), "    void BridgeWorldToGameState_RouteInfo(")
    if leg10 is None:
        print("NUMERIC (bridge): missing body: BridgeWorldToGameState_RouteInfo")
        return None
    shadow = {EVENTS_H: tree.read(EVENTS_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxBridgesRouteInfoBridge.cpp"),
                           "fxbridges_route_info_leg10.inc", leg10, "FxBridgesRouteInfoBridge",
                           shadow=shadow, extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    game_state = numeric_gamestate(tree) or (NUMERIC_GAMESTATE, NUMERIC_GAMESTATE)
    bridge = numeric_bridge(tree) or (NUMERIC_BRIDGE, NUMERIC_BRIDGE)
    numeric = (game_state[0] + bridge[0], game_state[1] + bridge[1])
    return report("run_fxbridges_route_info", list(wiring(tree)), numeric, NUMERIC_GAMESTATE + NUMERIC_BRIDGE)


if __name__ == "__main__":
    sys.exit(main())
