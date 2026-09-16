// ===================================================================================
// BrnGui::CrashNavAccountManagement -- CN_ACCT_MAN, the crash-nav online-account
// management tab (crash-nav settings row 5).
//   class:BrnGui::CrashNavAccountManagement
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The class's whole
// ledger set is here (16 of 16); addresses are on each body. See the header banner
// for why this TU exists -- the class was the LAST header-only shell in the pause
// ring, so the tab registered for nothing, received nothing and drew nothing.
//
// THE SCREEN. A scrolling terms-of-service text field ("TOSText") over three YES/NO
// toggle rows, taken from .rdata @0x82F26FE8..0x82F27004:
//     row 0  "$ONLINE_LOGIN_QUESTION_SHARE_1"     { "$GENERAL_OPTION_YES", "$GENERAL_OPTION_NO" }
//     row 1  "$ONLINE_LOGIN_QUESTION_SHARE_2"     (same option pair, ids { 0, 1 })
//     row 2  "$ONLINE_LOGIN_QUESTION_TELEMETRY"
// and the TOS field cycles "$ONLINE_NEWS_DOWNLOADING_TOS" -> "~TOS_TEXT" ->
// "$ONLINE_NEWS_FAILED_DOWNLOAD_TOS" as the news/TOS download reports in.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavAccountManagement.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiShared.h"          // gGuiResourceIdentifier (the apt movie names)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"   // GuiOverlayRequest / GuiOverlayWaitFinishRequest / GuiAudioTriggerEvent

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // getenv ([acct])

#include <cstdlib>   // getenv ([acct])
#include <cstring>   // memcpy (the 112-byte audio-trigger record)

namespace BrnGui
{
    namespace
    {
        // ---- the nine ids this state observes (X360 .rdata @0x82066478, `li r5, 9`) ----
        // 6 controller input, 21 apt trigger, 64 the GuiCache publish, 266 the news/TOS
        // status, 44 the NETWORK DISCONNECT, 8 controller AXIS, 26 the per-frame tick the
        // TOS scroll integrates, 127 account-update complete, 125 the account settings.
        // (44 is "connection lost" -- BrnOnlinePreEvent.cpp:150, BrnInGame.cpp:869 and the
        //  CrashNavEnterOnline siblings' HandleDisconnectedEvent all read it that way, and
        //  the arm below agrees: it raises the lobby-DISCONNECT overlay.)
        const s32 KAI_EVENTS_TO_OBSERVE[] = { 6, 21, 64, 266, 44, 8, 26, 127, 125 };
        const s32 KI_NUM_EVENTS_OBSERVED = 9;

        const s32 KI_EVENT_CONTROLLER_INPUT   = 6;
        const s32 KI_EVENT_CONTROLLER_AXIS    = 8;
        const s32 KI_EVENT_APT_TRIGGER        = 21;
        const s32 KI_EVENT_FRAME_TICK         = 26;
        const s32 KI_EVENT_GUI_CACHE          = 64;
        const s32 KI_EVENT_ACCOUNT_SETTINGS   = 125;
        const s32 KI_EVENT_ACCOUNT_UPDATE_END = 127;
        const s32 KI_EVENT_DISCONNECTED       = 44;
        const s32 KI_EVENT_NEWS_AND_TOS       = 266;

        // EGameInputActions -- the six arms HandleControllerInput's jump table
        // (@0x824D9754, cases 41..50) actually carries; 45..48 fall to the default.
        const s32 KI_ACTION_TOGGLE_PREV = 41;   // 0x29 -- previous ROW
        const s32 KI_ACTION_TOGGLE_NEXT = 42;   // 0x2A -- next ROW
        const s32 KI_ACTION_OPTION_PREV = 43;   // 0x2B -- previous OPTION within the row
        const s32 KI_ACTION_OPTION_NEXT = 44;   // 0x2C -- next OPTION within the row
        const s32 KI_ACTION_ACCEPT      = 49;   // 0x31 -- GUI_SELECT: apply + save
        const s32 KI_ACTION_BACK        = 50;   // 0x32 -- GUI_CANCEL: leave, discarding

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const char* const KAC_TOGGLE_NAME   = "optionToggle";       // @0x820664A0
        const char* const KAC_TOS_TEXT_NAME = "TOSText";            // @0x820664B0
        const char* const KAC_EMPTY         = "";                   // @0x820046A7
        const char* const KAC_OVERLAY_ACCT  = "CNOnlAcctMan";       // @0x8206A09C
        const char* const KAC_OVERLAY_DISC  = "CNLobbyDisc";        // @0x82062190
        const s32         KI_APT_DISPLAY_LEVEL = 3;
        const s32         KI_RESOURCE_ID_ACCT  = 142;               // 0x8E, the tuple's id

