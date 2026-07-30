#include "GameSource/Gui/Flow/Screen/States/BrnInGame.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension / GuiEventPlayAptMovie
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiOverlayCompleteEvent / GuiEventActivateCrashNav
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"                        // GuiOverlayWaitFinishRequest (the 188 handshake payload)
#include "GameSource/Gui/Flow/Screen/States/Shared/BrnScreenShared.h"     // GetSplashScreenIDForGameMode (+ GsmIO::EGameModeType)
#include "GameSource/GameState/Progression/BrnProfile.h"                  // BrnProgression::Profile::GetIsNewProfile (the intro gate)

// BrnGui::InGame -- reconstructed from BURNOUT_X360_ARTIST.XEX (addresses in the
// header). The SCREEN flow's in-game root state: it owns the pause / main-map /
// event-map / driver-details entry points, the online main-menu handshakes
// (privileges -> "CNOnlStrtQn" question -> SelectOnlineMenuOption -> "TO_*" state
// events), the trophy-unlock / completion countdowns, and the EA-TRAX next-track
// debounce. The registered-event table (30 ids @0x82066680) and every .data constant
// below were read from the image; the class shape is the DecFIGS DWARF (BrnIngame.h).
//
// GUI-CACHE BOUNDARY (FLAG'd below): the state reads/writes a set of GuiCache members
// past the committed layout tail (the online bools @+0x4B4C.., the pause gates, the
// current-landmark s16 @+0x5284, the tracker pointer @+0x4054). Where the committed
// GuiCache exposes a named accessor it is called; each remaining touch is a FLAG'd
// boundary helper documenting the X360 member (DWARF name where recovered) so the real
// accessor can replace it when the GuiCache TU grows -- same discipline as
// BrnBootProfile.cpp's boundaries.
namespace BrnGui
{
    // DWARF BrnInGame.cpp:85/86 -- namespace-scope float globals (they live in .data on
    // the console builds). Values read from the X360 image: both 3.5 seconds.
    f32 KF_TIME_UNTIL_TROPHY_CAR_UNLOCK_SEQUENCE = 3.5f;   // @ 0x82F272D8
    f32 KF_TIME_UNTIL_COMPLETION_SEQUENCE        = 3.5f;   // @ 0x82F272DC

    namespace
    {
        // ---- observed-event ids (roles from the Update dispatch / assert strings; the
        //      full 30-entry registered table is the static below) ----------------------
        const s32 KI_EVENT_CONTROLLER          = 6;    // controller action (sub-id @+4)
        const s32 KI_EVENT_PAD_DISCONNECTED    = 9;    // per-frame pad-disconnect tick
        const s32 KI_EVENT_GUI_CACHE           = 64;   // per-frame cache event (GuiCache* payload)
        const s32 KI_EVENT_OVERLAY_COMPLETE    = 189;  // GuiOverlayCompleteEvent payload
        const s32 KI_EVENT_NETWORK_SPLASH      = 269;  // GuiEventNetworkSplashEvent payload
        const s32 KI_EVENT_PERFORM_MENU_OPTION = 284;  // GuiEventPerformOnlineMainMenuOption payload

        // ---- controller action sub-ids (payload word +4 of the action event). FLAG:
        //      producer-side names not recovered -- roles from this state's switch
        //      (same convention as BrnPauseScreen.cpp's action set). ---------------------
        const s32 KI_ACTION_PAUSE_DRIVER_DETAILS  = 45;  // pause -> driver details (offline)
        const s32 KI_ACTION_PAUSE_MAIN_MAP        = 46;  // pause -> main map (offline)
        const s32 KI_ACTION_EA_TRAX_NEXT_DISABLE  = 54;  // suppress a pending next-track
        const s32 KI_ACTION_EA_TRAX_NEXT          = 55;  // arm the next-EA-track debounce
        const s32 KI_ACTION_OPEN_EVENT_MAP        = 58;  // open the event map (if at a start)

        const f32 KF_EA_TRAX_DEBOUNCE = 0.3f;   // flt_82066050 (.rodata, 0x3E99999A)

        // The "no current event start location" LandmarkIndex sentinel the per-frame
        // event-map gate compares against. word_82F27394 (.data s16 == -1, the
        // BrnInGame.cpp statics block); its source name is not in the recovered DWARF
        // slice.
        const s16 KI_NO_EVENT_START_LANDMARK = -1;

        // GuiCache::meLastDisconnectedError value that selects the "CNLobbyDiscD"
        // overlay variant (any other non-zero error shows "CNLobbyDisc"). The
        // CgsNetwork::EServerInterfaceError enumerator name is not recovered.
        const s32 KI_SERVER_ERROR_LOBBY_DISCONNECT_D = 19;

        const s32 KI_CHANNEL_GUI_OUT      = 40;  // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE   = 41;  // GuiOutViewState
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;  // internal/HUD-component channel

