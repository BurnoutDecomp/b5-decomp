#pragma once

#include <cstddef>                                                          // offsetof (_AssertLayout)

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"                               // BrnUpdateSet
#include "SharedClasses/World/BrnWorldRegion.h"                             // BrnWorld::EDistrict
#include "GameSource/CompilerDefines/gameshared_network_defines.h"          // KI_MAX_NETWORK_PLAYERS
#include "GameShared/GameClasses/Core/CgsID.h"                              // CgsID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                    // CgsSystem::Time
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"
#include "GameShared/GameClasses/Network/CgsNetworkManager.h"               // CgsNetwork::NetworkManager (base)
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"        // CgsSystem::EFrameRate
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"       // mTextureCompressor
#include "GameSource/GameState/BrnCgsPlayerName.h"                          // CgsNetwork::PlayerName
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                 // NetworkPlayerID, ETelemetryHook, EActiveRaceCarIndex
#include "GameSource/Network/BrnServerInterface.h"                          // mServerInterface
#include "GameSource/Network/BrnNetworkServers.h"                           // mNetworkServers
#include "GameSource/Network/BrnNetworkPlayer.h"                            // maNetworkPlayer
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                    // maNetworkPlayerMenuData, mLocalPlayerMenuSelections
#include "GameSource/Network/Managers/X360/BrnNetworkLoginManagerX360.h"    // mLoginManager
#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"            // mLaunchManager
#include "GameSource/Network/Managers/BrnNetworkConnectionManager.h"        // mConnectionManager
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"       // mMatchMakingManager
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"        // mSuspensionManager
#include "GameSource/Network/Managers/BrnNetworkPostRoundManager.h"         // mPostRoundManager
#include "GameSource/Network/Managers/BrnNetworkStandingsManager.h"         // mStandingsManager
#include "GameSource/Network/Managers/BrnNetworkTrafficManager.h"           // mTrafficManager
#include "GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.h"    // mBuddyManager
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"       // mStatsManager
#include "GameSource/Network/Managers/BrnNetworkScoreboardManager.h"        // mScoreboardManager
#include "GameSource/Network/Managers/BrnNetworkAggressiveDrivingManager.h" // mAggressiveDrivingManager
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"       // mLiveRevengeManager
#include "GameSource/Network/Managers/BrnNetworkDirtyTrickManager.h"        // mDirtyTrickManager
#include "GameSource/Network/Managers/BrnNetworkImageManager.h"             // mImageManager
#include "GameSource/Network/Managers/X360/BrnNetworkGamerPictureManagerX360.h"  // mGamerPictureManager
#include "GameSource/Network/Managers/X360/BrnNetworkNotificationManagerX360.h"  // mNetworkNotificationManager
#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"            // mNetworkInviteManager
#include "GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h"    // mSelectedRoutesManager
#include "GameSource/Network/Managers/BrnNetworkMarkedManManager.h"         // mMarkedManManager
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"             // mStateManager
#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"         // mRoadRulesManager
#include "GameSource/Network/Managers/BrnChallengeSuccessManager.h"         // mChallengeSuccessManager
#include "GameSource/Network/Managers/X360/BrnNetworkGamerCardManagerX360.h"     // mGamerCardManager
#include "GameSource/Network/Managers/BrnEventScoresManager.h"              // mEventScoresManager
#include "GameSource/Network/Managers/BrnNetworkAutoLoginManager.h"         // mAutoLoginManager
#include "GameSource/Network/Managers/BrnNetworkTeamSelectionManager.h"     // mTeamSelectionManager
#include "GameSource/Network/Managers/X360/BrnNetworkCameraX360.h"          // mCamera

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- the online hub embedded in BrnNetworkModule at +0x280.
//
// LAYOUT
// ------
// CgsNetwork::NetworkManager is the base (+0x0000 .. +0x38E8); every game-side manager follows
// as a typed member at the offset the constructor, Construct, Prepare, Release, Destruct and
// the per-frame updates address it at (console byte offsets in the comments).
//
// The texture compressor is 128-byte aligned, so the whole object is: the module places it at
// +0x280 and its own members resume at +0x96100, i.e. the console sizeof is 0x95E80 (the last
// member ends at +0x95E0C).
//
// The server interface is the one sub-object whose committed hierarchy is short of its console
// span; a byte reserve after it keeps the following offsets.
//
// BrnNetworkPlayer.h must not include this header back (it reaches it through the message
// headers it embeds), or the two classes cannot both be complete.
//
// Host layout: pointers widen on the x64 host, so the host offsets differ from the console
// ones; code must reach members by name only. _AssertLayout() pins every console offset in a
// 32-bit build and is inert on x64.
// ============================================================================================

