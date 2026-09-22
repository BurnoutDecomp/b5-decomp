#pragma once

#include <cstddef>                                                          // offsetof (_AssertLayout)

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"                               // BrnUpdateSet
#include "SharedClasses/World/BrnWorldRegion.h"                             // BrnWorld::EDistrict
#include "GameShared/GameClasses/Core/CgsID.h"                              // CgsID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                    // CgsSystem::Time
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"        // CgsSystem::EFrameRate
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameSource/GameState/BrnCgsPlayerName.h"                          // CgsNetwork::PlayerName
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                 // NetworkPlayerID, ETelemetryHook, EActiveRaceCarIndex
#include "GameSource/Network/BrnServerInterface.h"                          // mServerInterface
#include "GameSource/Network/BrnNetworkServers.h"                           // mNetworkServers
#include "GameSource/Network/Managers/X360/BrnNetworkLoginManagerX360.h"    // mLoginManager
#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"            // mLaunchManager
#include "GameSource/Network/Managers/BrnNetworkConnectionManager.h"        // mConnectionManager
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"        // mSuspensionManager
#include "GameSource/Network/Managers/BrnNetworkScoreboardManager.h"        // mScoreboardManager
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"       // GetLiveRevengeManager() callers
#include "GameSource/Network/Managers/X360/BrnNetworkGamerPictureManagerX360.h"  // mGamerPictureManager
#include "GameSource/Network/Managers/X360/BrnNetworkNotificationManagerX360.h"  // mNetworkNotificationManager
#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"            // mNetworkInviteManager
#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"         // mRoadRulesManager
#include "GameSource/Network/Managers/X360/BrnNetworkGamerCardManagerX360.h"     // mGamerCardManager
#include "GameSource/Network/Managers/BrnNetworkAutoLoginManager.h"         // mAutoLoginManager
#include "GameSource/Network/Managers/BrnNetworkTeamSelectionManager.h"     // mTeamSelectionManager

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- the online hub embedded in BrnNetworkModule at +0x280.
//
// LAYOUT
// ------
// Every member is placed by name at the offset the constructor, Construct, Prepare, Release,
// Destruct and the per-frame updates address it at (console byte offsets in the comments).
// A sub-object whose committed header reproduces its console span exactly is embedded as a
// typed member. A sub-object whose header is a thin slice, over-models its span, or has no
// class home is kept as offset-pinned byte storage of the console span; its accessor stays
// declared-only until the owning header reconciles. The per-member notes say which header
// needs growing (or shrinking) and by how much.
//
// Host layout: pointers widen on the x64 host, so the host offsets differ from the console
// ones; code must reach members by name only. _AssertLayout() pins every console offset in a
// 32-bit build (where the committed headers reproduce the console widths) and is inert on x64.
//
// The CgsNetwork::NetworkManager base class has no header home yet (only a translation-unit
// local placeholder in CgsNetworkManager.cpp), so its span is pinned storage at +0x0000 rather
// than a base class. Attested members inside it: VersionDisplay +0x00, NetworkAdapter +0x1C,
// active controller port +0x60, PlayerManager +0x78, HostMigrationManager +0x2578,
// StartTimeManager +0x2B68, TimeManager +0x32D0 (frame counter read at +0x3658),
// VoIPManager +0x366C.
//
// The console object is at least 0x95E0C bytes (last attested member) and at most 0x95E80
// (the module's own members resume at module +0x96100).
// ============================================================================================

namespace CgsNetwork
{
    struct Message;                      // pointer-only (PackOrUnpack)
    struct PlayerManager;                // pointer-only (GetPlayerManager)
    class PlayersConnectionManager;      // pointer-only (GetPlayersConnectionManager)
    class NetworkTexture;                // pointer-only (PackTextureAndSendDisplayEventToGui)
}

namespace CgsSystem
{
    class TimerStatus;                   // pointer-only (mpGameTimerStatus)
    class TimerStatusInterface;          // pointer-only (GetTimerStatus)
}

namespace CgsMemory
{
    class HeapMalloc;                    // pointer-only (Prepare)
}

namespace BrnHW
{
    struct LaunchData;                   // pointer-only (Prepare); no class home yet
}

