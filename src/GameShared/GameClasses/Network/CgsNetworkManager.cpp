#include "GameShared/GameClasses/Network/CgsNetworkManager.h"

// CgsNetwork::NetworkManager -- the platform-independent core of the online hub. See the
// header for the layout.
//
// Bodied here: the constructor, OnGameStart, OnGameFinish, OnRoundFinish and the
// StartTimeMessageArrivedLate callback. Construct, Destruct, Prepare, Release, Update,
// PostUpdate, OnEnterGame, OnLeaveGame and OnRoundStart reach the player manager or the
// start-time manager, which are still pinned storage here, so they stay declared-only.

namespace CgsNetwork
{
    // Registered by Construct (the only writer), read by Update.
    s32 NetworkManager::_miPlayerManagerUpdatePerfMon;
    s32 NetworkManager::_miNetworkAdapterUpdatePerfMon;
    s32 NetworkManager::_miHostMigrationManagerUpdatePerfMon;
    s32 NetworkManager::_miStartTimeManagerUpdatePerfMon;
    s32 NetworkManager::_miVOIPManagerUpdatePerfMon;

    // Only the sub-objects' own constructors run here (vtables, embedded messages, the
    // network clocks); every manager field is seeded later by Construct.
    NetworkManager::NetworkManager()
    {
    }

    // Empty on the console: the base keeps no per-game state to start.
    void NetworkManager::OnGameStart(CgsSystem::Time lStartTime, u16 lu16CurrentFrame)
    {
    }

    // The game is over: forget the start frame so frame-since-start queries go invalid.
    // The time step is not read on the invalid path.
    void NetworkManager::OnGameFinish(CgsSystem::Time lFinishTime, u16 lu16CurrentFrame)
    {
        mTimeManager.SetStartFrame(TimeManager::E_START_FRAME_INVALID, nullptr, 0.0f);
    }

    // The round is over: forget the start frame, as at the end of a game.
    void NetworkManager::OnRoundFinish(CgsSystem::Time lFinishTime, u16 lu16CurrentFrame)
    {
        mTimeManager.SetStartFrame(TimeManager::E_START_FRAME_INVALID, nullptr, 0.0f);
    }

    // Registered with the start-time manager; the base class does nothing when the start
    // message arrives late.
    void NetworkManager::StartTimeMessageArrivedLate()
    {
    }
}
