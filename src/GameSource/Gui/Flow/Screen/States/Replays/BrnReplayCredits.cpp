// BrnGui::ReplayCredits -- the "RE_CREDITS" screen state, reconstructed from the console build.

#include "GameSource/Gui/Flow/Screen/States/Replays/BrnReplayCredits.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                     // CgsCore::SnPrintf / StrCat
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                         // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                        // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"    // StateInterface
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"             // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                    // gGuiResourceIdentifier

namespace BrnGui
{
namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;

    const s32 KI_CHANNEL_GUI_OUT = 40;

    // gGuiResourceIdentifier[163] == "ReplaysCredits", played on level 3.
    const u32 KU_REPLAYS_CREDITS_RESOURCE = 163;
    const s32 KI_CREDITS_MOVIE_LEVEL      = 3;

    // The replay status bit that holds the roll on screen: the replay module sets it in its
    // save mode (with the playing bit) and while a played reel is not yet ready. FLAG: name
    // inferred from those writers (ReplayModule::SetStatusInterface).
    const u32 KU_REPLAY_STATUS_SAVING = 0x10;

    // The roll's display time: a base, a slice per listed car and a pause per tier.
    const f32 KF_CREDITS_BASE_TIME     = 8.0f;
    const f32 KF_CREDITS_TIME_PER_CAR  = 0.4f;
    const f32 KF_CREDITS_TIME_PER_TIER = 0.9f;

    // The three tiers of the sorted active-car table, by its activity count.
    const u32 KU_TIER_1_MIN_ACTIVE_COUNT = 450;   // more than this -> DETAIL_1
    const u32 KU_TIER_2_MIN_ACTIVE_COUNT = 250;   // more than this -> DETAIL_2
    const u32 KU_TIER_3_MIN_ACTIVE_COUNT = 0;     // any activity   -> DETAIL_3

    const u32 KU_MAX_CREDITED_CARS = 16;
    const u32 KU_LINE_LENGTH       = 128;
    const u32 KU_TIER_TEXT_LENGTH  = 1024;

    // GuiEventEnterCredits (console id 587): { 4, 587, 12 } + one word; the replay roll posts 1.
    struct GuiEventEnterCreditsWire : public CgsGui::GuiEvent<587>
    {
        u32 muPayload;   // +0x0C
        explicit GuiEventEnterCreditsWire(u32 luPayload)
            : CgsGui::GuiEvent<587>(4, 12), muPayload(luPayload) {}
    };

    // GuiEventLeaveCredits (console id 588): { 1, 588, 12 }, 16 bytes; the console writes no
    // payload byte.
    struct GuiEventLeaveCreditsWire : public CgsGui::GuiEvent<588>
    {
        u8 muReserved;   // +0x0C (never written by the console)
        GuiEventLeaveCreditsWire() : CgsGui::GuiEvent<588>(1, 12), muReserved(0) {}
    };

    const char* const KAC_DETAIL_PLAYER = "REPLAY_CREDITS_DETAIL_0";
    const char* const KAC_DETAIL_TIER_1 = "REPLAY_CREDITS_DETAIL_1";
    const char* const KAC_DETAIL_TIER_2 = "REPLAY_CREDITS_DETAIL_2";
    const char* const KAC_DETAIL_TIER_3 = "REPLAY_CREDITS_DETAIL_3";
}

const CgsGui::sResourceTuple ReplayCredits::maResourcesToLoad[1] =
{
    { KU_REPLAYS_CREDITS_RESOURCE, CgsGui::E_GUI_RESOURCETYPE_APT },
};
const u32 ReplayCredits::muNumResourcesToLoad = 1;

void ReplayCredits::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                       u32* lpuNumberOfResources) const
{
    *lppResourceTuples    = maResourcesToLoad;
    *lpuNumberOfResources = muNumResourcesToLoad;
}

