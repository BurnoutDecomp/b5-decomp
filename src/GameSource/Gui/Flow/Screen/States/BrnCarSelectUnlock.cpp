// ===================================================================================
// BrnGui::CarSelectUnlock -- out-of-line bodies + statics for the "car unlocked"
// celebration screen flow state.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   GetResourcesToLoad  @0x824B5A20
//   OnEnter             @0x824CA198
//   OnLeave             @0x824CA2E8
//   UpdateGetCache      @0x824C15A8
//   UpdateLoadResources @0x824D8268
//   UpdateRunning       @0x824CA420  (raw-asm walked; the Hex-Rays local allocation fails)
//   UpdateWFInit        @0x824B5B28
//
// NOT BODIED HERE (declared in the header; their own ledger slices):
//   * Update / PlayMovie.
//
// The statics below are MEASURED (wave L headless-IDA dump,
// scratchpad/waveL/carselectunlock_rodata.txt): maiEventToObserve @0x82066054 =
// {64,73,74,76,538,6} (xrefs: OnEnter/OnLeave only), maResourcesToLoad @0x82F26D78 =
// {148,4},{55,4} (xref: UpdateLoadResources only; count is its `li r5, 2`), and
// unk_820046A7 is a genuine "" (the NUL tail after the "%s..." pool string).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectUnlock.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // E_FORMAT_ID_LOOKUP
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue / Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (register / stop-loading / play-apt / out-queue)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry
#include "GameSource/Input/GameInputActions.h"                       // EGameInputActions (the controller action vocabulary)

#include <cstring>   // std::strncpy / std::memset (ticker message build)

namespace BrnGui
{
    // ---- statics (DWARF cpp:26-53; .rdata) ----------------------------------------
    // The 6 event ids this state registers for -- DUMPED big-endian @0x82066054 (wave L).
    // Self-corroborating: 64 (gui cache) is what UpdateGetCache scans for, and 73/74/76,
    // 538 and 6 are exactly the five ids UpdateRunning dispatches. The count word 6 and
    // the muNumResourcesToLoad word 2 sit directly after the array in .rdata
    // (@0x8206606C / @0x82066070), exactly the DWARF's cpp:26/:36/:44 static order.
    const s32 CarSelectUnlock::maiEventToObserve[6] = { 64, 73, 74, 76, 538, 6 };
    const s32 CarSelectUnlock::miNumEventsObserved  = 6;

