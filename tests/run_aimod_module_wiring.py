"""FX-AIMOD (crash parity 2026-09-22): structural regression for AIModule wiring the console has.

Each check extracts a PRODUCTION function body from the real source file and requires the console's
stores/calls, citing the ARTIST addresses that prove them. Pure wiring (a store, a call, an argument)
is what this runner covers; numeric behaviour lives in the AIMod*.cpp unit tests.

Run from the workflow checkout:
    python b5-decomp/tests/run_aimod_module_wiring.py [--rev <b5 git rev>]
--rev reads every source file from that b5-decomp revision instead of the working tree (the RED side:
e.g. --rev <commit>~1 for the commit that landed a check).
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]


class Tree:
    def __init__(self, rev):
        self.rev = rev

    def read(self, relative):
        if self.rev is None:
            return (REPO / relative).read_text(encoding="utf-8-sig")
        return subprocess.run(["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"],
                              check=True, capture_output=True, text=True, encoding="utf-8").stdout


def function_body(source, signature):
    start = source.index(signature)
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError("unterminated body: " + signature)


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


AIMODULE = "src/GameSource/World/AI/BrnAIModule.cpp"
EVENTS = "src/GameSource/World/AI/BrnAIModule_Events.cpp"
ROUTES = "src/GameSource/World/AI/BrnAIModule_Routes.cpp"
PUMP = "src/GameSource/World/AI/BrnAIModule_ResetPump.cpp"


def optional_body(source, signature):
    try:
        return code_only(function_body(source, signature))
    except ValueError:
        return ""


def checks_routes(tree):
    routes = tree.read(ROUTES)
    member = optional_body(routes, "void AIModule::UpdateCarRoutes(")
    # G04-D4: 0x82795710..0x8279575C -- opponent != -1 && !player && in game mode ->
    #         RaceBalancingManager::UpdateOpponentRoute(car, GetAISectionsData()) off this+0x3D9D0
    yield ("G04-D4 UpdateCarRoutes is the AIModule member (DWARF BrnAIModule.cpp:1495)", member != "")
    call = re.search(r"mRaceBalancingManager\.UpdateOpponentRoute\(\s*lpAICar\s*,\s*GetAISectionsData\(\)\s*\)", member)
    gate = re.search(r"GetOpponentIndex\(\)\s*!=\s*-1\s*&&\s*!\s*lpAICar->IsPlayerCar\(\)\s*&&\s*lpAICar->mbIsInGameMode", member)
    yield ("G04-D4 UpdateCarRoutes calls mRaceBalancingManager.UpdateOpponentRoute (0x8279575C)", call is not None)
    yield ("G04-D4 ...under the console's opponent/player/in-mode gate (0x82795710..0x82795744)",
           call is not None and gate is not None and gate.start() < call.start())
    # G04-D5 / G05-D5: 0x8279577C GetAIDriver(mePlayerActiveRaceCarIndex) ; 0x82795780 lwz 0x1CE0 (mpCar)
    driver = re.search(r"GetAIDriver\(\s*mePlayerActiveRaceCarIndex\s*\)", member)
    getcar = re.search(r"->GetCar\(\)\s*!=\s*0", member)
    player = re.search(r"GetAICar\(\s*static_cast<u32>\(\s*mePlayerGlobalRaceCarIndex\s*\)\s*\)", member)
    event = member.find("KI_EVENT_PLAYER_ROUTE_UPDATED")
    yield ("G04-D5/G05-D5 event 117 gated on the player's active-slot driver mpCar (0x8279577C..0x82795788)",
           driver is not None and getcar is not None and player is not None and 0 <= event
           and driver.start() < getcar.start() < player.start() < event)
    pump = tree.read(PUMP)
    update = code_only(function_body(pump, "void AIModule::Update("))
    yield ("G04-D5/G05-D5 Update no longer hands UpdateCarRoutes a pre-computed player car",
           "ProcessRouteResponses(this, lpOutputBuffer, lpRouteOut, lpPlayerCar)" not in update)
    # G05-D4: 0x8279B678..0x8279B6C0 -- RaceBalancingManager::Update inlined right after row 12,
    #         before the route-input lock / HandleGameActions (0x8279B6CC / 0x8279B6E0)
    clock = update.find("mRaceBalancingManager.Update(lpPlayerCar, lfDt)")
    actions = update.find("HandleGameActions(")
    yield ("G05-D4 Update runs the race clock (RaceBalancingManager::Update) before HandleGameActions",
           0 <= clock < actions)


def checks(tree):
    """Yield (name, passed) pairs."""
    yield from checks_routes(tree)
    events = tree.read(EVENTS)
    mode_start = code_only(function_body(events, "void AIModule::OnModeStart("))
    # G04-D2: 0x82791DF4 lbz 0x94 ; cntlzw ; extrwi -> stbx 0x4EB7C and 0x82791E24 lbz 0x94 -> stbx 0x4EB7D
    enable = re.search(r"mbEnableDrivingInput\s*=\s*!\s*lpGameModeParams->mbIsOnline\s*;", mode_start)
    online = re.search(r"mbIsInOnlineGameMode\s*=\s*lpGameModeParams->mbIsOnline\s*;", mode_start)
    balance = mode_start.find("SetupRaceBalancingManager(")
    yield ("G04-D2 OnModeStart: mbEnableDrivingInput = !params.mbIsOnline (0x82791DF4..0x82791E1C)", enable is not None)
    yield ("G04-D2 OnModeStart: mbIsInOnlineGameMode = params.mbIsOnline (0x82791E24/0x82791E38)", online is not None)
    yield ("G04-D2 both stores precede SetupRaceBalancingManager (0x82791E44), as on the console",
           enable is not None and online is not None and 0 <= balance
           and enable.start() < balance and online.start() < balance)

    construct = code_only(function_body(tree.read(AIMODULE), "void AIModule::Construct()"))
    # G04-D6: 0x82794D34 li r30,0 ; 0x827952F4 stwx r30 -> +0x4E9F8 ; 0x8279530C stwx r30 -> +0x4E9FC
    yield ("G04-D6 Construct stores 0 to mePlayerActiveRaceCarIndex (0x827952F4)",
           re.search(r"mePlayerActiveRaceCarIndex\s*=\s*E_ACTIVE_RACE_CAR_INDEX_0\s*;", construct) is not None)
    yield ("G04-D6 Construct stores 0 to mePlayerGlobalRaceCarIndex (0x8279530C)",
           re.search(r"mePlayerGlobalRaceCarIndex\s*=\s*E_GLOBAL_RACE_CAR_INDEX_0\s*;", construct) is not None)
    yield ("G04-D6 no INVALID seed of the active cursor",
           "mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID" not in construct)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None)
    args = parser.parse_args()
    results = list(checks(Tree(args.rev)))
    failures = [name for name, passed in results if not passed]
    for name in failures:
        print("FAIL", name)
    print(f"AIModModuleWiring: {len(results)} checks, {len(failures)} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
