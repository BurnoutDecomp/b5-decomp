#pragma once

// ===================================================================================
// BrnNetwork::StateManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkStateManager.{h,cpp}
//
// The network session state machine. BrnNetworkManager embeds one by value and drives it
// every frame: UpdateEvents drains the gui-event, game-action and network-event queues,
// then Update ticks the per-state work (login, launching, time sync, matchmaking,
// suspension, post round, car select, news/TOS download, ...). The lifecycle is the usual
// Construct / Prepare / Release / Destruct quartet.
//
// Console layout (32-bit), every offset proven by the Construct / Prepare / Release /
// Destruct stores and by the readers named on each member below:
//   +0x00 (0x80) mCurrentPlayerXUIDs            CurrentPlayerXUIDs (8 x 16-byte slots)
//   +0x80  mpNetworkModule                        BrnNetworkModule*
//   +0x84  mpNetworkManager                       BrnNetworkManager*
//   +0x88  mpTOS                                  CgsUtf8*  (downloaded terms of service)
//   +0x8C  mpNews                                 CgsUtf8*  (downloaded news)
//   +0x90  meState                                EState
//   +0x94  meOutputPlayerTexture                  GuiEventNetworkOutputPlayerTexture::EOutput
//   +0x98  mNetworkPlayerIDToOutput               NetworkPlayerID
//   +0xA0  mNewCarData        (0x18)              CachedNewCarData
//   +0xB8  mMarkedManData     (0x08)              CachedMarkedManData
//   +0xC0  mDistrictData      (0x0C)              CachedDistrictData
//   +0xCC  mCarColourData     (0x0C)              CachedCarColourData
//   +0xD8  mFeverData         (0x02)              CachedFeverData
//   +0xDC  meLeftReason                           ELeftGameReason
//   +0xE0  meKickReason                           CgsNetwork::EKickReason
//   +0xE4 .. +0xF2  fifteen bool flags (declaration order below)
//   sizeof == 0xF8 (8-aligned by the CgsID members)
// BrnNetworkManager holds this object at +0x3DD50 (console); the next member,
// NetworkRoadRulesManager, starts at +0x3DE48.
//
// On the x64 host the four pointers widen, so only the pointer-free prefix is pinned
// absolutely and the pointer-free run from meState onward is pinned relative to meState
// (see _AssertLayout). Members are always reached by name.
//
// EState: the console build carries one more state than the reference declaration, a
// team-selection wait inserted at 11 (UpdateLaunching enters it after starting team
// selection; TeamSelectionFinishedCallback leaves it for E_STATE_WAIT_CAR_SELECT), so every
// later value is one higher. E_STATE_COUNT (23) doubles as the idle / limbo state: Construct,
// Prepare, Release and Destruct all store it, Update has a dedicated arm for it, and the
// auto-login flow tests meState against it (IsIdle).
// ===================================================================================

#include <cstddef>                                                                  // offsetof (_AssertLayout)
#include "types.hpp"
#include "BrnCommonTypes.h"                                                         // CgsID
#include "SharedClasses/World/BrnWorldRegion.h"                                     // BrnWorld::EDistrict
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // CgsNetwork::EKickReason
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"                                // CgsUnicode::CgsUtf8
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::NetworkPlayerID
#include "GameSource/Network/Managers/BrnNetworkLoginManagerBase.h"                 // LoginManagerBase::ESignInType

// Pointer-only forwards. Each home header is large (or includes this one), so the
// declarations below take these by pointer without pulling the cascade in.
namespace CgsModule
{
    template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue;   // home: CgsVariableEventQueue.h
}
namespace CgsSystem
{
    class TimerStatus;                                             // HostChangedCallback param
}
namespace BrnGui
{
    struct GuiEventNetworkConnect;                                 // HandleConnectEvent param
    struct GuiEventNetworkCreateGame;                              // CreateGame param
    struct GuiEventNetworkGameParams;                              // ModifyGame param
}
namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        struct StopModeIntroAction;                                // IntroStopped param
        struct MarkedManLoadedAction;                              // MarkedManLoaded param (not yet homed)
    }
}

