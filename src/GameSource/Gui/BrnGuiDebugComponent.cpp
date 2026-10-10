// BrnGuiDebugComponent.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiDebugComponent::GetSingletonPtr @ 0x824ECC40
//
// Loads the static singleton pointer (X360 off_82FB5080), runs a non-fatal null guard
// (the X360 fires the "mspSingletonThis" assert when the pointer is null but returns it
// either way), and returns it. The X360-baked assert file/line are discarded per project
// convention; the stringized condition matches the X360 assert message text.

#include "GameSource/Gui/BrnGuiDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsID.h"   // CgsIDCompress (TriggerPostEvent's unlock ids)
#include "GameSource/Gui/BrnGuiModule.h"         // GuiModule::mGuiCache (TriggerPostEvent)

#include <cstring>                               // strlen / strncpy / memset

namespace BrnGui
{

// X360 off_82FB5080 -- the live singleton instance, populated when the debug component
// is constructed. Defaults to null until then.
GuiDebugComponent* GuiDebugComponent::mspSingletonThis = nullptr;

// @ 0x824ECC40
GuiDebugComponent* GuiDebugComponent::GetSingletonPtr()
{
    GuiDebugComponent* lpSingleton = mspSingletonThis;
    CGS_ASSERT( lpSingleton != nullptr, "mspSingletonThis" );
    return lpSingleton;
}

// Post event 495 for the hook picked in the debug menu: every maximum weight 1.0 and the hook's
// name (asserted to fit the event's 32-character name field, then strncpy'd). Nothing is posted
// while no hook is selected (index -1). The console never writes the GUID word; it is zeroed here
// so the arbitrator resolves the hook by its name.
void GuiDebugComponent::TriggerPFX()
{
    if (miCurrentPFXHookIndex == -1)
        return;

    GuiPFXHookEvent lEvent;
    lEvent.muGuid                      = 0;
    lEvent.mfMaximumBloomWeight        = 1.0f;
    lEvent.mfMaximumVignetteWeight     = 1.0f;
    lEvent.mfMaximumBlurWeight         = 1.0f;
    lEvent.mfMaximumDepthOfFieldWeight = 1.0f;
    lEvent.mfMaximum2DTintWeight       = 1.0f;
    lEvent.mfMaximum3DTintWeight       = 1.0f;

    const char* lpcHookName = mPFXHookEnumeration.mapHookNames[miCurrentPFXHookIndex];
    CGS_ASSERT(std::strlen(lpcHookName) < static_cast<size_t>(KI_MAX_PFX_ID_LENGTH), "String too long: ");
    std::strncpy(lEvent.macName, lpcHookName, KI_MAX_PFX_ID_LENGTH);

    // The queued record is the payload past the GuiEvent<495> header (64 bytes).
    const s32 KI_PAYLOAD_SIZE = static_cast<s32>(sizeof(GuiPFXHookEvent) - sizeof(CgsGui::GuiEvent<495>));
    static_assert(sizeof(GuiPFXHookEvent) - sizeof(CgsGui::GuiEvent<495>) == 64, "event 495 payload is 64 bytes");
    mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent.muGuid), 495, KI_PAYLOAD_SIZE);
}

// Post the payload-less event 291, then a made-up offline post-event result (event 289) to the
// GUI and record the same result in the GUI cache. The result carries the current game mode (0
// when none), score 5318008, time 69.42, the debug finish position and rank settings, and -- for
// a first place only -- the car / free car / rival unlocks and the rank-up flag the debug menu
// selected. Bytes the console leaves unwritten are zero here.
void GuiDebugComponent::TriggerPostEvent()
{
    CgsGui::GuiEvent<291> lPostEventStart;
    mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPostEventStart), 291, 1);

    GuiEventOfflinePostEvent::OfflinePostEventData lResult;
    std::memset(&lResult, 0, sizeof(lResult));
    lResult.maCarsToUnlockFromSpecialEvent.MarkUnconstructed();
    lResult.miCtorSentinel98 = -1;

    const s32 liGameMode = mpGuiModule->mGuiCache.GetGameMode();
    lResult.meFinishedGameModeType = (liGameMode == -1) ? 0 : liGameMode;

    lResult.mbCrashedOut         = false;
    lResult.mbTimedOut           = false;
    lResult.mbEliminated         = false;
    lResult.mbHasUnlockedFreeCar = false;
    lResult.maCarsToUnlockFromSpecialEvent.Clear();
    lResult.mNewlyUnlockedRivalID   = 0;
    lResult.mNewlyUnlockedFreeCarID = 0;
    lResult.mBeatenRival            = 0x52D41;
    lResult.miPlayerFinishPosition  = static_cast<s8>(miDebugFinishPos);
    lResult.miModeScore             = 5318008;
    lResult.mfTime                  = 69.42f;

    if (miDebugFinishPos == 1)
    {
        lResult.maCarsToUnlockFromSpecialEvent.Clear();
        if (mbDebugFinishUnlockNormalCar)
        {
            lResult.maCarsToUnlockFromSpecialEvent.Append(CgsIDCompress("PUSMC01"));
            lResult.maCarsToUnlockFromSpecialEvent.Append(CgsIDCompress("XUSM1B1"));
        }
        if (mbDebugFinishUnlockFreeCar)
        {
            lResult.mbHasUnlockedFreeCar    = true;
            lResult.mNewlyUnlockedFreeCarID = CgsIDCompress("PASBCC01");
        }
        lResult.mbHasRankedUp = mbDebugFinishRankup;
        if (mbDebugFinishUnlockRival)
            lResult.mNewlyUnlockedRivalID = CgsIDCompress("PUSCC01");
    }

    // Ranks 0..4 rank up by one; rank 5 completes the last rank (old 5, no new rank); anything
    // higher completes it with neither rank set.
    if (miDebugPlayerRank > 4)
    {
        lResult.mbCompletedLastRank = true;
        lResult.miPlayerOldRank     = (miDebugPlayerRank == 5) ? 5 : -1;
        lResult.miPlayerNewRank     = -1;
    }
    else
    {
        lResult.mbCompletedLastRank = false;
        lResult.miPlayerOldRank     = miDebugPlayerRank;
        lResult.miPlayerNewRank     = miDebugPlayerRank + 1;
    }
    lResult.mbCountsTowardsProgression = true;

    const CgsModule::Event* lpResultEvent = reinterpret_cast<const CgsModule::Event*>(&lResult);
    mInputQueue.AddEvent(lpResultEvent, 289, static_cast<s32>(sizeof(lResult)));
    mpGuiModule->mGuiCache.RecEvent(lpResultEvent, 289);
}

} // namespace BrnGui
