// BrnGui::ReplayCredits -- the "RE_CREDITS" screen state, reconstructed from the console build.

#include "GameSource/Gui/Flow/Screen/States/Replays/BrnReplayCredits.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/Replays/BrnReplayCredits.cpp"
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
            : muPayload(luPayload) {}
        GuiEventEnterCreditsWire() {}
    };

    // GuiEventLeaveCredits (console id 588): { 1, 588, 12 }, 16 bytes; the console writes no
    // payload byte.
    // FLAG: X360-only (the replay screens have no PS3 DWARF); empty, as ARTIST stores no
    // payload byte (OnLeave @0x824D56D0).
    struct GuiEventLeaveCreditsWire : public CgsGui::GuiEvent<588>
    {
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

void ReplayCredits::OnEnter()
{
    mpGuiCache      = mpStateInterface->GetAccessPointers()->GetGuiCache();
    meInternalState = E_INTERNALSTATE_LOADRESOURCES;

    GuiEventEnterCreditsWire lEnter(1);
    mpStateInterface->OutputGuiEvent(lEnter);

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
    mpStateInterface->OutputGuiEvent(lLeave);

    mpStateInterface->PlayAptMovie("", KI_CREDITS_MOVIE_LEVEL);

    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_PLAYER);
    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_TIER_1);
    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_TIER_2);
    mpStateInterface->GetLanguageManager()->RemoveString(KAC_DETAIL_TIER_3);

    meInternalState = E_INTERNALSTATE_LEFT;
}

void ReplayCredits::Update()
{


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
        // Streamed in place, as ARTIST builds it (cpp:202).
        CGS_ASSERT_STREAM_AT(false, "Invalid internal state : " << meInternalState << "\n",
                             CGS_ARTIST_FILE, 202);
        break;
    }

    mpInGuiEventQueue->Clear();
}

bool ReplayCredits::UpdateLoadResources()
{
    CGS_ASSERT_AT(mpGuiCache != NULL, "mpGuiCache", CGS_ARTIST_FILE, 255);

    // The loaded arm returns inside the if (ARTIST branches to a shared `return false` tail).
    if (mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad) == true)   // cmplwi 1 @0x824D5820
    {
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_REPLAYS_CREDITS_RESOURCE],
                                       KI_CREDITS_MOVIE_LEVEL);
        return true;
    }
    return false;
}

void ReplayCredits::UpdateRunning()
{
    mfTimeRemaining -= mpGuiCache->GetTimeStep();

    if (mpGuiCache->mReplayStatusInterface.IsSaving())
        return;
    if (!(mfTimeRemaining > 0.0f))
        SendStateEvent("ADVANCE");
}
}