void ReplayCredits::OnEnter()
{
    mpGuiCache      = mpStateInterface->GetAccessPointers()->GetGuiCache();
    meInternalState = E_INTERNALSTATE_LOADRESOURCES;

    GuiEventEnterCreditsWire lEnter(1);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lEnter, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lEnter)));

    CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();
    lpLanguageManager->AddString(KAC_DETAIL_PLAYER,
                                 lpLanguageManager->FindString(mpGuiCache->GetPlayerName()));

    mpGuiCache->SortReplayPlayersActive();

    char lacLine[KU_LINE_LENGTH];
    char lacTierText[KU_TIER_TEXT_LENGTH];
    u32  luCarIndex  = 0;
    u32  luTierCount = 0;
    lacTierText[0] = 0;

    const ReplayPlayerActive* lpCar = mpGuiCache->GetSortedReplayPlayerActive(0);
    mfTimeRemaining = KF_CREDITS_BASE_TIME;

    while (lpCar->muActiveCount > KU_TIER_1_MIN_ACTIVE_COUNT)
    {
        if (luCarIndex >= KU_MAX_CREDITED_CARS)
            break;
        ++luTierCount;
        CgsCore::SnPrintf(lacLine, KU_LINE_LENGTH, "<Car Name> - %s\n", lpCar->mName.GetPlayerName());
        CgsCore::StrCat(lacTierText, KU_TIER_TEXT_LENGTH, lacLine);
        ++luCarIndex;
        mfTimeRemaining += KF_CREDITS_TIME_PER_CAR;
        lpCar = mpGuiCache->GetSortedReplayPlayerActive(luCarIndex);
    }
    if (luTierCount != 0)
    {
        lpLanguageManager->AddString(KAC_DETAIL_TIER_1, reinterpret_cast<const u8*>(lacTierText));
        lacTierText[0] = 0;
        luTierCount    = 0;
        mfTimeRemaining += KF_CREDITS_TIME_PER_TIER;
    }

    while (lpCar->muActiveCount > KU_TIER_2_MIN_ACTIVE_COUNT)
    {
        if (luCarIndex >= KU_MAX_CREDITED_CARS)
            break;
        ++luTierCount;
        CgsCore::SnPrintf(lacLine, KU_LINE_LENGTH, "<Car Name> - %s\n", lpCar->mName.GetPlayerName());
        CgsCore::StrCat(lacTierText, KU_TIER_TEXT_LENGTH, lacLine);
        ++luCarIndex;
        mfTimeRemaining += KF_CREDITS_TIME_PER_CAR;
        lpCar = mpGuiCache->GetSortedReplayPlayerActive(luCarIndex);
    }
    if (luTierCount != 0)
    {
        lpLanguageManager->AddString(KAC_DETAIL_TIER_2, reinterpret_cast<const u8*>(lacTierText));
        lacTierText[0] = 0;
        luTierCount    = 0;
        mfTimeRemaining += KF_CREDITS_TIME_PER_TIER;
    }

    while (lpCar->muActiveCount > KU_TIER_3_MIN_ACTIVE_COUNT)
    {
        if (luCarIndex >= KU_MAX_CREDITED_CARS)
            break;
        ++luTierCount;
        CgsCore::SnPrintf(lacLine, KU_LINE_LENGTH, "<Car Name> - %s\n", lpCar->mName.GetPlayerName());
        CgsCore::StrCat(lacTierText, KU_TIER_TEXT_LENGTH, lacLine);
        ++luCarIndex;
        mfTimeRemaining += KF_CREDITS_TIME_PER_CAR;
        lpCar = mpGuiCache->GetSortedReplayPlayerActive(luCarIndex);
    }
    if (luTierCount != 0)
    {
        lpLanguageManager->AddString(KAC_DETAIL_TIER_3, reinterpret_cast<const u8*>(lacTierText));
        mfTimeRemaining += KF_CREDITS_TIME_PER_TIER;
    }
}

void ReplayCredits::OnLeave()
{
    GuiEventLeaveCreditsWire lLeave;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lLeave, KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lLeave)));

    mpStateInterface->PlayAptMovie("", KI_CREDITS_MOVIE_LEVEL);

    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_PLAYER);
    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_TIER_1);
    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_TIER_2);
    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_TIER_3);

    meInternalState = E_INTERNALSTATE_LEFT;
}

void ReplayCredits::Update()
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

bool ReplayCredits::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        return false;

    mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_CREDITS_RESOURCE],
                                   KI_CREDITS_MOVIE_LEVEL);
    return true;
}

void ReplayCredits::UpdateRunning()
{
    mfTimeRemaining -= mpGuiCache->GetTimeStep();

    if ((mpGuiCache->mReplayStatusInterface.mxStatusFlags & KU_REPLAY_STATUS_SAVING) != 0)
        return;
    if (!(mfTimeRemaining > 0.0f))
        SendStateEvent("ADVANCE");
}
}
