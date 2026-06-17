#pragma once

#include "BrnCommonTypes.h"

namespace BrnNetwork
{
    // Base of the CgsModule event hierarchy (no data members on the X360 spine).
    struct Event
    {
    };

    // Recovered from BrnNetworkSharedIO.h / CgsNetworkConstants.h (DecFIGS DWARF).
    enum EActiveRaceCarIndex : s32 { E_ACTIVE_RACE_CAR_NONE = -1 };
    // Per-car boost flavour (DWARF BrnNetworkSharedIO.h:14). StartNetworkGameEvent stores
    // this as a raw s32 field; the race-car output interfaces use the enum directly.
    enum EBoostType : s32
    {
        E_BOOST_TYPE_NORMAL     = 0,
        E_BOOST_TYPE_DANGER     = 1,
        E_BOOST_TYPE_AGGRESSION = 2,
        E_BOOST_TYPE_STUNT      = 3,
        E_BOOST_TYPE_INFINITE   = 4,
        E_BOOST_TYPE_COUNT      = 5,
    };
    enum EPaybackType : s32
    {
        E_PAYBACK_NONE                                = 0,  // legacy alias retained for existing consumers
        E_PAYBACK_TYPE_START                          = 0,
        E_PAYBACK_TYPE_REVERSE_STEERING               = 0,
        E_PAYBACK_TYPE_BOOST_LOCK                      = 1,
        E_PAYBACK_TYPE_AGGRESSORS_CONTROLS_AFFECTS_VICTIM = 2,
        E_PAYBACK_TYPE_SIX_AXIS_STEERING              = 3,
        E_PAYBACK_TYPE_COUNT                          = 4,
    };
    enum EDirtyTrickStatus   : s32 { E_DIRTY_TRICK_NONE = 0 };

    namespace Road { typedef s32 ChallengeIndex; }

    typedef s32 NetworkPlayerID;

    // Fixed 20-char player name (CgsNetwork::PlayerName).
    struct PlayerName { char macName[20]; };

    struct RoadRulesMessageData
    {
        s32 maScores[2];
        u64 mu64RoadRulesID;
        Road::ChallengeIndex mChallengeIndex;
    };

    struct RoadRulesRecvData : public Event
    {
        RoadRulesMessageData maRoadRulesData[10];
        PlayerName           mPlayerName;
        NetworkPlayerID      mPlayerID;
        s32                  miNumRoadRulesScoresRecv;
    };

    // No struct body survives in the DecFIGS DWARF (forward-declared only); the
    // X360 queue aligns its inline buffer to 16 and the element spans the same
    // 40-slot capacity as the other road-rules events. Full member recovery is a
    // separate TU; here it only needs to be a complete, correctly-aligned type.
    struct alignas(16) RoadRulesDownloadEvent
    {
        RoadRulesMessageData mMessage;
        u8                   mPad0[8];
    };

    namespace BrnNetworkModuleIO
    {
        struct DirtyTrickEvent
        {
            EActiveRaceCarIndex meAggressorActiveRaceCarIndex;
            EActiveRaceCarIndex meVictimActiveRaceCarIndex;
            EPaybackType        meDirtyTrickType;
            EDirtyTrickStatus   meDirtyTrickStatus;
        };
    }
}
