#include "GameSource/Gui/Flow/Screen/States/BrnOnlineViewChallenges.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiShared.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineViewChallenges.cpp"
namespace BrnGui {
namespace {
    // The player-count strings (read from the image).
    const char* KAPC_PLAYER_COUNTS[] = {"2", "3", "4", "5", "6", "7", "8"};
    template<class T> void Output(CgsGui::StateInterface* state, T& event)
    {
        state->OutputGuiEvent(event);
    }
}
const s32 OnlineViewChallenges::maiEventToObserve[5] = {14, 21, 6, 64, 581};

// The challenges screen's apt package (read from the image).
const CgsGui::sResourceTuple OnlineViewChallenges::maResourceTuplesToLoad[] =
    { { 187, CgsGui::E_GUI_RESOURCETYPE_APT } };
const s32 OnlineViewChallenges::miNumResourcesToLoad = 1;

// ARTIST compiler ctor 0x825005C0; native members own the original subobjects.
OnlineViewChallenges::OnlineViewChallenges() : CgsGui::State() {}

// ARTIST 0x824A1D68.
void OnlineViewChallenges::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, 5);
    mChallengeToggle.Construct("optionToggle_0", mpStateInterface, 0, Selectable::K_INVALID_ID);
    mChallengeListComponent.Construct("ChallengeList", mpStateInterface, 0);
    mUpArrowAnimation.Construct("UpArrowAnimation", mpStateInterface, 0);
    mDownArrowAnimation.Construct("DownArrowAnimation", mpStateInterface, 0);
    meSubState = E_SUBSTATE_LOADING_SCREEN;
    mpGuiCache = 0;
    // DWARF locals cpp:104 / :109 / :113.
    GuiEventActivateCrashNav lActivateCrashNav;
    lActivateCrashNav.muActivate = 0u;
    lActivateCrashNav.muParam = 0;
    mpStateInterface->OutputGuiEvent(lActivateCrashNav);

    GuiEventShowHideHud lShowHideHudEvent;
    lShowHideHudEvent.mbShow = false;
    mpStateInterface->OutputGuiEvent(lShowHideHudEvent);

    GuiEventFreeburnChallengeRequestEveryPlayerStatus lRequestCompletionStatus;
    mpStateInterface->OutputGuiEvent(lRequestCompletionStatus);
}

// ARTIST 0x824A1EC8.
void OnlineViewChallenges::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, 5);
    mpStateInterface->PlayAptMovie("", 3);
    // DWARF local cpp:209.
    GuiEventTickerClearMessages lClearTicker;
    lClearTicker.mbForceFadeOut = false;
    lClearTicker.mbDeleteChallengeMessages = false;
    mpStateInterface->OutputGuiEvent(lClearTicker);
}

// ARTIST 0x82487A30.
void OnlineViewChallenges::HandleGuiCacheEvent(const CgsModule::Event* event)
{
    GuiCache* cache = *reinterpret_cast<GuiCache* const*>(event);
    CGS_ASSERT(cache != 0, "Invalid cache in OnlineViewChallenges::HandleGuiCacheEvent");
    if (mpGuiCache == 0)
    {
        mpGuiCache = cache;
        mChallengeToggle.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, cache, true);
        cache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mChallengeListComponent.GetName());
        cache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mUpArrowAnimation.GetName());
        cache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDownArrowAnimation.GetName());
    }
}

// ARTIST 0x824A1F98.
void OnlineViewChallenges::CheckForCompletedLoads()
{
    CGS_ASSERT(mpGuiCache != 0, CGS_ASSERT_MESSAGE_AT("mpGuiCache", CGS_ARTIST_FILE, 227 /* ARTIST 0x824A1FD0 */));
    if (meSubState == E_SUBSTATE_LOADING_SCREEN)
    {
        if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad, miNumResourcesToLoad))
        {
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[maResourceTuplesToLoad[0].muId], 3);
            meSubState = E_SUBSTATE_LOADING_COMPONENTS;
        }
    }
    else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS && mpGuiCache != 0 &&
             mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
    {
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        meSubState = E_SUBSTATE_SELECTING_PARAMS;
        ShowScreen();
    }
}

