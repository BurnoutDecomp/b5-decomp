#pragma once

// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO network OUT-event leaves -- owning header
//   b5-decomp/src/GameSource/Network/BrnNetworkOutEventTypeDefs.h
//
// The records the network module posts onto its OutputBuffer's NetworkEventQueue
// (VariableEventQueue<14000,16>) for the game to read: the three translators in
// GameBridgeNetworkToX.cpp (to game-state events, to GUI events, to the world input) drain
// them. Each leaf is a NetworkEvent<N> (the empty Event spine + the compile-time tag N), and
// N is the CONSOLE tag the producers pass to AddEvent. The console numbering drifts from the
// reference numbering as the console build inserted leaves the reference lacks
// (tags 21, 34, 47, 53, 54, 73, 74 here):
//
//   console  1.. 20  == reference  1.. 20
//   console 22.. 33  == reference 21.. 32
//   console 35.. 46  == reference 33.. 44
//   console 48.. 52  == reference 45.. 49
//   console 55.. 72  == reference 50.. 67
//
// Every member offset below is the one the translators (and the producers' AddEvent stack
// images) read; the byte size in each banner is the producer's AddEvent size. Records that
// hold a pointer (29, 58, 69, 72) are larger on the host than the console size; their consumers
// copy by member, never by the console byte count.
//
// Six OUT records already have homes of their own and are pulled in from there, not
// redefined:
//   BrnNetworkOutPlayerEvents.h           NetworkOutPlayerAddedEvent (16),
//                                         NetworkOutPlayerRemovedEvent (17),
//                                         NetworkOutPlayerFinalisedEvent (20),
//                                         NetworkPlayerDisconnectedEvent (23)
//   BrnNetworkOutGameParamsChanged.h      NetworkOutGameParamsChanged (15)
//   BrnNetworkOutScoreboardHeadingList.h  NetworkOutScoreboardHeadingList (52)
// and NetworkOutRecvRoadRulesPBEvent (35) lives in BrnNetworkModuleIO.h.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // CgsID
#include "GameSource/BurnoutConstants.h"                                 // ::EActiveRaceCarIndex, BrnGameState::EChallengeStatus
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"              // NetworkEvent<N>, NetworkPlayerID, InviteOrJoinParams, Road::ChallengeIndex
#include "GameSource/Network/SharedIO/BrnBuddyInformation.h"             // BuddyInformation (132-byte record)
#include "GameSource/Network/SharedIO/BrnNetworkOutPlayerEvents.h"       // tags 16 / 17 / 20 / 23
#include "GameSource/Network/SharedIO/BrnNetworkOutGameParamsChanged.h"  // tag 15
#include "GameSource/Network/SharedIO/BrnNetworkOutScoreboardHeadingList.h" // tag 52
#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"            // BrnNetwork::Scoreboard (2924 bytes)
#include "GameSource/GameState/BrnCgsPlayerName.h"                       // CgsNetwork::PlayerName (16 bytes)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // EGameModeType / EPlayerTeam / EImageType / LastSecondChallengeSuccess
#include "GameSource/GameState/BrnGameStateTypes.h"                      // BrnGameState::StuntElementType
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h" // BrnGameState::BurnoutSkillzData (56 bytes)
#include "GameShared/GameClasses/Network/Players/X360/CgsUniquePlayerIDX360.h" // CgsNetwork::UniquePlayerIDX360 (24 bytes)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                 // CgsSystem::Time
#include "GameShared/GameClasses/Containers/CgsArray.h"                  // Array<T,N>
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (the changed-car id setters)
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"          // CgsNetwork::K_INVALID_PLAYER_ID
#include "pc/gcm/renderengine/pixelformat.h"                             // renderengine::PixelFormat

#include <cstddef>   // offsetof (layout pins)

namespace CgsNetwork
{
    class NetworkTexture;           // Network/Texture/CgsNetworkTexture.h (held by pointer only)
}

namespace BrnNetwork
{
    struct LiveRevengeProfile;      // Managers/BrnNetworkLiveRevengeManager.h (held by pointer only)

namespace BrnNetworkModuleIO
{
    // ---------------------------------------------------------------------------------
    // 1 -- the number of buddies on the friends list. 4 bytes.
    struct NetworkOutBuddyCount : public NetworkEvent<1>
    {
        s32 miBuddyCount;                                   // +0x00
    };

    // 2 -- one batch of up to five buddy records. 672 bytes.
    struct NetworkOutBuddyInformation : public NetworkEvent<2>
    {
        static const s32 KI_MAX_BUDDIES_TO_GET = 5;