namespace BrnNetwork
{
    class BrnNetworkModule;
    class BrnNetworkManager;

    namespace BrnNetworkModuleIO
    {
        struct PostSimulationInputBuffer;
        class  NetworkEventQueue;   // class-key follows BrnNetworkModuleIO.h (the reference declares a struct)
    }

    struct StateManager
    {
    public:
        // BrnNetworkStateManager.h reference enum, re-numbered to the console build (see banner).
        enum EState
        {
            E_STATE_LOGIN                             = 0,
            E_STATE_LAUNCHING                         = 1,
            E_STATE_WAIT_GAME_MODE_START              = 2,
            E_STATE_SYNC_TIME                         = 3,
            E_STATE_RACING                            = 4,
            E_STATE_WAIT_MATCHMAKING                  = 5,
            E_STATE_WAIT_SUSPENSION_IDLE_TO_SUSPEND   = 6,
            E_STATE_WAIT_SUSPENSION_IDLE_LEAVING_GAME = 7,
            E_STATE_WAIT_SUSPENSION                   = 8,
            E_STATE_WAIT_SUSPENSION_LEAVING_GAME      = 9,
            E_STATE_WAIT_POST_ROUND                   = 10,
            // FLAG: name not attested (the reference enum predates this state); role from
            // UpdateLaunching / TeamSelectionFinishedCallback / Update's no-op arm.
            E_STATE_WAIT_TEAM_SELECTION               = 11,
            E_STATE_WAIT_CAR_SELECT                   = 12,
            E_STATE_WAIT_MODIFY_GAME                  = 13,
            E_STATE_WAIT_DOWNLOAD_NEWS_IDLE           = 14,
            E_STATE_WAIT_DOWNLOAD_TOS_IDLE            = 15,
            E_STATE_WAIT_DOWNLOAD_NEWS                = 16,
            E_STATE_WAIT_DOWNLOAD_TOS                 = 17,
            E_STATE_WAIT_SERVER_INTERFACE_ACTION      = 18,
            E_STATE_PREPARING_FOR_INVITE              = 19,
            E_STATE_PREPARED_FOR_INVITE               = 20,
            E_STATE_WAIT_FOR_LOADING_SCREEN           = 21,
            E_STATE_WAIT_UPDATE_ACCOUNT               = 22,
            E_STATE_COUNT                             = 23    // also the idle state
        };

        // Cached car choice, pushed to the server as player parameters while idle in a
        // lobby. Filled from a reset-player-car game action.
        struct CachedNewCarData
        {
            CgsID mCarModelId;        // +0x00
            CgsID mWheelModelId;      // +0x08
            // FLAG: member absent from the reference declaration; it caches the action's
            // deform amount (Update sets a player-params flag when it is >= 0.85).
            f32   mfDeformAmount;     // +0x10
            bool  mbValid;            // +0x14
        };

        struct CachedFeverData
        {
            bool mbHasFever;          // +0x00
            bool mbValid;             // +0x01
        };

        struct CachedMarkedManData
        {
            NetworkPlayerID mPlayerID;   // +0x00
            bool            mbValid;     // +0x04
        };

        struct CachedDistrictData
        {
            static const f32 KF_DISTRICT_UPDATE_INTERVAL;   // defined with the bodies (rodata)

            BrnWorld::EDistrict meDistrict;       // +0x00
            f32                 mfLastSentTime;   // +0x04
            bool                mbValid;          // +0x08
        };

        struct CachedCarColourData
        {
            static const f32 KF_CAR_COLOUR_UPDATE_INTERVAL; // defined with the bodies (rodata)

            f32  mfLastSentTime;         // +0x00
            u16  mu16CarColourIndex;     // +0x04
            u16  mu16PaintFinishIndex;   // +0x06
            bool mbValid;                // +0x08
        };

        // Fixed 8-slot network-player-id -> XUID table (console-only nested type). A slot is
        // free while its miPlayerId holds KI_FREE_SLOT; StartGameMode refills the table.
        struct CurrentPlayerXUIDs
        {
            static const s32 KI_NUM_SLOTS = 8;
            static const s32 KI_FREE_SLOT = -1;