namespace BrnNetwork
{
    class BrnNetworkModule;              // pointer-only (mpNetworkModule)

    // Pointer-only accessor returns for the sub-managers held as pinned storage below.
    class  MatchMakingManager;
    class  PostRoundManager;
    struct StandingsManager;
    class  NetworkPlayerStatsManager;
    struct NetworkAggressiveDrivingManager;
    struct NetworkDirtyTrickManager;
    struct NetworkImageManager;
    class  SelectedRoutesManager;
    class  MarkedManManager;
    struct StateManager;
    class  EventScoresManager;
    class  CameraX360;
    class  BuddyManagerX360;
    struct ChallengeSuccessManager;
    class  BrnNetworkPlayer;

    namespace BrnNetworkModuleIO
    {
        struct PreSimulationInputBuffer;         // ProcessBeforeSimulation
        struct PostSimulationInputBuffer;        // ProcessAfterSimulation
        struct OutputBuffer;                     // ProcessBeforeSimulation / Output*Info
        struct NetworkInDxtDecodeImageEvent;     // ProcessNetworkTextureDecodeEvent
    }
}

namespace BrnNetwork
{
    class BrnNetworkManager
    {
        friend class NetworkServers;

    public:
        // Staged bring-up. The console switch has 33 cases; stages 0..28 carry the declared
        // names, stages 29..31 are the three managers the console build adds after the
        // gamer-card manager (named after the manager each one prepares).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START                     = 0,
            E_PREPARESTAGE_BASE_NETWORK_MANAGER      = 1,
            E_PREPARESTAGE_SERVER_INTERFACE          = 2,
            E_PREPARESTAGE_LOGIN_MANAGER             = 3,
            E_PREPARESTAGE_LAUNCH_MANAGER            = 4,
            E_PREPARESTAGE_CONNECTION_MANAGER        = 5,
            E_PREPARESTAGE_MATCHMAKING_MANAGER       = 6,
            E_PREPARESTAGE_SUSPENSION_MANAGER        = 7,
            E_PREPARESTAGE_POSTROUND_MANAGER         = 8,
            E_PREPARESTAGE_CAMERA                    = 9,
            E_PREPARESTAGE_TEXTURE_COMPRESSOR        = 10,
            E_PREPARESTAGE_BUDDY_MANAGER             = 11,
            E_PREPARESTAGE_STANDINGS_MANAGER         = 12,
            E_PREPARESTAGE_TRAFFIC_SYNC_MANAGER      = 13,
            E_PREPARESTAGE_STATS_MANAGER             = 14,
            E_PREPARESTAGE_SCOREBOARD_MANAGER        = 15,
            E_PREPARESTAGE_AGGRESSIVE_DRIVING_MANAGER = 16,
            E_PREPARESTAGE_LIVE_REVENGE_MANAGER      = 17,
            E_PREPARESTAGE_DIRTY_TRICK_MANAGER       = 18,
            E_PREPARESTAGE_IMAGE_MANAGER             = 19,
            E_PREPARESTAGE_GAMER_PICTURE_MANAGER     = 20,
            E_PREPARESTAGE_NOTIFICATION_MANAGER      = 21,
            E_PREPARESTAGE_INVITE_MANAGER            = 22,
            E_PREPARESTAGE_SELECTED_ROUTES_MANAGER   = 23,
            E_PREPARESTAGE_MARKED_MAN_MANAGER        = 24,
            E_PREPARESTAGE_STATE_MANAGER             = 25,
            E_PREPARESTAGE_ROAD_RULES_MANAGER        = 26,
            E_PREPARESTAGE_CHALLEGE_SUCCESS_MANAGER  = 27,
            E_PREPARESTAGE_GAMERCARD_MANAGER         = 28,
            E_PREPARESTAGE_EVENT_SCORES_MANAGER      = 29,
            E_PREPARESTAGE_AUTO_LOGIN_MANAGER        = 30,
            E_PREPARESTAGE_TEAM_SELECTION_MANAGER    = 31,
            E_PREPARESTAGE_DONE                      = 32,
        };

