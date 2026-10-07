// BrnGui::ReplayOptions -- the "RE_OPTIONS" screen state, reconstructed from the console build.

#include "GameSource/Gui/Flow/Screen/States/Replays/BrnReplayOptions.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                         // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                        // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"    // StateInterface / GuiEventClearScreenSet
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                             // GuiEventActivateCrashNav / GuiOverlayRequest / GuiFlow
#include "GameSource/Gui/BrnGuiShared.h"                                    // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                              // EGameInputActions

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;

    const s32 KI_CHANNEL_GUI_OUT    = 40;
    const s32 KI_CHANNEL_VIEW_STATE = 41;

    const s32 KI_EVENT_CONTROLLER_PRESSED = 6;

    // gGuiResourceIdentifier ids: [157] "ReplaysOptions" (level 3), [162] "ReplaysInfo" (level 4).
    const u32 KU_REPLAYS_OPTIONS_RESOURCE = 157;
    const u32 KU_REPLAYS_INFO_RESOURCE    = 162;
    const s32 KI_OPTIONS_MOVIE_LEVEL      = 3;
    const s32 KI_INFO_MOVIE_LEVEL         = 4;

    // The two options each toggle offers; option 0 of the on/off rows is "on".
    const s32 KI_NUM_TOGGLE_OPTIONS = 2;
    const s32 KI_OPTION_ON          = 0;
    const s32 KI_OPTION_PSP_QUALITY = 1;

    const char* KAPC_CAMERA_OPTIONS[KI_NUM_TOGGLE_OPTIONS]  = { "$CHASE_CAMERA", "$BUMPER_CAMERA" };
    const char* KAPC_QUALITY_OPTIONS[KI_NUM_TOGGLE_OPTIONS] = { "$MAXIMUM_QUALITY", "$PSP_QUALITY" };
    const char* KAPC_ON_OFF_OPTIONS[KI_NUM_TOGGLE_OPTIONS]  = { "$BUTTON_ON", "$BUTTON_OFF" };

    // The in-queue delivers the controller press without its GuiEvent header.
    struct ControllerButtonPayload : public CgsModule::Event
    {
        s32 miPadId;      // +0x00
        s32 miButtonId;   // +0x04 (EGameInputActions)
    };

    // GuiReplayPlayReelEvent (console id 526): { 8, 526, 12 } + the reel index and three flag
    // bytes, 20 bytes. The console never writes the fourth byte.
    struct GuiReplayPlayReelWire : public CgsGui::GuiEvent<526>
    {
        s32 miReel;          // +0x0C
        u8  mbExport;        // +0x10 (the cache's exporting byte)
        u8  mbFlag11;        // +0x11 FLAG: role not recovered; always 1 here
        u8  mbPspQuality;    // +0x12 (the export-quality toggle is on "$PSP_QUALITY")
        u8  muReserved;      // +0x13 (never written by the console)
        GuiReplayPlayReelWire()
            : CgsGui::GuiEvent<526>(8, 12), miReel(0), mbExport(0), mbFlag11(0),
              mbPspQuality(0), muReserved(0) {}
    };

    // The director's camera-type request (console id 591): { 4, 591, 12 } + the camera, 16 bytes.
    struct GuiEventSetCameraTypeWire : public CgsGui::GuiEvent<591>
    {
        s32 miCameraType;   // +0x0C (0 chase, 1 bumper)
        explicit GuiEventSetCameraTypeWire(s32 liCameraType)
            : CgsGui::GuiEvent<591>(4, 12), miCameraType(liCameraType) {}
    };

    // OutputGuiEvent<GuiOverlayRequest>: { 288, 184, 16, <pad>, request }, 304 bytes.
    struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
    {
        u32               muPad0C;    // +0x0C
        GuiOverlayRequest mRequest;   // +0x10
        GuiOverlayRequestWire()
            : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16), muPad0C(0) {}
    };

    void PostClearScreen(CgsGui::StateInterface* lpStateInterface, s32 liChannel,
                         CgsGui::GuiEventClearScreenSet::EClearScreen leClearScreen)
    {
        CgsGui::GuiEventClearScreenSet lEvent;
        lEvent.meClearScreen = leClearScreen;
        lEvent.mfAlpha       = 1.0f;
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            &lEvent, liChannel, static_cast<s32>(sizeof(lEvent)));
    }

    template <typename TWire>
    void PostOut(CgsGui::StateInterface* lpStateInterface, TWire& lrWire)
    {
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            &lrWire, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lrWire)));
    }
}

const s32 ReplayOptions::maiEventToObserve[1] = { KI_EVENT_CONTROLLER_PRESSED };
const s32 ReplayOptions::miNumEventsObserved  = 1;

