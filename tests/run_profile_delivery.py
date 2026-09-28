"""L4 boot order (owner's list 2026-09-28): the loaded profile reaches the game state, as on the console.

THE CONSOLE'S BOOT DELIVERY. The profile finishes loading in the MemoryCard flow state, a video state: there
DoUpdate_GameStatePostWorld @0x823E92A8 skips PostWorldUpdate (`(updateSet & 0x20) == 0` gate), so the GUI's
352 -> game event 109 is dropped. What hands the profile over is game event 8 (E_EVENT_GAME_START), which
DoUpdate_GameStatePreWorld @0x823EE0E8 posts once MainGameFlowStateMemoryCard::Update @0x823F2F98 leaves the state:
ProcessGameEvents @0x823A0A18 case 8 (0x823A2758..0x823A2788) runs
    if (mbWaitForStreaming /* +0x38B70, armed by Construct, never cleared */) {
        OnProfileLoaded(this, out, queue);  WaitForStreaming(this, queue); }
The PC's event-8 seat called only WaitForStreaming, and OnProfileLoaded @0x82397310, GetSpawnCar @0x823763C8,
ProgressionManager::OnLoadProfile @0x823893A8 and ModeManager::OnProfileLoaded @0x82337A18 had no body. So on every
PC session the road-rule records were wiped (StreetManager::Update copies its zeroed tables into the profile), the
max-car count missed the owned sponsor cars, the junkyard spawned the default car, and the smashed props respawned.

  1. WIRING -- the seat, the case-8 arm, OnProfileLoaded's statement order and arguments, case 109, the two
     one-line callees, the retired PreWorldUpdate stand-in, Profile::FixUp.
  2. NUMERIC -- tests/ProfileDelivery.cpp compiles the PRODUCTION GetSpawnCar and OnLoadProfile onto a fixture.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_profile_delivery.py [--rev <b5 rev>]
                                                                                             [--root <shadow root>]
(--root reads any file present under <root>/... in place of the working tree's.)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

GAME_MODULE_CPP = "src/GameSource/Game/BrnGameModule.cpp"
GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
DISPATCH_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
PM_CPP = "src/GameSource/GameState/Progression/BrnProgressionManager.cpp"
PM_PREWORLD_CPP = "src/GameSource/GameState/Progression/BrnProgressionManager_PreWorldUpdate.cpp"
MODE_MANAGER_CPP = "src/GameSource/GameState/ModeManager/BrnModeManager.cpp"
MODE_MANAGER_H = "src/GameSource/GameState/ModeManager/BrnModeManager.h"
PROFILE_H = "src/GameSource/GameState/Progression/BrnProfile.h"
EVENTS_H = "src/GameSource/GameState/BrnGameEvents.h"

GAME_MAIN = "bool BrnGameModule::GameMain()"
CASE8_ARM = "void GameStateModule::ProcessGameEventsGameStartBringUp()"
ON_PROFILE_LOADED = "void GameStateModule::OnProfileLoaded("
GET_SPAWN_CAR = "CgsID GameStateModule::GetSpawnCar("
PROP_ARM = "void GameStateModule::ProcessGameEventsPropProgressionBringUp("
ON_LOAD_PROFILE = "void ProgressionManager::OnLoadProfile()"
PM_PREWORLD = "void ProgressionManager::PreWorldUpdate("
MM_ON_PROFILE_LOADED = "void ModeManager::OnProfileLoaded()"

NUMERIC_CHECKS = 14   # see ProfileDelivery.cpp


class RootTree(Tree):
    """The working tree (or --rev) with an optional shadow root whose files take precedence."""

    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig")
        try:
            return super().read(relative)
        except FileNotFoundError:
            return ""


def source(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def body(tree, relative, signature):
    try:
        return definition(source(tree, relative), signature)
    except ValueError:
        return ""


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def in_order(text, needles):
    """True when every needle occurs in `text`, each after the previous one."""
    position = -1
    for needle in needles:
        found = text.find(needle, position + 1)
        if found < 0:
            return False
        position = found
    return True


def wiring(tree):
    game_main = squash(body(tree, GAME_MODULE_CPP, GAME_MAIN))
    yield ("the MemoryCard-exit seat (game event 8) runs the extracted case-8 arm, not WaitForStreaming alone "
           "(DoUpdate_GameStatePreWorld @0x823EE0E8 posts 8; ProcessGameEvents case 8 0x823A2758)",
           "mGameStateModule.ProcessGameEventsGameStartBringUp()" in game_main
           and "mGameStateModule.WaitForStreaming(" not in game_main)

    arm = squash(body(tree, GSM_CPP, CASE8_ARM))
    yield ("the case-8 arm is the console's: if (mbWaitForStreaming /*+0x38B70*/) { OnProfileLoaded(out, queue); "
           "WaitForStreaming(queue); }, under the output write lock with mbIsUpdating raised",
           "if(mbWaitForStreaming){OnProfileLoaded(mpOutputBuffer,lpActionQueue);WaitForStreaming(lpActionQueue);}" in arm
           and in_order(arm, ["LockForWrite()", "mbIsUpdating=true", "if(mbWaitForStreaming)", "mbIsUpdating=false",
                              "UnlockForWrite()"]))

    opl = squash(body(tree, GSM_CPP, ON_PROFILE_LOADED))
    order = ["lpProfile->FixUp()",
             "mModeManager.ExitCurrentMode(lpOutput,true,static_cast<GameStateModuleIO::EGameModeType>(ModeManager::KI_GAME_MODE_SLOTS))",
             "mRoadRulesManager.QuitAnyActiveRules(lpOutput)",
             "mProgressionManager.OnLoadProfile()",
             "FindNearestJunkyardID(lpProfile->GetCarPosition())",
             "GetSpawnCar(lpProfile)",
             "FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex())",
             "mCarSelectManager.EnterJunkyardAtStartOfGame(lpOutputActionQueue,lJunkyardId,lSpawnCarId,0,leScoringIndex,&mCachedCarSelectChangedAction)",
             "OnSpecialEventPlayerCarChange(lpProfile->GetSpawnCarId(),lSpawnWheelId,lpOutputActionQueue,true)",
             "mStreetManager.OnProfileLoaded(lpOutput)",
             "mModeManager.OnProfileLoaded()",
             "GameStateModuleIO::E_ACTION_LOAD_PROFILE",
             "mbWaitingToPutPlayerInJunkyard=true",
             "static_cast<s8>(mProgressionManager.GetProgressionRank())<1",
             "GameStateModuleIO::E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK,4",
             "lpProfile->GetEventScoresToUpload()",
             "GameStateModuleIO::E_ACTION_NON_UPLOADED_MODE_SCORES",
             "RequestUnpause(2,lpOutputActionQueue)"]
    yield ("OnProfileLoaded @0x82397310 runs the console's 18 statements in the console's order (FixUp, ExitCurrentMode "
           "(out, 1, 0x12), QuitAnyActiveRules, OnLoadProfile, the junkyard nearest the SAVED position with GetSpawnCar's "
           "car and wheel 0, OnSpecialEventPlayerCarChange(saved car, 1), Street/ModeManager OnProfileLoaded, 194, "
           "+0x38B72, rank < 1 -> 28, 19, RequestUnpause(2))", in_order(opl, order))
    mode_manager_h = code_only(source(tree, MODE_MANAGER_H))
    gsm_source = source(tree, GSM_CPP)
    yield ("its two constants are the image's: ExitCurrentMode's `li r6, 0x12` == KI_GAME_MODE_SLOTS (18) and the "
           "rank-0 traffic scale flt_82005450 == 0x3F666666 == 0.9f",
           re.search(r"static\s+const\s+s32\s+KI_GAME_MODE_SLOTS\s*=\s*18\s*;", mode_manager_h) is not None
           and re.search(r"KF_ONPROFILELOADED_RANK_ZERO_TRAFFIC_SCALE\s*=\s*0\.9f\s*;", gsm_source) is not None)

    prop_arm = squash(body(tree, DISPATCH_CPP, PROP_ARM))
    events = code_only(source(tree, EVENTS_H))
    yield ("case 109 (an in-game load, GUI 352 bridged by BridgeGuiToGameState @0x823DDB78) calls OnProfileLoaded "
           "(0x823A33D4..0x823A33E0)",
           "if(liType==GameStateModuleIO::E_EVENT_PROGRESSION_PROFILE_LOADED){OnProfileLoaded(mpOutputBuffer,lpActionQueue);" in prop_arm
           and re.search(r"E_EVENT_PROGRESSION_PROFILE_LOADED\s*=\s*109\s*,", events) is not None)

    mm = squash(body(tree, MODE_MANAGER_CPP, MM_ON_PROFILE_LOADED))
    yield ("ModeManager::OnProfileLoaded @0x82337A18 is `addi r3, r3, 0x6E00 ; b ChallengeManager::OnProfileLoaded`",
           mm.endswith("{mChallengeManager.OnProfileLoaded();}"))

    pm_preworld = code_only(body(tree, PM_PREWORLD_CPP, PM_PREWORLD))
    olp = squash(body(tree, PM_CPP, ON_LOAD_PROFILE))
    yield ("ProgressionManager::OnLoadProfile @0x823893A8 exists and the PreWorldUpdate stand-in that restored the "
           "rank cache in its place is retired",
           olp.startswith("voidProgressionManager::OnLoadProfile(){UnlockDefaultPlayerCars();")
           and "sbRankCacheRestoredFromProfile" not in pm_preworld and bool(pm_preworld))

    profile = squash(source(tree, PROFILE_H))
    yield ("Profile::FixUp (DWARF BrnProfile.h:425, inlined @0x82397364..0x8239738C) re-points the licence picture: "
           "NetworkTexture::Construct, then Prepare(buffer, 9600, 160, 120, DXT1)",
           "voidFixUp(){mPlayerLicencePicture.Construct();mPlayerLicencePicture.Prepare(&macPlayerLicenceTextureData[0],"
           "KI_PLAYERLICENCEPICTURE_TEXTURESIZEINBYTES,KI_PLAYERLICENCEPICTURE_WIDTH,KI_PLAYERLICENCEPICTURE_HEIGHT,"
           "renderengine::PIXELFORMAT_DXT1);}" in profile)


def numeric(tree):
    get_spawn_car = body(tree, GSM_CPP, GET_SPAWN_CAR)
    on_load_profile = body(tree, PM_CPP, ON_LOAD_PROFILE)
    missing = [name for name, text in (("GetSpawnCar", get_spawn_car), ("OnLoadProfile", on_load_profile)) if not text]
    if missing:
        print("NUMERIC: cannot build -- no " + ", ".join(missing))
        return None
    return compile_and_run(Path(__file__).with_name("ProfileDelivery.cpp"), "profile_getspawncar.inc",
                           get_spawn_car + "\n", "ProfileDelivery",
                           extra_files={"profile_onloadprofile.inc": on_load_profile + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    return report("run_profile_delivery", checks, numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