            struct Slot
            {
                s32 miPlayerId;   // +0x00 (free == KI_FREE_SLOT)
                s32 miPad;        // +0x04 (XUID 8-byte alignment)
                u64 mu64XUID;     // +0x08
            };

            // Claim the first free slot for liPlayerId / lqXUID.
            void SetXUID(s32 liPlayerId, u64 lqXUID);
            // Find liPlayerId and write its XUID to *lpXUID.
            void GetXUID(s32 liPlayerId, u64* lpXUID);

            Slot maSlots[KI_NUM_SLOTS];   // 8 * 16 == 0x80 bytes
        };

        static char KAC_TOS_DATABASE_ID[];
        static char KAC_NEWS_DATABASE_ID[];

        // ---- lifecycle / per-frame -----------------------------------------------------
        void Construct(BrnNetworkModule* lpNetworkModule);
        bool Prepare();
        bool Release();
        void Destruct();
        void UpdateEvents(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);
        void Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);

        // ---- driven by BrnNetworkManager / the other managers ---------------------------
        void Disconnected();
        void PrepareForInvite();
        void IntroStopped(const BrnGameState::GameStateModuleIO::StopModeIntroAction* lpStopIntroAction);
        void MarkedManLoaded(const BrnGameState::GameStateModuleIO::MarkedManLoadedAction* lpMarkedManLoadedAction);
        void JoinGameSession(char* lpcSessionID);
        void UpdateMarkedMan(NetworkPlayerID lMarkedManID);
        void PlayerAdded();
        void PlayerRemoved();
        void OnGameIDChanged();
        // FLAG: first parameter is BrnNetwork::ELeftGameReason, which has no home yet
        // (requested for BrnNetworkSharedIO.h); carried as its s32 storage until then.
        void SuspendToLeaveGame(s32 leLeftReason, CgsNetwork::EKickReason leKickReason);
        bool GameModeHasEnoughTeams();
        bool IsInLimbo();
        void HandleConnectEvent(const BrnGui::GuiEventNetworkConnect* lpConnectEvent);

        // Inline accessors (no out-of-line console body; the callers inline them).
        // The auto-login flow reads meState and compares it with the idle value.
        bool IsIdle() const { return meState == E_STATE_COUNT; }
        // The buddy manager, once its retry timer elapses in limbo, stores true here.
        void CloseLimboGame() { mbCloseLimboGameWhenIdle = true; }

        // ---- LEGACY CALL-SITE SHIMS -- not console functions, no bodies ------------------
        // Kept only so the existing callers keep compiling until their owners move them
        // to the real API (requests filed):
        //   BrnNetworkAutoLoginManager.cpp: ConnectEvent + HandleConnectEvent(const ConnectEvent*)
        //     -> HandleConnectEvent(const BrnGui::GuiEventNetworkConnect*) (the payload word at
        //     +0 is a LoginManagerBase::ESignInType; the flow passes E_SIGN_IN_TYPE_SILENT),
        //     GetConnectionStatus() == 23 -> IsIdle().
        //   BrnNetworkBuddyManagerBase.cpp: SetRetryGetServerBuddies(true) -> CloseLimboGame().
        struct ConnectEvent
        {
            bool mbTriggerSignIn;
        };
        void HandleConnectEvent(const ConnectEvent* lpConnectEvent);
        s32  GetConnectionStatus() const;
        void SetRetryGetServerBuddies(bool lbRetry);

    private:
        // ---- event processing (UpdateEvents) -------------------------------------------
        // Queue types: the gui-event queue is VariableEventQueue<18432,16>, the game-action
        // queue VariableEventQueue<13312,16>.
        void ProcessGuiEvents(const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue);
        void ProcessGameStateActions(const CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue);
        void ProcessNetworkEvents(const BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue);