        BuddyInformation mBuddyInformation[KI_MAX_BUDDIES_TO_GET]; // +0x000 (5 x 132)
        s32              miNumberOfBuddiesInEvent;          // +0x294
        s32              miIndexOfFirstBuddyInFullBuddyList; // +0x298
        bool             mbConnected;                       // +0x29C
    };

    // 6 -- an Xbox Live invite was accepted; the game starts the invite flow. 156 bytes.
    struct NetworkOutInviteRequest : public NetworkEvent<6>
    {
        InviteOrJoinParams mInviteParams;                   // +0x00 (0x9C)
    };

    // 8 -- the invite could not be followed. 4 bytes. meFailReason is the reference's
    // EInviteFailReason, which has no home in the tree yet.
    struct NetworkOutInviteFailed : public NetworkEvent<8>
    {
        s32 meFailReason;                                   // +0x00 (EInviteFailReason)
    };

    // 9 -- one buddy (or several) came online. 24 bytes. The console order puts the count
    // before the name (the reference lists the name second).
    struct NetworkOutBuddyNotification : public NetworkEvent<9>
    {
        s32                    meBuddyNotification;         // +0x00 (EBuddyNotification)
        s32                    miNumBuddies;                // +0x04
        CgsNetwork::PlayerName mBuddyName;                  // +0x08
    };

    struct NetworkOutBuddyListChanged : public NetworkEvent<10> {};   // 1 byte
    struct NetworkOutInviteSent       : public NetworkEvent<11> {};   // 1 byte

    // 12 -- the local player entered or left an instant freeburn. 1 byte.
    struct NetworkOutInstantFreeburnEvent : public NetworkEvent<12>
    {
        bool mbIsDoingInstantFreeburn;                      // +0x00
    };

    // 13 -- the instant freeburn finished. 1 byte.
    struct NetworkOutInstantFreeburnComplete : public NetworkEvent<13>
    {
        bool mbSuccess;                                     // +0x00
    };

    // 14 -- a buddy was removed from the friends list. 16 bytes.
    struct NetworkOutBuddyRemovedEvent : public NetworkEvent<14>
    {
        CgsNetwork::PlayerName mRemovedBuddyName;           // +0x00
    };

    // 18 -- a remote player changed car. 24 bytes. The console record carries a float the
    // reference lacks, between the wheel id and the player id; the game-state event copies it
    // through unread by the translator.
    struct NetworkOutPlayerChangedCarEvent : public NetworkEvent<18>
    {
        CgsID           mModelID;                           // +0x00
        CgsID           mWheelID;                           // +0x08
        f32             mf10;                               // +0x10 (console-only member; unnamed)
        NetworkPlayerID mNetworkPlayerID;                   // +0x14

        NetworkPlayerID GetNetworkPlayerID() const { return mNetworkPlayerID; }
        CgsID           GetModelID() const         { return mModelID; }
        CgsID           GetWheelID() const         { return mWheelID; }

        void SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID)
        {
            CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                       "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            mNetworkPlayerID = lNetworkPlayerID;
        }
        void SetModelID(CgsID lModelID) { mModelID = lModelID; }
        void SetWheelID(CgsID lWheelID) { mWheelID = lWheelID; }
    };

    // 19 -- a remote player changed paint. 12 bytes.
    struct NetworkOutPlayerChangedCarColourEvent : public NetworkEvent<19>
    {
        NetworkPlayerID GetNetworkPlayerID() const            { return mNetworkPlayerID; }
        EActiveRaceCarIndex GetActiveRaceCarIndex() const     { return meActiveRaceCarIndex; }
        u16             GetCarColourIndex() const             { return mu16CarColourIndex; }
        u16             GetPaintFinishIndex() const           { return mu16PaintFinishIndex; }

