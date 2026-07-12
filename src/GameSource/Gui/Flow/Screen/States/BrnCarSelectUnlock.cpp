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
//   UpdateWFInit        @0x824B5B28
//
// NOT BODIED HERE (declared in the header; left for a later wave):
//   * UpdateRunning @0x824CA420 -- the running-state in-queue drain. It posts two GUI event
//     types this slice cannot ground: an event-TYPE-77 output record (no committed
//     GuiEvent<77> / demangled struct) and calls BrnGui::GuiEventTickerCustomMessage::AddString
//     (a real method proven by the call site but with no recovered body/declaration on the
//     committed GuiEventTickerCustomMessage struct). Its X360 body is also a "local variable
//     allocation has failed" giant; reconstructing the un-homed event records from this call
//     site alone would be invention. Bodied when event-77 + AddString land.
//   * Update / PlayMovie -- their own ledger slices.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectUnlock.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue / Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (register / stop-loading / play-apt)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow

namespace BrnGui
{
    // ---- statics (DWARF cpp:26-53; .rdata) ----------------------------------------
    // The 6 event ids this state registers for (X360 @0x82066054). Values not recovered
    // from the .data blob; FLAG: zero-filled per the OnlinePreEvent sibling precedent.
    const s32 CarSelectUnlock::maiEventToObserve[6] = { 0, 0, 0, 0, 0, 0 };
    const s32 CarSelectUnlock::miNumEventsObserved  = 6;

    // The two apt resources the unlock screen loads internally (X360 @0x82F26D78, count 2).
    // FLAG: the resource ids/types are not attested in scope (only the base address + count 2);
    // modelled as apt-resource placeholders, adopted with the XEX-recovered tuples when decoded.
    const CgsGui::sResourceTuple CarSelectUnlock::maResourcesToLoad[] =
    {
        { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: resource id unattested in scope
        { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: resource id unattested in scope
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

    // The GuiCache event carried on the in-queue is event type 64.
    static const s32 KI_EVENT_GUI_CACHE = 64;

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
        // FLAG: the leave-movie name (rodata @0x820046A7) is not recovered; modelled as "" (the
        // same empty string the OnlinePreEvent sibling reconstructs for @0x820046A7).
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
}