        // DWARF GuiEventNetworkSplashEvent::ESplashState (BrnGuiEventTypeDefs.h:5462);
        // local until that header's TU grows the event type.
        const u32 KU_SPLASH_STATE_SHOW     = 0;
        const u32 KU_SPLASH_STATE_STOP     = 1;
        const u32 KU_SPLASH_STATE_FINISHED = 2;

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- in-queue payload views (the queue delivers the header-stripped payload;
        //      same idiom as BrnGuiOverlaysDirector.cpp's notification views) -----------
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        struct GuiEventProfilePointer : public CgsModule::Event   // event 350 payload
        {
            BrnProgression::Profile* mpProfile;
        };

        // The GuiOverlayCompleteEvent payload (id @+0, leave method @+8 -- the X360
        // HandleOverlayComplete `ld 0(r30)` / `lwz 8(r30)` loads).
        struct GuiOverlayCompletePayload : public CgsModule::Event
        {
            CgsID mOverlayId;
            s32   miLeaveMethod;   // GuiOverlayCompleteEvent::LeaveMethod
        };

        // The GuiEventPerformOnlineMainMenuOption payload (the option word leads).
        struct GuiEventMainMenuOptionPayload : public CgsModule::Event
        {
            EMainMenuOptions meMainMenuOption;
        };

        // ---- out-queue wire records --------------------------------------------------
        // 16-byte GuiEvent<N> command { 1, N, 12 } (the shared channel-command record;
        // same shape as BrnBootProfile.cpp's).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // 20-byte GuiEvent<258> view-state record { 8, 258, 12, w0, w1 } (the OnEnter
        // view reset; the X360 posts { 0, -1 }).
        struct GuiViewStateEvent20 : public CgsGui::GuiEvent<258>
        {
            u32 muWord0;   // +0x0C
            u32 muWord1;   // +0x10
            GuiViewStateEvent20(u32 luWord0, u32 luWord1)
                : CgsGui::GuiEvent<258>(8, 12), muWord0(luWord0), muWord1(luWord1) {}
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> wire record (@0x82436BE0):
        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10
            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0) {}
        };

        void PostOverlayRequest(CgsGui::StateInterface* lpInterface, const char* lpcOverlayId)
        {
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct(lpcOverlayId);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }

        // The OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> wire record
        // (@0x82476E98): { 8, 188, 16, <pad>, CgsID }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32                         muPad0C;    // +0x0C (8-aligned payload)
            GuiOverlayWaitFinishRequest mRequest;   // +0x10 (the compressed overlay id)
            GuiOverlayWaitFinishWire() : CgsGui::GuiEvent<188>(8, 16), muPad0C(0) {}
        };