        // ---- session / game-mode helpers ------------------------------------------------
        void  CreateGame(const BrnGui::GuiEventNetworkCreateGame* lpGameParamsEvent, bool lbCreatedFromMenus);
        void  ModifyGame(const BrnGui::GuiEventNetworkGameParams* lpGameParamsEvent);
        void  PostRoundProcessingFinished();
        void  PostGameProcessingFinished();
        void  OutputPlayerTexture();
        void  StartGameMode();
        void  StartFreeBurnLobbyGameMode(bool lbStartingAfterJoin, bool lbStartingAfterOnlineEvent,
                                         bool lbForceStartFreeburnLobby, bool lbRefreshOnly);
        void  RefreshFreeBurnLobbyGameMode();
        bool  HaveAllSelectedACar();
        void  StartLogIn(LoginManagerBase::ESignInType leSignInType);
        void  UpdateLogin(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);
        void  UpdateLaunching();
        void  UpdateSyncTime();
        void  UpdateNewsAndTOSDownload();
        bool  GameModeHasTeams();
        void  OnFullConnection();
        void  StartNewsDownload();
        void  StartTOSDownload();
        void  ReleaseNewsAndTOSDownload();
        char* StripWebOfferControlCodes(char* lpcBuffer);
        void  SendStartNextRoundMessage(s32 liRoundIndex);
        void  CreateInviteGame();
        void  DoInstantFreeburn();

        // ---- completion callbacks (static; lpUserData is the StateManager) -------------
        static void CreateGameFinishedCallback(bool lbSuccess, void* lpUserData);
        static void JoinGameFinishedCallback(bool lbSuccess, void* lpUserData);
        static void LeaveGameForOfflineGameFinishedCallback(bool lbSuccess, void* lpUserData);
        static void QuickJoinFinishedCallback(bool lbSuccess, void* lpUserData);
        static void SearchFinishedCallback(bool lbSuccess, void* lpUserData);
        static void SuspensionFinishedCallback(bool lbSuccess, void* lpUserData);
        static void ResumeFinishedCallback(bool lbSuccess, void* lpUserData);
        static void PostRoundFinishedCallback(bool lbSuccess, void* lpUserData);
        static void PostGameFinishedCallback(bool lbSuccess, void* lpUserData);
        static void TeamSelectionFinishedCallback(bool lbSuccess, void* lpUserData);
        static void HostChangedCallback(const CgsSystem::TimerStatus* lpTimerStatus,
                                        NetworkPlayerID lOldHostID, NetworkPlayerID lNewHostID,
                                        void* lpUserData);
        static void GetCompressedCameraPicCallback(void* lpPixels, void* lpUserData);

        static void _AssertLayout()
        {
            static_assert(offsetof(StateManager, mCurrentPlayerXUIDs) == 0x00, "mCurrentPlayerXUIDs @ +0x00");
            static_assert(sizeof(CurrentPlayerXUIDs) == 0x80, "CurrentPlayerXUIDs is 8 x 16 bytes");
            static_assert(offsetof(StateManager, mpNetworkModule) == 0x80, "mpNetworkModule @ +0x80");
            static_assert(sizeof(CachedNewCarData) == 0x18, "CachedNewCarData 0x18");
            static_assert(sizeof(CachedMarkedManData) == 0x08, "CachedMarkedManData 0x08");
            static_assert(sizeof(CachedDistrictData) == 0x0C, "CachedDistrictData 0x0C");
            static_assert(sizeof(CachedCarColourData) == 0x0C, "CachedCarColourData 0x0C");
            static_assert(sizeof(CachedFeverData) == 0x02, "CachedFeverData 0x02");
            // Pointer-free run from meState: relative offsets equal the console ones.
            static_assert(offsetof(StateManager, meOutputPlayerTexture)    - offsetof(StateManager, meState) == 0x94 - 0x90, "+0x94");
            static_assert(offsetof(StateManager, mNetworkPlayerIDToOutput) - offsetof(StateManager, meState) == 0x98 - 0x90, "+0x98");
            static_assert(offsetof(StateManager, mNewCarData)              - offsetof(StateManager, meState) == 0xA0 - 0x90, "+0xA0");
            static_assert(offsetof(StateManager, mMarkedManData)           - offsetof(StateManager, meState) == 0xB8 - 0x90, "+0xB8");
            static_assert(offsetof(StateManager, mDistrictData)            - offsetof(StateManager, meState) == 0xC0 - 0x90, "+0xC0");
            static_assert(offsetof(StateManager, mCarColourData)           - offsetof(StateManager, meState) == 0xCC - 0x90, "+0xCC");
            static_assert(offsetof(StateManager, mFeverData)               - offsetof(StateManager, meState) == 0xD8 - 0x90, "+0xD8");
            static_assert(offsetof(StateManager, meLeftReason)             - offsetof(StateManager, meState) == 0xDC - 0x90, "+0xDC");
            static_assert(offsetof(StateManager, meKickReason)             - offsetof(StateManager, meState) == 0xE0 - 0x90, "+0xE0");
            static_assert(offsetof(StateManager, mbSetNotPlaying)          - offsetof(StateManager, meState) == 0xE4 - 0x90, "+0xE4");
            static_assert(offsetof(StateManager, mbCloseLimboGameWhenIdle) - offsetof(StateManager, meState) == 0xED - 0x90, "+0xED");
            static_assert(offsetof(StateManager, mbRefreshingFreeburnLobbyThisFrame) - offsetof(StateManager, meState) == 0xF2 - 0x90, "+0xF2");
        }