// ARTIST 0x8248E1F8.
void OnlineViewChallenges::ShowScreen()
{
    u64 ids[7] = {2, 3, 4, 5, 6, 7, 8};
    mChallengeToggle.Clear();
    mChallengeToggle.SetActive(true);
    mChallengeToggle.SetupMenuToggle(7, true, "$ONLINE_CHALLENGE_PLAYERS", KAPC_PLAYER_COUNTS, ids);
    mChallengeToggle.SetHighlightable(true);
    mChallengeToggle.SetHighlighted(true);
    mChallengeToggle.SetSelectable(true);
    if (mChallengeToggle.mItemText.HighlightId(2)) mChallengeToggle.SetDirty();
    mChallengeListComponent.Setup(mpGuiCache, static_cast<s32>(mChallengeToggle.GetHighlightedId()), false);
    UpdateArrows();
}

void OnlineViewChallenges::UpdateArrows()
{
    // ARTIST 0x8248E340 / 0x824A2280 (inlined IsAtTop/BottomOfList).
    mUpArrowAnimation.AddOutputAptViewState("apt_Transition", mChallengeListComponent.IsAtTopOfList() ? "invisible" : "visible", false);
    mDownArrowAnimation.AddOutputAptViewState("apt_Transition", mChallengeListComponent.IsAtBottomOfList() ? "invisible" : "visible", false);
}

// ARTIST 0x824A2090.
void OnlineViewChallenges::HandleControllerInputSelectParams(const s32* event)
{
    CGS_ASSERT(event != 0, "Invalid event sent to OnlineViewChallenges::HandleControllerInputSelectParams");
    bool changed = false;
    switch (event[1])
    {
    case 41: changed = mChallengeListComponent.HighlightPrevious(); break;
    case 42: changed = mChallengeListComponent.HighlightNext(); break;
    case 43: case 44:
        if (event[1] == 43 ? mChallengeToggle.HighlightPrevious() : mChallengeToggle.HighlightNext())
        {
            mChallengeListComponent.Setup(mpGuiCache, static_cast<s32>(mChallengeToggle.GetHighlightedId()), false);
            changed = true;
        }
        break;
    case 50:
        if (mpGuiCache->IsOnlineStartPending())
        {
            CgsGui::GuiEventNetworkSuspension suspension;
            suspension.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;
            mpStateInterface->OutputGuiEvent(suspension);
            mpGuiCache->SetOnlineStartPending(false);
            SendStateEvent("GO_BACK_EASY");
        }
        else SendStateEvent("GO_BACK");
        break;
    }
    if (changed) UpdateArrows();
}

// ARTIST 0x824AB580.
void OnlineViewChallenges::HandleControllerInput(const s32* event)
{
    CGS_ASSERT(event != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid event sent to OnlineViewChallenges::HandleControllerInput", CGS_ARTIST_FILE, 268));
    if (meSubState == E_SUBSTATE_SELECTING_PARAMS) HandleControllerInputSelectParams(event);
}

// ARTIST 0x824AECF8.
void OnlineViewChallenges::Update()
{
    CgsModule::VariableEventQueue<18432, 16>* queue = reinterpret_cast<CgsModule::VariableEventQueue<18432, 16>*>(mpInGuiEventQueue);
    const CgsModule::Event* event = 0;
    s32 size = 0;
    for (s32 type = queue->GetFirstEvent(&event, &size); event; type = queue->GetNextEvent(event, &event, &size))
    {
        switch (type)
        {
        case 6: HandleControllerInput(reinterpret_cast<const s32*>(event)); break;
        case 64: HandleGuiCacheEvent(event); break;
        case 581: mChallengeListComponent.HandleEveryPlayerCompletionStatus(
            reinterpret_cast<const BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData*>(event)); break;
        case 14: case 21: break;
        default:
            if (CgsDev::Log::gpDebugPrint) *CgsDev::Log::gpDebugPrint << "Unexpected challenge browser event " << type << "\n";
            break;
        }
    }
    queue->Clear();
    CheckForCompletedLoads();
    mChallengeToggle.Update();
    mChallengeListComponent.Update();
}
}

#undef CGS_ARTIST_FILE
