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

    // Fixed player name. X360-AUTHORITATIVE width is 16 (KI_USERNAME_LENGTH == 0x10) -- every
    // X360 build site copies/strides 16 bytes -- NOT the PS3-DWARF char[20]. Mirrors the committed
    // CgsNetwork::PlayerName (BrnCgsPlayerName.h). 16 is load-bearing for the RoadRules* event
    // strides (RoadRulesDownloadEvent==56, RoadRulesRecvData==264).
    struct PlayerName { char macName[16]; };

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

    // DWARF BrnNetworkSharedIO.h:376 -- RoadRulesDownloadEvent : Event. Two player names
    // (the winner + the previous holder being notified) followed by the road-rules score
    // payload. sizeof == 2*16 + 24 == 56, matching the X360 56-byte queue element stride.
    struct RoadRulesDownloadEvent : public Event
    {
        PlayerName           maPlayerNames[2];   // :378
        RoadRulesMessageData mRoadRulesData;     // :379
        void Construct();                        // :376 (declared-only; body is its own TU)
    };

    namespace BrnNetworkModuleIO
    {
        // FLAG (additive header grow): the Xbox-Live invite/join parameters block, homed here
        // because BrnGameStateInviteManager.cpp embeds one by value (mInviteOrJoinParams) and
        // needs its exact layout. Members + order are DWARF-authoritative
        // (BrnNetworkSharedIO.h:523-527, struct BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams).
        // Offsets are X360-pinned by the InviteManager bodies: miUserControllerPort lands at +0x14
        // (read in UpdatePrepareForInvite case BIND) and mbIsLocalUserChangeNeeded at +0x98 (read
        // in CheckPreparedForInvite). PlayerName is the committed 16-byte X360 width, so the block
        // is 0x9C bytes (padded to s32). This is layout-only (no methods); the BrnNetwork invite
        // TUs that construct/consume it own its behaviour.
        struct InviteOrJoinParams
        {
            PlayerName mPlayerName;               // +0x00 (16B)
            s32        miGameID;                  // +0x10
            s32        miUserControllerPort;      // +0x14
            char       macSessionID[128];         // +0x18 .. +0x98
            bool       mbIsLocalUserChangeNeeded; // +0x98
        };

        struct DirtyTrickEvent
        {
            EActiveRaceCarIndex meAggressorActiveRaceCarIndex;
            EActiveRaceCarIndex meVictimActiveRaceCarIndex;
            EPaybackType        meDirtyTrickType;
            EDirtyTrickStatus   meDirtyTrickStatus;
        };
    }
}