        // X360 `li r8, -1 ; clrldi r8, r8, 32` -- Selectable::K_INVALID_ID.
        const u64 KU_INVALID_APT_ID = 0xFFFFFFFFull;

        // ---- the row tables (X360 .rdata, image-read) --------------------------------
        // @0x82F26FE8 -- every row offers the same YES/NO pair.
        const char* const KAPC_YES_NO_OPTIONS[2] =
        {
            "$GENERAL_OPTION_YES",   // id 0
            "$GENERAL_OPTION_NO",    // id 1
        };
        // The option ids the X360 builds inline on the stack for each SetupToggle
        // (`std r30(0), 0x50(r1)` / `std r27(1), 0x58(r1)`).
        u64 KAU_YES_NO_IDS[2] = { 0ull, 1ull };

        // @0x82F26FF0 -- one title per row.
        const char* const KAPC_ROW_TEXT[3] =
        {
            "$ONLINE_LOGIN_QUESTION_SHARE_1",
            "$ONLINE_LOGIN_QUESTION_SHARE_2",
            "$ONLINE_LOGIN_QUESTION_TELEMETRY",
        };

        // @0x82F26FFC -- indexed by ETOSTextState (`slwi r10, meTOSTextState, 2`).
        const char* const KAPC_TOS_TEXTS[3] =
        {
            "$ONLINE_NEWS_DOWNLOADING_TOS",
            "~TOS_TEXT",
            "$ONLINE_NEWS_FAILED_DOWNLOAD_TOS",
        };

        // ---- the TOS scroll kernel's constants ---------------------------------------
        // HandleControllerAxis's dead zone (.rdata 0x8200D56C / 0x82066260) and
        // UpdateTOSScroll's step (0x82020A84 / 0x820662F8 -- the same magnitude, signed).
        const f32 KF_AXIS_DEAD_ZONE_LOW  = -0.25f;
        const f32 KF_AXIS_DEAD_ZONE_HIGH =  0.25f;
        const f32 KF_SCROLL_STEP_LOW     = -0.2f;
        const f32 KF_SCROLL_STEP_HIGH    =  0.2f;

        // The two axis ids the scroll reads; any other axis leaves mfScrollAxis alone
        // (the X360 `bne` jumps PAST the store, it does not zero it).
        const s32 KI_AXIS_ID_A = 1;
        const s32 KI_AXIS_ID_B = 2;

        // ---- payload views (the same file-local idiom the sibling CrashNav TUs use) ---
        // The event-64 payload view.
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpCachePointer;
        };