        // Staged tear-down, the mirror of EPrepareStage (33 console cases). Stages 1..3 are
        // the three console-build managers, released first.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START                     = 0,
            E_RELEASESTAGE_TEAM_SELECTION_MANAGER    = 1,
            E_RELEASESTAGE_AUTO_LOGIN_MANAGER        = 2,
            E_RELEASESTAGE_EVENT_SCORES_MANAGER      = 3,
            E_RELEASESTAGE_GAMERCARD_MANAGER         = 4,
            E_RELEASESTAGE_CHALLEGE_SUCCESS_MANAGER  = 5,
            E_RELEASESTAGE_ROAD_RULES_MANAGER        = 6,
            E_RELEASESTAGE_STATE_MANAGER             = 7,
            E_RELEASESTAGE_MARKED_MAN_MANAGER        = 8,
            E_RELEASESTAGE_SELECTED_ROUTES_MANAGER   = 9,
            E_RELEASESTAGE_INVITE_MANAGER            = 10,
            E_RELEASESTAGE_NOTIFICATION_MANAGER      = 11,
            E_RELEASESTAGE_GAMER_PICTURE_MANAGER     = 12,
            E_RELEASESTAGE_IMAGE_MANAGER             = 13,
            E_RELEASESTAGE_DIRTY_TRICK_MANAGER       = 14,
            E_RELEASESTAGE_LIVE_REVENGE_MANAGER      = 15,
            E_RELEASESTAGE_AGGRESSIVE_DRIVING_MANAGER = 16,
            E_RELEASESTAGE_SCOREBOARD_MANAGER        = 17,
            E_RELEASESTAGE_STATS_MANAGER             = 18,
            E_RELEASESTAGE_TRAFFIC_SYNC_MANAGER      = 19,
            E_RELEASESTAGE_STANDINGS_MANAGER         = 20,
            E_RELEASESTAGE_BUDDY_MANAGER             = 21,
            E_RELEASESTAGE_TEXTURE_COMPRESSOR        = 22,
            E_RELEASESTAGE_CAMERA                    = 23,
            E_RELEASESTAGE_POSTROUND_MANAGER         = 24,
            E_RELEASESTAGE_SUSPENSION_MANAGER        = 25,
            E_RELEASESTAGE_MATCHMAKING_MANAGER       = 26,
            E_RELEASESTAGE_CONNECTION_MANAGER        = 27,
            E_RELEASESTAGE_LAUNCH_MANAGER            = 28,
            E_RELEASESTAGE_LOGIN_MANAGER             = 29,
            E_RELEASESTAGE_SERVER_INTERFACE          = 30,
            E_RELEASESTAGE_BASE_NETWORK_MANAGER      = 31,
            E_RELEASESTAGE_DONE                      = 32,
        };

        // Login-flow events raised by the login state machine (TriggerEventFromLogin switches
        // on 8 cases).
        enum ELoginEvent
        {
            E_LOGIN_EVENT_SHOW_TOS              = 0,
            E_LOGIN_EVENT_SHOW_CREATE_ACCOUNT   = 1,
            E_LOGIN_EVENT_SHOW_SHARE            = 2,
            E_LOGIN_EVENT_SHOW_OPEN_US_ACCOUNT  = 3,
            E_LOGIN_EVENT_SHOW_NO_AGREEMENT     = 4,
            E_LOGIN_EVENT_SHOW_SIGN_IN          = 5,
            E_LOGIN_EVENT_SHOW_CHAT_RESTRICTED  = 6,
            E_LOGIN_EVENT_PROCEED               = 7,
            E_LOGIN_EVENT_COUNT                 = 8,
        };

        // Per-field (de)serialise status; 0 == success, OR-ed together by the callers.
        typedef u8 PackOrUnpackResult;

        // ---- lifecycle -------------------------------------------------------------------
        BrnNetworkManager();
        void Construct(BrnNetworkModule* lpNetworkModule, bool lbEnableJuice);
        bool Prepare(CgsSystem::EFrameRate leLocalConsoleFrameRate,
                     const BrnHW::LaunchData* lpLaunchData,
                     CgsMemory::HeapMalloc* lpHeapMalloc);
        bool Release();
        void Destruct();

        // ---- per-frame -------------------------------------------------------------------
        void ProcessBeforeSimulation(const BrnNetworkModuleIO::PreSimulationInputBuffer* lpInputBuffer,
                                     BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer,
                                     CgsSystem::TimerStatus* lpTimerStatus,
                                     BrnUpdateSet luUpdateSet);
        void ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer,
                                    BrnUpdateSet luUpdateSet);

        // ---- raised events ---------------------------------------------------------------
        void TriggerEventFromLogin(ELoginEvent leEvent, void* lpData);
        void TriggerEventFromServerInterface(CgsNetwork::EServerInterfaceEvent leEvent, void* lpData);

        // ---- session lifecycle hooks -----------------------------------------------------
        void JoinGameSession(char* lpcSessionID);        // forwards to mStateManager
        void OnGameLaunching();
        void OnGameStart();
        void OnRoundStart();
        void OnRoundFinish();
        void OnGameFinish();
        void OnEnterGame();
        void OnLeaveGame();

        // ---- auto-login hooks (forward to mAutoLoginManager) ------------------------------
        void OnLogIn();
        void OnAutoLogin();
        void OnAutoLoginProcessComplete(u32 luCompletedProcess);
        void OnFinishedBoot();

        // ---- queries ---------------------------------------------------------------------
        CgsSystem::EFrameRate GetLocalConsoleFrameRate();
        bool IsLocalPlayer(NetworkPlayerID lPlayerID);
        bool IsDoingFreeBurnLobby();
        EActiveRaceCarIndex GetActiveRaceCarIndex(NetworkPlayerID lPlayerID);

        // ---- GUI output ------------------------------------------------------------------
        void PackTextureAndSendDisplayEventToGui(const CgsNetwork::NetworkTexture* lpTexture,
                                                 s32 liPlayerIndex);
        void OutputPlayerStatsToGui(NetworkPlayerID lPlayerID);
        void OutputGameParameters();
        void ProcessNetworkTextureDecodeEvent(
                const BrnNetworkModuleIO::NetworkInDxtDecodeImageEvent* lpDxtDecodeRequestEvent);

        // ---- telemetry (the const char* overload; the others are inlined at their callers) --
        void CaptureTelemetryEvent(ETelemetryHook leHook, const char* lpcData);

        // (De)serialise one NetworkPlayerID field over the full s32 range. The call sites pass
        // only the message and the field, so this is static.
        static PackOrUnpackResult PackOrUnpack(CgsNetwork::Message* lpMessage,
                                               NetworkPlayerID* lpNetworkPlayerID);

        // ---- sub-object accessors: typed members --------------------------------------------
        BrnServerInterface*       GetServerInterface()       { return &mServerInterface; }
        const BrnServerInterface* GetServerInterface() const { return &mServerInterface; }
        LoginManagerX360*         GetLoginManager()          { return &mLoginManager; }
        LaunchManager*            GetLaunchManager()         { return &mLaunchManager; }
        ConnectionManager*        GetConnectionManager()     { return &mConnectionManager; }
        SuspensionManager*        GetSuspensionManager()     { return &mSuspensionManager; }
        ScoreboardManager*        GetScoreboardManager()     { return &mScoreboardManager; }
        GamerPictureManagerX360*  GetGamerPictureManager()   { return &mGamerPictureManager; }
        NetworkNotificationManagerX360* GetNotificationManager() { return &mNetworkNotificationManager; }
        NetworkInviteManager*     GetNetworkInviteManager()  { return &mNetworkInviteManager; }
        NetworkRoadRulesManager*  GetRoadRulesManager()      { return &mRoadRulesManager; }
        NetworkGamerCardManagerX360* GetGamerCardManager()   { return &mGamerCardManager; }
        NetworkServers*           GetNetworkServers()        { return &mNetworkServers; }
        CgsSystem::Time           GetTime() const            { return mTime; }
        f32                       GetTimeStep() const        { return mfTimeStep; }

        // ---- sub-object accessors: pinned-storage members (declared-only until the owning
        //      header reconciles with its console span) ----------------------------------
        MatchMakingManager*              GetMatchMakingManager();
        PostRoundManager*                GetPostRoundManager();
        StandingsManager*                GetStandingsManager();
        NetworkPlayerStatsManager*       GetStatsManager();
        NetworkAggressiveDrivingManager* GetAggressiveDrivingManager();
        LiveRevengeManager*              GetLiveRevengeManager();
        NetworkImageManager*             GetNetworkImageManager();
        SelectedRoutesManager*           GetSelectedRoutesManager();
        MarkedManManager*                GetMarkedManManager();
        StateManager*                    GetStateManager();
        CameraX360*                      GetCamera();
        BuddyManagerX360*                GetBuddyManager();
        ChallengeSuccessManager*         GetChallengeSuccessManager();

        // ---- accessors into the CgsNetwork::NetworkManager base span (declared-only until
        //      that class has a header) ---------------------------------------------------
        CgsNetwork::PlayerManager*            GetPlayerManager();                  // base +0x78
        CgsNetwork::PlayersConnectionManager* GetPlayersConnectionManager();
        s32  GetLocalUserControllerPort() const;                                   // base +0x60
        u32  GetCurrentFrame() const;                                              // base +0x3658

        // ---- other declared-only accessors used by committed callers ----------------------
        CgsSystem::TimerStatusInterface* GetTimerStatus();       // reads mpGameTimerStatus
        u8   GetCurrentRoundNumber() const;                      // reads miRoundNumber
        void ClearLocalUserSignedInFlag();                       // a byte inside mGamerPictureManager
        bool HasLoginManager() const;                            // mLoginManager's vtable slot
        s32  GetNetworkLoginState() const;                       // mLoginManager's state word

    private:
        s32  GetMaxMessageSize(bool lbReliableOnly);
        void OutputPlayerStatusInfo(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void OutputPlayerResultsInfo(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void UpdateMenuDataFromPlayerParams(s32 liPlayerIndex);
        void ProcessHLUpdateFlags(BrnUpdateSet luUpdateSet);

        // Callbacks registered by address; the user-data word is the manager. The player
        // manager's event enum has no home in CgsPlayerManager.h yet, so the event is an s32.
        static void PlayerManagerEventCallback(s32 leEvent, void* lpEventData, void* lpUserData);
        static void SyncTimeClientReadyCallback(NetworkPlayerID lClientReadyID, void* lpUserData);
        static void DxtDecodeCallback(void* lpPixels, void* lpUserData);

        // Console offsets are pinned in a 32-bit build; inert on the x64 host.
        static void _AssertLayout();

        // ---- +0x00000 : CgsNetwork::NetworkManager base span (no header home) ---------------
        u8 maNetworkManagerBaseStorage[0x38E8];

        // ---- +0x038E8 : server interface. The committed hierarchy is 1168 bytes short of
        //      its 0x14CC console span; the reserve keeps the following offsets. Shrink it as
        //      the server-interface base and platform layers grow. ---------------------------
        BrnServerInterface mServerInterface;                          // +0x038E8
        u8 maServerInterfaceReserve[1168];

        LoginManagerX360   mLoginManager;                             // +0x04DB4
        LaunchManager      mLaunchManager;                            // +0x04DDC
        ConnectionManager  mConnectionManager;                        // +0x04E18

        // MatchMakingManager: the committed header models 108 of 5652 bytes (grow by 5544).
        u8 maMatchMakingManagerStorage[0x1614];                       // +0x04E24

        SuspensionManager  mSuspensionManager;                        // +0x06438

        // PostRoundManager: the committed header is 356 bytes larger than its 420-byte span.
        u8 maPostRoundManagerStorage[0x1A4];                          // +0x0644C

        // StandingsManager: the committed header is 72 bytes larger than its 1296-byte span.
        u8 maStandingsManagerStorage[0x510];                          // +0x065F0

        // TrafficManager: committed as a namespace; the console object spans 99520 bytes.
        u8 maTrafficManagerStorage[0x184C0];                          // +0x06B00

        // Buddy manager: its header reproduces the 85848-byte span, but it pulls in
        // BrnNetworkBuddyManagerDebugComponent.h, whose BrnNetwork::NetworkEventQueue is
        // defined differently in BrnNetworkImageManager.h. Pinned until that is fixed.
        u8 maBuddyManagerStorage[0x14F58];                            // +0x1EFC0

        // NetworkPlayerStatsManager: the committed header is 8 bytes larger than its span.
        u8 maStatsManagerStorage[0x1788];                             // +0x33F18

        ScoreboardManager  mScoreboardManager;                        // +0x356A0

        // NetworkAggressiveDrivingManager: the header is 224 bytes larger than its span.
        u8 maAggressiveDrivingManagerStorage[0x2F60];                 // +0x37070

        // LiveRevengeManager: the header is 112 bytes larger than its 2728-byte span.
        u8 maLiveRevengeManagerStorage[0xAA8];                        // +0x39FD0

        // NetworkDirtyTrickManager: the header is 56 bytes larger than its 768-byte span.
        u8 maDirtyTrickManagerStorage[0x300];                         // +0x3AA78

        // NetworkImageManager: the header is 112 bytes larger than its 8664-byte span.
        u8 maImageManagerStorage[0x21D8];                             // +0x3AD78

        GamerPictureManagerX360 mGamerPictureManager;                 // +0x3CF50

        // Notification manager: header 4 bytes short of its 16-byte span.
        NetworkNotificationManagerX360 mNetworkNotificationManager;   // +0x3D108
        u8 maNotificationManagerReserve[4];

        // Invite manager: header 4 bytes short of its 448-byte span.
        NetworkInviteManager mNetworkInviteManager;                   // +0x3D118
        u8 maInviteManagerReserve[4];

        // SelectedRoutesManager: the header is 56 bytes larger than its 1968-byte span.
        u8 maSelectedRoutesManagerStorage[0x7B0];                     // +0x3D2D8

        // MarkedManManager: the header is 52 bytes larger than its 712-byte span.
        u8 maMarkedManManagerStorage[0x2C8];                          // +0x3DA88

        // StateManager: the committed header models 4 of 248 bytes (grow by 244).
        u8 maStateManagerStorage[0xF8];                               // +0x3DD50

        // Road-rules manager: header 8 bytes short of its 13920-byte span.
        NetworkRoadRulesManager mRoadRulesManager;                    // +0x3DE48
        u8 maRoadRulesManagerReserve[8];

        // ChallengeSuccessManager: its header reproduces the 1544-byte span, but it defines
        // BrnNetwork::KI_MAX_NETWORK_PLAYERS a second time (also in
        // BrnNetworkAggressiveDrivingManager.h). Pinned until the duplicate is removed.
        u8 maChallengeSuccessManagerStorage[0x608];                   // +0x414A8
        NetworkGamerCardManagerX360 mGamerCardManager;                // +0x41AB0

        // EventScoresManager: the header is 8 bytes larger than its 832-byte span.
        u8 maEventScoresManagerStorage[0x340];                        // +0x41B50

        AutoLoginManager     mAutoLoginManager;                       // +0x41E90
        TeamSelectionManager mTeamSelectionManager;                   // +0x41EB8

        // Server selection: header 4 bytes short of its 20-byte span.
        NetworkServers mNetworkServers;                               // +0x425CC
        u8 maNetworkServersReserve[4];

        // Camera: the header is 8 bytes larger than its 277536-byte span.
        u8 maCameraStorage[0x43C20];                                  // +0x425E0

        // NetworkTextureDXTCompress plus the unattributed bytes up to +0x86A80. Its header
        // documents at least 0x840 console bytes but reproduces only 0x700 in a 32-bit build
        // (the embedded job objects do not match their console width).
        u8 maTextureCompressorStorage[0x880];                         // +0x86200

        CgsID                 mFreeBurnCarID;                         // +0x86A80
        CgsID                 mFreeBurnWheelID;                       // +0x86A88
        f32                   mfField86A90;      // +0x86A90 ratio the matchmaking actions test against 0.85
        BrnWorld::EDistrict   meCurrentDistrict;                      // +0x86A94 (constructed as 11)
        EPrepareStage         mePrepareStage;                         // +0x86A98
        EReleaseStage         meReleaseStage;                         // +0x86A9C
        BrnNetworkModule*     mpNetworkModule;                        // +0x86AA0
        u8                    maUnattributed86AA4[12];                // +0x86AA4

        // BrnNetworkPlayer: the committed header models 3264 of 8768 bytes (grow by 5504);
        // stored as the console-stride array.
        u8 maNetworkPlayerStorage[7][0x2240];                         // +0x86AB0

        // PlayerMenuData (0x60 each): its header reproduces the stride, but it pulls in
        // BrnCameraStatusMessage.h, whose BrnNetwork::ECameraStatus is defined again in
        // BrnNetworkModuleInGamePlayerStatusInterface.h; every translation unit that also sees
        // the module IO header then fails. Pinned until the duplicate enum is removed.
        u8 maNetworkPlayerMenuDataStorage[7][0x60];                   // +0x95A70
        u8 mLocalPlayerMenuSelectionsStorage[0x60];                   // +0x95D10
        NetworkPlayerID       mPlayerIDStatsGet;                      // +0x95D70
        CgsSystem::Time       mLastStatsSentToOnlinePlayStamp;        // +0x95D74
        CgsNetwork::PlayerName mExportPlayerName;                     // +0x95D7C
        CgsSystem::TimerStatus* mpGameTimerStatus;                    // +0x95D8C
        CgsSystem::Time       mTime;                                  // +0x95D90
        f32                   mfTimeStep;                             // +0x95D98
        s32                   miFrameNum;                             // +0x95D9C
        CgsSystem::EFrameRate meLocalConsoleFrameRate;                // +0x95DA0
        s32                   miRoundNumber;                          // +0x95DA4
        u16                   mu16CarColourIndex;                     // +0x95DA8
        u16                   mu16PaintFinishIndex;                   // +0x95DAA
        bool                  mbHasFever;                             // +0x95DAC
        bool                  mbIsDeveloper;                          // +0x95DAD
        bool                  mbDiskReadErrorThisFrame;               // +0x95DAE
        bool                  mbNotifyGameOfDisconnectAfterDiskError; // +0x95DAF

        // Per-subsystem CPU performance-monitor handles (AddMonitor results).
        s32 miPackTextureToSendToGuiPM;                               // +0x95DB0
        s32 miCameraUpdatePM;                                         // +0x95DB4
        s32 miGamerPicUpdatPM;                                        // +0x95DB8
        s32 miCGSNetworkAfterSimPM;                                   // +0x95DBC
        s32 miNetworkManagerSelectedRoutesPM;                         // +0x95DC0
        s32 miNetworkManagerOutputPlayerStatsPM;                      // +0x95DC4
        s32 miNetworkManagerPostUpdatePM;                             // +0x95DC8
        s32 miNetworkManagerMiscPM;                                   // +0x95DCC
        s32 miNetworkServerInterfacePM;                               // +0x95DD0
        s32 miNetworkBuddyPM;                                         // +0x95DD4
        s32 miNetworkTrafficPM;                                       // +0x95DD8
        s32 miNetworkAggressiveDrivingPM;                             // +0x95DDC
        s32 miNetworkDirtyTrickPM;                                    // +0x95DE0
        s32 miNetworkHostStatusPM;                                    // +0x95DE4
        s32 miNetworkNotificationPM;                                  // +0x95DE8
        s32 miNetworkInvitePM;                                        // +0x95DEC
        s32 miNetworkStatsPM;                                         // +0x95DF0
        s32 miNetworkLiveRevengePM;                                   // +0x95DF4
        s32 miNetworkScoreboardPM;                                    // +0x95DF8
        s32 miNetworkRoutesManagerPM;                                 // +0x95DFC
        s32 miNetworkStateManagerPM;                                  // +0x95E00
        s32 miNetworkRoadRulesManagerPM;                              // +0x95E04
        s32 miNetworkChallengeManagerPM;                              // +0x95E08
    };

    inline void BrnNetworkManager::_AssertLayout()
    {
#define BRN_NM_AT(member, off) \
        static_assert(sizeof(void*) != 4 || offsetof(BrnNetworkManager, member) == off, #member " @ " #off)
        BRN_NM_AT(mServerInterface,               0x038E8);
        BRN_NM_AT(mLoginManager,                  0x04DB4);
        BRN_NM_AT(mLaunchManager,                 0x04DDC);
        BRN_NM_AT(mConnectionManager,             0x04E18);
        BRN_NM_AT(maMatchMakingManagerStorage,    0x04E24);
        BRN_NM_AT(mSuspensionManager,             0x06438);
        BRN_NM_AT(maPostRoundManagerStorage,      0x0644C);
        BRN_NM_AT(maStandingsManagerStorage,      0x065F0);
        BRN_NM_AT(maTrafficManagerStorage,        0x06B00);
        BRN_NM_AT(maBuddyManagerStorage,          0x1EFC0);
        BRN_NM_AT(maStatsManagerStorage,          0x33F18);
        BRN_NM_AT(mScoreboardManager,             0x356A0);
        BRN_NM_AT(maAggressiveDrivingManagerStorage, 0x37070);
        BRN_NM_AT(maLiveRevengeManagerStorage,    0x39FD0);
        BRN_NM_AT(maDirtyTrickManagerStorage,     0x3AA78);
        BRN_NM_AT(maImageManagerStorage,          0x3AD78);
        BRN_NM_AT(mGamerPictureManager,           0x3CF50);
        BRN_NM_AT(mNetworkNotificationManager,    0x3D108);
        BRN_NM_AT(mNetworkInviteManager,          0x3D118);
        BRN_NM_AT(maSelectedRoutesManagerStorage, 0x3D2D8);
        BRN_NM_AT(maMarkedManManagerStorage,      0x3DA88);
        BRN_NM_AT(maStateManagerStorage,          0x3DD50);
        BRN_NM_AT(mRoadRulesManager,              0x3DE48);
        BRN_NM_AT(maChallengeSuccessManagerStorage, 0x414A8);
        BRN_NM_AT(mGamerCardManager,              0x41AB0);
        BRN_NM_AT(maEventScoresManagerStorage,    0x41B50);
        BRN_NM_AT(mAutoLoginManager,              0x41E90);
        BRN_NM_AT(mTeamSelectionManager,          0x41EB8);
        BRN_NM_AT(mNetworkServers,                0x425CC);
        BRN_NM_AT(maCameraStorage,                0x425E0);
        BRN_NM_AT(maTextureCompressorStorage,     0x86200);
        BRN_NM_AT(mFreeBurnCarID,                 0x86A80);
        BRN_NM_AT(mfField86A90,                   0x86A90);
        BRN_NM_AT(meCurrentDistrict,              0x86A94);
        BRN_NM_AT(mePrepareStage,                 0x86A98);
        BRN_NM_AT(meReleaseStage,                 0x86A9C);
        BRN_NM_AT(mpNetworkModule,                0x86AA0);
        BRN_NM_AT(maNetworkPlayerStorage,         0x86AB0);
        BRN_NM_AT(maNetworkPlayerMenuDataStorage, 0x95A70);
        BRN_NM_AT(mLocalPlayerMenuSelectionsStorage, 0x95D10);
        BRN_NM_AT(mPlayerIDStatsGet,              0x95D70);
        BRN_NM_AT(mLastStatsSentToOnlinePlayStamp, 0x95D74);
        BRN_NM_AT(mExportPlayerName,              0x95D7C);
        BRN_NM_AT(mpGameTimerStatus,              0x95D8C);
        BRN_NM_AT(mTime,                          0x95D90);
        BRN_NM_AT(mfTimeStep,                     0x95D98);
        BRN_NM_AT(miFrameNum,                     0x95D9C);
        BRN_NM_AT(meLocalConsoleFrameRate,        0x95DA0);
        BRN_NM_AT(miRoundNumber,                  0x95DA4);
        BRN_NM_AT(mu16CarColourIndex,             0x95DA8);
        BRN_NM_AT(mbHasFever,                     0x95DAC);
        BRN_NM_AT(mbNotifyGameOfDisconnectAfterDiskError, 0x95DAF);
        BRN_NM_AT(miPackTextureToSendToGuiPM,     0x95DB0);
        BRN_NM_AT(miNetworkHostStatusPM,          0x95DE4);
        BRN_NM_AT(miNetworkChallengeManagerPM,    0x95E08);
#undef BRN_NM_AT
    }
}
