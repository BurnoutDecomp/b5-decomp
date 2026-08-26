// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_OnlineGridStubs.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// stuntrace wave-B MOUNT-CLOSURE round, 2026-08-26.
//
// FLAG link gate -- NEITHER BODY BELOW IS A RECONSTRUCTION. Two ONLINE-ONLY grid legs that
// wave B DECLARED (BrnModeManager.h:356-359) and that the online mode TUs CALL, so the mount
// needs the symbols; both real bodies are the online wave's work and are deliberately NOT
// guessed here. Each stub is inert and each records the console's own shape verbatim so
// whoever bodies it starts from the asm, not from this file.
//
// WHY INERT IS SAFE TODAY -- the complete call-site set (grepped, whole tree):
//   ModeManager::SetOnlineRaceCars
//     * BrnOnlineFreeBurnLobbyMode.cpp:72   OnlineFreeBurnLobbyMode::SetupGameModeParams
//     * BrnOnlineShowtimeMode.cpp:150       OnlineShowtimeMode::SetupGameModeParams
//     * BrnOnlineStuntRunMode.cpp:311       OnlineStuntRunMode::SetupGameModeParams
//   ModeManager::SetupOnlineStartingGrid
//     * BrnOnlineStuntRunMode.cpp:309       OnlineStuntRunMode::SetupGameModeParams
// Every one is inside an Online*Mode override, reached only once ModeManager has entered an
// online game mode, and each is fed a GameStateModuleIO::StartNetworkGameEvent that only the
// network round manager produces. The offline boot -> title -> junkyard -> DRIVING path never
// constructs an online mode, so neither leg executes. Both console asserts confirm the gate is
// the mode itself: SetOnlineRaceCars fires "mpCurrentGameMode->IsOnline()"
// (BrnModeManager.cpp:4343) before it copies anything.
//
// ⛔ DELETE-WHEN: the ONLINE MODE-START wave lands the real
// ModeManager::SetOnlineRaceCars / ModeManager::SetupOnlineStartingGrid. Delete this WHOLE
// file in the same change that adds them -- two definitions of either symbol is an LNK2005,
// and `cl /c` cannot see the collision.
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint (the one-shot gap log)

namespace BrnGameState
{

// ------------------------------------------------------------------------------------------
// ModeManager::SetOnlineRaceCars -- X360 0x82311E98. INERT STUB.
//
// The console body is a fixed 8-slot unrolled record copy out of the StartNetworkGameEvent into
// the GameModeParams, plus a head and four asserts. Dumped in full this session; recorded here
// as OFFSETS ONLY, because the tree's GameModeParams is explicitly NOT offset-faithful
// (BrnOnlineFreeBurnLobbyMode.cpp:55-60 says so in-tree, mapping two of these very words
// "best-effort ... LOW confidence on the exact named identity"). Naming members off these
// offsets is precisely the one-slot-accessor guess this campaign just paid for, so it is not
// done here.
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
// Doing nothing leaves the params exactly as the caller's own explicit copies left them (each
// Online*Mode::SetupGameModeParams already copies the network-id and team runs by NAME before
// calling this), which is the closest inert state to "the grid was never populated".
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
// Not a cheap export: 261 instructions, VMX-wide, and its closure is four template
// instantiations plus two record types this partfile has no faithful model for --
//   * Array<GridPositionAndScoreData,7>[9] + a 10th overflow bucket, built on the stack and
//     seeded to the CgsArray.h "used before Construct/Clear" sentinel (-1);
//   * BubbleSort<GridPositionAndScoreData,...> (lbPushForwards arm) vs
//     Shuffle<GridPositionAndScoreData,...>(..., lpRandom) (the else arm) -- the seeded-order
//     vs randomised-order fork;
//   * Array<int,8>::Append / ::FindFirstInstanceOf (CgsArrayInt8.cpp already names this
//     function as the reason those instantiations exist);
//   * BrnTraffic::LightTriggerStartData::GetStartPosition / GetStartDirection (both real, in
//     BrnTrafficLightTrigger.cpp) reached through a hull start block resolved from
//     GameModeParams' junction word, then GameModeParams::StartLocation<8>::Append with the
//     BrnGameModeParams.h:1168 "BrnMath::IsNormal( lDirection )" assert.
// Bodying it against the tree's non-offset-faithful GameModeParams would be a guess; it is the
// online wave's function.
//
// Doing nothing leaves the params' start-location array empty, which the caller's very next
// statement (SetOnlineRaceCars, also stubbed above) does not depend on.
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