namespace CgsNetwork
{
    class NetworkTexture;                // pointer-only (PackTextureAndSendDisplayEventToGui)
}

namespace CgsSystem
{
    class TimerStatus;                   // pointer-only (mpGameTimerStatus)
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
    class BrnNetworkManager : public CgsNetwork::NetworkManager
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

        // ---- telemetry: a text payload, or an integer printed as decimal text ---------------
        void CaptureTelemetryEvent(ETelemetryHook leHook, const char* lpcData);
        void CaptureTelemetryEvent(ETelemetryHook leHook, s32 liData);

        // ---- sub-object accessors ---------------------------------------------------------
        BrnServerInterface*                    GetServerInterface()                 { return &mServerInterface; }
        const BrnServerInterface*              GetServerInterface() const           { return &mServerInterface; }
        LoginManagerX360*                      GetLoginManager()                    { return &mLoginManager; }
        const LoginManagerX360*                GetLoginManager() const              { return &mLoginManager; }
        LaunchManager*                         GetLaunchManager()                   { return &mLaunchManager; }
        const LaunchManager*                   GetLaunchManager() const             { return &mLaunchManager; }
        ConnectionManager*                     GetConnectionManager()               { return &mConnectionManager; }
        const ConnectionManager*               GetConnectionManager() const         { return &mConnectionManager; }
        MatchMakingManager*                    GetMatchMakingManager()              { return &mMatchMakingManager; }
        const MatchMakingManager*              GetMatchMakingManager() const        { return &mMatchMakingManager; }
        SuspensionManager*                     GetSuspensionManager()               { return &mSuspensionManager; }
        const SuspensionManager*               GetSuspensionManager() const         { return &mSuspensionManager; }
        PostRoundManager*                      GetPostRoundManager()                { return &mPostRoundManager; }
        const PostRoundManager*                GetPostRoundManager() const          { return &mPostRoundManager; }
        StandingsManager*                      GetStandingsManager()                { return &mStandingsManager; }
        const StandingsManager*                GetStandingsManager() const          { return &mStandingsManager; }
        TrafficManager*                        GetTrafficManager()                  { return &mTrafficManager; }
        const TrafficManager*                  GetTrafficManager() const            { return &mTrafficManager; }
        BuddyManagerX360*                      GetBuddyManager()                    { return &mBuddyManager; }
        const BuddyManagerX360*                GetBuddyManager() const              { return &mBuddyManager; }
        NetworkPlayerStatsManager*             GetStatsManager()                    { return &mStatsManager; }
        const NetworkPlayerStatsManager*       GetStatsManager() const              { return &mStatsManager; }
        ScoreboardManager*                     GetScoreboardManager()               { return &mScoreboardManager; }
        const ScoreboardManager*               GetScoreboardManager() const         { return &mScoreboardManager; }
        NetworkAggressiveDrivingManager*       GetAggressiveDrivingManager()        { return &mAggressiveDrivingManager; }
        const NetworkAggressiveDrivingManager* GetAggressiveDrivingManager() const  { return &mAggressiveDrivingManager; }
        LiveRevengeManager*                    GetLiveRevengeManager()              { return &mLiveRevengeManager; }
        const LiveRevengeManager*              GetLiveRevengeManager() const        { return &mLiveRevengeManager; }
        NetworkDirtyTrickManager*              GetDirtyTrickManager()               { return &mDirtyTrickManager; }
        const NetworkDirtyTrickManager*        GetDirtyTrickManager() const         { return &mDirtyTrickManager; }
        NetworkImageManager*                   GetNetworkImageManager()             { return &mImageManager; }
        const NetworkImageManager*             GetNetworkImageManager() const       { return &mImageManager; }
        GamerPictureManagerX360*               GetGamerPictureManager()             { return &mGamerPictureManager; }
        const GamerPictureManagerX360*         GetGamerPictureManager() const       { return &mGamerPictureManager; }
        NetworkNotificationManagerX360*        GetNetworkNotificationManager()      { return &mNetworkNotificationManager; }
        const NetworkNotificationManagerX360*  GetNetworkNotificationManager() const { return &mNetworkNotificationManager; }
        NetworkNotificationManagerX360*        GetNotificationManager()             { return &mNetworkNotificationManager; }
        NetworkInviteManager*                  GetNetworkInviteManager()            { return &mNetworkInviteManager; }
        const NetworkInviteManager*            GetNetworkInviteManager() const      { return &mNetworkInviteManager; }
        SelectedRoutesManager*                 GetSelectedRoutesManager()           { return &mSelectedRoutesManager; }
        const SelectedRoutesManager*           GetSelectedRoutesManager() const     { return &mSelectedRoutesManager; }
        MarkedManManager*                      GetMarkedManManager()                { return &mMarkedManManager; }
        const MarkedManManager*                GetMarkedManManager() const          { return &mMarkedManManager; }
        StateManager*                          GetStateManager()                    { return &mStateManager; }
        const StateManager*                    GetStateManager() const              { return &mStateManager; }
        NetworkRoadRulesManager*               GetRoadRulesManager()                { return &mRoadRulesManager; }
        const NetworkRoadRulesManager*         GetRoadRulesManager() const          { return &mRoadRulesManager; }
        ChallengeSuccessManager*               GetChallengeSuccessManager()         { return &mChallengeSuccessManager; }
        const ChallengeSuccessManager*         GetChallengeSuccessManager() const   { return &mChallengeSuccessManager; }
        NetworkGamerCardManagerX360*           GetGamerCardManager()                { return &mGamerCardManager; }
        const NetworkGamerCardManagerX360*     GetGamerCardManager() const          { return &mGamerCardManager; }
        EventScoresManager*                    GetEventScoresManager()              { return &mEventScoresManager; }
        const EventScoresManager*              GetEventScoresManager() const        { return &mEventScoresManager; }
        AutoLoginManager*                      GetAutoLoginManager()                { return &mAutoLoginManager; }
        const AutoLoginManager*                GetAutoLoginManager() const          { return &mAutoLoginManager; }
        TeamSelectionManager*                  GetTeamSelectionManager()            { return &mTeamSelectionManager; }
        const TeamSelectionManager*            GetTeamSelectionManager() const      { return &mTeamSelectionManager; }
        NetworkServers*                        GetNetworkServers()                  { return &mNetworkServers; }
        const NetworkServers*                  GetNetworkServers() const            { return &mNetworkServers; }
        CameraX360*                            GetCamera()                          { return &mCamera; }
        const CameraX360*                      GetCamera() const                    { return &mCamera; }
        CgsNetwork::NetworkTextureDXTCompress*       GetTextureCompressor()         { return &mTextureCompressor; }
        const CgsNetwork::NetworkTextureDXTCompress* GetTextureCompressor() const   { return &mTextureCompressor; }
        PlayerMenuData*                        GetPlayerMenuData(s32 liIndex)       { return &maNetworkPlayerMenuData[liIndex]; }
        const PlayerMenuData*                  GetPlayerMenuData(s32 liIndex) const { return &maNetworkPlayerMenuData[liIndex]; }
        PlayerMenuData*                        GetLocalPlayerMenuSelections()       { return &mLocalPlayerMenuSelections; }
        const PlayerMenuData*                  GetLocalPlayerMenuSelections() const { return &mLocalPlayerMenuSelections; }
        BrnNetworkModule*                      GetNetworkModule()                   { return mpNetworkModule; }
        const BrnNetworkModule*                GetNetworkModule() const             { return mpNetworkModule; }
        BrnNetworkPlayer*                      GetNetworkPlayer(s32 liIndex)        { return &maNetworkPlayer[liIndex]; }
        const BrnNetworkPlayer*                GetNetworkPlayer(s32 liIndex) const  { return &maNetworkPlayer[liIndex]; }
        CgsSystem::TimerStatus*                GetTimerStatus()                     { return mpGameTimerStatus; }
        CgsSystem::Time                        GetTime() const                      { return mTime; }
        f32                                    GetTimeStep() const                  { return mfTimeStep; }