    // The two apt resources the unlock screen loads internally -- DUMPED @0x82F26D78
    // (wave L): { 148, 4 } and { 55, 4 }, bounded by the named neighbour table
    // unk_82F26D88 (BrnCrashNavStats' 1-entry list) and UpdateLoadResources' immediate
    // count (`li r5, 2`). gGuiResourceIdentifier @0x82F278E0 names the ids:
    // [148] "BrnCarSelectUnlock" (this screen's own movie) and [55] "B5ManufacturersIcon"
    // (the badge component bundle -- the same id 55 the CarSelectLivery sibling loads).
    const CgsGui::sResourceTuple CarSelectUnlock::maResourcesToLoad[] =
    {
        { 148u, CgsGui::E_GUI_RESOURCETYPE_APT },   // BrnCarSelectUnlock
        {  55u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5ManufacturersIcon
    };
    const u32 CarSelectUnlock::muNumResourcesToLoad = 2;

    // The apt component names passed to GuiComponent::Construct / AppendExpectedAptComponent.
    const char CarSelectUnlock::KAC_MANUFACTURER_LOGO[20]          = "ManufacturerLogo_mc";
    const char CarSelectUnlock::KAC_CAR_NAME[11]                   = "CarName_mc";
    const char CarSelectUnlock::macAnimComponentName[13]           = "LogoAnim_mcp";
    const char CarSelectUnlock::KAC_HELPITEM_CONTINUE[20]          = "HelpItemContinue_mc";
    const char CarSelectUnlock::macHelpPromptAnimComponentName[15] = "HelpPrompt_mcp";

    // The state's in-event queue (State +0x18) is the DWARF InputBuffer::GuiEventQueue, an
    // incomplete alias for this concrete queue instantiation the X360 walks by name.
    typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;

    // The GuiCache event carried on the in-queue is event type 64 (== maiEventToObserve[0],
    // measured @0x82066054).
    static const s32 KI_EVENT_GUI_CACHE = 64;

    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut (the family's out channel)

        // ---- the observed event ids UpdateRunning dispatches (maiEventToObserve) ----
        const s32 KI_EVENT_CONTROLLER_PRESSED  = 6;    // ControllerButtonPayload
        const s32 KI_EVENT_CAR_UNLOCK_NEW_CAR  = 73;   // GuiCarUnlockNewCarEvent (8-byte CgsID)
        const s32 KI_EVENT_CAR_UNLOCK_ADVANCE  = 74;   // advance request -> SendStateEvent("ADVANCE")
        const s32 KI_EVENT_CAR_UNLOCK          = 76;   // GuiCarUnlockEvent (8-byte CgsID)
        const s32 KI_EVENT_TICKER_ONOFF        = 538;  // ticker on/off (leading payload byte)

        // ---- the two accept action ids (X360 `cmpwi 0x2D / 0x31`) ----------------------
        // ⭐ VERIFIED GENUINE, NOT A PC COMPENSATION (input-vocabulary wave, 2026-08-29).
        // Four sibling screens carried an invented `case 45` arm to work around the old
        // KA_BINDINGS accept binding and all four were deleted in that wave; THIS one stays,
        // because the console body really does test both: UpdateRunning @0x824CA420 reads
        // `if (v19 == 45 || v19 == 49)`. Both ids are DWARF-named below
        // (GameSource/Input/GameInputActions.h) -- START and SELECT, two different controls
        // that this one screen deliberately treats alike.
        const s32 KI_ACTION_GUI_START  = E_GAMEINPUTACTIONS_GUI_START;    // 45 (0x2D)
        const s32 KI_ACTION_GUI_SELECT = E_GAMEINPUTACTIONS_GUI_SELECT;   // 49 (0x31)

        // ---- in-queue payload view (the queue delivers the HEADER-STRIPPED payload) ----
        // CgsGui::GuiEventControllerInputPressed, minus the header (the sibling screens'
        // ControllerButtonPayload idiom).
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // ---- out-queue wire records (TU-local per the family rule: the canonical opaque
        //      twins live in BrnGuiDemangledEventTypes.h, which is mutually exclusive with
        //      the BrnGuiEventTypeDefs.h this TU includes -- a second definition would be a
        //      live ODR fork; see BrnCarSelectVehicle_Input.cpp:141) ----------------------

        // The "clear the ticker" command: { 2, 536, 12, u8 1, u8 0 }, channel 40, 16 bytes.
        struct GuiTickerFlagsWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbFlagA;   // +0x0C
            u8 mbFlagB;   // +0x0D
            GuiTickerFlagsWire536()
                : CgsGui::GuiEvent<536>(2, 12), mbFlagA(1), mbFlagB(0) {}
        };

        // The custom ticker message payload (0x818 bytes) -- layout recovered from
        // BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940 (asserts bake
        // BrnGuiEventTypeDefs.h:390-392); the SAME TU-local mirror the three sibling
        // screens carry (BrnCarSelectVehicle_Input.cpp / BrnOnlineCustomMatch_wJ_05.cpp /
        // BrnOnlineGameOptions_wI_09.cpp).
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;     // AddString's bound (h:391)
            static const s32 KI_MAX_STRING_LENGTH = 512;   // AddString's strncpy count

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            // FLAG: four flag bytes at +0x811..+0x814 whose roles are not recovered; every
            // observed producer seeds them { 0, 0, 1, 0 } (this TU's UpdateRunning included:
            // stb 0/0/1/0 at var_8E0+1..+4 @0x824CA843..50).
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815 (sizeof == 0x818)

            // @0x823A6940 -- copy lpString into the next free 512-byte slot and record its
            // format type. The count is read as a SIGNED byte (X360 `lbz` + `extsb`).
            void AddString(const char* lpString, s32 liType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392

                std::strncpy(maacStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maiStringTypes[mi8NumStrings] = liType;
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes.
        struct GuiTickerCustomMessageWire : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload mMessage;   // +0x0C
            GuiTickerCustomMessageWire()
                : CgsGui::GuiEvent<537>(static_cast<u32>(sizeof(GuiTickerCustomMessagePayload)), 12)
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
                mMessage.maFlags[2] = 1;   // the one non-zero seed (+0x813)
            }
        };

        // The "car-select ticker closed" signal: { 1, 77, 12 }, channel 40, 16 bytes.
        // Consumer-named: BrnGameModule.cpp case 77 ("the car-select ticker closed" ->
        // DirectorInput::SetCarSelectTickerClosedThisFrame) and GameBridgeGUIToX case 77
        // (re-emitted as type 83, "signal (uninitialised payload byte)"). The X360 never
        // writes the payload byte; zeroed here rather than shipped as stack garbage.
        struct GuiCarSelectTickerClosedWire77 : public CgsGui::GuiEvent<77>
        {
            u8 maPad[4];   // +0x0C (record tail up to the 16 bytes AddEvent copies)
            GuiCarSelectTickerClosedWire77()
                : CgsGui::GuiEvent<77>(1, 12)
            { maPad[0] = 0; maPad[1] = 0; maPad[2] = 0; maPad[3] = 0; }
        };

        // KF_SCREENSKIPTIMEOUT (DWARF cpp:55) -- MEASURED: flt_82065B68 == 10.0f, the one
        // float UpdateRunning compares mrTimeScreenVisible against (both sites).
        const f32 KF_SCREENSKIPTIMEOUT = 10.0f;

        // The per-tick time step UpdateRunning accumulates: flt_820139F8 == 1/60 s.
        const f32 KF_FRAME_TIME = 0.016666668f;
    }