const CgsGui::sResourceTuple ReplayOptions::maResourcesToLoad[1] =
{
    { KU_REPLAYS_OPTIONS_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT },
};
const u32 ReplayOptions::muNumResourcesToLoad = 1;

const CgsGui::sResourceTuple ReplayOptions::maInfoResourcesToLoad[1] =
{
    { KU_REPLAYS_INFO_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT_LOADING_SCREEN },
};

void ReplayOptions::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                       u32* lpuNumberOfResources) const
{
    *lppResourceTuples    = maResourcesToLoad;
    *lpuNumberOfResources = muNumResourcesToLoad;
}

void ReplayOptions::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mpGuiCache      = mpStateInterface->GetAccessPointers()->GetGuiCache();
    meInternalState = E_INTERNALSTATE_LOADRESOURCES;

    mOptionToggles.Construct("MenuToggle", mpStateInterface, E_OPTION_TOGGLE_COUNT, 0,
                             Selectable::K_INVALID_ID);
    mOptionToggles.SetupGroup(E_OPTION_TOGGLE_COUNT, true);

    // This screen posts the clear-screen record on the plain out channel.
    PostClearScreen(mpStateInterface, KI_CHANNEL_GUI_OUT,
                    CgsGui::GuiEventClearScreenSet::E_CLEAR_SCREEN_INACTIVE);

    GuiEventActivateCrashNav lDeactivate(false);
    PostOut(mpStateInterface, lDeactivate);

    // Back from an export: confirm it.
    if (mpGuiCache->mbReplayExporting)
    {
        GuiOverlayRequestWire lOverlay;
        lOverlay.mRequest.Construct("ReplayExOK");
        PostOut(mpStateInterface, lOverlay);
        mpGuiCache->mbReplayExporting = false;
    }
}

void ReplayOptions::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mpStateInterface->PlayAptMovie("", KI_OPTIONS_MOVIE_LEVEL);
    meInternalState = E_INTERNALSTATE_LEFT;
}

void ReplayOptions::Update()
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

bool ReplayOptions::UpdateLoadResources()
{
    if (!mpGuiCache->EnsureResourcesAreLoaded(maInfoResourcesToLoad, 1))
        return false;
    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, 1))
        return false;

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    mOptionToggles.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);

    mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_OPTIONS_RESOURCE],
                                   KI_OPTIONS_MOVIE_LEVEL);
    if (!mpGuiCache->IsReplayInfoVisible())
    {
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_INFO_RESOURCE],
                                       KI_INFO_MOVIE_LEVEL);
        mpGuiCache->mbReplayInfoVisible = true;
    }
    return true;
}

bool ReplayOptions::UpdateWFInit()
{
    if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        return false;

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

    mOptionToggles.SetupToggle(E_OPTION_TOGGLE_CAMERA, KI_NUM_TOGGLE_OPTIONS, true,
                               "$CAMERA", KAPC_CAMERA_OPTIONS, 0);
    mOptionToggles.SetupToggle(E_OPTION_TOGGLE_EXPORT_QUALITY, KI_NUM_TOGGLE_OPTIONS, true,
                               "$EXPORT_QUALITY", KAPC_QUALITY_OPTIONS, 0);
    mOptionToggles.SetupToggle(E_OPTION_TOGGLE_DISPLAY_HUD, KI_NUM_TOGGLE_OPTIONS, true,
                               "$DISPLAY_REPLAY_HUD", KAPC_ON_OFF_OPTIONS, 0);
    mOptionToggles.SetupToggle(E_OPTION_TOGGLE_SHOW_CREDITS, KI_NUM_TOGGLE_OPTIONS, true,
                               "$SHOW_CREDITS", KAPC_ON_OFF_OPTIONS, 0);
    mOptionToggles.SetupToggle(E_OPTION_TOGGLE_SHOW_PLAYER_NAMES, KI_NUM_TOGGLE_OPTIONS, true,
                               "$SHOW_PLAYER_NAMES", KAPC_ON_OFF_OPTIONS, 0);

    mOptionToggles.HighlightItem(E_OPTION_TOGGLE_CAMERA, mpGuiCache->miReplayCamera);
    mOptionToggles.HighlightItem(E_OPTION_TOGGLE_DISPLAY_HUD, !mpGuiCache->mbReplayDisplayHud);
    mOptionToggles.HighlightItem(E_OPTION_TOGGLE_EXPORT_QUALITY,
                                 !mpGuiCache->mbReplayExportMaximumQuality);
    mOptionToggles.HighlightItem(E_OPTION_TOGGLE_SHOW_PLAYER_NAMES,
                                 !mpGuiCache->mbRenderReplayPlayerNames);
    mOptionToggles.HighlightItem(E_OPTION_TOGGLE_SHOW_CREDITS, !mpGuiCache->mbReplayShowCredits);
    return true;
}

