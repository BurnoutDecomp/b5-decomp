#pragma once

// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO network IN-event leaves -- owning header
//   b5-decomp/src/GameSource/Network/BrnNetworkInEventTypeDefs.h
//
// Each leaf is a NetworkEvent<N> (the empty Event spine + the compile-time event-type tag
// N it is queued under). N is the console tag the producers pass to
// VariableEventQueue<14000,16>::AddEvent, which drifts from the reference numbering for the
// later leaves (the select-scoreboard event is queued as 49, the reference says 47).
// A leaf whose tag is not yet attested is modelled on the empty Event base directly; both
// bases are byte-identical (no data members, first field at +0x00).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (builder >= 0 guards)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"      // BrnNetwork::Event, NetworkEvent<N>, TelemetryData
#include "GameSource/Network/Shared Server Types/BrnNetworkSharedServerTypes.h" // ServerGeneratedTypes::OfflineProgressionT
#include "pc/gcm/renderengine/pixelformat.h"                     // renderengine::PixelFormat (NetworkInReqCamPicEvent)
#include "GameShared/GameClasses/Containers/CgsArray.h"          // Array<T,N> (NetworkInNonUploadedScoresEvent)
#include "GameSource/Network/BrnNetworkModuleIO.h"               // EChallengeEventType (NetworkInFreeburnChallengeEvent)
#include "GameSource/GameState/BrnGameStateSharedIO.h"           // EMugshotResponse, EImageType, CompletedFburnChallenges, LastSecondChallengeSuccess
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h" // BrnGameState::BurnoutSkillzData
#include "SharedClasses/StreetData/BrnChallengeData.h"           // BrnStreetData::ChallengePlayerScoreEntry
#include "SharedClasses/World/BrnWorldRegion.h"                  // BrnWorld::WorldRegion
#include "GameSource/GameState/BrnCgsPlayerName.h"               // CgsNetwork::PlayerName (16 bytes)

#include <cstddef>   // offsetof (layout pins)

namespace CgsNetwork
{
    class NetworkTexture;   // GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h (held by pointer only)
}