        // The event-6 payload view: the action sub-id rides in the payload's +4 word.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // The event-8 payload view. HandleControllerAxis reads the axis id at +0x00 and
        // the axis value at +0x08 (`lwz r11, 0(a2)` / `lfs f0, 8(a2)`).
        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxisId;     // +0x00
            s32 miPad04;      // +0x04
            f32 mfValue;      // +0x08
        };

        // The event-26 payload view: the frame delta is the payload's first float.
        struct FrameTickPayload : public CgsModule::Event
        {
            f32 mfDeltaTime;  // +0x00
        };

        // The event-266 payload view. HandleNewsAndTOSEvent switches on the first word;
        // only 3 and 5 carry an arm (`cmpwi 3` / `cmpwi 5`).
        struct NewsAndTOSPayload : public CgsModule::Event
        {
            s32 miStatus;     // +0x00 (3 == the text arrived, 5 == the download failed)
        };

        // The event-125 payload view: the three answers the account service last stored,
        // copied byte-for-byte into the three row bools.
        struct AccountSettingsPayload : public CgsModule::Event
        {
            u8 mbShareInfo1;  // +0x00
            u8 mbShareInfo2;  // +0x01
            u8 mbTelemetry;   // +0x02
        };

        // ---- the records this TU posts -----------------------------------------------
        // ⚠️ THE RECORD SHAPES ARE X360-ATTESTED; THE NAMES ARE OURS. Each one is exactly
        // the { size, type, offset } header the console stores on the stack plus the bytes
        // it leaves after it; the ids have no committed type home in this tree yet, so
        // they are named here from the event each one is PAIRED with in the observe list
        // (266 is answered by HandleNewsAndTOSEvent, 124 by HandleAccountSettings/125).

        // OnEnter's first post: { 4, 266, 12 } + the s32 1.
        struct GuiEventNewsAndTOSRequest : public CgsGui::GuiEvent<266>
        {
            s32 miRequest;   // +0x0C
            GuiEventNewsAndTOSRequest() : CgsGui::GuiEvent<266>(4, 12), miRequest(1) {}
        };

        // OnEnter's second post: { 1, 124, 12 } + ONE byte.
        //
        // ⭐ THE PAYLOAD BYTE IS 0, AND THAT IS NOT A TYPO. The X360 reuses the same stack
        // record: it rewrites +0x50/+0x54/+0x58 (size/type/offset) and leaves +0x5C alone,
        // so the payload word still holds the 1 the type-266 record put there. The record
        // declares ONE byte of payload, and on the big-endian X360 the first byte of the
        // word 0x00000001 is 0x00. Spelling it as an explicit 0 keeps the queued bytes
        // identical; writing `1` here would be a little-endian host reading a big-endian
        // leftover (the x64-widening-ghost class). [FLAG] If a consumer of 124 ever lands
        // and wants a 1 here, re-read 0x824CE1F4..0x824CE218 before changing it.
        struct GuiEventAccountSettingsRequest : public CgsGui::GuiEvent<124>
        {
            u8  mbRequest;   // +0x0C
            u8  maPad[3];    // +0x0D (the rest of the leftover word; never read, size == 1)
            GuiEventAccountSettingsRequest()
                : CgsGui::GuiEvent<124>(1, 12), mbRequest(0) { maPad[0] = 0; maPad[1] = 0; maPad[2] = 1; }
        };

        // ApplyAndSaveSettings's post: { 3, 126, 12 } + the three answer bytes.
        struct GuiEventAccountSettingsUpdate : public CgsGui::GuiEvent<126>
        {
            u8 mbShareInfo1;   // +0x0C
            u8 mbShareInfo2;   // +0x0D
            u8 mbTelemetry;    // +0x0E
            u8 mPad0F;         // +0x0F
            GuiEventAccountSettingsUpdate()
                : CgsGui::GuiEvent<126>(3, 12)
                , mbShareInfo1(0), mbShareInfo2(0), mbTelemetry(0), mPad0F(0) {}
        };

        // The OutputGuiEvent<GuiOverlayRequest> wire record: { 288, 184, 16, <pad>, request },
        // channel 40, 304 bytes -- the shape BrnRaceMainHudState.cpp already posts.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C
            GuiOverlayRequest mRequest;   // +0x10
            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0) {}
        };

        // The wait-finish wire record: { 8, 188, 16, <pad>, id }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32                        muPad0C;    // +0x0C
            GuiOverlayWaitFinishRequest mRequest;  // +0x10
            GuiOverlayWaitFinishWire()
                : CgsGui::GuiEvent<188>(static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)), 16)
                , muPad0C(0) {}
        };

        // The state's in-event queue (CgsGui::State +0x18).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // [DIAG] NOT IN THE X360 BINARY -- BRN_ACCTMAN_DIAG=1.
        bool AcctDiagOn()
        {
            return getenv("BRN_ACCTMAN_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0;
        }
    }

    // ---- the static load table (X360 .rdata @0x82F26FDC / @0x82F26FE4) --------------
    const CgsGui::sResourceTuple CrashNavAccountManagement::maResourcesToLoad[1] =
    {
        { KI_RESOURCE_ID_ACCT, CgsGui::E_GUI_RESOURCETYPE_APT },   // { 142, 4 }
    };
    const u32 CrashNavAccountManagement::muNumResourcesToLoad = 1;

    // ---- ctor ----------------------------------------------------------------------
    // The X360 constructor is vptr installs plus the embedded components' own
    // constructors, all of which C++ emits implicitly. The scalar fields are seeded by
    // OnEnter; they are listed here so a state that never enters is still defined.
    CrashNavAccountManagement::CrashNavAccountManagement()
        : meState(E_STATE_INIT_SETUP)
        , mpGuiCache(0)
        , mMenuToggleGroup()
        , mTOSText()
        , meTOSTextState(E_TOSTEXT_DOWNLOADING)
        , mfScrollAccumulator(0.0f)
        , mfScrollAxis(0.0f)
        , mbShareInfo1(true)
        , mbShareInfo2(true)
        , mbTelemetry(true)
    {
    }

    // ---- OnEnter @0x824CE110 -------------------------------------------------------
    // ⭐ REGISTER FIRST. Without this the state observes nothing and every arm below is
    // dead code -- which is precisely the state this tab was in.
    void CrashNavAccountManagement::OnEnter()
    {
        mpStateInterface->RegisterForEvents(KAI_EVENTS_TO_OBSERVE, KI_NUM_EVENTS_OBSERVED);

        // The entry seeding, in the X360's own store order (`stw r11, 0x38` / `0x3C`,
        // `stfs f0(0.0), 0x2F78` / `0x2F74`, `stw r11(0), 0x2F70`, then the three `stb 1`).
        meState             = E_STATE_INIT_SETUP;
        mpGuiCache          = 0;
        mfScrollAxis        = 0.0f;
        mfScrollAccumulator = 0.0f;
        meTOSTextState      = E_TOSTEXT_DOWNLOADING;
        mbShareInfo1        = true;
        mbShareInfo2        = true;
        mbTelemetry         = true;

        // The X360 dispatches the TOS field's Construct through its vtable slot 0
        // (`lwz r9, 0x2E48(this); lwz r9, 0(r9); bctrl`) with a NULL parent name.
        mTOSText.Construct(KAC_TOS_TEXT_NAME, mpStateInterface, 0);

        mMenuToggleGroup.Construct(KAC_TOGGLE_NAME, mpStateInterface,
                                   E_TOGGLEROW_COUNT, 0, KU_INVALID_APT_ID);
        mMenuToggleGroup.SetupGroup(E_TOGGLEROW_COUNT, 0);

        // Ask for the terms-of-service text and for the account's stored answers. The
        // replies come back as events 266 and 125, both of which this state observes.
        {
            GuiEventNewsAndTOSRequest lRequest;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lRequest, KI_CHANNEL_GUI_OUT, 16);
        }
        {
            GuiEventAccountSettingsRequest lRequest;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lRequest, KI_CHANNEL_GUI_OUT, 16);
        }

        if (AcctDiagOn())
            *CgsDev::Log::gpDebugPrint << "[acct] OnEnter -- registered 9 events, asked for TOS (266) and settings (124)\n";
    }

    // ---- OnLeave @0x824CE228 -------------------------------------------------------
    void CrashNavAccountManagement::OnLeave()
    {
        mMenuToggleGroup.Clear();      // group vtable slot 6 (`lwz r11, 0x18(vtbl)`)

        meState = E_STATE_LEAVING;

        // Symmetric with OnEnter: the observer table is only KI_MAX_OBSERVERS wide.
        mpStateInterface->UnRegisterForEvents(KAI_EVENTS_TO_OBSERVE, KI_NUM_EVENTS_OBSERVED);

        // The apt UNLOAD sentinel: the EMPTY movie name at the display level.
        mpStateInterface->PlayAptMovie(KAC_EMPTY, KI_APT_DISPLAY_LEVEL);
    }

    // ---- Update @0x824E00A0 --------------------------------------------------------
    // The console's fall-through ladder (jump table @0x824E00D8). Each stage runs and,
    // when it reports done, the NEXT stage runs in the SAME visit. E_STATE_LEAVING is
    // the only arm that suppresses UpdatePermanent (`li r27, 0` @0x824E0150).
    void CrashNavAccountManagement::Update()
    {
        bool lbRunPermanent = true;

        switch (meState)
        {
        case E_STATE_INIT_SETUP:
            meState = E_STATE_INIT_SETUP;
            if (!UpdateInitSetup())
            {
                break;
            }
            // fall through
        case E_STATE_LOADING:
            meState = E_STATE_LOADING;
            if (!UpdateLoading())
            {
                break;
            }
            // fall through
        case E_STATE_WF_INIT:
            meState = E_STATE_WF_INIT;
            if (!UpdateWFInit())
            {
                break;
            }
            // fall through
        case E_STATE_MAIN:
            meState = E_STATE_MAIN;
            break;

        case E_STATE_SAVING:
            meState = E_STATE_SAVING;
            break;

        case E_STATE_LEAVING:
            lbRunPermanent = false;
            meState = E_STATE_LEAVING;
            break;

        default:
            CGS_ASSERT(false, "Invalid internal state (");
            break;
        }

        // [DIAG] NOT IN THE X360 BINARY -- [acct] the ladder stage, once per change.
        {
            static s32 siDiagLastStage = -1;
            if (static_cast<s32>(meState) != siDiagLastStage && AcctDiagOn())
            {
                *CgsDev::Log::gpDebugPrint
                    << "[acct] stage " << siDiagLastStage << " -> " << static_cast<s32>(meState)
                    << " (0 initsetup 1 loading 2 wfinit 3 main 4 saving 5 leaving)\n";
            }
            siDiagLastStage = static_cast<s32>(meState);
        }

        if (lbRunPermanent)
        {
            UpdatePermanent();
        }

        // @0x824E0218, UNCONDITIONAL -- the queue is emptied even on the LEAVING arm that
        // skipped UpdatePermanent, so nothing this frame's state ignored survives into the
        // next one.
        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ---- UpdateInitSetup @0x824C1A58 -----------------------------------------------
    // Walk this frame's inbound events for the GuiCache publish and latch it.
    bool CrashNavAccountManagement::UpdateInitSetup()
    {
        bool lbGotCache = false;

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                const GuiEventCache* lpCacheEvent =
                    reinterpret_cast<const GuiEventCache*>(lpEvent);
                CGS_ASSERT(lpCacheEvent->mpCachePointer != 0,
                           "Invalid cache in CrashNavAccountManagement::OnEnter");   // cpp:279
                lbGotCache = true;
                mpGuiCache = lpCacheEvent->mpCachePointer;
            }
        }

        return lbGotCache;
    }

    // ---- UpdateLoading @0x824CE2D0 -------------------------------------------------
    // Hold until the screen's one apt resource is resident, then declare the components
    // this tab expects and ask the view to play the movie.
    bool CrashNavAccountManagement::UpdateLoading()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:306

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mMenuToggleGroup.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KI_RESOURCE_ID_ACCT],
                                       KI_APT_DISPLAY_LEVEL);
        return true;
    }

    // ---- UpdateWFInit @0x824BFC88 --------------------------------------------------
    // Wait for those components, then build the three rows from the answers currently in
    // hand and show whichever TOS text the download has reached.
    bool CrashNavAccountManagement::UpdateWFInit()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:344

        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:854 (the console asserts twice)

        // Each row is the same YES/NO pair with ids { 0, 1 }, and "yes" highlights item 0.
        mMenuToggleGroup.SetupToggle(E_TOGGLEROW_SHARE_INFO_1, 2, true,
                                     KAPC_ROW_TEXT[E_TOGGLEROW_SHARE_INFO_1],
                                     const_cast<const char**>(KAPC_YES_NO_OPTIONS),
                                     KAU_YES_NO_IDS);
        mMenuToggleGroup.HighlightItem(E_TOGGLEROW_SHARE_INFO_1, mbShareInfo1 ? 0 : 1);

        mMenuToggleGroup.SetupToggle(E_TOGGLEROW_SHARE_INFO_2, 2, true,
                                     KAPC_ROW_TEXT[E_TOGGLEROW_SHARE_INFO_2],
                                     const_cast<const char**>(KAPC_YES_NO_OPTIONS),
                                     KAU_YES_NO_IDS);
        mMenuToggleGroup.HighlightItem(E_TOGGLEROW_SHARE_INFO_2, mbShareInfo2 ? 0 : 1);

        mMenuToggleGroup.SetupToggle(E_TOGGLEROW_TELEMETRY, 2, true,
                                     KAPC_ROW_TEXT[E_TOGGLEROW_TELEMETRY],
                                     const_cast<const char**>(KAPC_YES_NO_OPTIONS),
                                     KAU_YES_NO_IDS);
        mMenuToggleGroup.HighlightItem(E_TOGGLEROW_TELEMETRY, mbTelemetry ? 0 : 1);

        RefreshTOSText();
        return true;
    }

    // ---- UpdatePermanent @0x824DE818 -----------------------------------------------
    // Drain the events (jump table @0x824DE8C4 for ids 6..26, then the three high ids)
    // and tick the group.
    void CrashNavAccountManagement::UpdatePermanent()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:   HandleControllerInput(lpEvent);       break;
            case KI_EVENT_CONTROLLER_AXIS:    HandleControllerAxis(lpEvent);        break;
            case KI_EVENT_APT_TRIGGER:        HandleTriggers(lpEvent);              break;
            case KI_EVENT_FRAME_TICK:         UpdateTOSScroll(lpEvent);             break;
            case KI_EVENT_ACCOUNT_SETTINGS:   HandleAccountSettings(lpEvent);       break;
            case KI_EVENT_ACCOUNT_UPDATE_END: HandleAccountUpdateComplete(lpEvent); break;
            case KI_EVENT_NEWS_AND_TOS:       HandleNewsAndTOSEvent(lpEvent);       break;

            case KI_EVENT_DISCONNECTED:
                // The online session dropped out from under the tab: stop waiting on the
                // account-management overlay and raise the lobby-disconnect one instead.
                {
                    GuiOverlayWaitFinishWire lWait;
                    lWait.mRequest.Construct(KAC_OVERLAY_ACCT);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        &lWait, KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(GuiOverlayWaitFinishWire)));

                    GuiOverlayRequestWire lRequest;
                    lRequest.mRequest.Construct(KAC_OVERLAY_DISC);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        &lRequest, KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(GuiOverlayRequestWire)));
                }
                break;

            default:
                break;
            }
        }

        mMenuToggleGroup.Update();   // group vtable slot 5 (`lwz r11, 0x14(vtbl)`)
    }

    // ---- HandleControllerInput @0x824D9698 -----------------------------------------
    // Every arm is gated on the tab having settled (meState == MAIN) and, for the four
    // navigation arms, on the group actually moving -- a press that changes nothing
    // makes no sound. The console returns void here (no arm sets r3).
    void CrashNavAccountManagement::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavAccountManagement::HandleControllerInput");   // cpp:565

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_TOGGLE_PREV:
            if (meState == E_STATE_MAIN && mMenuToggleGroup.HighlightPrevious(false))
            {
                TriggerSound(lpInput->miButtonId);
            }
            break;

        case KI_ACTION_TOGGLE_NEXT:
            if (meState == E_STATE_MAIN && mMenuToggleGroup.HighlightNext(false))
            {
                TriggerSound(lpInput->miButtonId);
            }
            break;

        case KI_ACTION_OPTION_PREV:
            if (meState == E_STATE_MAIN && mMenuToggleGroup.HighlightPreviousItem())
            {
                TriggerSound(lpInput->miButtonId);
            }
            break;

        case KI_ACTION_OPTION_NEXT:
            if (meState == E_STATE_MAIN && mMenuToggleGroup.HighlightNextItem())
            {
                TriggerSound(lpInput->miButtonId);
            }
            break;

        case KI_ACTION_ACCEPT:
            // ⭐ THE SAVE. Publish the three answers and wait for the service to confirm.
            if (meState == E_STATE_MAIN)
            {
                ApplyAndSaveSettings();
            }
            break;

        case KI_ACTION_BACK:
            // Leaving discards: nothing is published and nothing is waited on.
            if (meState == E_STATE_MAIN)
            {
                mMenuToggleGroup.Unloaded();
                SendStateEvent("GO_BACK");
                meState = E_STATE_LEAVING;
            }
            break;

        default:
            break;
        }
    }

    // ---- HandleControllerAxis @0x824B8090 ------------------------------------------
    // Latch the stick position the TOS scroll integrates, with a +/-0.25 dead zone.
    // ⚠️ The console's assert message here is SelectRoutes's, not this class's -- the
    // two screens share the literal. Kept verbatim; it is not a mis-copy.
    void CrashNavAccountManagement::HandleControllerAxis(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in SelectRoutes::HandleControllerAxis");   // cpp:688

        const ControllerAxisPayload* lpAxis =
            reinterpret_cast<const ControllerAxisPayload*>(lpEvent);

        if (meState < E_STATE_MAIN)
        {
            mfScrollAxis = 0.0f;
            return;
        }

        if (lpAxis->miAxisId != KI_AXIS_ID_A && lpAxis->miAxisId != KI_AXIS_ID_B)
        {
            // The X360 `bne` jumps PAST the store: an unrelated axis leaves the last
            // latched value alone rather than clearing it.
            return;
        }

        const f32 lfValue = lpAxis->mfValue;
        mfScrollAxis = (lfValue < KF_AXIS_DEAD_ZONE_LOW || lfValue > KF_AXIS_DEAD_ZONE_HIGH)
                           ? lfValue
                           : 0.0f;
    }

    // ---- UpdateTOSScroll (UpdatePermanent's event-26 arm @0x824DE948) --------------
    // Integrate the latched axis into an accumulator and step the text field one line
    // each time it crosses +/-0.2, carrying the remainder.
    void CrashNavAccountManagement::UpdateTOSScroll(const CgsModule::Event* lpEvent)
    {
        if (mfScrollAxis == 0.0f)
        {
            mfScrollAccumulator = 0.0f;
            return;
        }

        const FrameTickPayload* lpTick = reinterpret_cast<const FrameTickPayload*>(lpEvent);

        // X360 `fmadds f0, dt, axis, accumulator`.
        mfScrollAccumulator = lpTick->mfDeltaTime * mfScrollAxis + mfScrollAccumulator;

        // The two arms store +1 and -1 into the field's scroll delta; see the [FLAG] on
        // TextField::ScrollDown/ScrollUp for why those two names carry those two signs.
        if (mfScrollAccumulator <= KF_SCROLL_STEP_LOW)
        {
            mTOSText.ScrollDown();                       // X360 `stw 1, 0x90(field)`
            mTOSText.OutputAptData();
            mfScrollAccumulator = mfScrollAccumulator + KF_SCROLL_STEP_HIGH;
        }
        else if (mfScrollAccumulator >= KF_SCROLL_STEP_HIGH)
        {
            mTOSText.ScrollUp();                         // X360 `stw -1, 0x90(field)`
            mTOSText.OutputAptData();
            mfScrollAccumulator = mfScrollAccumulator - KF_SCROLL_STEP_HIGH;
        }
    }

    // ---- HandleTriggers @0x824B8180 ------------------------------------------------
    // The X360 body is the argument assert and nothing else: this screen's apt triggers
    // carry no arm. Kept so the dispatcher's case is faithful rather than absent.
    void CrashNavAccountManagement::HandleTriggers(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavAccountManagement::HandleTriggers");   // cpp:759
        (void)lpEvent;
    }

    // ---- HandleAccountSettings @0x824B8218 -----------------------------------------
    // The account service's stored answers. Copied straight into the three row bools;
    // UpdateWFInit is what turns them into highlighted options.
    void CrashNavAccountManagement::HandleAccountSettings(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavAccountManagement::HandleAccountSettings");   // cpp:796

        const AccountSettingsPayload* lpSettings =
            reinterpret_cast<const AccountSettingsPayload*>(lpEvent);

        mbShareInfo1 = lpSettings->mbShareInfo1 != 0;
        mbShareInfo2 = lpSettings->mbShareInfo2 != 0;
        mbTelemetry  = lpSettings->mbTelemetry != 0;

        if (AcctDiagOn())
            *CgsDev::Log::gpDebugPrint
                << "[acct] settings in: share1=" << (mbShareInfo1 ? 1 : 0)
                << " share2=" << (mbShareInfo2 ? 1 : 0)
                << " telemetry=" << (mbTelemetry ? 1 : 0) << "\n";
    }

    // ---- HandleNewsAndTOSEvent @0x824BCFB0 -----------------------------------------
    // The news/TOS download reporting in. Only statuses 3 and 5 move the text; anything
    // else leaves it on whatever it was showing.
    void CrashNavAccountManagement::HandleNewsAndTOSEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavAccountManagement::HandleNewsAndTOSEvent");   // cpp:722

        const NewsAndTOSPayload* lpNews = reinterpret_cast<const NewsAndTOSPayload*>(lpEvent);

        if (lpNews->miStatus == 3)
        {
            meTOSTextState = E_TOSTEXT_LOADED;
        }
        else if (lpNews->miStatus == 5)
        {
            meTOSTextState = E_TOSTEXT_FAILED;
        }

        RefreshTOSText();

        if (AcctDiagOn())
            *CgsDev::Log::gpDebugPrint
                << "[acct] news/TOS status " << lpNews->miStatus
                << " -> text state " << static_cast<s32>(meTOSTextState) << "\n";
    }

    // ---- HandleAccountUpdateComplete @0x824CE3D0 -----------------------------------
    // The service confirmed the update: stop waiting on the overlay and leave.
    void CrashNavAccountManagement::HandleAccountUpdateComplete(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in CrashNavAccountManagement::HandleAccountUpdateComplete");   // cpp:773
        (void)lpEvent;

        GuiOverlayWaitFinishWire lWait;
        lWait.mRequest.Construct(KAC_OVERLAY_ACCT);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            &lWait, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(GuiOverlayWaitFinishWire)));

        mMenuToggleGroup.Unloaded();
        SendStateEvent("GO_BACK");

        if (AcctDiagOn())
            *CgsDev::Log::gpDebugPrint << "[acct] account update complete -- leaving\n";
    }

    // ---- ApplyAndSaveSettings @0x824CE4C8 ------------------------------------------
    // ⭐ THE WHOLE POINT OF THE TAB. Read the three rows' highlighted option ids back out
    // of the group, publish them as event 126, raise the "please wait" overlay and park
    // in E_STATE_SAVING until the service answers with event 127.
    void CrashNavAccountManagement::ApplyAndSaveSettings()
    {
        // The console reads each answer the long way -- GetSelectable(row), then the row's
        // INNER SelectableGroup at +0xA8, then its highlighted Selectable's id qword at
        // +0x10 -- and folds `id == 0` (i.e. "$GENERAL_OPTION_YES") to the bool with
        // `cntlzw` + `rlwinm 27,31,31`.
        bool labAnswers[E_TOGGLEROW_COUNT];
        for (s32 liRow = 0; liRow < E_TOGGLEROW_COUNT; ++liRow)
        {
            MenuToggle* lpRow = mMenuToggleGroup.GetSelectable(liRow);
            CGS_ASSERT(lpRow != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218
            // MenuToggle::GetHighlightedId @0x82489080 IS that chain (row + 0xA8, then
            // SelectableGroup::GetHighlighted, then the id qword), assert included.
            labAnswers[liRow] = (lpRow->GetHighlightedId() == 0ull);
        }

        {
            GuiEventAccountSettingsUpdate lUpdate;
            lUpdate.mbShareInfo1 = labAnswers[E_TOGGLEROW_SHARE_INFO_1] ? 1 : 0;
            lUpdate.mbShareInfo2 = labAnswers[E_TOGGLEROW_SHARE_INFO_2] ? 1 : 0;
            lUpdate.mbTelemetry  = labAnswers[E_TOGGLEROW_TELEMETRY]    ? 1 : 0;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lUpdate, KI_CHANNEL_GUI_OUT, 16);
        }

        {
            GuiOverlayRequestWire lRequest;
            lRequest.mRequest.Construct(KAC_OVERLAY_ACCT);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                &lRequest, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }

        meState = E_STATE_SAVING;

        if (AcctDiagOn())
            *CgsDev::Log::gpDebugPrint
                << "[acct] SAVE share1=" << (labAnswers[0] ? 1 : 0)
                << " share2=" << (labAnswers[1] ? 1 : 0)
                << " telemetry=" << (labAnswers[2] ? 1 : 0)
                << " -- published 126, raised CNOnlAcctMan, waiting on 127\n";
    }

    // ---- TriggerSound @0x824CE698 --------------------------------------------------
    // The menu click. Moving between ROWS and moving between OPTIONS are different cues,
    // and any other action reaching here is the console's own assert.
    void CrashNavAccountManagement::TriggerSound(s32 liAction)
    {
        const char* lpacLabel = 0;

        switch (liAction)
        {
        case 0x25:   // 37
        case 0x26:   // 38
        case KI_ACTION_TOGGLE_PREV:
        case KI_ACTION_TOGGLE_NEXT:
            lpacLabel = "MenuToggleDefault";
            break;

        case 0x27:   // 39
        case 0x28:   // 40
        case KI_ACTION_OPTION_PREV:
        case KI_ACTION_OPTION_NEXT:
            lpacLabel = "MenuItemToggleDefault";
            break;

        default:
            CGS_ASSERT(false, "lpcLabel");   // cpp:941
            break;
        }

        if (lpacLabel == 0)
        {
            return;
        }

        PostAudioTrigger(7, lpacLabel);
    }

    // ---- PostAudioTrigger ----------------------------------------------------------
    // The 112-byte audio-trigger record the X360 builds inline in TriggerSound:
    // { size 100, type 457, offset 12 } followed by the native
    // { component[32], action, label[32], movie[32] } payload -- the same hand-built
    // record BrnCrashNavOptions.cpp and BrnChallengeListComponent.cpp already post.
    void CrashNavAccountManagement::PostAudioTrigger(s32 liAction, const char* lpacLabel)
    {
        GuiAudioTriggerEvent lEvent;
        lEvent.Construct(liAction, KAC_EMPTY, lpacLabel);

        struct Record
        {
            s32 miSize;
            s32 miType;
            s32 miOffset;
            GuiAudioTriggerWirePayload457 mPayload;
        } lRecord = { 100, 457, 12, {} };
        std::memcpy(&lRecord.mPayload, lEvent.macComponent, sizeof(lRecord.mPayload));

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRecord)));
    }

    // ---- RefreshTOSText ------------------------------------------------------------
    // The three-statement run the X360 inlines at both of its sites (UpdateWFInit's tail
    // @0x824BFDF8 and HandleNewsAndTOSEvent's tail @0x824BD06C): pick the text by state,
    // ask the field to reset its scroll, and push it to apt.
    void CrashNavAccountManagement::RefreshTOSText()
    {
        mTOSText.SetText(KAPC_TOS_TEXTS[meTOSTextState]);
        mTOSText.ResetScroll();          // X360 `li 1 ; stb r11, 0x125(field)`
        mTOSText.OutputAptData();
    }
}