void ReplayOptions::UpdateRunning()
{
    InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liEventId == KI_EVENT_CONTROLLER_PRESSED)
            HandleControllerInputPressed(lpEvent);
    }

    mOptionToggles.Update();
}

void ReplayOptions::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
{
    if (meInternalState != E_INTERNALSTATE_RUNNING)
        return;

    const ControllerButtonPayload* lpPress = static_cast<const ControllerButtonPayload*>(lpEvent);
    switch (lpPress->miButtonId)
    {
    case E_GAMEINPUTACTIONS_GUI_UP:
        mOptionToggles.HighlightPrevious(false);
        break;

    case E_GAMEINPUTACTIONS_GUI_DOWN:
        mOptionToggles.HighlightNext(false);
        break;

    case E_GAMEINPUTACTIONS_GUI_LEFT:
        mOptionToggles.HighlightPreviousItem();
        break;

    case E_GAMEINPUTACTIONS_GUI_RIGHT:
        mOptionToggles.HighlightNextItem();
        break;

    case E_GAMEINPUTACTIONS_GUI_SELECT:
        mpGuiCache->mbReplayExporting = false;
        PlayReel(mpGuiCache->miReplayCurrentSlot);
        break;

    case E_GAMEINPUTACTIONS_GUI_CANCEL:
        SendStateEvent("GO_BACK");
        break;

    case E_GAMEINPUTACTIONS_GUI_OPTION1:
        mpGuiCache->mbReplayExporting = true;
        PlayReel(mpGuiCache->miReplayCurrentSlot);
        break;

    default:
        break;
    }
}

void ReplayOptions::PlayReel(s32 liSlotIndex)
{
    CGS_ASSERT(liSlotIndex >= 0, "liSlotIndex >= 0");
    CGS_ASSERT(liSlotIndex < mpGuiCache->GetReplayNumberSlotsUsed(),
               "liSlotIndex < mpGuiCache->GetReplayNumberSlotsUsed()");

    const MenuToggle* lpQualityToggle = mOptionToggles.GetSelectable(E_OPTION_TOGGLE_EXPORT_QUALITY);

    GuiReplayPlayReelWire lPlayReel;
    lPlayReel.miReel       = mpGuiCache->ReplayConvertGuiSlotIndexToReelIndex(liSlotIndex);
    lPlayReel.mbExport     = mpGuiCache->mbReplayExporting;
    lPlayReel.mbFlag11     = 1;
    lPlayReel.mbPspQuality = lpQualityToggle->mItemText.GetHighlightedIndex() == KI_OPTION_PSP_QUALITY;
    PostOut(mpStateInterface, lPlayReel);

    GuiEventActivateCrashNav lActivate(true);
    PostOut(mpStateInterface, lActivate);

    CGS_ASSERT(mpGuiCache->IsReplayInfoVisible(), "mpGuiCache->IsReplayInfoVisible()");
    mpStateInterface->PlayAptMovie("", KI_INFO_MOVIE_LEVEL);
    mpGuiCache->mbReplayInfoVisible = false;
    mpGuiCache->UnloadResources(maInfoResourcesToLoad, 1);

    mpGuiCache->mbRenderReplayPlayerNames =
        mOptionToggles.GetSelectable(E_OPTION_TOGGLE_SHOW_PLAYER_NAMES)->mItemText.GetHighlightedIndex() == KI_OPTION_ON;
    mpGuiCache->miReplayCamera =
        mOptionToggles.GetSelectable(E_OPTION_TOGGLE_CAMERA)->mItemText.GetHighlightedIndex();
    mpGuiCache->mbReplayDisplayHud =
        mOptionToggles.GetSelectable(E_OPTION_TOGGLE_DISPLAY_HUD)->mItemText.GetHighlightedIndex() == KI_OPTION_ON;
    mpGuiCache->mbReplayExportMaximumQuality =
        mOptionToggles.GetSelectable(E_OPTION_TOGGLE_EXPORT_QUALITY)->mItemText.GetHighlightedIndex() == KI_OPTION_ON;
    mpGuiCache->mbReplayShowCredits =
        mOptionToggles.GetSelectable(E_OPTION_TOGGLE_SHOW_CREDITS)->mItemText.GetHighlightedIndex() == KI_OPTION_ON;

    GuiEventSetCameraTypeWire lCamera(mpGuiCache->miReplayCamera);
    PostOut(mpStateInterface, lCamera);

    PostClearScreen(mpStateInterface, KI_CHANNEL_VIEW_STATE,
                    CgsGui::GuiEventClearScreenSet::E_CLEAR_SCREEN_ACTIVE);

    SendStateEvent("ADVANCE");
}
}