        // ---- data ------------------------------------------------------------------------
        CurrentPlayerXUIDs   mCurrentPlayerXUIDs;       // +0x00 (console-only member; named after its type)
        BrnNetworkModule*    mpNetworkModule;           // +0x80
        BrnNetworkManager*   mpNetworkManager;          // +0x84
        CgsUnicode::CgsUtf8* mpTOS;                     // +0x88
        CgsUnicode::CgsUtf8* mpNews;                    // +0x8C
        EState               meState;                   // +0x90
        // FLAG: BrnGui::GuiEventNetworkOutputPlayerTexture::EOutput has no home yet
        // (E_OUTPUT_OFF 0 .. E_OUTPUT_NUM 6, requested); carried as its s32 storage.
        s32                  meOutputPlayerTexture;     // +0x94
        NetworkPlayerID      mNetworkPlayerIDToOutput;  // +0x98
        CachedNewCarData     mNewCarData;               // +0xA0
        CachedMarkedManData  mMarkedManData;            // +0xB8
        CachedDistrictData   mDistrictData;             // +0xC0
        CachedCarColourData  mCarColourData;            // +0xCC
        CachedFeverData      mFeverData;                // +0xD8
        // FLAG: BrnNetwork::ELeftGameReason (E_LEFT_GAME_REASON_LEFT 0 .. _COUNT 4) has no
        // home yet (requested for BrnNetworkSharedIO.h); carried as its s32 storage.
        s32                  meLeftReason;              // +0xDC
        CgsNetwork::EKickReason meKickReason;           // +0xE0

        bool mbSetNotPlaying;                           // +0xE4
        bool mbDoInviteAfterCreate;                     // +0xE5
        bool mbSuspendAfterSignIn;                      // +0xE6
        bool mbCreatedFromMenus;                        // +0xE7
        bool mbForceStartFreeburnLobby;                 // +0xE8
        bool mbInstantFreeburn;                         // +0xE9
        bool mbReadyToJoinGameSession;                  // +0xEA
        bool mbLoadingScreenVisible;                    // +0xEB
        bool mbAreWeAutosaving;                         // +0xEC
        bool mbCloseLimboGameWhenIdle;                  // +0xED
        bool mbStartFreeburnLobbyThisFrame;             // +0xEE
        bool mbStartingAfterJoinThisFrame;              // +0xEF
        bool mbStartingAfterOnlineEventThisFrame;       // +0xF0
        bool mbForceStartFreeburnLobbyThisFrame;        // +0xF1
        bool mbRefreshingFreeburnLobbyThisFrame;        // +0xF2
    };
} // namespace BrnNetwork
