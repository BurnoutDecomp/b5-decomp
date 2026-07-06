// ===================================================================================
// BrnGui::OnlineNews -- out-of-line bodies for the online-news screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   HandleControllerAxis @0x82486B58, HandleControllerInput @0x824AA518,
//   HandleGuiCacheEvent @0x82486C48, HandleNewsAndTOSEvent @0x824AA5D0, OnEnter @0x824A0180.
// (CheckForCompletedLoads / HandleControllerInputSelectParams / ShowText are SKIPPED --
//  declared-only.)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineNews.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiStackEventQueue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache / GuiFlow

namespace BrnGui
{
    // ---- statics -----------------------------------------------------------------
    // Stick dead-zone (HandleControllerAxis rodata). Value not recovered in this slice;
    // 0.0f placeholder (any non-zero axis registers) -- adopt the XEX rodata when decoded.
    const f32 OnlineNews::KF_AXIS_DEAD_ZONE = 0.0f;

    const s32 OnlineNews::maiEventToObserve[] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // @0x8205F7EC (8 entries; values not recovered)
    const s32 OnlineNews::miNumEventsObserved = 8;

    const CgsGui::sResourceTuple OnlineNews::maResourceTuplesToLoad[] =
    {
        { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
    };
    const s32 OnlineNews::miNumResourcesToLoad = 1;

    // ---- HandleControllerAxis @ 0x82486B58 ----------------------------------------
    void OnlineNews::HandleControllerAxis(const GuiEventControllerAxis* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL,
                   "Invalid event in SelectRoutes::HandleControllerAxis");

        // Zero the latch until the screen reaches param selection.
        if (meSubState < E_SUBSTATE_SELECTING_PARAMS)
        {
            mfLastAxisValue = 0.0f;
            return;
        }

        // Axis discriminator at event+0, value at event+8 (raw payload view); accept axes 1/2.
        const s32 liAxis =
            *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 0);
        if (liAxis == 1 || liAxis == 2)
        {
            const f32 lfValue =
                *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpEvent) + 8);
            mfLastAxisValue =
                (lfValue < -KF_AXIS_DEAD_ZONE || lfValue > KF_AXIS_DEAD_ZONE) ? lfValue : 0.0f;
        }
        else
        {
            mfLastAxisValue = 0.0f;
        }
    }

    // ---- HandleControllerInput @ 0x824AA518 ---------------------------------------
    void OnlineNews::HandleControllerInput(const GuiEventControllerInputPressed* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL,
                   "Invalid event sent to OnlineNews::HandleControllerInput");

        if (meSubState == E_SUBSTATE_SELECTING_PARAMS)
            HandleControllerInputSelectParams(lpEvent);
    }

    // ---- HandleGuiCacheEvent @ 0x82486C48 -----------------------------------------
    void OnlineNews::HandleGuiCacheEvent(const GuiEventCache* lpEvent)
    {
        // The event's carried cache rides in its leading word (the X360 asserts on *a2).
        GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
        CGS_ASSERT(lpCache != NULL, "Invalid cache in OnlineNews::HandleGuiCacheEvent");

        if (mpGuiCache == NULL)
        {
            mpGuiCache = lpCache;
            mpGuiCache->AppendExpectedAptComponent(static_cast<GuiFlow>(0), mToggleText.GetName());
            mpGuiCache->AppendExpectedAptComponent(static_cast<GuiFlow>(0), mNewsText.GetName());
        }
    }

    // ---- HandleNewsAndTOSEvent @ 0x824AA5D0 ---------------------------------------
    void OnlineNews::HandleNewsAndTOSEvent(const GuiEventNetworkNewsAndTOS* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL, "Invalid event in OnlineNews::HandleNewsAndTOSEvent");

        const s32 liStatus = *reinterpret_cast<const s32*>(lpEvent);
        switch (liStatus)
        {
            case 2: maeDownloadState[E_TOGGLE_ITEM_NEWS] = E_DOWNLOAD_STATE_DONE;   break;
            case 3: maeDownloadState[E_TOGGLE_ITEM_TOS]  = E_DOWNLOAD_STATE_DONE;   break;
            case 4: maeDownloadState[E_TOGGLE_ITEM_NEWS] = E_DOWNLOAD_STATE_FAILED; break;
            case 5: maeDownloadState[E_TOGGLE_ITEM_TOS]  = E_DOWNLOAD_STATE_FAILED; break;
            default: break;
        }

        ShowText();
    }

    // ---- OnEnter @ 0x824A0180 -----------------------------------------------------
    void OnlineNews::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mToggleText.Construct("ToggleText", mpStateInterface, 0);
        mNewsText.Construct("NewsText", mpStateInterface, 0);

        mfLastAxisValue     = 0.0f;   // +0x2A0
        mfScrollAmount      = 0.0f;   // +0x29C
        meSubState          = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache          = NULL;
        meCurrentToggleItem = E_TOGGLE_ITEM_NEWS;
        maeDownloadState[0] = E_DOWNLOAD_STATE_DOWNLOADING;
        maeDownloadState[1] = E_DOWNLOAD_STATE_DOWNLOADING;

        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        // Record 1: { 4, 266, 12, payload=0 } -- channel 40, 16 bytes.
        u32 lauRecord1[4] = { 4u, 266u, 12u, 0u };
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord1), 40, 16);

        // Record 2: { 20, 323, 12 } + the embedded { 42, 266, 12, 0, x } sub-record the X360
        // copies from the reused scratch (last word uninitialised -> reproduced as 0). Channel 40, 32 bytes.
        u32 lauRecord2[8] = { 20u, 323u, 12u, 42u, 266u, 12u, 0u, 0u };
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauRecord2), 40, 32);
    }
}