    // ---- GetResourcesToLoad @ 0x824B5A20 ------------------------------------------
    // This state loads its resources internally (UpdateLoadResources), so the generic loader
    // path is handed an EMPTY list -- after null-guarding both out params it writes {NULL, 0}.
    void CarSelectUnlock::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                             u32* lpuNumberOfResources) const
    {
        CGS_ASSERT(lppResourceTuples != NULL, "Invalid pointer");   // cpp:250
        CGS_ASSERT(lpuNumberOfResources != NULL, "Invalid pointer"); // cpp:251

        *lppResourceTuples    = NULL;
        *lpuNumberOfResources = 0;
    }

    // ---- OnEnter @ 0x824CA198 -----------------------------------------------------
    void CarSelectUnlock::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // X360: an inlined bare GuiEventStopAptLoadingMovie (GuiEvent<20>(1,12)) pushed onto the
        // output queue (channel 40, size 16) -- StopLoadingScreen() posts exactly that record.
        mpStateInterface->StopLoadingScreen();

        mAnimComponent.Construct(macAnimComponentName, mpStateInterface, NULL);
        mManufacturerLogo.Construct(KAC_MANUFACTURER_LOGO, mpStateInterface, NULL);
        mCarName.Construct(KAC_CAR_NAME, mpStateInterface, NULL);

        mbTickerVisible = false;   // +0x284 (cleared before the help item is constructed)
        mHelpItemContinue.Construct(KAC_HELPITEM_CONTINUE, mpStateInterface, NULL);
        mHelpItemContinue.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);

        mHelpPromptAnimComponent.Construct(macHelpPromptAnimComponentName, mpStateInterface, NULL);

        mHelpPromptVisible  = false;                        // +0x285
        mpGuiCache          = NULL;                         // +0x38
        meInternalState     = E_INTERNALSTATE_GETCACHE;     // +0x3C (0)
        mrTimeScreenVisible = 0.0f;                         // +0x280
    }

    // ---- OnLeave @ 0x824CA2E8 -----------------------------------------------------
    void CarSelectUnlock::OnLeave()
    {
        CGS_ASSERT(mpGuiCache != NULL, "NULL != mpGuiCache");   // cpp:184

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // X360: an inlined bare GuiEventPlayAptMovie (GuiEvent<18>(8,12) { name, level }) pushed
        // onto the view-state channel (41, size 20) -- PlayAptMovie() posts exactly that record.
        // The leave-movie name @0x820046A7 is MEASURED "" (wave L dump: the NUL tail after the
        // "%s..." pool string), confirming the OnlinePreEvent sibling's reading.
        mpStateInterface->PlayAptMovie("", 3);

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        meInternalState = E_INTERNALSTATE_LEFT;   // +0x3C (4)
    }

    // ---- UpdateGetCache @ 0x824C15A8 ----------------------------------------------
    // Scan the in-event queue for the GuiCache event (type 64) and latch its carried cache
    // pointer into mpGuiCache.
    void CarSelectUnlock::UpdateGetCache()
    {
        CGS_ASSERT(mpGuiCache == NULL, "NULL == mpGuiCache");   // cpp:215

        InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        if (lpEvent != NULL)
        {
            while (liEventType != KI_EVENT_GUI_CACHE)
            {
                liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
                if (lpEvent == NULL)
                    break;
            }

            if (lpEvent != NULL)
            {
                // The cache event carries the GuiCache pointer in its leading word.
                GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                CGS_ASSERT(lpCache != NULL, "NULL != lpCacheEvent->mpCachePointer");   // cpp:225
                mpGuiCache = lpCache;
            }
        }

        CGS_ASSERT(mpGuiCache != NULL, "NULL != mpGuiCache");   // cpp:234
    }

    // ---- UpdateLoadResources @ 0x824D8268 -----------------------------------------
    // Once the cache reports the unlock resources loaded, play the movie and prime the
    // expected-apt-component list with the badge + the two animation clips.
    bool CarSelectUnlock::UpdateLoadResources()
    {
        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            return false;

        PlayMovie();

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, KAC_MANUFACTURER_LOGO);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mAnimComponent.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mHelpPromptAnimComponent.GetName());
        return true;
    }

    // ---- UpdateWFInit @ 0x824B5B28 ------------------------------------------------
    // Wait for the flow's apt components to finish initialising.
    bool CarSelectUnlock::UpdateWFInit()
    {
        CGS_ASSERT(mpGuiCache != NULL, "Should have aquired GuiCache in the previous state");  // cpp:317

        return mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN);
    }

    // ---- UpdateRunning @ 0x824CA420 -----------------------------------------------
    // Drain the in-event queue while the celebration screen runs: tick the visible timer
    // (raising the CONTINUE prompt after the skip timeout), show the unlocked car's badge +
    // name on the new-car event, run the blurb ticker on the unlock event, and advance /
    // close out on player input. Reconstructed from the RAW ASM (the Hex-Rays local
    // allocation fails on this body); every branch/store walked at
    // scratchpad/finishA/GameSource_Gui_Flow_Screen_States_BrnCarSelectUnlock.cpp__v.txt.
    void CarSelectUnlock::UpdateRunning()
    {
        InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        // fcmpu + ble (skip when <= or unordered): the prompt raises only on the ordered
        // strictly-greater side, which C++ `>` matches exactly.
        mrTimeScreenVisible += KF_FRAME_TIME;
        if (mrTimeScreenVisible > KF_SCREENSKIPTIMEOUT && !mHelpPromptVisible)
        {
            mHelpItemContinue.SetItem("$GENERAL_OPTION_CONTINUE",
                                      ButtonIconComponent::E_PADBUTTON_SELECT,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpPromptAnimComponent.AddOutputAptViewState("apt_Transition", "transin", false);
            mHelpPromptVisible = true;
        }

        while (lpEvent != NULL)
        {
            switch (liEventType)
            {
                case KI_EVENT_CAR_UNLOCK_NEW_CAR:   // 73 -- the car to celebrate (8-byte CgsID)
                {
                    // Reset the skip timer and drop the CONTINUE prompt back to invisible
                    // ("" @0x820046A7, measured) while the logo animation transitions in.
                    mrTimeScreenVisible = 0.0f;
                    mHelpPromptVisible  = false;
                    mHelpItemContinue.SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                    mAnimComponent.AddOutputAptViewState("apt_Transition", "transin", false);

                    const CgsID lNewCarId = *reinterpret_cast<const CgsID*>(lpEvent);

                    // Both list fetches go through the cache's world-data controller; the
                    // X360 inlines GetWorldDataController (its "mpWorldDataController"
                    // assert bakes BrnGuiCache.h:2324 at both sites).
                    const BrnResource::VehicleList* lpVehicleList =
                        mpGuiCache->GetWorldDataController()->GetVehicleList();
                    mManufacturerLogo.Set(lpVehicleList, lNewCarId);

                    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");   // cpp:445
                    lpVehicleList = mpGuiCache->GetWorldDataController()->GetVehicleList();

                    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lNewCarId);
                    const BrnResource::VehicleListEntry* lpVehicleData =
                        (liVehicleIndex < 0) ? NULL : lpVehicleList->GetVehicleData(liVehicleIndex);
                    CGS_ASSERT(lpVehicleData != NULL, "lpVehicleData");   // cpp:451

                    // ⚠️ ENTRY guard (the SetTicker sibling's shape (b)): the console
                    // dereferences lpVehicleData unconditionally below (its assert is
                    // non-gating -- `lbz 0xE9(r29)` runs with r29 == 0 on the miss path).
                    if (lpVehicleData != NULL)
                    {
                        // A livery variant shows its PARENT car's name; finish type 2 (or no
                        // parent) keeps the event's own id (`lbz +0xE9` == muLiveryType, then
                        // `ld +0x8` == mParentId).
                        CgsID lCapsCarId = lNewCarId;
                        if (lpVehicleData->GetLiveryType() != 2 && lpVehicleData->GetParentId() != 0)
                            lCapsCarId = lpVehicleData->GetParentId();

                        char lacCarId[16];
                        CgsIDConvertToString(lCapsCarId, lacCarId);

                        char lacCapsKey[32];
                        CgsCore::SPrintf(lacCapsKey, 31, "CAR_CAPS_%s", lacCarId);
                        lacCapsKey[31] = 0;

                        mCarName.SetLocalisedText(lacCapsKey,
                                                  CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                    }
                    break;
                }

                case KI_EVENT_CAR_UNLOCK_ADVANCE:   // 74 -- leave the screen
                    SendStateEvent("ADVANCE");
                    break;

                case KI_EVENT_CONTROLLER_PRESSED:   // 6 -- skip-press clears the ticker
                {
                    // Same ordered strictly-greater compare as the prompt raise above.
                    if (mrTimeScreenVisible > KF_SCREENSKIPTIMEOUT)
                    {
                        const ControllerButtonPayload* lpButton =
                            static_cast<const ControllerButtonPayload*>(lpEvent);
                        if (lpButton->miButtonId == KI_ACTION_GUI_START ||
                            lpButton->miButtonId == KI_ACTION_GUI_SELECT)
                        {
                            GuiTickerFlagsWire536 lTickerClear;
                            mpStateInterface->GetOutputEventQueue()->AddEvent(
                                &lTickerClear, KI_CHANNEL_GUI_OUT,
                                static_cast<s32>(sizeof(lTickerClear)));
                        }
                    }
                    break;
                }

                case KI_EVENT_CAR_UNLOCK:   // 76 -- run the unlocked car's blurb ticker
                {
                    CGS_ASSERT(lpEvent != NULL, "lpCarUnlockEvent");   // cpp:367

                    {
                        GuiTickerFlagsWire536 lTickerClear;
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            &lTickerClear, KI_CHANNEL_GUI_OUT,
                            static_cast<s32>(sizeof(lTickerClear)));
                    }

                    const CgsID lUnlockedCarId = *reinterpret_cast<const CgsID*>(lpEvent);

                    char lacCarId[16];
                    CgsIDConvertToString(lUnlockedCarId, lacCarId);

                    char lacBlurbKey[32];
                    CgsCore::SPrintf(lacBlurbKey, 31, "CAR_BLURB_%s", lacCarId);
                    lacBlurbKey[31] = 0;

                    // Format type 2 -- the same pass-through word the SetTicker sibling posts.
                    GuiTickerCustomMessageWire lTicker;
                    lTicker.mMessage.AddString(lacBlurbKey, 2);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        &lTicker, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lTicker)));

                    mbTickerVisible = true;
                    break;
                }

                case KI_EVENT_TICKER_ONOFF:   // 538 -- the ticker just closed
                {
                    // Leading payload byte == the ticker-active flag (the HudMessageAnalyzer
                    // reads the same record's first byte).
                    const bool lbTickerActive = (*reinterpret_cast<const u8*>(lpEvent) != 0);
                    if (mbTickerVisible && !lbTickerActive)
                    {
                        mbTickerVisible = false;
                        mHelpPromptAnimComponent.AddOutputAptViewState("apt_Transition",
                                                                       "transout", false);

                        GuiCarSelectTickerClosedWire77 lTickerClosed;
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            &lTickerClosed, KI_CHANNEL_GUI_OUT,
                            static_cast<s32>(sizeof(lTickerClosed)));
                    }
                    break;
                }

                default:
                    break;
            }

            liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }
}
