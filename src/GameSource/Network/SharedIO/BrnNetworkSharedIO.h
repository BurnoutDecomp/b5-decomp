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

    // ------------------------------------------------------------------------------------
    // ETelemetryHook -- DWARF BrnNetworkSharedIO.h:207, values verbatim (0..50). The hook id a
    // TelemetryData record carries (meHook). Producer-pinned immediates: GameStateModule::
    // ProcessTakedownEvents @0x8238FC50 selects 13 / 11 / 10 / 14 (`li r11, 0xD` @0x8238FF58,
    // `0xB` @0x8238FF14, `0xA` @0x8238FF0C, `0xE` @0x8238FF9C) == TAKEDOWN_TBONE / TAKEDOWN_VERTICAL
    // / TAKEDOWN / TAKEDOWN_MARKED_MAN below. [takedown P1 wave 2026-09-03, additive.]
    // ------------------------------------------------------------------------------------
    enum ETelemetryHook : s32
    {
        E_TELEMETRY_UNKNOWN                        = 0,
        E_TELEMETRY_EVENT_STARTED                  = 1,
        E_TELEMETRY_EVENT_STARTED_ONLINE           = 2,
        E_TELEMETRY_EVENT_FINISHED                 = 3,
        E_TELEMETRY_EVENT_QUIT                     = 4,
        E_TELEMETRY_EVENT_DISCONNECT               = 5,
        E_TELEMETRY_CRASHED_INTO_WORLD             = 6,
        E_TELEMETRY_CRASHED_INTO_TRAFFIC           = 7,
        E_TELEMETRY_CRASHED_INTO_RACE_CAR          = 8,
        E_TELEMETRY_CRASHED_STARTED_SHOWTIME       = 9,
        E_TELEMETRY_TAKEDOWN                       = 10,
        E_TELEMETRY_TAKEDOWN_VERTICAL              = 11,
        E_TELEMETRY_TAKEDOWN_AFTERTOUCH            = 12,
        E_TELEMETRY_TAKEDOWN_TBONE                 = 13,
        E_TELEMETRY_TAKEDOWN_MARKED_MAN            = 14,
        E_TELEMETRY_RIVAL_ADDED                    = 15,
        E_TELEMETRY_RIVAL_REMOVED                  = 16,
        E_TELEMETRY_MUGSHOT_GAMERPIC               = 17,
        E_TELEMETRY_MUGSHOT_PHOTO                  = 18,
        E_TELEMETRY_ROAD_RULES_NEW_PB_TIME         = 19,
        E_TELEMETRY_ROAD_RULES_NEW_PB_CRASH        = 20,
        E_TELEMETRY_EVENT_CAR_MODEL                = 21,
        E_TELEMETRY_EVENT_EARNED_LICENCE           = 22,
        E_TELEMETRY_NETWORK_CONNECT                = 23,
        E_TELEMETRY_NETWORK_GAME_CREATED           = 24,
        E_TELEMETRY_NETWORK_GAME_JOINED            = 25,
        E_TELEMETRY_NETWORK_GAME_STARTED           = 26,
        E_TELEMETRY_NETWORK_CHALLENGE_STARTED      = 27,
        E_TELEMETRY_NETWORK_CHALLENGE_FINISHED     = 28,
        E_TELEMETRY_NETWORK_CHALLENGE_CANCELLED    = 29,
        E_TELEMETRY_NETWORK_HOST_PLAYERS_IN_GAME   = 30,
        E_TELEMETRY_NETWORK_HOST_TRACKID           = 31,
        E_TELEMETRY_NETWORK_HOST_NUMBER_OF_ROUNDS  = 32,
        E_TELEMETRY_NETWORK_HOST_IS_RANKED         = 33,
        E_TELEMETRY_GAME_FINISH_TIME               = 34,
        E_TELEMETRY_GAME_FINISH_TIMEOUT            = 35,
        E_TELEMETRY_DRIVE_THRU_PAINT_SHOP          = 36,
        E_TELEMETRY_DRIVE_THRU_BODY_SHOP           = 37,
        E_TELEMETRY_DRIVE_THRU_JUNK_YARD           = 38,
        E_TELEMETRY_DRIVE_THRU_CAR_WASH            = 39,
        E_TELEMETRY_DRIVE_THRU_AUTO_PARTS          = 40,
        E_TELEMETRY_DRIVE_THRU_GAS_STATION         = 41,
        E_TELEMETRY_NEWS_READ                      = 42,
        E_TELEMETRY_LEADERBOARD_VIEWED             = 43,
        E_TELEMETRY_CUSTOM_ROUTE_CREATED           = 44,
        E_TELEMETRY_EASY_DRIVE_OPENED              = 45,
        E_TELEMETRY_NETWORK_GAME_LEFT              = 46,
        E_TELEMETRY_ACHIEVEMENT_EARNT              = 47,
        E_TELEMETRY_DIRTYSOCK_UPNP                 = 48,
        E_TELEMETRY_DIRTYSOCK_CONNECTION           = 49,
        E_TELEMETRY_HOOKS_COUNT                    = 50,
    };

    namespace BrnNetworkModuleIO
    {
        // ------------------------------------------------------------------------
        // NetworkEvent<N> -- the BrnNetwork module-IO event spine.
        //   DWARF: struct NetworkEvent<N> : public BrnNetwork::Event { int32_t GetEventType() const; }
        //   (references/DecFIGS/dwarfdump/.../BrnNetworkEvent.h:43). It carries NO data
        //   members -- the empty Event base + a single GetEventType() that returns the
        //   compile-time event-type tag N -- so every leaf event's first member lands at
        //   offset 0 (confirmed by NetworkPlayerDisconnectedEvent::Construct @0x82581088,
        //   whose first store is mNetworkPlayerID at +0x00). Modelled generic-first so any
        //   NetworkEvent<N> leaf can reuse the base by name.
        template <s32 N>
        struct NetworkEvent : public Event
        {
            static const s32 KI_EVENT_TYPE = N;
            s32 GetEventType() const { return N; }
        };

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

        // ------------------------------------------------------------------------------------
        // TelemetryData -- DWARF BrnNetworkSharedIO.h:540. The 20-byte record every
        // E_ACTION_SEND_TELEMETRY (228) post carries: GameStateModule::ProcessTakedownEvents
        // @0x8238FF7C..0x8238FF8C posts it as `li r5, 0xE4 / li r6, 0x14` (20 bytes).
        //   +0x00  ETelemetryHook meHook       (DWARF :542)  `stw hook, var_D0`  @0x8238FF5C
        //   +0x04  char           macBuffer[16] (DWARF :543)  `stb 0, var_CC`     @0x8238FEF8
        // Parameters are dot-separated inside macBuffer (AddParameter(const char*) @0x82354010
        // StrCat's "." then the parameter; CgsNetwork::KI_MAX_TELEMETRY_DATA_SIZE == 16 is the
        // cap it asserts). Bodies: BrnNetworkSharedIO_Telemetry.cpp.
        // Only the members with a reconstructed body are declared: the DWARF's remaining
        // overloads (:558 int32 / :562 uint32 / :566 float / :570 ClearParameter / :580 CgsID /
        // :585 Time) have no body in the tree yet; add each with its body, not before.
        // [takedown P1 wave 2026-09-03, additive.]
        // ------------------------------------------------------------------------------------
        struct TelemetryData
        {
            void Construct(ETelemetryHook leHook);      // DWARF :548 -- inlined at every X360 site: {hook, ""}
            void AddParameter(const char* lpcParam);     // DWARF :553 -- X360 0x82354010
            void AddParameter(Vector3 lVector);          // DWARF :575 -- X360 sub_8236A8B8 ("%i.%i" of X and Z)

            ETelemetryHook meHook;                       // +0x00
            char           macBuffer[16];                // +0x04
        };
        static_assert(sizeof(TelemetryData) == 20, "X360 posts TelemetryData as 20 bytes (li r6, 0x14 @0x8238FF7C)");
    }
}
