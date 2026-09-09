// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_OnlineGridStubs.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
//
// FLAG link gate -- NEITHER BODY BELOW IS A RECONSTRUCTION. Two online-only grid legs
// declared in BrnModeManager.h and called by the Online*Mode::SetupGameModeParams overrides
// (BrnOnlineFreeBurnLobbyMode / BrnOnlineShowtimeMode / BrnOnlineStuntRunMode), which only
// run once an online game mode is entered and only on a StartNetworkGameEvent the network
// round manager produces. The offline boot -> title -> junkyard -> driving path never
// constructs an online mode, so neither leg executes. Each records the console's own shape
// so whoever bodies it starts from the asm.
//
// DELETE-WHEN: the online mode-start wave lands the real ModeManager::SetOnlineRaceCars /
// ModeManager::SetupOnlineStartingGrid. Delete this whole file in the same change (two
// definitions of either symbol is an LNK2005).
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint (the one-shot gap log)

namespace BrnGameState
{

// ------------------------------------------------------------------------------------------
// ModeManager::SetOnlineRaceCars -- X360 0x82311E98. INERT STUB.
//
// The console body is a fixed 8-slot unrolled record copy out of the StartNetworkGameEvent into
// the GameModeParams, plus a head and four asserts. Recorded as offsets only: the tree's
// GameModeParams is not offset-faithful (BrnOnlineFreeBurnLobbyMode.cpp), so naming
// members off these offsets would be a guess.
//
//   head:
//     0x82311EB0  mpGameStateModule->GetProgressionManager() assert  (.cpp:4334)
//     0x82311EE4  lpStartNetworkGameEvent->miNumRaceCars > 0 assert  (.cpp:4336), field event+0x00
//     0x82311F0C  lwz r11, 0(ev) / addi -1 / stb r11, 1(params)   ; params+0x01 = numRaceCars - 1
//     0x82311F18  lwz r11, 0xE0(ev)        / stw r11, 0x138(params)
//     0x82311F20  mpCurrentGameMode assert          (.cpp:4342)
//     0x82311F48  mpCurrentGameMode->IsOnline() assert, byte mode+0xAC   (.cpp:4343)
//     0x82311F74  lpCurrentOnlineGameMode assert    (.cpp:4345)
//   then i = 0..7, five copies per slot (0x82311F9C..0x823120D8):
//     lwz  ev+0x98 + 4*i   -> stw  params+0x008 + 4*i      (word)
//     ld   ev+0x18 + 8*i   -> std  params+0x098 + 8*i      (qword)
//     lhz  ev+0x58 + 2*i   -> sth  params+0x0F8 + 2*i      (halfword)
//     lhz  ev+0x68 + 2*i   -> sth  params+0x108 + 2*i      (halfword)
//     lfs  ev+0xB8 + 4*i   -> stfs params+0x0D8 + 4*i      (float)
//
// Doing nothing leaves the params as the caller's own explicit copies left them (each
// Online*Mode::SetupGameModeParams copies the network-id and team runs by name before
// calling this).
// ------------------------------------------------------------------------------------------
void ModeManager::SetOnlineRaceCars(GameModeParams* /*lpGameModeParams*/,
                                    const GameStateModuleIO::StartNetworkGameEvent* /*lpStartNetworkGameEvent*/) const
{
    static bool lsbLogged = false;
    if (!lsbLogged)
    {
        lsbLogged = true;
        *CgsDev::Log::gpDebugPrint
            << "[ModeManager] SetOnlineRaceCars: inert link stub (FLAG online-only leg, X360 0x82311E98).\n";
    }
}

// ------------------------------------------------------------------------------------------
// ModeManager::SetupOnlineStartingGrid -- X360 0x82337600. INERT STUB.
//
// 261 instructions, VMX-wide; its closure is four template instantiations plus two record
// types this partfile has no faithful model for:
//   * Array<GridPositionAndScoreData,7>[9] + a 10th overflow bucket, built on the stack and
//     seeded to the CgsArray.h "used before Construct/Clear" sentinel (-1);
//   * BubbleSort<GridPositionAndScoreData,...> (lbPushForwards arm) vs
//     Shuffle<GridPositionAndScoreData,...>(..., lpRandom) (the else arm);
//   * Array<int,8>::Append / ::FindFirstInstanceOf (CgsArrayInt8.cpp);
//   * BrnTraffic::LightTriggerStartData::GetStartPosition / GetStartDirection (both real, in
//     BrnTrafficLightTrigger.cpp) reached through a hull start block resolved from
//     GameModeParams' junction word, then GameModeParams::StartLocation<8>::Append with the
//     BrnGameModeParams.h:1168 "BrnMath::IsNormal( lDirection )" assert.
//
// Doing nothing leaves the params' start-location array empty, which the caller's next
// statement (SetOnlineRaceCars, above) does not depend on.
// ------------------------------------------------------------------------------------------
void ModeManager::SetupOnlineStartingGrid(GameModeParams* /*lpGameModeParams*/,
                                          s32 /*liNumRaceCars*/,
                                          CgsNumeric::Random* /*lpRandom*/,
                                          bool /*lbPushForwards*/) const
{
    static bool lsbLogged = false;
    if (!lsbLogged)
    {
        lsbLogged = true;
        *CgsDev::Log::gpDebugPrint
            << "[ModeManager] SetupOnlineStartingGrid: inert link stub (FLAG online-only leg, X360 0x82337600).\n";
    }
}

}
