#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // BrnGameState::ModeManager::GetCurrentGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // BrnGameState::GameMode::IsOnline
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::EGameModeType (E_MODE_*_SHOWTIME)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // BrnProgression::ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                // BrnProgression::Profile::SetCarUnlockAlreadyShown
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // GameStateModuleIO::OutputBuffer (owned by pointer)

namespace BrnGameState
{
// ----------------------------------------------------------------------------
// Construct / Destruct.
//
// The console's GameStateModule is a CgsModule::ModuleSingleBuffered, so its
// GameStateModuleIO::OutputBuffer is the DataStructure the base allocates from the module's
// own DataBuffer inside Prepare() -> CreateOutputDataStructure(). ⚠️ FLAG (PC bring-up seam):
// nothing on PC calls this module's Prepare(), so that allocation point never runs. The
// buffer is newed here instead and freed in Destruct(); the buffer TYPE, its Construct
// (X360 0x82382940) and its accessors are the real console ones -- only the allocation SITE
// moves. DELETE-WHEN the module's Prepare()/CreateOutputDataStructure() path is real.
//
// Constructing it is what makes the game-state -> director game-action route exist at all:
// BrnGameModule::BridgeGameStateToDirector @0x823CD170 Appends this buffer's +0x04 queue into
// the director input buffer's queue every frame, and MainDirector::ProcessInputQueue drains it.
// ----------------------------------------------------------------------------
void GameStateModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    if (mpOutputBuffer == 0)
    {
        mpOutputBuffer = new GameStateModuleIO::OutputBuffer();
        mpOutputBuffer->Construct();
    }
}

void GameStateModule::Destruct()
{
    if (mpOutputBuffer != 0)
    {
        delete mpOutputBuffer;
        mpOutputBuffer = 0;
    }

    CgsModule::ModuleSingleBuffered::Destruct();
}

// X360 @ 0x823116D0. Returns whether the currently-running game mode is one of the online modes. May
// only be called while the module is updating (asserts mbIsUpdating). Fetches the current game mode
// from the embedded ModeManager and forwards to GameMode::IsOnline(); if there is no current mode,
// returns false. (X360 reads the current-mode pointer inline as *(this + 0x1DB8) inside mModeManager
// and its mbIsOnline at *(mode + 172); de-inlined to the two logical calls.)
bool GameStateModule::IsOnlineGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
    if (lpCurrentGameMode != nullptr)
    {
        return lpCurrentGameMode->IsOnline();
    }
    return false;
}

// The cached current-game-mode type. The X360 reads it as the raw scalar just below the embedded
// mModeManager (GameStateModule+7604 == mModeManager+0xD94, meCurrentGameModeType); de-inlined to
// the ModeManager's own named accessor -- same read, no offset poke.
GameStateModuleIO::EGameModeType GameStateModule::GetCurrentGameModeType() const
{
    return mModeManager.GetCurrentGameModeType();
}

// X360 @ 0x82311620. Returns the player's GLOBAL race-car index (its slot in the full world race-car
// table). May only be called while the module is updating (asserts mbIsUpdating).
s32 GameStateModule::GetPlayerGlobalRaceCarIndex()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return miPlayerGlobalRaceCarIndex;
}

// X360 @ 0x82356870. Returns whether the active race car in slot leRaceCarIndex is currently crashing.
// Asserts the module is updating and that the index is in [E_ACTIVE_RACE_CAR_INDEX_0,
// E_ACTIVE_RACE_CAR_INDEX_COUNT); reproduces the three X360 asserts verbatim (the module-updating one,
// then the two range guards) before returning the cached per-slot crash flag.
bool GameStateModule::IsRaceCarCrashing(::EActiveRaceCarIndex leRaceCarIndex)
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return maRaceCarCrashing[leRaceCarIndex];
}

// X360 @ 0x823567A8. True when the current game mode is a showtime mode (offline or online). May only
// be called while the module is updating (asserts mbIsUpdating). The X360 reads the current game-mode
// type inline (this+7604) and tests == E_MODE_OFFLINE_SHOWTIME (2) || == E_MODE_ONLINE_SHOWTIME (16);
// de-inlined to the GetCurrentGameModeType() accessor (same read) to avoid a raw offset access.
bool GameStateModule::IsShowtimeGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    return leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
        || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
}

// X360 @ 0x82356978. True when the simulation is currently paused. miSimPauseFlags is a bitfield of
// active pause reasons; a nonzero value means paused. When lbCheckGameMode is set and the current mode
// is online, some pause-reason bits are ignored: the X360 masks off bits {2,3,5} (mask 0xFFFFFFD3, the
// strict path -- returns immediately) when lbStrictMask is set, otherwise bits {1,2,3,5}
// (mask 0xFFFFFFD1). (The two bool parameters are named from the asm's branch structure; their exact
// call-site meaning is confirmed when the pause callers -- RequestPause/RequestUnpause -- are homed.)
bool GameStateModule::IsSimPaused(bool lbCheckGameMode, bool lbStrictMask) const
{
    s32 liPauseFlags = miSimPauseFlags;
    if (lbCheckGameMode)
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode != nullptr && lpCurrentGameMode->IsOnline())
        {
            if (lbStrictMask)
            {
                return (liPauseFlags & ~0x2C) != 0;   // X360 mask 0xFFFFFFD3 -- clear bits {2,3,5}
            }
            liPauseFlags &= ~0x2E;                     // X360 mask 0xFFFFFFD1 -- clear bits {1,2,3,5}
        }
    }
    return liPauseFlags != 0;
}

// X360 @ 0x823566F8. Hands back the module's per-frame output GUI event queue (the
// CgsModule::VariableEventQueue<18432,16> that the PaybackManager and other managers publish their
// HUD/GUI events onto). May only be called while the module is updating (asserts mbIsUpdating).
CgsModule::VariableEventQueue<18432, 16>* GameStateModule::GetOutputGuiEventQueue()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return &mOutputGuiEventQueue;
}

// X360 @ 0x82363698. Mark car lCarId as already-shown in the unlock sequence. The X360 resolves the
// player Profile through the embedded progression manager (GetProfile inlines to the by-value Profile
// sub-object), asserts it is non-null with the verbatim message, then forwards to
// Profile::SetCarUnlockAlreadyShown.
void GameStateModule::SetCarUnlockAlreadyShown(CgsID lCarId)
{
    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile != nullptr, "mProgressionManager.GetProfile()");
    lpProfile->SetCarUnlockAlreadyShown(lCarId);
}
}