namespace BrnGameState { struct StreetManager; }             // NetworkInRoadRulesDataEvent (held by pointer only)
namespace BrnNetwork { struct LocalEventScoreUploadData; }  // NetworkInNonUploadedScoresEvent (held by pointer only)
namespace BrnNetwork { struct LiveRevengeProfile; }         // NetworkInLiveRevengeProfileLoaded (held by pointer only)

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // The camera-feed user setting (options screen, saved in the GUI options profile at
    // +0x7364, carried to the network side by NetworkInSettingsUpdateEvent).
    enum ECameraUserOptions
    {
        CAMERA_USER_OFF          = 0,
        CAMERA_USER_ON           = 1,
        CAMERA_USER_FRIENDS_ONLY = 2,
    };

    // The IN-event that delivers a player's accumulated offline-play progress to the network
    // player-stats manager (tag 33). NetworkPlayerStatsManager::HandleOfflineProgressionEvent
    // copies exactly 0x44 bytes of it and reads the word at +0x40 as the freeburn-challenge
    // success count (its count argument to ServerInterfaceCustomCommands::UploadOfflineProgress):
    // the 64-byte OfflineProgressionT plus that trailing s32. The trailing field is missing from
    // the reference member list; FLAGGED as recovered from the consumer.
    struct NetworkInOfflineProgression : public NetworkEvent<33>
    {
        ServerGeneratedTypes::OfflineProgressionT mOfflineProgression; // +0x00 (64 bytes)
        s32                                       miFreeburnChallengeSuccessCount; // +0x40 (asm-recovered)
    };

    // The select-scoreboard IN-event (reference home line 668). The GUI bridge and the
    // scoreboard debug component build it on the stack: Prepare() (inlined: the three headings
    // to KI_INVALID_HEADING, meType to E_TYPE_NONE), then ONE selector, then AddEvent. The
    // debug component skips Prepare, so its unset headings are stack garbage exactly as on the
    // console. ScoreboardManager::ProcessEventQueue pops it and reads it back through the
    // GetChosen* accessors (their asserts sit at the reference header's lines 1109/1124/1139).
    //
    //   +0x00 miCategory   GetIndexes     (asserts >= 0; reference line 1031)
    //   +0x04 miIndex      GetVariations  (asserts >= 0; reference line 1048)
    //   +0x08 miVariation  GetScoreboard  (asserts >= 0; reference line 1065)
    //   +0x0C meType       every selector
    //
    // GetIndexes / GetVariations / GetScoreboard are out of line on the console (their
    // asserts keep them from inlining); the rest inline at every call site.
    struct NetworkInSelectScoreboardEvent : public NetworkEvent<49>
    {
        enum EScoreboardEventType
        {
            E_TYPE_NONE           = 0,
            E_TYPE_GET_CATEGORY   = 1,
            E_TYPE_GET_INDEX      = 2,
            E_TYPE_GET_VARIATION  = 3,
            E_TYPE_GET_SCOREBOARD = 4,
            E_TYPE_PAGE_UP        = 5,
            E_TYPE_PAGE_DOWN      = 6,
            E_TYPE_COUNT          = 7,
        };

        static const s32 KI_INVALID_HEADING = -1;

        void Prepare()
        {
            miCategory  = KI_INVALID_HEADING;
            miIndex     = KI_INVALID_HEADING;
            miVariation = KI_INVALID_HEADING;
            meType      = E_TYPE_NONE;
        }

        void GetCategories() { meType = E_TYPE_GET_CATEGORY; }
        void GetIndexes(s32 liCategory);
        void GetVariations(s32 liIndex);
        void GetScoreboard(s32 liVariation);
        void PageUp()        { meType = E_TYPE_PAGE_UP; }
        void PageDown()      { meType = E_TYPE_PAGE_DOWN; }

        s32 GetChosenCategory() const
        {
            CGS_ASSERT(miCategory != KI_INVALID_HEADING, "miCategory != KI_INVALID_HEADING");
            return miCategory;
        }
        s32 GetChosenIndex() const
        {
            CGS_ASSERT(miIndex != KI_INVALID_HEADING, "miIndex != KI_INVALID_HEADING");
            return miIndex;
        }
        s32 GetChosenVariation() const
        {
            CGS_ASSERT(miVariation != KI_INVALID_HEADING, "miVariation != KI_INVALID_HEADING");
            return miVariation;
        }
        EScoreboardEventType GetScoreboardEventType() { return meType; }

    private:
        s32                  miCategory;    // +0x00
        s32                  miIndex;       // +0x04
        s32                  miVariation;   // +0x08
        EScoreboardEventType meType;        // +0x0C

        static void _AssertLayout();
    };

    // The telemetry IN-event (reference home line 273, tag 16 on both builds): one
    // TelemetryData record, queued with its full 20-byte size.
    struct NetworkInTelemetryEvent : public NetworkEvent<16>
    {
        TelemetryData mEventData;   // +0x00
    };
    static_assert(sizeof(NetworkInTelemetryEvent) == 20, "NetworkInTelemetryEvent is queued as 20 bytes");

    // The compressed camera-picture request (reference home line 776). Queued as tag 53 with
    // the console size 12; the texture pointer makes the host record wider, so producers queue
    // sizeof().
    struct NetworkInReqCamPicEvent : public NetworkEvent<53>
    {
        s32                         miQualitySetting;            // +0x00
        renderengine::PixelFormat   meCompressedFormat;          // +0x04
        CgsNetwork::NetworkTexture* mpTextureToCompressedInto;   // +0x08 (4 bytes on the console)
    };

    // ====================================================================================
    // [network wave N1] the IN-events BrnGameModule::BridgeGameStateToNetwork queues. Tags
    // are the console's (the bridge's AddEvent immediates), sizes the console's AddEvent sizes;
    // three records carry a pointer and are wider on the host, so producers queue sizeof().
    // The later tags drift from the reference numbering: the console inserted 45 and 46 ahead
    // of the gamer-card event (reference 45 -> 47, 46 -> 48) and three more after the
    // local-player-crashes event (reference 51 -> 54; 55, 56, 57 are console-only).
    // ====================================================================================

    struct NetworkInUpdateRichPresence : public NetworkEvent<15>
    {
        char macRichPresenceString[100];                          // +0x00
    };
    static_assert(sizeof(NetworkInUpdateRichPresence) == 100, "queued as 100 bytes");

    // Console 16 bytes; the street-manager pointer is 8 bytes on the host.
    struct NetworkInRoadRulesDataEvent : public NetworkEvent<18>
    {
        void Construct(u64 lu64RoadRulesID, u32 luTimeStamp, BrnGameState::StreetManager* lpStreetManager)
        {
            mu64RoadRulesID                    = lu64RoadRulesID;
            muTimeStampOfLastRoadRulesDownload = luTimeStamp;
            mpStreetManager                    = lpStreetManager;
        }
        u32 GetTimeStampOfLastRoadRulesDownload() const { return muTimeStampOfLastRoadRulesDownload; }
        u64 GetRoadRulesID() const                      { return mu64RoadRulesID; }

    private:
        u64                          mu64RoadRulesID;                     // +0x00
        u32                          muTimeStampOfLastRoadRulesDownload;  // +0x08
        BrnGameState::StreetManager* mpStreetManager;                     // +0x0C (console 4 bytes)
    };

    struct NetworkInRoadRulesPBEvent : public NetworkEvent<19>
    {
        BrnStreetData::ChallengePlayerScoreEntry mPersonalBestScore;      // +0x00 (40)
        Road::ChallengeIndex                     mChallengeIndex;         // +0x28
        bool                                     mbLobbyPersonalBest;     // +0x2C
    };
    static_assert(sizeof(NetworkInRoadRulesPBEvent) == 48, "queued as 48 bytes");

    // Empty signal, queued as one byte.
    struct NetworkInRoadRulesOverwriteServerRecord : public NetworkEvent<20> {};

    struct NetworkInPaybackMugshotEvent : public NetworkEvent<29>
    {
        EActiveRaceCarIndex                               meTakedownAggressorIndex;          // +0x00
        EActiveRaceCarIndex                               meTakedownVictimIndex;             // +0x04
        BrnGameState::GameStateModuleIO::EMugshotResponse meMugshotResponse;                 // +0x08
        BrnGameState::GameStateModuleIO::EImageType       meMugshotType;                     // +0x0C
        CgsID                                             mRoadRuleBeatenRoadID;             // +0x10
        bool                                              mbIsTakedownAggressorLocalPlayer;  // +0x18
        bool                                              mbMugshotRequiresBroadcast;        // +0x19
    };
    static_assert(sizeof(NetworkInPaybackMugshotEvent) == 32, "queued as 32 bytes");

    // Empty signal, queued as one byte.
    struct NetworkInAbortMugshotCaptureEvent : public NetworkEvent<30> {};

    struct NetworkInPaybackIntialised : public NetworkEvent<31>
    {
        EActiveRaceCarIndex mePaybackAggressorIndex;              // +0x00
        EActiveRaceCarIndex mePaybackVictimIndex;                 // +0x04
    };

    struct NetworkInPaybackSucceeded : public NetworkEvent<32>
    {
        EActiveRaceCarIndex mePaybackAggressorIndex;              // +0x00
        EActiveRaceCarIndex mePaybackVictimIndex;                 // +0x04
    };

    struct NetworkInSwitchBurningHomeRunRunner : public NetworkEvent<34>
    {
        NetworkPlayerID mNewRunnerID;                             // +0x00
    };

    struct NetworkInBurnoutSkillzEvent : public NetworkEvent<35>
    {
        enum EEventType
        {
            E_EVENT_TYPE_SEND_TO_ALL_PLAYERS       = 0,
            E_EVENT_TYPE_SEND_TO_A_SPECIFIC_PLAYER = 1,
            E_EVENT_TYPE_COUNT                     = 2,
        };

        BrnGameState::BurnoutSkillzData mNewSkillzData;           // +0x00 (56)
        NetworkPlayerID                 mPlayerID;                // +0x38
        EEventType                      meEventType;              // +0x3C
    };
    static_assert(sizeof(NetworkInBurnoutSkillzEvent) == 64, "queued as 64 bytes");

    struct NetworkInShowtimeUpdateEvent : public NetworkEvent<36>
    {
        s32             miShowtimeScore;                          // +0x00
        NetworkPlayerID mPlayerID;                                // +0x04
    };
    static_assert(sizeof(NetworkInShowtimeUpdateEvent) == 8, "queued as 8 bytes");

    struct NetworkInShowtimeSwitchEvent : public NetworkEvent<37>
    {
        s32             miFinalShowtimeScore;                     // +0x00
        NetworkPlayerID mPlayerID;                                // +0x04
        bool            mbEnteringShowtime;                       // +0x08
    };
    static_assert(sizeof(NetworkInShowtimeSwitchEvent) == 12, "queued as 12 bytes");

    struct NetworkInFreeburnChallengeEvent : public NetworkEvent<38>
    {
        NetworkPlayerID                mPlayerID;                        // +0x00
        CgsID                          mChallengeID;                     // +0x08
        EChallengeEventType            meEventType;                      // +0x10
        BrnGameState::EChallengeStatus meChallengeStatus;                // +0x14
        s32                            miActionIndex;                    // +0x18
        s32                            miNumberOfCompletedChallenges;    // +0x1C
    };
    static_assert(sizeof(NetworkInFreeburnChallengeEvent) == 32, "queued as 32 bytes");

    struct NetworkInFburnChallengeStatusEvent : public NetworkEvent<39>
    {
        BrnGameState::GameStateModuleIO::CompletedFburnChallenges mCompletedChallenges;  // +0x000 (256)
        NetworkPlayerID                                           mPlayerID;             // +0x100
    };
    static_assert(sizeof(NetworkInFburnChallengeStatusEvent) == 0x108, "queued as 0x108 bytes");

    struct NetworkInFburnSuccessUpdateEvent : public NetworkEvent<40>
    {
        BrnGameState::GameStateModuleIO::LastSecondChallengeSuccess mChallengeSuccessUpdate;  // +0x00 (8)
        s32                                                          miActionIndex;           // +0x08
    };
    static_assert(sizeof(NetworkInFburnSuccessUpdateEvent) == 16, "queued as 16 bytes");

    struct NetworkInFburnChallengeSuccessEvent : public NetworkEvent<41>
    {
        f32  mafActionScores[2];                                  // +0x00
        bool mabSuccessfulActions[2];                             // +0x08
        bool mabAccumulationThisFrame[2];                         // +0x0A
    };
    static_assert(sizeof(NetworkInFburnChallengeSuccessEvent) == 12, "queued as 12 bytes");

    struct NetworkInActiveFburnChallengeEvent : public NetworkEvent<42>
    {
        EActiveRaceCarIndex maePlayersInChallengeARCI[7];         // +0x00
        CgsID               mChallengeID;                         // +0x20
        NetworkPlayerID     mPlayerToSendToID;                    // +0x28
        s32                 miNumPlayersInChallenge;              // +0x2C
    };
    static_assert(sizeof(NetworkInActiveFburnChallengeEvent) == 48, "queued as 48 bytes");

    struct NetworkInChangeDistrictEvent : public NetworkEvent<43>
    {
        BrnWorld::WorldRegion mNewWorldRegion;                    // +0x00 (8)
    };

    struct NetworkInLocalPlayerReachesCheckpoint : public NetworkEvent<44>
    {
        s32 miCheckpointIndex;                                    // +0x00
    };

    // FLAG name (console-only tag): StateManager::ProcessNetworkEvents hands the word to
    // BrnNetworkPlayer::SendStuntScoreUpdatedMessage for every remote player.
    struct NetworkInStuntScoreUpdatedEvent : public NetworkEvent<45>
    {
        s32 miStuntScore;                                         // +0x00
    };

    // FLAG name (console-only tag): StateManager::ProcessNetworkEvents hands the 8-byte record
    // whole to BrnNetworkPlayer::SendStuntMultiplierMessage for every remote player.
    struct NetworkInStuntMultiplierEvent : public NetworkEvent<46>
    {
        u32 muStuntTypes;                                         // +0x00
        u16 mu16FlatSpins;                                        // +0x04
        u16 mu16BarrelRolls;                                      // +0x06
    };
    static_assert(sizeof(NetworkInStuntMultiplierEvent) == 8, "queued as 8 bytes");

    struct NetworkInShowGamerCard : public NetworkEvent<47>
    {
        PlayerName mPlayerName;                                   // +0x00 (16)
    };

    // Console 20 bytes; the texture pointer is 8 bytes on the host.
    struct NetworkInDxtDecodeImageEvent : public NetworkEvent<48>
    {
        CgsNetwork::NetworkTexture* mpTextureToDecode;            // +0x00 (console 4 bytes)
        PlayerName                  mPlayerName;                  // +0x04 on the console
    };

    struct NetworkInLocalPlayerCrashesEvent : public NetworkEvent<54>
    {
        EActiveRaceCarIndex meCrasherRaceCarIndex;                // +0x00
    };

    // FLAG name (console-only tag): StateManager::ProcessNetworkEvents passes the 64-bit id to
    // the gamer-card manager's ShowGamerCardForXuid.
    struct NetworkInShowGamerCardForXuidEvent : public NetworkEvent<55>
    {
        u64 mu64Xuid;                                             // +0x00
    };

    // FLAG name (console-only tag): EventScoresManager::ProcessNetworkEvents hands {+0x00 id,
    // +0x0C score, +0x08 game mode} to StoreEventScoreForUpload.
    struct NetworkInScoreLeaderboardEvent : public NetworkEvent<56>
    {
        CgsID mEventID;                                           // +0x00
        s32   meGameModeType;                                     // +0x08 (GameStateModuleIO::EGameModeType)
        s32   miScore;                                            // +0x0C
    };
    static_assert(sizeof(NetworkInScoreLeaderboardEvent) == 16, "queued as 16 bytes");

    // FLAG name (console-only tag): EventScoresManager::ProcessNetworkEvents appends the pointed-to
    // array onto its pending uploads. Console 4 bytes; the pointer is 8 bytes on the host.
    struct NetworkInNonUploadedScoresEvent : public NetworkEvent<57>
    {
        const Array<LocalEventScoreUploadData, 49>* mpNonUploadedScores;  // +0x00
    };

    // ====================================================================================
    // The IN-events BrnGameModule::TranslateGuiEventsToNetworkEvents queues (tags and sizes
    // are the translator's AddEvent immediates) and StateManager::ProcessNetworkEvents drains.
    // ====================================================================================

    // The Guide's friends UI closed. Empty signal, queued as one byte.
    struct NetworkInFriendsUtilShut : public NetworkEvent<6> {};

    // Invite a buddy (an empty name: sign in and create a game first). 16 bytes.
    struct NetworkInSendInvite : public NetworkEvent<8>
    {
        CgsNetwork::PlayerName mBuddyToSendTo;                    // +0x00
    };

    // Revoke an invite. 17 bytes.
    struct NetworkInRevokeInvite : public NetworkEvent<9>
    {
        CgsNetwork::PlayerName mBuddyToRevokeInviteTo;            // +0x00
        bool                   mbHasOnlineGameBeenStarted;        // +0x10
    };

    // Empty signals, queued as one byte.
    typedef NetworkEvent<14> NetworkInInstantFreeburn;

    // The voice-chat volume option changed (the option screen's 0..10 step). 4 bytes.
    struct NetworkInVoipEvent : public NetworkEvent<17>
    {
        s32 miVoipVolume;                                         // +0x00
    };

    // The camera-feed option changed. 4 bytes.
    struct NetworkInSettingsUpdateEvent : public NetworkEvent<23>
    {
        ECameraUserOptions meCameraFeedSetting;                   // +0x00
    };

    // The live-revenge profile finished loading. Console 4 bytes; the pointer is 8 bytes on
    // the host.
    struct NetworkInLiveRevengeProfileLoaded : public NetworkEvent<24>
    {
        LiveRevengeProfile* mpLiveRevengeProfile;                 // +0x00
    };

    // Empty signals, queued as one byte.
    typedef NetworkEvent<25> NetworkInLoadingScreenShown;
    typedef NetworkEvent<26> NetworkInLeftPostEvent;
    typedef NetworkEvent<27> NetworkInCancelLogin;
    struct NetworkInLeavingJunkyard : public NetworkEvent<28> {};

    // FLAG name (console-only tag): the scoreboard's "challenge this score" request, handed to
    // ScoreboardManager::HandleEvScoreTargetEvent. 36 bytes. The translator copies the GUI
    // request's name to +0x00, its four words to +0x10..+0x1C and its flag to +0x20.
    struct NetworkInScoreTargetEvent : public NetworkEvent<50>
    {
        CgsNetwork::PlayerName mPlayerName;                       // +0x00
        s32                    miScore;                           // +0x10
        s32                    miCategory;                        // +0x14
        s32                    miIndex;                           // +0x18
        s32                    miVariation;                       // +0x1C
        bool                   mbIsCurrentTarget;                 // +0x20
    };

    // Empty signal, queued as one byte.
    typedef NetworkEvent<51> NetworkInRequestAccountSettings;

    // The account's data-sharing settings were edited. 3 bytes.
    struct NetworkInAccountUpdate : public NetworkEvent<52>
    {
        bool mbAgreeToShareInfoEA;                                // +0x00
        bool mbAgreeToShareInfoPartners;                          // +0x01
        bool mbTelemetryEnable;                                   // +0x02
    };

    // Layout pins for the pointer-free records above (console offsets and AddEvent sizes).
    // Never called.
    inline void AssertNetworkInGuiEventLayouts()
    {
        static_assert(sizeof(NetworkInFriendsUtilShut) == 1, "tag 6 size");
        static_assert(sizeof(NetworkInSendInvite) == 16, "tag 8 size");
        static_assert(offsetof(NetworkInRevokeInvite, mbHasOnlineGameBeenStarted) == 0x10, "tag 9 +0x10");
        static_assert(sizeof(NetworkInRevokeInvite) == 17, "tag 9 size");
        static_assert(sizeof(NetworkInInstantFreeburn) == 1, "tag 14 size");
        static_assert(sizeof(NetworkInVoipEvent) == 4, "tag 17 size");
        static_assert(sizeof(NetworkInSettingsUpdateEvent) == 4, "tag 23 size");
        static_assert(sizeof(NetworkInLoadingScreenShown) == 1, "tag 25 size");
        static_assert(sizeof(NetworkInLeftPostEvent) == 1, "tag 26 size");
        static_assert(sizeof(NetworkInCancelLogin) == 1, "tag 27 size");
        static_assert(sizeof(NetworkInLeavingJunkyard) == 1, "tag 28 size");
        static_assert(offsetof(NetworkInScoreTargetEvent, miScore) == 0x10, "tag 50 +0x10");
        static_assert(offsetof(NetworkInScoreTargetEvent, miVariation) == 0x1C, "tag 50 +0x1C");
        static_assert(offsetof(NetworkInScoreTargetEvent, mbIsCurrentTarget) == 0x20, "tag 50 +0x20");
        static_assert(sizeof(NetworkInScoreTargetEvent) == 36, "tag 50 size");
        static_assert(sizeof(NetworkInRequestAccountSettings) == 1, "tag 51 size");
        static_assert(offsetof(NetworkInAccountUpdate, mbTelemetryEnable) == 0x02, "tag 52 +0x02");
        static_assert(sizeof(NetworkInAccountUpdate) == 3, "tag 52 size");
    }
}
} // namespace BrnNetwork
