// BrnGui::ReplayClipsOnline -- the "RE_CLIPS_ON" screen state, reconstructed from the console build.

#include "GameSource/Gui/Flow/Screen/States/Replays/BrnReplayClipsOnline.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                         // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                        // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"    // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                             // GuiFlow
#include "GameSource/Gui/BrnGuiShared.h"                                    // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                              // EGameInputActions
#include "GameSource/Replays/BrnReplayReels.h"                              // BrnReplays::Reel

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;

    const s32 KI_CHANNEL_GUI_OUT = 40;

    const s32 KI_EVENT_CONTROLLER_PRESSED = 6;

    // gGuiResourceIdentifier ids: [156] "ReplaysClipsOnline" (level 3), [162] "ReplaysInfo" (level 4).
    const u32 KU_REPLAYS_CLIPS_RESOURCE = 156;
    const u32 KU_REPLAYS_INFO_RESOURCE  = 162;
    const s32 KI_CLIPS_MOVIE_LEVEL      = 3;
    const s32 KI_INFO_MOVIE_LEVEL       = 4;

    const s32 KI_NUM_MENU_ROWS = 6;   // one row per replay reel

    // The in-queue delivers the controller press without its GuiEvent header.
    struct ControllerButtonPayload : public CgsModule::Event
    {
        s32 miPadId;      // +0x00
        s32 miButtonId;   // +0x04 (EGameInputActions)
    };

    // GuiReplaySetModeEvent (console id 525): { 4, 525, 12 } + the mode word, 16 bytes.
    struct GuiReplaySetModeWire : public CgsGui::GuiEvent<525>
    {
        s32 miMode;   // +0x0C
        explicit GuiReplaySetModeWire(s32 liMode) : CgsGui::GuiEvent<525>(4, 12), miMode(liMode) {}
    };

    // GuiReplayDeleteReelEvent (console id 527): { 4, 527, 12 } + the reel index, 16 bytes.
    struct GuiReplayDeleteReelWire : public CgsGui::GuiEvent<527>
    {
        s32 miReel;   // +0x0C
        explicit GuiReplayDeleteReelWire(s32 liReel) : CgsGui::GuiEvent<527>(4, 12), miReel(liReel) {}
    };

    template <typename TWire>
    void PostOut(CgsGui::StateInterface* lpStateInterface, TWire& lrWire)
    {
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            &lrWire, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lrWire)));
    }
}

const s32 ReplayClipsOnline::maiEventToObserve[1] = { KI_EVENT_CONTROLLER_PRESSED };
const s32 ReplayClipsOnline::miNumEventsObserved  = 1;

const CgsGui::sResourceTuple ReplayClipsOnline::maResourcesToLoad[1] =
{
    { KU_REPLAYS_CLIPS_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT },
};
const u32 ReplayClipsOnline::muNumResourcesToLoad = 1;

const CgsGui::sResourceTuple ReplayClipsOnline::maInfoResourcesToLoad[1] =
{
    { KU_REPLAYS_INFO_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT_LOADING_SCREEN },
};

void ReplayClipsOnline::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                     u32* lpuNumberOfResources) const
{
    *lppResourceTuples    = maResourcesToLoad;
    *lpuNumberOfResources = muNumResourcesToLoad;
}

void ReplayClipsOnline::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mpGuiCache      = mpStateInterface->GetAccessPointers()->GetGuiCache();
    meInternalState = E_INTERNALSTATE_LOADRESOURCES;

    mSlotMenu.Construct("MenuItem", mpStateInterface, KI_NUM_MENU_ROWS, 0, Selectable::K_INVALID_ID);
    mButtonsAnimComponent.Construct("buttons_anim", mpStateInterface, 0);
    miNumSlotsShown = 0;
}

void ReplayClipsOnline::OnLeave()
{
    if (meInternalState == E_INTERNALSTATE_RUNNING)
        mSlotMenu.Clear();

    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mpStateInterface->PlayAptMovie("", KI_CLIPS_MOVIE_LEVEL);
    meInternalState = E_INTERNALSTATE_LEFT;
}

void ReplayClipsOnline::Update()
{
    InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

    switch (meInternalState)
    {
    case E_INTERNALSTATE_LOADRESOURCES:
        meInternalState = E_INTERNALSTATE_LOADRESOURCES;
        if (!UpdateLoadResources())
            break;
        /* fallthrough */

    case E_INTERNALSTATE_WFINIT:
        meInternalState = E_INTERNALSTATE_WFINIT;
        if (!UpdateWFInit())
            break;
        /* fallthrough */

    case E_INTERNALSTATE_RUNNING:
        meInternalState = E_INTERNALSTATE_RUNNING;
        UpdateRunning();
        break;

    case E_INTERNALSTATE_LEFT:
        break;

    default:
        CGS_ASSERT(false, "Invalid internal state : ");
        break;
    }

    lpInQueue->Clear();
}

