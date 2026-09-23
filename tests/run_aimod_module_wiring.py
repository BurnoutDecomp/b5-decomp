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


RRM = "src/GameSource/World/AI/BrnRouteRequestManager.cpp"


def checks_route_requests(tree):
    events = tree.read(EVENTS)
    mode_start = code_only(function_body(events, "void AIModule::OnModeStart("))
    mode_end = code_only(function_body(events, "void AIModule::OnModeEnd("))
    # G04-D1: 0x82791E4C lwz 0x854(params) -> 0x82791E64 stwx this+0x424A8 (RRM+0x240), after
    #         SetupRaceBalancingManager (0x82791E44)
    astar = re.search(r"mRouteRequestManager\.SetDefaultAStarDistanceFunction\([^;]*GetAStarDistanceFunction\(\)", mode_start)
    balance = mode_start.find("SetupRaceBalancingManager(")
    yield ("G04-D1 OnModeStart sets the RRM default A* function from the mode (0x82791E64)",
           astar is not None and 0 <= balance < astar.start())
    # G04-D3: 0x8277BAAC stwx 0 -> +0x424A8 BEFORE the lbRestoreDrivingInput test (0x8277BAB0) and the
    #         inlined ClearBlockSections 0x8277BB8C..0x8277BBC8
    reset = re.search(r"mRouteRequestManager\.SetDefaultAStarDistanceFunction\(\s*E_ASTAR_DISTANCE_EUCLIDEAN\s*\)", mode_end)
    restore = mode_end.find("if (lbRestoreDrivingInput)")
    yield ("G04-D3 OnModeEnd resets the RRM default A* function first (0x8277BAAC)",
           reset is not None and 0 <= restore and reset.start() < restore)
    yield ("G04-D3 OnModeEnd clears every checkpoint's block sections (0x8277BB8C..0x8277BBC8)",
           "mRouteRequestManager.ClearBlockSections()" in mode_end)
    rrm = tree.read(RRM)
    construct = optional_body(rrm, "void RouteRequestManager::Construct(")
    yield ("G04-D1 RRM::Construct constructs the file-static mRandom (0x8278A3B0..0x8278A434)",
           "mRandom.Construct()" in construct)
    yield ("G04-D1 RRM::Construct zeroes every slot count and +0x240 (0x8278A438..0x8278A454)",
           re.search(r"mauBlockSectionIds\[\w+\]\.Construct\(\)", construct) is not None
           and re.search(r"meDefaultAStarDistanceFunction\s*=\s*E_ASTAR_DISTANCE_EUCLIDEAN", construct) is not None)
    setter = optional_body(rrm, "void RouteRequestManager::SetBlockSections(")
    yield ("G04-D1 RRM::SetBlockSections: :101 assert, count = 0, AppendArray<8> (0x82791ED4..0x82791F08)",
           "KI_MAX_LANDMARKS_IN_MODE" in setter and ".Construct()" in setter and ".AppendArray(" in setter)


def checks_prepare(tree):
    prepare = code_only(function_body(tree.read(AIMODULE), "bool AIModule::Prepare("))
    stage4 = prepare[prepare.find("case E_PREPARESTAGE_AICARS"):prepare.find("case E_PREPARESTAGE_DONE")]
    # G05-D1 (stage-4 tail): 0x8279829C stwx 0 -> +0x4EB60 and 0x8279838C stbx 0 -> +0x4EB81
    yield ("G05-D1 Prepare stage 4 zeroes miLineUpdateTokenCounter (0x8279829C)",
           re.search(r"miLineUpdateTokenCounter\s*=\s*0\s*;", stage4) is not None)
    yield ("G05-D1 Prepare stage 4 zeroes mbHighTakenDownPenalty (0x8279838C)",
           re.search(r"mbHighTakenDownPenalty\s*=\s*false\s*;", stage4) is not None)


def checks_paused(tree):
    pump = tree.read(PUMP)
    paused = optional_body(pump, "void AIModule::PausedUpdate(")
    update = code_only(function_body(pump, "void AIModule::Update("))
    # G05-D2: 0x8279A390..0x8279A4D8 -- the route round trip, then ProcessRequestInterface(r6 = lUpdateSet)
    sequence = ["HandleManagementEvents(lpInputBuffer)",
                "IOHelper<RouteMapModuleIO::OutputBuffer>",
                "AIModuleRoutes::AppendRaceRouteRequests(",
                "mRouteMapModule.Update(",
                "AIModuleRoutes::AppendRouteResponses(",
                "ProcessRequestInterface(lpInputBuffer, lpOutputBuffer, lUpdateSet)",
                "lpInputBuffer->UnlockForRead()",
                "lpOutputBuffer->UnlockForWrite()"]
    positions = [paused.find(item) for item in sequence]
    yield ("G05-D2 PausedUpdate runs the route round trip and ProcessRequestInterface (0x8279A390..0x8279A4C8)",
           all(p >= 0 for p in positions[:6]))
    yield ("G05-D2 ...in the console's order, unlocking input then output (0x8279A4D0/0x8279A4D8)",
           all(p >= 0 for p in positions) and positions == sorted(positions))
    yield ("G05-D2 PausedUpdate has no UpdateCarRoutes (the paused path only appends)",
           paused != "" and "UpdateCarRoutes(" not in paused)
    yield ("G05-D2 Update forwards lUpdateSet to PausedUpdate (0x8279B49C..0x8279B4B8)",
           re.search(r"PausedUpdate\([^;]*lUpdateSet\s*\)", update) is not None)