        // ---- session scalars ------------------------------------------------------------
        void                SetFreeBurnCar(CgsID lCarID, CgsID lWheelID, f32 lfDeformation)
        {
            mFreeBurnCarID   = lCarID;
            mFreeBurnWheelID = lWheelID;
            mfField86A90     = lfDeformation;
        }
        CgsID               GetFreeBurnCarID() const                            { return mFreeBurnCarID; }
        // The free-burn car's deformation amount (the matchmaking actions' lobby "car deformed"
        // flag). FLAG: accessor name is ours.
        f32                 GetFreeBurnCarDeformation() const                   { return mfField86A90; }
        CgsID               GetFreeBurnWheelID() const                          { return mFreeBurnWheelID; }
        void                SetCurrentDistrict(BrnWorld::EDistrict leDistrict)  { meCurrentDistrict = leDistrict; }
        BrnWorld::EDistrict GetCurrentDistrict() const                          { return meCurrentDistrict; }
        void                SetCurrentCarColourIndex(u16 lu16Index)             { mu16CarColourIndex = lu16Index; }
        u16                 GetCurrentCarColourIndex() const                    { return mu16CarColourIndex; }
        void                SetCurrentPaintFinishIndex(u16 lu16Index)           { mu16PaintFinishIndex = lu16Index; }
        u16                 GetCurrentPaintFinishIndex() const                  { return mu16PaintFinishIndex; }
        bool                HasFever()                                          { return mbHasFever; }
        void                SetHasFever(bool lbHasFever)                        { mbHasFever = lbHasFever; }
        bool                IsDeveloper()                                       { return mbIsDeveloper; }
        void                SetIsDeveloper(bool lbIsDeveloper)                  { mbIsDeveloper = lbIsDeveloper; }
        void                SetRound(s32 liRoundNumber)                         { miRoundNumber = liRoundNumber; }
        s32                 GetRound() const                                    { return miRoundNumber; }