        void SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID)
        {
            CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                       "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            mNetworkPlayerID = lNetworkPlayerID;
        }
        void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex) { meActiveRaceCarIndex = leActiveRaceCarIndex; }
        void SetCarColourIndex(u16 lu16CarColourIndex)     { mu16CarColourIndex = lu16CarColourIndex; }
        void SetPaintFinishIndex(u16 lu16PaintFinishIndex) { mu16PaintFinishIndex = lu16PaintFinishIndex; }

    private:
        NetworkPlayerID     mNetworkPlayerID;               // +0x00
        EActiveRaceCarIndex meActiveRaceCarIndex;           // +0x04
        u16                 mu16CarColourIndex;             // +0x08
        u16                 mu16PaintFinishIndex;           // +0x0A

        friend void AssertNetworkOutEventLayouts();
    };

    // 21 -- the host's final team assignment, one team per active-race-car slot. 32 bytes.
    // Console-only record; the game-state side hands each entry to ScoringSystem::SetPlayerTeam.
    struct NetworkOutTeamSelectionEvent : public NetworkEvent<21>
    {
        BrnGameState::GameStateModuleIO::EPlayerTeam maePlayerTeam[8]; // +0x00
    };

    // 22 -- the post-event scalp list. 68 bytes.
    struct NetworkOutPostEventScalps : public NetworkEvent<22>
    {
        struct OnlineScalp
        {
            s32 miAggressorIndex;                           // +0x00
            s32 miVictimIndex;                              // +0x04
        };

        OnlineScalp maOnlineScalps[8];                      // +0x00
        s32         miNumScalpsWon;                         // +0x40
    };

    // 24 -- the local player left the game. 8 bytes. meLeftGameReason is the reference's
    // BrnNetwork::ELeftGameReason and meKickReason its CgsNetwork::EKickReason.
    struct NetworkOutPlayerLeftGame : public NetworkEvent<24>
    {
        s32 meLeftGameReason;                               // +0x00 (ELeftGameReason)
        s32 meKickReason;                                   // +0x04 (CgsNetwork::EKickReason)
    };

    // 25 -- the host finished its post-game processing. 1 byte.
    struct NetworkOutPostGameProcessingFinished : public NetworkEvent<25>
    {
        bool mbIsStillInGame;                               // +0x00
    };

    // 26 -- the host is launching a mode. 8 bytes.
    struct NetworkOutLaunchEvent : public NetworkEvent<26>
    {
        BrnGameState::GameStateModuleIO::EGameModeType meGameModeType; // +0x00
        NetworkPlayerID                                mHostPlayerID;  // +0x04
    };

    // 27 -- the launch finished. 8 bytes.
    struct NetworkOutLaunchedEvent : public NetworkEvent<27>
    {
        s32  miVehicleClassLimit;                           // +0x00
        bool mbHostChoiceCarAndNotHost;                     // +0x04
        bool mbSuccessfulLaunch;                            // +0x05
    };

    // 28 -- the sign-in UI must be shown. 1 byte (no payload).
    typedef NetworkEvent<28> NetworkOutShowSignInGui;

    // 29 -- the live-revenge profile is ready. Console 4 bytes (one pointer).
    struct NetworkOutLiveRevengeProfileData : public NetworkEvent<29>
    {
        LiveRevengeProfile* mpLiveRevengeProfile;           // +0x00
    };

    // 30 -- the loading screen must be shown. 1 byte (no payload).
    typedef NetworkEvent<30> NetworkOutShowLoadingScreen;

    // 31 -- the time left in the online event. 8 bytes.
    struct NetworkOutEventTimeRemaining : public NetworkEvent<31>
    {
        CgsSystem::Time mTimeRemaining;                     // +0x00
    };

    struct NetworkOutAutosaveProfile : public NetworkEvent<32> {};    // 1 byte

    // 34 -- a remote player's stunt multiplier arrived. 16 bytes. Console-only record; the
    // game-state side hands it to ScoringSystem::SetNetworkStuntMultiplier.
    struct NetworkOutStuntMultiplierEvent : public NetworkEvent<34>
    {
        s64             mi64MultiplierData;                 // +0x00
        s32             miStuntMultiplier;                  // +0x08
        NetworkPlayerID mNetworkPlayerID;                   // +0x0C
    };

    // 36 -- a road-rules upload finished. 8 bytes.
    struct NetworkOutRecvRoadRulesUploadedEvent : public NetworkEvent<36>
    {
        Road::ChallengeIndex mStartUploadIndex;             // +0x00
        Road::ChallengeIndex mEndUploadIndex;               // +0x04
    };

    // 37 -- a road-rules download finished. 4 bytes.
    struct NetworkOutRoadRulesDownloadedEvent : public NetworkEvent<37>
    {
        u32 muTimestampOfDownload;                          // +0x00
    };

    // 38 -- the road-rules server was reached. 4 bytes.
    struct NetworkOutRoadRulesConnectedOnlineEvent : public NetworkEvent<38>
    {
        u32 muLastRoadRulesResetTime;                       // +0x00
    };

    // 39 -- the local player signed in. 4 bytes.
    struct NetworkOutLocalPlayerConnected : public NetworkEvent<39>
    {
        NetworkPlayerID mPlayerID;                          // +0x00
    };

    // 40 -- the local player lost the session. 1 byte (no payload).
    struct NetworkOutLocalPlayerDisconnected : public NetworkEvent<40> {};

    // 41 -- the local player is joining a session. 1 byte (no payload).
    typedef NetworkEvent<41> NetworkOutJoiningGameEvent;

    // 42 -- a player joined the freeburn lobby, so the game starts. 1 byte (no payload).
    typedef NetworkEvent<42> NetworkOutStartingGameDueToPlayerJoin;

    // 43 -- the host restarted traffic; the active hull of each of the eight PVS slots. 16 bytes.
    struct NetworkOutRestartTrafficEvent : public NetworkEvent<43>
    {
        u16 mau16ActveHulls[8];                             // +0x00
    };

    // 44 -- a remote player picked up a collectable. 16 bytes.
    struct NetworkOutNetworkPlayerCollectableEvent : public NetworkEvent<44>
    {
        CgsID                          mID;                 // +0x00
        NetworkPlayerID                mNetworkPlayerID;    // +0x08
        BrnGameState::StuntElementType meType;              // +0x0C
    };

    // 45 -- the session host changed. 2 bytes.
    struct NetworkOutHostChangedEvent : public NetworkEvent<45>
    {
        bool mbIsLocalPlayerNowHost;                        // +0x00
        bool mbIsFirstHost;                                 // +0x01
    };

    // 46 -- a remote player hit a checkpoint. 8 bytes.
    struct NetworkOutRemotePlayerHitCheckpoint : public NetworkEvent<46>
    {
        NetworkPlayerID mNetworkPlayerID;                   // +0x00
        s32             miCheckpointIndex;                  // +0x04
    };

    // 47 -- a remote player's stunt score arrived. 8 bytes. Console-only record; the game-state
    // side hands both words to ModeManager::SetNetworkStuntScore.
    struct NetworkOutStuntScoreUpdatedEvent : public NetworkEvent<47>
    {
        NetworkPlayerID mNetworkPlayerID;                   // +0x00
        s32             miStuntScore;                       // +0x04
    };

    // 48 -- a player entered or left car select. 8 bytes.
    struct NetworkOutPlayerCarSelectStatus : public NetworkEvent<48>
    {
        EActiveRaceCarIndex meActiveRaceCarIndex;           // +0x00
        bool                mbInCarSelect;                  // +0x04
    };

    // 49 -- the number of rivals. 4 bytes.
    struct NetworkOutRivalCount : public NetworkEvent<49>
    {
        s32 miNumberOfRivals;                               // +0x00
    };

    struct NetworkOutCaughtFever : public NetworkEvent<50> {};        // 1 byte

    // 51 -- one scoreboard page. 2924 bytes.
    struct NetworkOutScoreboardEvent : public NetworkEvent<51>
    {
        // Inlined where the scoreboard manager builds the page on its stack.
        void              Construct()           { mScoreboard.Construct(); }
        Scoreboard*       GetScoreboard()       { return &mScoreboard; }
        const Scoreboard* GetScoreboard() const { return &mScoreboard; }

    private:
        Scoreboard mScoreboard;                             // +0x000
    };

    // 53 -- a score-target challenge for one event. 40 bytes. Console-only record.
    struct NetworkOutTargetScoreEvent : public NetworkEvent<53>
    {
        CgsNetwork::UniquePlayerIDX360 mPlayerID;           // +0x00 (24)
        u64                            mScoreboardSlot;     // +0x18
        s32                            miValue;             // +0x20
        u8                             muChallengeType;     // +0x24
    };

    // 54 -- a downloaded event became challengeable. 8 bytes. Console-only record; the
    // game-state side looks the id up with Profile::GetTargetEvent.
    struct NetworkOutDldChallengeableEvent : public NetworkEvent<54>
    {
        CgsID mID;                                          // +0x00
    };

    struct NetworkOutGetOfflineProgression : public NetworkEvent<55> {};  // 1 byte

    // 56 -- a remote player is capturing the local player's image. 16 bytes.
    struct NetworkOutCapturingTheirImageEvent : public NetworkEvent<56>
    {
        CgsID                                       mRoadId;       // +0x00
        BrnGameState::GameStateModuleIO::EImageType meImageType;   // +0x08
        EActiveRaceCarIndex                         meVictimARCI;  // +0x0C
    };

    // 57 -- an image arrived from a remote player. 16 bytes.
    struct NetworkOutImageReceivedEvent : public NetworkEvent<57>
    {
        CgsID                                       mRoadID;                   // +0x00
        EActiveRaceCarIndex                         meImageSenderRaceCarIndex; // +0x08
        BrnGameState::GameStateModuleIO::EImageType meReceivedImageType;       // +0x0C
    };

    // 58 -- a captured mugshot should be saved. Console 32 bytes; the texture pointer sits at
    // console +0x1C and at host +0x20.
    struct NetworkOutMugshotToSaveEvent : public NetworkEvent<58>
    {
        CgsNetwork::UniquePlayerIDX360              mUniquePlayerID;     // +0x00 (24)
        BrnGameState::GameStateModuleIO::EImageType meImageGalleryType;  // +0x18
        CgsNetwork::NetworkTexture*                 mpTexture;           // console +0x1C
    };

    // 59 -- an image capture or show was aborted. 1 byte.
    struct NetworkOutAbortImageCaptureEvent : public NetworkEvent<59>
    {
        bool mbCapture;                                     // +0x00
    };

    struct NetworkOutSentMugshot : public NetworkEvent<60> {};        // 1 byte

    // 61 -- the Burning Home Run runner changed. 4 bytes.
    struct NetworkOutSwitchBurningHomeRunRunner : public NetworkEvent<61>
    {
        NetworkPlayerID mNewRunnerID;                       // +0x00
    };

    // 62 -- a player's Burnout Skillz changed. 64 bytes.
    struct NetworkOutBurnoutSkillzChanged : public NetworkEvent<62>
    {
        NetworkPlayerID                 mPlayerID;          // +0x00
        BrnGameState::BurnoutSkillzData mNewSkillzData;     // +0x04 (56)
        bool                            mbInitialData;      // +0x3C
    };

    // 63 -- a remote player's Showtime score. 8 bytes.
    struct NetworkOutShowtimeUpdate : public NetworkEvent<63>
    {
        NetworkPlayerID mPlayerID;                          // +0x00
        s32             miShowtimeScore;                    // +0x04
    };

    // 64 -- a remote player entered or left Showtime. 12 bytes.
    struct NetworkOutShowtimeSwitch : public NetworkEvent<64>
    {
        NetworkPlayerID mPlayerID;                          // +0x00
        s32             miFinalShowtimeScore;               // +0x04
        bool            mbEnteringShowtime;                 // +0x08
    };

    // 65 -- a freeburn-challenge lifecycle event from a remote player. 32 bytes. meEventType is
    // BrnNetworkModuleIO::EChallengeEventType (BrnNetworkModuleIO.h), held as its s32 here so
    // this header does not pull in the IO buffers.
    struct NetworkOutFreeburnChallenge : public NetworkEvent<65>
    {
        NetworkPlayerID                mPlayerID;           // +0x00
        CgsID                          mChallengeID;        // +0x08
        s32                            meEventType;         // +0x10 (EChallengeEventType)
        BrnGameState::EChallengeStatus meChallengeStatus;   // +0x14
        s32                            miActionIndex;       // +0x18
    };

    // 66 -- the challenge already running when a player joins. 48 bytes.
    struct NetworkOutActiveFburnChallengeEvent : public NetworkEvent<66>
    {
        static const s32 KI_MAX_NETWORK_PLAYERS = 7;

        EActiveRaceCarIndex maePlayersInChallengeARCI[KI_MAX_NETWORK_PLAYERS]; // +0x00
        CgsID               mChallengeID;                   // +0x20
        s32                 miNumPlayersInChallenge;        // +0x28
    };

    // 67 -- a remote player's last-second success mask. 24 bytes.
    struct NetworkOutFburnChallengeSuccessUpdateEvent : public NetworkEvent<67>
    {
        BrnGameState::GameStateModuleIO::LastSecondChallengeSuccess mChallengeSuccessUpdate; // +0x00 (8)
        EActiveRaceCarIndex meActiveRaceCarIndex;           // +0x08
        s32                 miChallengeUpdateFrame;         // +0x0C
        s32                 miActionIndex;                  // +0x10
    };

    // 68 -- a remote player's per-action challenge result. 20 bytes. The console record puts
    // the update frame and the race-car index BEFORE the two flag pairs (the reference has the
    // index last and no frame).
    struct NetworkOutFburnChallengeSuccessEvent : public NetworkEvent<68>
    {
        f32                 mafActionScores[2];             // +0x00
        s32                 miChallengeUpdateFrame;         // +0x08
        EActiveRaceCarIndex meActiveRaceCarIndex;           // +0x0C
        bool                mabSuccessfulActions[2];        // +0x10
        bool                mabAccumulationThisFrame[2];    // +0x12
    };

    // 69 -- a received image finished DXT decoding. Console 20 bytes; the pixel pointer is 8
    // bytes on the host, so the name sits at console +0x04 and at host +0x08.
    struct NetworkOutImageDxtDecodedEvent : public NetworkEvent<69>
    {
        void*                  mpDecodedPixels;             // +0x00
        CgsNetwork::PlayerName mPlayerName;                 // console +0x04
    };

    // 70 -- the account's data-sharing settings. 3 bytes.
    struct NetworkOutAccountSettings : public NetworkEvent<70>
    {
        bool mbAgreeToShareInfoEA;                          // +0x00
        bool mbAgreeToShareInfoPartners;                    // +0x01
        bool mbTelemetryEnable;                             // +0x02
    };

    // 71 -- the account settings edit finished. 1 byte (no payload).
    typedef NetworkEvent<71> NetworkOutAccountUpdateComplete;

    // 72 -- the Xbox camera picture was compressed. Console 12 bytes; the pixel pointer sits at
    // console +0x08 and at host +0x08 (8 bytes wide).
    struct NetworkOutCamPicCompressedEvent : public NetworkEvent<72>
    {
        s32                      miCompressedPixelSize;     // +0x00
        renderengine::PixelFormat meCompressedFormat;       // +0x04
        char*                    mpcCompressedPixels;       // +0x08
    };

    // 73 -- the photo booth has no gamer picture for the player. 1 byte (no payload).
    typedef NetworkEvent<73> NetworkOutNoPhotoBoothGamerPic;

    // 74 -- the event scores uploaded this session. 128 bytes. Console-only record; the
    // game-state side reads it as the Array<s64,15> it is.
    struct NetworkOutUploadedModeScoresEvent : public NetworkEvent<74>
    {
        ::Array<s64, 15> maUploadedModeScores;                // +0x00 (15 x 8 + count)
    };

    // ---------------------------------------------------------------------------------
    // Layout pins. Every record without a pointer is laid out identically on the host, so
    // the console offsets and sizes are asserted exactly. Never called.
    inline void AssertNetworkOutEventLayouts()
    {
        static_assert(sizeof(NetworkOutBuddyCount) == 4, "tag 1 size");
        static_assert(offsetof(NetworkOutBuddyInformation, miNumberOfBuddiesInEvent) == 0x294, "tag 2 +0x294");
        static_assert(offsetof(NetworkOutBuddyInformation, miIndexOfFirstBuddyInFullBuddyList) == 0x298, "tag 2 +0x298");
        static_assert(offsetof(NetworkOutBuddyInformation, mbConnected) == 0x29C, "tag 2 +0x29C");
        static_assert(sizeof(NetworkOutBuddyInformation) == 672, "tag 2 size");
        static_assert(sizeof(NetworkOutInviteRequest) == 0x9C, "tag 6 size");
        static_assert(sizeof(NetworkOutInviteFailed) == 4, "tag 8 size");
        static_assert(offsetof(NetworkOutBuddyNotification, mBuddyName) == 0x08, "tag 9 +0x08");
        static_assert(sizeof(NetworkOutBuddyNotification) == 24, "tag 9 size");
        static_assert(sizeof(NetworkOutBuddyListChanged) == 1, "tag 10 size");
        static_assert(sizeof(NetworkOutInviteSent) == 1, "tag 11 size");
        static_assert(sizeof(NetworkOutInstantFreeburnEvent) == 1, "tag 12 size");
        static_assert(sizeof(NetworkOutInstantFreeburnComplete) == 1, "tag 13 size");
        static_assert(sizeof(NetworkOutBuddyRemovedEvent) == 16, "tag 14 size");
        static_assert(offsetof(NetworkOutPlayerChangedCarEvent, mf10) == 0x10, "tag 18 +0x10");
        static_assert(offsetof(NetworkOutPlayerChangedCarEvent, mNetworkPlayerID) == 0x14, "tag 18 +0x14");
        static_assert(sizeof(NetworkOutPlayerChangedCarEvent) == 24, "tag 18 size");
        static_assert(offsetof(NetworkOutPlayerChangedCarColourEvent, meActiveRaceCarIndex) == 0x04, "tag 19 +0x04");
        static_assert(offsetof(NetworkOutPlayerChangedCarColourEvent, mu16CarColourIndex) == 0x08, "tag 19 +0x08");
        static_assert(offsetof(NetworkOutPlayerChangedCarColourEvent, mu16PaintFinishIndex) == 0x0A, "tag 19 +0x0A");
        static_assert(sizeof(NetworkOutPlayerChangedCarColourEvent) == 12, "tag 19 size");
        static_assert(sizeof(NetworkOutTeamSelectionEvent) == 32, "tag 21 size");
        static_assert(offsetof(NetworkOutPostEventScalps, miNumScalpsWon) == 0x40, "tag 22 +0x40");
        static_assert(sizeof(NetworkOutPostEventScalps) == 68, "tag 22 size");
        static_assert(sizeof(NetworkOutPlayerLeftGame) == 8, "tag 24 size");
        static_assert(sizeof(NetworkOutPostGameProcessingFinished) == 1, "tag 25 size");
        static_assert(sizeof(NetworkOutLaunchEvent) == 8, "tag 26 size");
        static_assert(offsetof(NetworkOutLaunchedEvent, mbHostChoiceCarAndNotHost) == 0x04, "tag 27 +0x04");
        static_assert(offsetof(NetworkOutLaunchedEvent, mbSuccessfulLaunch) == 0x05, "tag 27 +0x05");
        static_assert(sizeof(NetworkOutLaunchedEvent) == 8, "tag 27 size");
        static_assert(sizeof(NetworkOutShowSignInGui) == 1, "tag 28 size");
        static_assert(sizeof(NetworkOutShowLoadingScreen) == 1, "tag 30 size");
        static_assert(sizeof(NetworkOutEventTimeRemaining) == 8, "tag 31 size");
        static_assert(sizeof(NetworkOutAutosaveProfile) == 1, "tag 32 size");
        static_assert(offsetof(NetworkOutStuntMultiplierEvent, miStuntMultiplier) == 0x08, "tag 34 +0x08");
        static_assert(offsetof(NetworkOutStuntMultiplierEvent, mNetworkPlayerID) == 0x0C, "tag 34 +0x0C");
        static_assert(sizeof(NetworkOutStuntMultiplierEvent) == 16, "tag 34 size");
        static_assert(sizeof(NetworkOutRecvRoadRulesUploadedEvent) == 8, "tag 36 size");
        static_assert(sizeof(NetworkOutRoadRulesDownloadedEvent) == 4, "tag 37 size");
        static_assert(sizeof(NetworkOutRoadRulesConnectedOnlineEvent) == 4, "tag 38 size");
        static_assert(sizeof(NetworkOutLocalPlayerConnected) == 4, "tag 39 size");
        static_assert(sizeof(NetworkOutLocalPlayerDisconnected) == 1, "tag 40 size");
        static_assert(sizeof(NetworkOutJoiningGameEvent) == 1, "tag 41 size");
        static_assert(sizeof(NetworkOutStartingGameDueToPlayerJoin) == 1, "tag 42 size");
        static_assert(sizeof(NetworkOutRestartTrafficEvent) == 16, "tag 43 size");
        static_assert(offsetof(NetworkOutNetworkPlayerCollectableEvent, mNetworkPlayerID) == 0x08, "tag 44 +0x08");
        static_assert(offsetof(NetworkOutNetworkPlayerCollectableEvent, meType) == 0x0C, "tag 44 +0x0C");
        static_assert(sizeof(NetworkOutNetworkPlayerCollectableEvent) == 16, "tag 44 size");
        static_assert(sizeof(NetworkOutHostChangedEvent) == 2, "tag 45 size");
        static_assert(sizeof(NetworkOutRemotePlayerHitCheckpoint) == 8, "tag 46 size");
        static_assert(sizeof(NetworkOutStuntScoreUpdatedEvent) == 8, "tag 47 size");
        static_assert(offsetof(NetworkOutPlayerCarSelectStatus, mbInCarSelect) == 0x04, "tag 48 +0x04");
        static_assert(sizeof(NetworkOutPlayerCarSelectStatus) == 8, "tag 48 size");
        static_assert(sizeof(NetworkOutRivalCount) == 4, "tag 49 size");
        static_assert(sizeof(NetworkOutCaughtFever) == 1, "tag 50 size");
        static_assert(sizeof(NetworkOutScoreboardEvent) == 2924, "tag 51 size");
        static_assert(offsetof(NetworkOutTargetScoreEvent, mScoreboardSlot) == 0x18, "tag 53 +0x18");
        static_assert(offsetof(NetworkOutTargetScoreEvent, miValue) == 0x20, "tag 53 +0x20");
        static_assert(offsetof(NetworkOutTargetScoreEvent, muChallengeType) == 0x24, "tag 53 +0x24");
        static_assert(sizeof(NetworkOutTargetScoreEvent) == 40, "tag 53 size");
        static_assert(sizeof(NetworkOutDldChallengeableEvent) == 8, "tag 54 size");
        static_assert(sizeof(NetworkOutGetOfflineProgression) == 1, "tag 55 size");
        static_assert(offsetof(NetworkOutCapturingTheirImageEvent, meImageType) == 0x08, "tag 56 +0x08");
        static_assert(offsetof(NetworkOutCapturingTheirImageEvent, meVictimARCI) == 0x0C, "tag 56 +0x0C");
        static_assert(sizeof(NetworkOutCapturingTheirImageEvent) == 16, "tag 56 size");
        static_assert(offsetof(NetworkOutImageReceivedEvent, meImageSenderRaceCarIndex) == 0x08, "tag 57 +0x08");
        static_assert(offsetof(NetworkOutImageReceivedEvent, meReceivedImageType) == 0x0C, "tag 57 +0x0C");
        static_assert(sizeof(NetworkOutImageReceivedEvent) == 16, "tag 57 size");
        static_assert(offsetof(NetworkOutMugshotToSaveEvent, meImageGalleryType) == 0x18, "tag 58 +0x18");
        static_assert(sizeof(NetworkOutAbortImageCaptureEvent) == 1, "tag 59 size");
        static_assert(sizeof(NetworkOutSentMugshot) == 1, "tag 60 size");
        static_assert(sizeof(NetworkOutSwitchBurningHomeRunRunner) == 4, "tag 61 size");
        static_assert(offsetof(NetworkOutBurnoutSkillzChanged, mNewSkillzData) == 0x04, "tag 62 +0x04");
        static_assert(offsetof(NetworkOutBurnoutSkillzChanged, mbInitialData) == 0x3C, "tag 62 +0x3C");
        static_assert(sizeof(NetworkOutBurnoutSkillzChanged) == 64, "tag 62 size");
        static_assert(sizeof(NetworkOutShowtimeUpdate) == 8, "tag 63 size");
        static_assert(offsetof(NetworkOutShowtimeSwitch, mbEnteringShowtime) == 0x08, "tag 64 +0x08");
        static_assert(sizeof(NetworkOutShowtimeSwitch) == 12, "tag 64 size");
        static_assert(offsetof(NetworkOutFreeburnChallenge, mChallengeID) == 0x08, "tag 65 +0x08");
        static_assert(offsetof(NetworkOutFreeburnChallenge, meEventType) == 0x10, "tag 65 +0x10");
        static_assert(offsetof(NetworkOutFreeburnChallenge, meChallengeStatus) == 0x14, "tag 65 +0x14");
        static_assert(offsetof(NetworkOutFreeburnChallenge, miActionIndex) == 0x18, "tag 65 +0x18");
        static_assert(sizeof(NetworkOutFreeburnChallenge) == 32, "tag 65 size");
        static_assert(offsetof(NetworkOutActiveFburnChallengeEvent, mChallengeID) == 0x20, "tag 66 +0x20");
        static_assert(offsetof(NetworkOutActiveFburnChallengeEvent, miNumPlayersInChallenge) == 0x28, "tag 66 +0x28");
        static_assert(sizeof(NetworkOutActiveFburnChallengeEvent) == 48, "tag 66 size");
        static_assert(offsetof(NetworkOutFburnChallengeSuccessUpdateEvent, meActiveRaceCarIndex) == 0x08, "tag 67 +0x08");
        static_assert(offsetof(NetworkOutFburnChallengeSuccessUpdateEvent, miChallengeUpdateFrame) == 0x0C, "tag 67 +0x0C");
        static_assert(offsetof(NetworkOutFburnChallengeSuccessUpdateEvent, miActionIndex) == 0x10, "tag 67 +0x10");
        static_assert(sizeof(NetworkOutFburnChallengeSuccessUpdateEvent) == 24, "tag 67 size");
        static_assert(offsetof(NetworkOutFburnChallengeSuccessEvent, miChallengeUpdateFrame) == 0x08, "tag 68 +0x08");
        static_assert(offsetof(NetworkOutFburnChallengeSuccessEvent, meActiveRaceCarIndex) == 0x0C, "tag 68 +0x0C");
        static_assert(offsetof(NetworkOutFburnChallengeSuccessEvent, mabSuccessfulActions) == 0x10, "tag 68 +0x10");
        static_assert(offsetof(NetworkOutFburnChallengeSuccessEvent, mabAccumulationThisFrame) == 0x12, "tag 68 +0x12");
        static_assert(sizeof(NetworkOutFburnChallengeSuccessEvent) == 20, "tag 68 size");
        static_assert(offsetof(NetworkOutImageDxtDecodedEvent, mPlayerName) == sizeof(void*), "tag 69 name follows the pointer");
        static_assert(sizeof(NetworkOutAccountSettings) == 3, "tag 70 size");
        static_assert(sizeof(NetworkOutAccountUpdateComplete) == 1, "tag 71 size");
        static_assert(offsetof(NetworkOutCamPicCompressedEvent, meCompressedFormat) == 0x04, "tag 72 +0x04");
        static_assert(offsetof(NetworkOutCamPicCompressedEvent, mpcCompressedPixels) == 0x08, "tag 72 +0x08");
        static_assert(sizeof(NetworkOutNoPhotoBoothGamerPic) == 1, "tag 73 size");
        static_assert(sizeof(NetworkOutUploadedModeScoresEvent) == 128, "tag 74 size");
    }
}
}
