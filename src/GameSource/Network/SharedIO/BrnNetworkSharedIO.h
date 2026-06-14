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
    enum EPaybackType        : s32 { E_PAYBACK_NONE = 0 };
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