bool ReplayClipsOnline::UpdateLoadResources()
{
    if (!mpGuiCache->EnsureResourcesAreLoaded(maInfoResourcesToLoad, 1))
        return false;
    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, 1))
        return false;

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    mSlotMenu.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);

    mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_CLIPS_RESOURCE],
                                   KI_CLIPS_MOVIE_LEVEL);
    if (!mpGuiCache->IsReplayInfoVisible())
    {
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_INFO_RESOURCE],
                                       KI_INFO_MOVIE_LEVEL);
        mpGuiCache->mbReplayInfoVisible = true;
    }
    return true;
}

bool ReplayClipsOnline::UpdateWFInit()
{
    if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        return false;

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    RefreshSlots();
    return true;
}

void ReplayClipsOnline::UpdateRunning()
{
    InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liEventId == KI_EVENT_CONTROLLER_PRESSED && HandleControllerInputPressed(lpEvent))
            return;
    }

    if (miNumSlotsShown != mpGuiCache->GetReplayNumberSlotsUsed())
        RefreshSlots();
    mSlotMenu.Update();
}

void ReplayClipsOnline::RefreshSlots()
{
    miNumSlotsShown = mpGuiCache->GetReplayNumberSlotsUsed();
    mSlotMenu.SetupMenu(miNumSlotsShown, true);

    for (s32 liSlotIndex = 0; liSlotIndex < miNumSlotsShown; ++liSlotIndex)
    {
        // The cache's inline slot accessor (BrnGuiCache.h) bounds-checks the slot.
        CGS_ASSERT(liSlotIndex >= 0, "liSlotIndex >= 0");
        CGS_ASSERT(liSlotIndex < mpGuiCache->miReplaySlotsUsed, "liSlotIndex < miReplaySlotsUsed");
        mSlotMenu.SetText(liSlotIndex, mpGuiCache->maReplayReelForSlot[liSlotIndex]->macName);
    }

    const char* lpacViewState = "empty";
    if (miNumSlotsShown > 0)
    {
        s32 liSlot = miNumSlotsShown - 1;
        if (liSlot >= mpGuiCache->miReplayCurrentSlot)
            liSlot = mpGuiCache->miReplayCurrentSlot;
        if (liSlot < 0)
            liSlot = 0;

        mSlotMenu.HighlightIndex(liSlot);
        lpacViewState = "items";
        mpGuiCache->miReplayCurrentSlot = liSlot;
    }
    mButtonsAnimComponent.AddOutputAptViewState("apt_Transition", lpacViewState, false);
}

bool ReplayClipsOnline::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
{
    if (meInternalState != E_INTERNALSTATE_RUNNING)
        return false;

    const ControllerButtonPayload* lpPress = static_cast<const ControllerButtonPayload*>(lpEvent);
    switch (lpPress->miButtonId)
    {
    case E_GAMEINPUTACTIONS_GUI_UP:
        if (mpGuiCache->GetReplayNumberSlotsUsed() > 0 && mSlotMenu.HighlightPrevious() == true)
            mpGuiCache->miReplayCurrentSlot = mSlotMenu.GetHighlightedIndex();
        return false;

    case E_GAMEINPUTACTIONS_GUI_DOWN:
        if (mpGuiCache->GetReplayNumberSlotsUsed() > 0 && mSlotMenu.HighlightNext() == true)
            mpGuiCache->miReplayCurrentSlot = mSlotMenu.GetHighlightedIndex();
        return false;

    case E_GAMEINPUTACTIONS_GUI_CANCEL:
    {
        GuiReplaySetModeWire lSetMode(0);
        PostOut(mpStateInterface, lSetMode);

        CGS_ASSERT(mpGuiCache->IsReplayInfoVisible(), "mpGuiCache->IsReplayInfoVisible()");
        mpStateInterface->PlayAptMovie("", KI_INFO_MOVIE_LEVEL);
        mpGuiCache->mbReplayInfoVisible = false;
        mpGuiCache->UnloadResources(maInfoResourcesToLoad, 1);
        mpGuiCache->miReplayCurrentSlot = -1;
        SendStateEvent("GO_BACK");
        return true;
    }

    case E_GAMEINPUTACTIONS_GUI_OPTION1:
        if (mpGuiCache->GetReplayNumberSlotsUsed() > 0)
        {
            GuiReplayDeleteReelWire lDelete(
                mpGuiCache->ReplayConvertGuiSlotIndexToReelIndex(mSlotMenu.GetHighlightedIndex()));
            PostOut(mpStateInterface, lDelete);
        }
        return false;

    default:
        return false;
    }
}
}