        // ---- accessors into the CgsNetwork::NetworkManager base --------------------------
        CgsNetwork::PlayersConnectionManager*  GetPlayersConnectionManager()        { return &GetPlayerManager()->mConnectionManager; }
        s32                                    GetLocalUserControllerPort() const   { return GetActiveControllerPort(); }
        u32                                    GetCurrentFrame() const              { return GetTimeManager()->GetFrameCount(); }

    private:
        s32  GetMaxMessageSize(bool lbReliableOnly);
        void OutputPlayerStatusInfo(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void OutputPlayerResultsInfo(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void UpdateMenuDataFromPlayerParams(s32 liPlayerIndex);
        void ProcessHLUpdateFlags(BrnUpdateSet luUpdateSet);

        // Callbacks registered by address; the user-data word is the manager.
        static void PlayerManagerEventCallback(CgsNetwork::PlayerManager::EEvent leEvent,
                                               void* lpEventData, void* lpUserData);
        static void SyncTimeClientReadyCallback(NetworkPlayerID lClientReadyID, void* lpUserData);
        static void DxtDecodeCallback(void* lpPixels, void* lpUserData);

        // Console offsets are pinned in a 32-bit build; inert on the x64 host.
        static void _AssertLayout();

        // ---- +0x038E8 : server interface. The committed hierarchy is 1164 bytes short of
        //      its 0x14CC console span; the reserve keeps the following offsets. Shrink it as
        //      the server-interface base and platform layers grow. ---------------------------
        BrnServerInterface              mServerInterface;             // +0x038E8
        u8                              maServerInterfaceReserve[1164];

        LoginManagerX360                mLoginManager;                // +0x04DB4
        LaunchManager                   mLaunchManager;               // +0x04DDC
        ConnectionManager               mConnectionManager;           // +0x04E18
        MatchMakingManager              mMatchMakingManager;          // +0x04E24
        SuspensionManager               mSuspensionManager;           // +0x06438
        PostRoundManager                mPostRoundManager;            // +0x0644C
        StandingsManager                mStandingsManager;            // +0x065F0
        TrafficManager                  mTrafficManager;              // +0x06B00
        BuddyManagerX360                mBuddyManager;                // +0x1EFC0
        NetworkPlayerStatsManager       mStatsManager;                // +0x33F18
        ScoreboardManager               mScoreboardManager;           // +0x356A0
        NetworkAggressiveDrivingManager mAggressiveDrivingManager;    // +0x37070
        LiveRevengeManager              mLiveRevengeManager;          // +0x39FD0
        NetworkDirtyTrickManager        mDirtyTrickManager;           // +0x3AA78
        NetworkImageManager             mImageManager;                // +0x3AD78
        GamerPictureManagerX360         mGamerPictureManager;         // +0x3CF50
        NetworkNotificationManagerX360  mNetworkNotificationManager;  // +0x3D108
        NetworkInviteManager            mNetworkInviteManager;        // +0x3D118
        SelectedRoutesManager           mSelectedRoutesManager;       // +0x3D2D8
        MarkedManManager                mMarkedManManager;            // +0x3DA88
        StateManager                    mStateManager;                // +0x3DD50
        NetworkRoadRulesManager         mRoadRulesManager;            // +0x3DE48
        ChallengeSuccessManager         mChallengeSuccessManager;     // +0x414A8
        NetworkGamerCardManagerX360     mGamerCardManager;            // +0x41AB0
        EventScoresManager              mEventScoresManager;          // +0x41B50
        AutoLoginManager                mAutoLoginManager;            // +0x41E90
        TeamSelectionManager            mTeamSelectionManager;        // +0x41EB8
        NetworkServers                  mNetworkServers;              // +0x425CC
        CameraX360                      mCamera;                      // +0x425E0
        CgsNetwork::NetworkTextureDXTCompress mTextureCompressor;     // +0x86200 (128-aligned)

        CgsID                 mFreeBurnCarID;                         // +0x86A80
        CgsID                 mFreeBurnWheelID;                       // +0x86A88
        f32                   mfField86A90;      // +0x86A90 the free-burn car's deformation amount (SetFreeBurnCar); the matchmaking actions test it against 0.85
        BrnWorld::EDistrict   meCurrentDistrict;                      // +0x86A94 (constructed as 11)
        EPrepareStage         mePrepareStage;                         // +0x86A98
        EReleaseStage         meReleaseStage;                         // +0x86A9C
        BrnNetworkModule*     mpNetworkModule;                        // +0x86AA0

        BrnNetworkPlayer      maNetworkPlayer[KI_MAX_NETWORK_PLAYERS];          // +0x86AB0 (0x2240 each, 16-aligned)

        PlayerMenuData        maNetworkPlayerMenuData[KI_MAX_NETWORK_PLAYERS];  // +0x95A70 (0x60 each)
        PlayerMenuData        mLocalPlayerMenuSelections;             // +0x95D10
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
        BRN_NM_AT(mMatchMakingManager,            0x04E24);
        BRN_NM_AT(mSuspensionManager,             0x06438);
        BRN_NM_AT(mPostRoundManager,              0x0644C);
        BRN_NM_AT(mStandingsManager,              0x065F0);
        BRN_NM_AT(mTrafficManager,                0x06B00);
        BRN_NM_AT(mBuddyManager,                  0x1EFC0);
        BRN_NM_AT(mStatsManager,                  0x33F18);
        BRN_NM_AT(mScoreboardManager,             0x356A0);
        BRN_NM_AT(mAggressiveDrivingManager,      0x37070);
        BRN_NM_AT(mLiveRevengeManager,            0x39FD0);
        BRN_NM_AT(mDirtyTrickManager,             0x3AA78);
        BRN_NM_AT(mImageManager,                  0x3AD78);
        BRN_NM_AT(mGamerPictureManager,           0x3CF50);
        BRN_NM_AT(mNetworkNotificationManager,    0x3D108);
        BRN_NM_AT(mNetworkInviteManager,          0x3D118);
        BRN_NM_AT(mSelectedRoutesManager,         0x3D2D8);
        BRN_NM_AT(mMarkedManManager,              0x3DA88);
        BRN_NM_AT(mStateManager,                  0x3DD50);
        BRN_NM_AT(mRoadRulesManager,              0x3DE48);
        BRN_NM_AT(mChallengeSuccessManager,       0x414A8);
        BRN_NM_AT(mGamerCardManager,              0x41AB0);
        BRN_NM_AT(mEventScoresManager,            0x41B50);
        BRN_NM_AT(mAutoLoginManager,              0x41E90);
        BRN_NM_AT(mTeamSelectionManager,          0x41EB8);
        BRN_NM_AT(mNetworkServers,                0x425CC);
        BRN_NM_AT(mCamera,                        0x425E0);
        BRN_NM_AT(mTextureCompressor,             0x86200);
        BRN_NM_AT(mFreeBurnCarID,                 0x86A80);
        BRN_NM_AT(mfField86A90,                   0x86A90);
        BRN_NM_AT(meCurrentDistrict,              0x86A94);
        BRN_NM_AT(mePrepareStage,                 0x86A98);
        BRN_NM_AT(meReleaseStage,                 0x86A9C);
        BRN_NM_AT(mpNetworkModule,                0x86AA0);
        BRN_NM_AT(maNetworkPlayer,                0x86AB0);
        BRN_NM_AT(maNetworkPlayerMenuData,        0x95A70);
        BRN_NM_AT(mLocalPlayerMenuSelections,     0x95D10);
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
        static_assert(sizeof(void*) != 4 || alignof(BrnNetworkManager) == 128, "BrnNetworkManager console alignment");
        static_assert(sizeof(void*) != 4 || sizeof(BrnNetworkManager) == 0x95E80, "BrnNetworkManager console size");
    }
}