        void PostOverlayWaitFinishRequest(CgsGui::StateInterface* lpInterface,
                                          const char* lpcOverlayId)
        {
            GuiOverlayWaitFinishWire lWire;
            lWire.mRequest.Construct(lpcOverlayId);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayWaitFinishWire)));
        }

        // The OutputGuiEvent<BrnGui::GuiEventNetworkSplashEvent> wire record
        // (@0x82493880): { 4, 269, 12, state }, channel 40, 16 bytes.
        struct GuiNetworkSplashWire16 : public CgsGui::GuiEvent<269>
        {
            u32 muSplashState;   // +0x0C (ESplashState)
            explicit GuiNetworkSplashWire16(u32 luSplashState)
                : CgsGui::GuiEvent<269>(4, 12), muSplashState(luSplashState) {}
        };

        // ---- GuiCache boundary (X360 members past the committed layout tail; the
        //      committed accessors are used where they exist: GetGameMode /
        //      GetInEventColouringGate / GetTimeStep / AreAllAptComponentsInitialised).
        //      Each helper names the X360 member + offset so the GuiCache TU can absorb
        //      it as a real accessor. ---------------------------------------------------

        // FLAG PC-platform leaf: GuiCache::meLastDisconnectedError (DWARF h:1657; X360
        // +0x4B40, read then cleared by InGame::Update's cache latch) has no committed
        // accessor; the PC network module is unreconstructed, so no disconnect is pending.
        s32 CacheTakeLastDisconnectedError(GuiCache* /*lpCache*/) { return 0; }

        // FLAG PC-platform leaf: GuiCache::mbIsOnline (DWARF h:1664; X360 +0x4B4C) has no
        // committed accessor; the PC boot is offline.
        bool CacheIsOnline(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mbIsPreparingForInvite (DWARF h:1665; X360
        // +0x4B4D) boundary read; no invites on PC.
        bool CacheIsPreparingForInvite(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mbIsStartingGameDueToPlayerJoin (DWARF h:1666;
        // X360 +0x4B4E) boundary read; no online joins on PC.
        bool CacheIsStartingGameDueToPlayerJoin(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mbIsPerformInviteReceived (DWARF h:1667; X360
        // +0x4B4F) boundary read; no invites on PC.
        bool CacheIsPerformInviteReceived(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mbDoJoinOnlineRankedGame (DWARF h:1669; X360
        // +0x4B51) boundary write; no-op until the GuiCache TU exposes it.
        void CacheSetDoJoinOnlineRankedGame(GuiCache* /*lpCache*/, bool /*lbRanked*/) {}

        // FLAG PC-platform leaf: GuiCache::mbDoJoinOnlineFreeburnGame (DWARF h:1670; X360
        // +0x4B52) boundary write; no-op until the GuiCache TU exposes it.
        void CacheSetDoJoinOnlineFreeburnGame(GuiCache* /*lpCache*/, bool /*lbFreeburn*/) {}

        // FLAG PC-platform leaf: GuiCache::mbEnteredOnlineViaEasyDrive (DWARF h:1671; X360
        // +0x4B53, set unconditionally by SelectOnlineMenuOption) boundary write.
        void CacheSetEnteredOnlineViaEasyDrive(GuiCache* /*lpCache*/) {}

        // FLAG PC-platform leaf: GuiCache::mbIsInJunkyard (DWARF h:1675; X360 +0x4B57)
        // pause gate; the PC boot is not in the junkyard.
        bool CacheIsInJunkyard(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mbMugshotActive (DWARF h:1677; X360 +0x4B59)
        // pause gate; no mugshot capture on PC yet.
        bool CacheIsMugshotActive(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mbCarUnlockPending (DWARF h:1689; X360 +0x4B74)
        // boundary read; no car-unlock sequence pending on the PC boot.
        bool CacheIsCarUnlockPending(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: GuiCache::mCurrentLandmarkIndex (DWARF h:1709; X360
        // s16 +0x5284) has no committed accessor; PC returns the "no event start"
        // sentinel so the event map stays gated until the cache member lands.
        s16 CacheGetCurrentLandmarkIndex(const GuiCache* /*lpCache*/)
        { return KI_NO_EVENT_START_LANDMARK; }

        // FLAG PC-platform leaf: unnamed GuiCache pause-gate byte @+0xA014 (X360; no
        // DWARF member maps onto this far offset in the recovered slice). Clear on PC.
        bool CacheFarPauseGateA(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: unnamed GuiCache pause-gate byte @+0xA015 (X360).
        bool CacheFarPauseGateB(const GuiCache* /*lpCache*/) { return false; }

        // FLAG PC-platform leaf: unnamed GuiCache byte @+0xA9E0 the X360
        // SelectOnlineMenuOption sets to 1 with the join bookkeeping; no-op boundary.
        void CacheMarkOnlineMenuActionPending(GuiCache* /*lpCache*/) {}

        // FLAG PC-platform leaf: unnamed GuiCache far gate byte @+0x13B90 (X360; it
        // suppresses both HandleControllerInput and PauseAllowed while set). Clear on PC.
        bool CacheIsInputSuppressed(const GuiCache* /*lpCache*/) { return false; }

        // GuiCache::ClearExpectedAptComponentList (X360 @0x824EE528, called with flow
        // 0) is not declared on the committed GuiCache (same as BrnBootProfile.cpp's).
        // FLAG PC-platform leaf: no-op boundary until the GuiCache TU exposes it.
        void CacheClearExpectedAptComponentList(GuiCache* /*lpCache*/) {}

        // FLAG PC-platform leaf: GuiCache::IsMultiplayerAllowed (X360 @0x824EEB48, the
        // signed-in profile's multiplayer privilege) is not declared on the committed
        // GuiCache; PC mirrors the signed-in default (allowed) while the cache holds.
        bool CacheIsMultiplayerAllowed(const GuiCache* lpCache) { return lpCache != 0; }

        // GuiCache's GuiTracker* (X360 +0x4054) and GuiTracker::ClearTracker
        // (@0x824FA0A8) have no committed PC surface. The X360 asserts the tracker
        // pointer first ("mpGuiCache->GetGuiTracker()"), preserved by the callers.
        // FLAG PC-platform leaf: no-op boundary until the GuiTracker surface lands.
        void TrackerClearTracker(GuiCache* /*lpCache*/) {}

        // FLAG PC-platform leaf: the tracker-pointer existence check backing the
        // "mpGuiCache->GetGuiTracker()" asserts (X360 lwz +0x4054 != 0).
        bool CacheHasGuiTracker(const GuiCache* lpCache) { return lpCache != 0; }

        // The X360 Update reads the profile gate byte at +118033 (= 0x1CD11) each frame
        // and, when it is set, fires "TO_INTRO" + command 476 -- the first-boot entry to
        // the intro (welcome text / photo booth / licence / fly-by) sequence. That byte is
        // BrnProgression::Profile::mbIsNewProfile (BrnProfile.h:476, Construct seeds true;
        // BrnGui::Intro::OnLeave @0x824D1640 clears it), reached here through the
        // DWARF-attested accessor. Named for the X360 semantic, which is "this profile has
        // never played", not "wants a title reset".
        bool ProfileIsNew(const BrnProgression::Profile* lpProfile)
        {
            return lpProfile->GetIsNewProfile();
        }
    }

    // @ 0x82066680 (.rdata, read from the image): the 30 observed event ids, in table
    // order. Update also carries a defensive case for id 3 (ignored) that is NOT in the
    // registered set.
    const s32 InGame::maiEventToObserve[30] =
    {
        2,                              // per-frame tick (ignored)
        KI_EVENT_CONTROLLER,            // 6
        KI_EVENT_PAD_DISCONNECTED,      // 9
        50,                             // invite join go-ahead -> ENTER_GAME
        44,                             // network in-game failed -> "OnLostConn"
        KI_EVENT_GUI_CACHE,             // 64
        228,                            // open-main-map request
        229,                            // car-select request -> "TO_CSELECT"
        134,                            // loading-screen handshake -> command 135
        136,                            // world reload request -> "START_LOAD"
        322,                            // entering-online state -> "TO_GAME_ROOM"
        93,                             // game-mode update -> "ENTER_GAME" (lobby/showtime)
        320,                            // online event finished -> "TO_ST_POST"/"TO_ON_POST"
        291,                            // offline event finished -> "TO_ST_POST"/"TO_OFF_POST"
        288,                            // return to title -> "TO_INTRO"
        81,                             // car select/unlock -> "TO_CUNLOCK"/"TO_CSELECT"
        75,                             // car unlock -> "TO_CUNLOCK"
        KI_EVENT_NETWORK_SPLASH,        // 269
        KI_EVENT_OVERLAY_COMPLETE,      // 189
        133,                            // invite failed -> "GMInvSmGame"
        271,                            // accept -> "ACCEPT"
        516,                            // UI-visible (system guide) update
        511,                            // video request -> "TO_VIDEO"
        132,                            // invite complete -> network suspension release
        350,                            // profile pointer
        KI_EVENT_PERFORM_MENU_OPTION,   // 284
        373,                            // rival event finished -> "TO_RVL_POST"
        375,                            // arm the trophy-car-unlock countdown
        285,                            // instant-freeburn search fail
        309,                            // arm the completion-sequence countdown
    };
    const s32 InGame::miNumEventsObserved = 30;   // @ 0x820666F8

    // @ 0x82F272A4 (.data, read from the image): option -> screen-flow state-event
    // string (DWARF BrnInGame.cpp:66).
    const char* const InGame::KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[13] =
    {
        "TO_FBURN_QK",    // E_MAIN_MENU_OPTIONS_FREEBURN_PLAY_NOW
        "TO_FBURN_CU",    // E_MAIN_MENU_OPTIONS_FREEBURN_CUSTOM_MATCH
        "TO_FBURN_CR",    // E_MAIN_MENU_OPTIONS_FREEBURN_CREATE
        "TO_IMG_GAL",     // E_MAIN_MENU_OPTIONS_IMAGE_GALLERY
        "TO_VIW_CHL",     // E_MAIN_MENU_OPTIONS_VIEW_CHALLENGES
        "TO_UNRANK_QK",   // E_MAIN_MENU_OPTIONS_UNRANKED_PLAY_NOW
        "TO_UNRANK_CU",   // E_MAIN_MENU_OPTIONS_UNRANKED_CUSTOM_MATCH
        "TO_UNRANK_CR",   // E_MAIN_MENU_OPTIONS_UNRANKED_CREATE
        "TO_RANKED_QK",   // E_MAIN_MENU_OPTIONS_RANKED_PLAY_NOW
        "TO_RANKED_CU",   // E_MAIN_MENU_OPTIONS_RANKED_CUSTOM_MATCH
        "TO_RANKED_CR",   // E_MAIN_MENU_OPTIONS_RANKED_CREATE
        "TO_SCOREB",      // E_MAIN_MENU_OPTIONS_SCOREBOARDS
        "TO_NEWS",        // E_MAIN_MENU_OPTIONS_NEWS
    };

    // @ 0x824B8D98 -- base construct, then zero the sequence timers + the disconnect
    // counter (OnEnter re-seeds the rest of the members).
    void InGame::Construct(CgsID lId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CgsGui::State::Construct(lId, lpFsm);
        mfTimeUntilTrophyCarUnlockSeq = 0.0f;
        miNumberOfIgnoredDisconnects  = 0;
        mfTimeUntilCompletionSeq      = 0.0f;
    }

    // @ 0x824D0498 -- register the 30-event table, reset the per-visit members, bring
    // the HUD components up, reset the view state, activate the CrashNav flow, post the
    // in-game entry commands and register the wait-finish handshake for every overlay
    // that must fully hide before the flow moves on.
    void InGame::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mfTimeUntilNextEATrack          = 0.0f;
        mfTimeToDisableNextEATrack      = 0.0f;
        mbIsInEventStartLocation        = false;
        mbIsGuideVisible                = false;
        meSelectedOnlineMainMenuOption  = E_MAIN_MENU_OPTIONS_COUNT;
        mpGuiCache                      = 0;
        mpProfile                       = 0;

        // { 1, 148, 12, flag=1 } on the internal channel -- HUD components up (the
        // flag=0 twin is ShutDownHudComponents).
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 1);

        // { 8, 258, 12, 0, -1 } on the view channel -- the view-state reset record.
        GuiViewStateEvent20 lViewReset(0u, 0xFFFFFFFFu);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lViewReset), KI_CHANNEL_VIEW_STATE, 20);

        // { 8, 191, 12, 1, 0 } -- activate the CrashNav (front-end map) flow.
        GuiEventActivateCrashNav lActivateCrashNav(true);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lActivateCrashNav), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(GuiEventActivateCrashNav)));

        PostCommand16<261>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        PostCommand16<145>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        PostCommand16<65>(mpStateInterface, KI_CHANNEL_GUI_OUT, 1);

        // The wait-finish registrations: each overlay that must finish hiding before
        // its flow transition proceeds (X360 posts the 24-byte 188 record per name).
        PostOverlayWaitFinishRequest(mpStateInterface, "CNOnlLvgGame");
        PostOverlayWaitFinishRequest(mpStateInterface, "CNOnlLvGmQn");
        PostOverlayWaitFinishRequest(mpStateInterface, "CNOnlLvChaQn");
        PostOverlayWaitFinishRequest(mpStateInterface, "GMOnlLvGmQn");
        PostOverlayWaitFinishRequest(mpStateInterface, "CNOnlEntGame");
        PostOverlayWaitFinishRequest(mpStateInterface, "CNOnlLchGame");
        PostOverlayWaitFinishRequest(mpStateInterface, "CNOnlLchGmH");
        PostOverlayWaitFinishRequest(mpStateInterface, "OnHReturnOn");
        PostOverlayWaitFinishRequest(mpStateInterface, "OnCReturnOn");
        PostOverlayWaitFinishRequest(mpStateInterface, "OnHEnterOn");
        PostOverlayWaitFinishRequest(mpStateInterface, "OnCEnterOn");
    }

    // @ 0x824B8DE0
    void InGame::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // @ the ICF fold 0x825011B0 (== BrnGui::Video::GetResourcesToLoad): only the count
    // is zeroed; the tuple out-pointer is deliberately left untouched.
    void InGame::GetResourcesToLoad(const CgsGui::sResourceTuple** /*lppResourceTuples*/,
                                    u32* lpuNumberOfResources) const
    {
        *lpuNumberOfResources = 0;
    }

    // @ 0x824E0468 -- gate on the cache + the input-suppression byte, then dispatch the
    // action sub-id. The X360 in-game action ids are kept verbatim (none of them is the
    // menu accept action, so the PC dual-accept mapping does not apply here).
    void InGame::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        if (mpGuiCache == 0 || CacheIsInputSuppressed(mpGuiCache))
            return;

        const s32 liAction = *reinterpret_cast<const s32*>(
            reinterpret_cast<const u8*>(lpEvent) + 4);
        switch (liAction)
        {
        case KI_ACTION_PAUSE_DRIVER_DETAILS:
            PauseGame(true, true);
            break;

        case KI_ACTION_PAUSE_MAIN_MAP:
            PauseGame(true, false);
            break;

        case KI_ACTION_EA_TRAX_NEXT_DISABLE:
            mfTimeToDisableNextEATrack = KF_EA_TRAX_DEBOUNCE;
            break;

        case KI_ACTION_EA_TRAX_NEXT:
            if (mfTimeUntilNextEATrack <= 0.0f)
                mfTimeUntilNextEATrack = KF_EA_TRAX_DEBOUNCE;
            break;

        case KI_ACTION_OPEN_EVENT_MAP:
            OpenEventMap();
            break;

        default:
            break;
        }
    }

    // @ 0x824D08C0 -- DWARF param: const GuiEventNetworkSplashEvent* (the payload's
    // ESplashState word leads).
    void InGame::HandleSplashScreenRequests(const CgsModule::Event* lpNetworkSplashEvent)
    {
        CGS_ASSERT(lpNetworkSplashEvent != 0, "lpNetworkSplashEvent");   // cpp:851

        const u32 luSplashState = *reinterpret_cast<const u32*>(lpNetworkSplashEvent);
        if (luSplashState == KU_SPLASH_STATE_SHOW)
        {
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                   // cpp:862
            const s32 liGameModeType = mpGuiCache->GetGameMode();
            // X360 bounds: > -1 (E_MODE_NONE) and < 18 (its E_MODE_COUNT -- one mode
            // more than the committed DecFIGS enum's 17; named form kept).
            CGS_ASSERT(liGameModeType > BrnGameState::GameStateModuleIO::E_MODE_NONE,
                       "leGameModeType > GsmIO::E_MODE_NONE");           // cpp:864
            CGS_ASSERT(liGameModeType < BrnGameState::GameStateModuleIO::E_MODE_COUNT + 1,
                       "leGameModeType < GsmIO::E_MODE_COUNT");          // cpp:865

            const char* lpcSplashScreenID = GetSplashScreenIDForGameMode(
                static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(liGameModeType));
            CGS_ASSERT(lpcSplashScreenID != 0, "lpcSplashScreenID != NULL");   // cpp:870
            PostOverlayRequest(mpStateInterface, lpcSplashScreenID);
        }
        else if (luSplashState == KU_SPLASH_STATE_STOP)
        {
            const s32 liGameModeType = mpGuiCache->GetGameMode();
            if (liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
            {
                PostOverlayWaitFinishRequest(mpStateInterface, "OnSplshFreeB");
            }
        }
    }

    // @ 0x824DA350 -- DWARF param: const GuiEventPerformOnlineMainMenuOption*. Privilege
    // check first; a join option picked while offline-but-in-a-mode raises the
    // "CNOnlStrtQn" (leave the current game?) question and parks the option for the
    // overlay-complete handshake.
    void InGame::HandlePerformOnlineMainMenuOption(const CgsModule::Event* lpEvent)
    {
        const EMainMenuOptions leOption =
            reinterpret_cast<const GuiEventMainMenuOptionPayload*>(lpEvent)->meMainMenuOption;

        if (!CheckPrivileges(leOption))
        {
            PostOverlayRequest(mpStateInterface, "CNOnlPrivErr");
            return;
        }

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:919

        // The non-join options (gallery / challenges / scoreboards / news) go straight
        // through.
        const bool lbJoinOption =
            leOption <= E_MAIN_MENU_OPTIONS_FREEBURN_CREATE ||
            (leOption >= E_MAIN_MENU_OPTIONS_UNRANKED_PLAY_NOW &&
             leOption <= E_MAIN_MENU_OPTIONS_RANKED_CREATE);
        if (!lbJoinOption)
        {
            SelectOnlineMenuOption(leOption);
            return;
        }

        if (!CacheIsOnline(mpGuiCache) &&
            mpGuiCache->GetGameMode() != BrnGameState::GameStateModuleIO::E_MODE_NONE)
        {
            PostOverlayRequest(mpStateInterface, "CNOnlStrtQn");
            meSelectedOnlineMainMenuOption = leOption;
        }
        else
        {
            SelectOnlineMenuOption(leOption);
        }
    }

    // @ 0x824D0A38 -- the instant-freeburn search failed: raise the "GMSrchFail"
    // overlay (its OK completion retries as a freeburn CREATE -- see
    // HandleOverlayComplete).
    void InGame::HandleInstantFreeburnSearchFail()
    {
        PostOverlayRequest(mpStateInterface, "GMSrchFail");
    }

    // @ 0x824DA4C8 -- DWARF param: const GuiOverlayCompleteEvent* (payload: id @+0,
    // leave method @+8).
    void InGame::HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent)
    {
        CGS_ASSERT(lpOverlayCompleteEvent != 0, "lpOverlayCompleteEvent");   // cpp:982
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                           // cpp:984

        const GuiOverlayCompletePayload* lpComplete =
            reinterpret_cast<const GuiOverlayCompletePayload*>(lpOverlayCompleteEvent);

        if (lpComplete->mOverlayId == CgsIDCompress("CNOnlStrtQn") &&
            lpComplete->miLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
        {
            // The "leave the current game?" question was accepted: run the parked option.
            SelectOnlineMenuOption(meSelectedOnlineMainMenuOption);
        }
        else if (lpComplete->mOverlayId == CgsIDCompress("GMSrchFail") &&
                 lpComplete->miLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
        {
            // Search-fail acknowledged: retry as "create your own freeburn".
            GuiEventMainMenuOptionPayload lRetry;
            lRetry.meMainMenuOption = E_MAIN_MENU_OPTIONS_FREEBURN_CREATE;
            HandlePerformOnlineMainMenuOption(
                reinterpret_cast<const CgsModule::Event*>(&lRetry));
        }
        else
        {
            const s32 liGameModeType = mpGuiCache->GetGameMode();
            if ((liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                 liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) &&
                lpComplete->mOverlayId == CgsIDCompress("OnSplshFreeB"))
            {
                GuiNetworkSplashWire16 lFinished(KU_SPLASH_STATE_FINISHED);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lFinished), KI_CHANNEL_GUI_OUT, 16);
            }
        }
    }

    // @ 0x824DEFE8 -- online pauses go to the online pause screen; offline "pauses" open
    // the driver details or the main map. (The X360 never reads the first parameter --
    // it is kept for the DWARF signature.)
    void InGame::PauseGame(bool /*lbUserInstigated*/, bool lbOpenDriverDetails)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1038

        if (!PauseAllowed())
            return;

        if (CacheIsOnline(mpGuiCache))
        {
            if (!CacheFarPauseGateA(mpGuiCache))
                SendStateEvent("ON_PAUSE");
        }
        else if (lbOpenDriverDetails)
        {
            OpenDriverDetails();
        }
        else
        {
            OpenMainMap();
        }
    }

    // @ 0x824B8DF8 -- pausing is blocked by the far pause gates, the mugshot capture and
    // the junkyard, and requires every GUI flow layer's apt components initialised.
    bool InGame::PauseAllowed()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1096

        if (CacheFarPauseGateA(mpGuiCache) || CacheIsMugshotActive(mpGuiCache) ||
            CacheIsInputSuppressed(mpGuiCache) || CacheFarPauseGateB(mpGuiCache) ||
            CacheIsInJunkyard(mpGuiCache))
        {
            return false;
        }

        // Walk the three flow layers (the X360 drives the GuiFlow post-increment helper
        // @0x824B2E98 over SCREEN/HUD/OVERLAY).
        GuiFlow leFlow = E_GUIFLOW_FIRST;
        while (mpGuiCache->AreAllAptComponentsInitialised(leFlow))
        {
            leFlow = static_cast<GuiFlow>(leFlow + 1);
            if (leFlow >= E_GUIFLOW_COUNT)
                return true;
        }
        return false;
    }

    // @ 0x824DA610 -- shut the HUD components down (the {1,148,12,0} internal command)
    // and hand the flow to the main map.
    void InGame::OpenMainMap()
    {
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);
        SendStateEvent("MAP_MAIN");
    }

    // @ 0x824DA680 -- as OpenMainMap, but only when standing at an event start location.
    void InGame::OpenEventMap()
    {
        if (!mbIsInEventStartLocation)
            return;
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);
        SendStateEvent("MAP_EVENT");
    }

    // @ 0x824DA700 -- as OpenMainMap, into the driver-details screen.
    void InGame::OpenDriverDetails()
    {
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);
        SendStateEvent("TO_D_DETAIL");
    }

    // @ 0x824D0AB0 -- the {1,148,12,0} internal command (flag=0 == shut down).
    void InGame::ShutDownHudComponents()
    {
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);
    }

    // @ 0x824B8EF8 -- the non-network options (gallery / challenges / scoreboards /
    // news) are always allowed; the join options need the multiplayer privilege.
    bool InGame::CheckPrivileges(EMainMenuOptions leMainMenuOption)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1253

        const bool lbMultiplayerAllowed = CacheIsMultiplayerAllowed(mpGuiCache);
        switch (leMainMenuOption)
        {
        case E_MAIN_MENU_OPTIONS_FREEBURN_PLAY_NOW:
        case E_MAIN_MENU_OPTIONS_FREEBURN_CUSTOM_MATCH:
        case E_MAIN_MENU_OPTIONS_FREEBURN_CREATE:
        case E_MAIN_MENU_OPTIONS_UNRANKED_PLAY_NOW:
        case E_MAIN_MENU_OPTIONS_UNRANKED_CUSTOM_MATCH:
        case E_MAIN_MENU_OPTIONS_UNRANKED_CREATE:
        case E_MAIN_MENU_OPTIONS_RANKED_PLAY_NOW:
        case E_MAIN_MENU_OPTIONS_RANKED_CUSTOM_MATCH:
        case E_MAIN_MENU_OPTIONS_RANKED_CREATE:
            return lbMultiplayerAllowed;

        case E_MAIN_MENU_OPTIONS_IMAGE_GALLERY:
        case E_MAIN_MENU_OPTIONS_VIEW_CHALLENGES:
        case E_MAIN_MENU_OPTIONS_SCOREBOARDS:
        case E_MAIN_MENU_OPTIONS_NEWS:
            return true;

        default:
            CGS_ASSERT(false, "Invalid menu option");   // cpp:1289
            return false;
        }
    }

    // @ 0x824D0B08 -- fire the option's screen-flow state event, record the ranked /
    // freeburn join intent on the cache for the join options (plus command 268), and
    // mark the easy-drive entry.
    void InGame::SelectOnlineMenuOption(EMainMenuOptions leMainMenuOption)
    {
        // The X360 streams "Invalid Cache pointer selecting an online menu(<option>)"
        // into the assert buffer; the plain condition string is kept per policy.
        CGS_ASSERT(mpGuiCache != 0, "Invalid Cache pointer selecting an online menu");   // cpp:1308

        SendStateEvent(KAPC_MAIN_MENU_STATE_ACTIONS_TEXT[leMainMenuOption]);

        bool lbJoinRequest = true;
        switch (leMainMenuOption)
        {
        case E_MAIN_MENU_OPTIONS_FREEBURN_PLAY_NOW:
        case E_MAIN_MENU_OPTIONS_FREEBURN_CUSTOM_MATCH:
        case E_MAIN_MENU_OPTIONS_FREEBURN_CREATE:
            CacheSetDoJoinOnlineRankedGame(mpGuiCache, false);
            CacheSetDoJoinOnlineFreeburnGame(mpGuiCache, true);
            break;

        case E_MAIN_MENU_OPTIONS_UNRANKED_PLAY_NOW:
        case E_MAIN_MENU_OPTIONS_UNRANKED_CUSTOM_MATCH:
        case E_MAIN_MENU_OPTIONS_UNRANKED_CREATE:
            CacheSetDoJoinOnlineRankedGame(mpGuiCache, false);
            CacheSetDoJoinOnlineFreeburnGame(mpGuiCache, false);
            break;

        case E_MAIN_MENU_OPTIONS_RANKED_PLAY_NOW:
        case E_MAIN_MENU_OPTIONS_RANKED_CUSTOM_MATCH:
        case E_MAIN_MENU_OPTIONS_RANKED_CREATE:
            CacheSetDoJoinOnlineRankedGame(mpGuiCache, true);
            CacheSetDoJoinOnlineFreeburnGame(mpGuiCache, false);
            break;

        default:   // gallery / challenges / scoreboards / news: no join bookkeeping
            lbJoinRequest = false;
            break;
        }

        if (lbJoinRequest)
        {
            CacheMarkOnlineMenuActionPending(mpGuiCache);
            PostCommand16<268>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        }

        CacheSetEnteredOnlineViaEasyDrive(mpGuiCache);
    }

    // @ 0x824E0ED0 -- two-pass in-queue drain (cache latch, then the 30-event dispatch),
    // then the per-frame tails: the trophy-unlock / completion countdowns, the
    // event-start-location gate, the EA-TRAX debounce, the profile title-reset gate and
    // the guide-forced pause.
    void InGame::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        // ---- pass 1: latch the GuiCache pointer (only while unset) and surface a
        //      pending lobby-disconnect overlay off the freshly-arrived cache ----------
        if (mpGuiCache == 0)
        {
            for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
                 lpEvent != 0;
                 liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liEventId != KI_EVENT_GUI_CACHE)
                    continue;

                const GuiEventCache* lpCacheEvent =
                    reinterpret_cast<const GuiEventCache*>(lpEvent);
                // The X360 streams "Invalid gui cached" into the assert buffer (cpp:239).
                CGS_ASSERT(lpCacheEvent->mpGuiCache != 0, "Invalid gui cached");
                mpGuiCache = lpCacheEvent->mpGuiCache;

                const s32 liDisconnectError = CacheTakeLastDisconnectedError(mpGuiCache);
                if (liDisconnectError != 0)
                {
                    PostOverlayRequest(mpStateInterface,
                                       liDisconnectError == KI_SERVER_ERROR_LOBBY_DISCONNECT_D
                                           ? "CNLobbyDiscD" : "CNLobbyDisc");
                }
            }
        }

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:275

        // ---- pass 2: the full dispatch ---------------------------------------------
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case 2:
            case 3:                      // defensive (id 3 is not in the registered set)
            case KI_EVENT_GUI_CACHE:     // consumed by pass 1
                break;

            case KI_EVENT_CONTROLLER:
                if (!CacheIsPreparingForInvite(mpGuiCache) &&
                    !CacheIsPerformInviteReceived(mpGuiCache))
                {
                    HandleControllerInput(lpEvent);
                }
                break;

            case KI_EVENT_PAD_DISCONNECTED:
                if (!CacheIsOnline(mpGuiCache) && PauseAllowed())
                {
                    ++miNumberOfIgnoredDisconnects;
                    if (miNumberOfIgnoredDisconnects > KI_NUMBER_OF_DISCONNECTS_TO_IGNORE)
                    {
                        PauseGame(false, false);
                        miNumberOfIgnoredDisconnects = 0;
                    }
                }
                break;

            case 44:   // network in-game failed (the DWARF HandleInGameFailedEvent body,
                       // inlined here by the X360 build)
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:568
                if (CacheIsOnline(mpGuiCache))
                    PostOverlayRequest(mpStateInterface, "OnLostConn");
                break;

            case 50:   // invite join go-ahead
                if (CacheIsPerformInviteReceived(mpGuiCache))
                {
                    CacheClearExpectedAptComponentList(mpGuiCache);
                    SendStateEvent("ENTER_GAME");
                }
                break;

            case 75:
                ShutDownHudComponents();
                SendStateEvent("TO_CUNLOCK");
                break;

            case 81:   // car select/unlock request (payload word: 1 == go)
                if (*reinterpret_cast<const s32*>(lpEvent) != 1)
                    break;
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:378
                // GuiCache::mbIsInEvent (the committed accessor named it from its
                // road-rule consumer).
                if (mpGuiCache->GetInEventColouringGate())
                    break;
                ShutDownHudComponents();
                SendStateEvent(CacheIsCarUnlockPending(mpGuiCache) ? "TO_CUNLOCK"
                                                                   : "TO_CSELECT");
                break;

            case 93:   // game-mode update (the mode word rides @+12)
            {
                const s32 liGameModeType =
                    *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 12);
                if ((liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                     liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) &&
                    !CacheIsStartingGameDueToPlayerJoin(mpGuiCache))
                {
                    SendStateEvent("ENTER_GAME");
                }
                break;
            }

            case 132:  // invite complete (payload word 0 == lift the network suspension)
                CGS_ASSERT(lpEvent != 0, "lpInviteCompleteEvent");   // cpp:587
                if (*reinterpret_cast<const u32*>(lpEvent) == 0)
                {
                    CgsGui::GuiEventNetworkSuspension lSuspension(false);
                    mpStateInterface->OutputGuiEvent(lSuspension);
                }
                break;

            case 133:  // invite failed (payload word 1 == the target game is smaller)
                CGS_ASSERT(lpEvent != 0, "lpInviteFailedEvent");     // cpp:534
                if (*reinterpret_cast<const u32*>(lpEvent) == 1)
                    PostOverlayRequest(mpStateInterface, "GMInvSmGame");
                break;

            case 134:  // loading-screen handshake -> command 135
                PostCommand16<135>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                break;

            case 136:
                SendStateEvent("START_LOAD");
                break;

            case KI_EVENT_OVERLAY_COMPLETE:
                HandleOverlayComplete(lpEvent);
                break;

            case 228:  // open-main-map request (state event first on the X360, then the
                       // HUD shutdown command -- kept in that order)
                SendStateEvent("MAP_MAIN");
                PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);
                break;

            case 229:
                SendStateEvent("TO_CSELECT");
                break;

            case KI_EVENT_NETWORK_SPLASH:
                HandleSplashScreenRequests(lpEvent);
                break;

            case 271:
                SendStateEvent("ACCEPT");
                break;

            case KI_EVENT_PERFORM_MENU_OPTION:
                HandlePerformOnlineMainMenuOption(lpEvent);
                break;

            case 285:
                HandleInstantFreeburnSearchFail();
                break;

            case 288:  // back to the title flow
                ShutDownHudComponents();
                SendStateEvent("TO_INTRO");
                break;

            case 291:  // offline event finished
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                        // cpp:461
                CGS_ASSERT(CacheHasGuiTracker(mpGuiCache), "mpGuiCache->GetGuiTracker()");   // cpp:462
                TrackerClearTracker(mpGuiCache);
                const s32 liGameModeType = mpGuiCache->GetGameMode();
                const bool lbShowtime =
                    liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
                    liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
                SendStateEvent(lbShowtime ? "TO_ST_POST" : "TO_OFF_POST");
                break;
            }

            case 309:  // arm the completion-sequence countdown (payload bytes {1, 0})
                if (reinterpret_cast<const u8*>(lpEvent)[0] == 1 &&
                    reinterpret_cast<const u8*>(lpEvent)[1] == 0)
                {
                    mfTimeUntilCompletionSeq = KF_TIME_UNTIL_COMPLETION_SEQUENCE;
                }
                break;

            case 320:  // online event finished
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                        // cpp:443
                CGS_ASSERT(CacheHasGuiTracker(mpGuiCache), "mpGuiCache->GetGuiTracker()");   // cpp:444
                TrackerClearTracker(mpGuiCache);
                const s32 liGameModeType = mpGuiCache->GetGameMode();
                const bool lbShowtime =
                    liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
                    liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
                SendStateEvent(lbShowtime ? "TO_ST_POST" : "TO_ON_POST");
                break;
            }

            case 322:  // entering-online state (payload byte @+9 == proceed)
                // The X360 streams the message through StrStream (cpp:418); plain form kept.
                CGS_ASSERT(mpGuiCache != 0, "INVALID MPGUICACHE IN INGAME::UPDATE");
                if (CacheIsOnline(mpGuiCache) &&
                    reinterpret_cast<const u8*>(lpEvent)[9] != 0)
                {
                    SendStateEvent("TO_GAME_ROOM");
                }
                break;

            case 350:  // the profile object arrives by pointer
                mpProfile = reinterpret_cast<const GuiEventProfilePointer*>(lpEvent)->mpProfile;
                break;

            case 373:  // rival event finished
                ShutDownHudComponents();
                TrackerClearTracker(mpGuiCache);
                SendStateEvent("TO_RVL_POST");
                break;

            case 375:  // arm the trophy-car-unlock countdown
                mfTimeUntilTrophyCarUnlockSeq = KF_TIME_UNTIL_TROPHY_CAR_UNLOCK_SEQUENCE;
                break;

            case 511:
                SendStateEvent("TO_VIDEO");
                break;

            case 516:  // UI-visible update: the guide is up while the UI is NOT visible
                CGS_ASSERT(lpEvent != 0, "lpUiVisibleEvent != NULL");   // cpp:315
                mbIsGuideVisible = (*reinterpret_cast<const u32*>(lpEvent) == 0);
                break;

            default:
                CGS_ASSERT(false, "Unexpected event in InGame::Update");   // cpp:624
                break;
            }
        }

        // ---- the sequence countdowns (trophy unlock outranks completion; both only
        //      tick outside a game mode and reset when one starts) --------------------
        if (mfTimeUntilTrophyCarUnlockSeq > 0.0f)
        {
            if (mpGuiCache->GetGameMode() == BrnGameState::GameStateModuleIO::E_MODE_NONE)
            {
                mfTimeUntilTrophyCarUnlockSeq -= mpGuiCache->GetTimeStep();
                if (mfTimeUntilTrophyCarUnlockSeq <= 0.0f)
                {
                    ShutDownHudComponents();
                    TrackerClearTracker(mpGuiCache);
                    SendStateEvent("TO_TRPHY_UNL");
                }
            }
            else
            {
                mfTimeUntilTrophyCarUnlockSeq = 0.0f;
            }
        }
        else if (mfTimeUntilCompletionSeq > 0.0f)
        {
            if (mpGuiCache->GetGameMode() == BrnGameState::GameStateModuleIO::E_MODE_NONE)
            {
                mfTimeUntilCompletionSeq -= mpGuiCache->GetTimeStep();
                if (mfTimeUntilCompletionSeq <= 0.0f)
                {
                    ShutDownHudComponents();
                    TrackerClearTracker(mpGuiCache);
                    SendStateEvent("TO_COMPLETED");
                }
            }
            else
            {
                mfTimeUntilCompletionSeq = 0.0f;
            }
        }

        // ---- the event-map gate: standing at an event start location? ---------------
        mbIsInEventStartLocation =
            (CacheGetCurrentLandmarkIndex(mpGuiCache) != KI_NO_EVENT_START_LANDMARK);

        // ---- the EA-TRAX next-track debounce ----------------------------------------
        if (mfTimeToDisableNextEATrack > 0.0f)
        {
            mfTimeUntilNextEATrack      = 0.0f;
            mfTimeToDisableNextEATrack -= mpGuiCache->GetTimeStep();
        }
        else if (mfTimeUntilNextEATrack > 0.0f)
        {
            mfTimeUntilNextEATrack -= mpGuiCache->GetTimeStep();
            if (mfTimeUntilNextEATrack <= 0.0f)
                PostCommand16<461>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        }

        // ---- the first-boot intro gate (X360 Update tail: `lwz mpProfile` ->
        //      `lbz +118033` -> SendStateEvent("TO_INTRO") + command 476) ---------------
        if (mpProfile != 0 && ProfileIsNew(mpProfile))
        {
            SendStateEvent("TO_INTRO");
            PostCommand16<476>(mpStateInterface, KI_CHANNEL_GUI_OUT);
        }

        // ---- the system guide forces the pause path ----------------------------------
        if (mbIsGuideVisible)
            PauseGame(false, false);

        lpInQueue->Clear();
    }
}