WORLD = "src/GameSource/World/BrnWorldModule.cpp"
AIMODULE_H = "src/GameSource/World/AI/BrnAIModule.h"
ROTM = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"


def checks_camera(tree):
    header = code_only(tree.read(AIMODULE_H))
    # G05-D3: DWARF BrnAIModule.h:354 `Camera mCamera` (X360 +0x4EA00) and :423 SetCamera(Camera&)
    yield ("G05-D3 AIModule has the DWARF mCamera member (X360 +0x4EA00)",
           re.search(r"BrnDirector::Camera::Camera\s+mCamera\s*;", header) is not None)
    yield ("G05-D3 AIModule::SetCamera(Camera&) copies into mCamera (DWARF :423)",
           re.search(r"void\s+SetCamera\(\s*BrnDirector::Camera::Camera&\s*\w+\s*\)\s*\{\s*mCamera\s*=\s*\w+\s*;\s*\}",
                     header) is not None)
    world = tree.read(WORLD)
    update = code_only(function_body(world, "WorldModule::Update( BrnUpdateSet lUpdateSet,"))
    drives = update.find("mAIModule.SetAIDrivesPlayer(")
    setcam = update.find("mAIModule.SetCamera(mLastCameraInput)")
    aiupdate = update.find("mAIModule.Update(")
    # 0x827D753C stbx (SetAIDrivesPlayer) -> 0x827D7540 Camera::operator= -> 0x827D7588 vtbl+0x44
    yield ("G05-D3 WorldModule::Update hands mLastCameraInput to the AI module (0x827D750C..0x827D7540)",
           setcam >= 0)
    yield ("G05-D3 ...after SetAIDrivesPlayer and before AIModule::Update, as on the console",
           0 <= drives < setcam < aiupdate)
    pump = code_only(function_body(tree.read(PUMP), "void AIModule::UpdateResetOnTrackManager("))
    # 0x8279AC10..0x8279AC24 Camera::Camera(stack, this+0x4EA00) -> r7 of ResetOnTrackManager::Update
    yield ("G05-D3 UpdateResetOnTrackManager passes mCamera as ROTM::Update's 4th argument (0x8279AC3C)",
           re.search(r"mResetOnTrackManager\.Update\(\s*lpResults\s*,[^;]*,\s*lfTime\s*,\s*mCamera\s*\)", pump)
           is not None)
    rotm = code_only(function_body(tree.read(ROTM), "    void ResetOnTrackManager::Update("))
    player = rotm.find("mePlayerGlobalRaceCarIndex = lePlayer;")
    copy = rotm.find("mCamera = lCamera;")
    ageing = rotm.find("mRecentResets.GetLength()")
    yield ("G08-D2 ROTM::Update stores the camera right after the player index (0x8279A8EC -> 0x8279A8F0)",
           0 <= player < copy < ageing)


STUBS = "src/GameSource/World/WorldLinkStubs.cpp"


def checks_destruct(tree):
    destruct = optional_body(tree.read(AIMODULE), "void AIModule::Destruct()")
    # G04-D7: 0x8276E398 base Destruct ; 0x8276E3A4 Clear(this+0x47FB0) ; 0x8276E3BC vtbl+0xC on
    #         this+0x483D8 ; 0x8276E40C ContactSpyInterface::Construct(this+0x4EB68)
    sequence = ["ModuleSingleBuffered::Destruct()", "mResourceReceiverQueue.Clear()",
                "mRouteMapModule.Destruct()", "mContactSpyInterface.Construct()"]
    positions = [destruct.find(item) for item in sequence]
    yield ("G04-D7 AIModule::Destruct is bodied in BrnAIModule.cpp (0x8276E380)", destruct != "")
    yield ("G04-D7 ...base Destruct, receiver-queue Clear, route-map Destruct, contact-spy Construct, in order",
           all(p >= 0 for p in positions) and positions == sorted(positions))
    stubs = code_only(tree.read(STUBS))
    yield ("G04-D7 the one-shot logging stub is gone from WorldLinkStubs.cpp",
           "BrnAI::AIModule::Destruct()" not in stubs)


def checks(tree):
    """Yield (name, passed) pairs."""
    yield from checks_routes(tree)
    yield from checks_route_requests(tree)
    yield from checks_prepare(tree)
    yield from checks_paused(tree)
    yield from checks_camera(tree)
    yield from checks_destruct(tree)
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
