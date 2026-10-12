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
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // ProfileHost's filtered start messages
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // profile log filter
#include "GameSource/GameState/BrnCgsPlayerName.h" // CgsNetwork::PlayerName (online-invite debug payload)
#include "GameSource/Gui/BrnGuiModule.h"         // GuiModule::mGuiCache (TriggerPostEvent)

#include <cstdint>                                // uintptr_t (debug-variable callback indices)
#include <cstring>                               // strlen / strncpy / memset

extern bool gbLogGuiAudioTriggers; // X360 byte_82FB5098, defined by CgsAptCommunicator.cpp

namespace
{
    // ARTIST 0x82F27548..0x82F2759C: exactly 21 NUL-terminated option names consumed by
    // GuiDebugComponent::OnActivate's 21-item race-HUD loop.
    const char* const KAP_TOGGLEABLE_RACE_HUD_NAMES[] =
    {
        "GAME_ICON",
        "HUD_MESSAGES",
        "BOOST_BAR",
        "BOOST_MSGS",
        "COUNTDOWN",
        "COUNTDOWN_RENDERS",
        "EVENT_INFO",
        "DISTRICT_MARKER",
        "SAT_NAV",
        "SAT_NAV_STATIC",
        "PLAYER_POSITION_TABLE",
        "FRIENDS_LIST",
        "REPLAY_INDICATOR",
        "ABOVE_CAR_ICONS",
        "EVENT_END_TIMER",
        "ROAD_RULE_CPT",
        "PREEVENT_OVERLAY",
        "MUGSHOT_CPT",
        "PAYBACK_CPT",
        "PLAYER_STATS_CPT",
        "SHOWTIME_BAR",
    };

    struct GuiDebugEATraxTriggerPayload
    {
        u64 muWord0;
        u64 muWord8;
        u32 muWord16;
        u8  muByte20;
        u8  mauUnwrittenTail[3];
    };
    static_assert(sizeof(GuiDebugEATraxTriggerPayload) == 24,
                  "ARTIST EA Trax trigger record is 24 bytes");

    struct GuiDebugOnlineInvitePayload
    {
        u32                  muWord0;
        u32                  muWord4;
        CgsNetwork::PlayerName mFriendName;
    };
    static_assert(sizeof(GuiDebugOnlineInvitePayload) == 24,
                  "ARTIST debug invite record is 24 bytes");
}

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

const char* GuiDebugComponent::GetName() const
{
    // ARTIST 0x824ED128.
    return "Gui Module";
}

// ARTIST 0x82505488. The call to BaseCollisionGenerator::Destruct at the same site is an
// empty ICF body (0x8284CB38); it has no state effect. The remaining stores and queue setup
// below reproduce the component's observed defaults. The final HUD-element byte is not
// initialized by the X360 loop, which writes exactly the first 21 entries.
void GuiDebugComponent::Construct(GuiModule* lpGuiModule)
{
    mspSingletonThis = this;
    CGS_ASSERT(lpGuiModule != nullptr,
               "Invalid Gui module passed to GuiDebugComponent::Construct");

    mpGuiModule = lpGuiModule;
    mbSpeedoVisible = false;
    mSpeedoInfo.miSpeedMph = 0;
    mSpeedoInfo.miRPM = 0;
    mSpeedoInfo.mi8Gear = 0;
    mSpeedoInfo.mau8Pad[0] = 0;
    mSpeedoInfo.mau8Pad[1] = 0;
    mSpeedoInfo.mau8Pad[2] = 0;
    mfSafePercent = 85.0f;
    mbRenderGui = true;
    mbMiniMapVisible = true;
    mbPriEventOverride = false;
    mbShow43Area = false;
    mbShowSafeArea = false;
    mfFontSpacing = 100.0f;

    mInputQueue.Construct();

    miScreenFlowStreamingMode = 1;
    maStreamingOptions[0].miValue = 0;
    maStreamingOptions[0].mpcName = "Off";
    maStreamingOptions[1].miValue = 1;
    maStreamingOptions[1].mpcName = "Load current only";
    maStreamingOptions[2].miValue = 2;
    maStreamingOptions[2].mpcName = "Preload adjacent";

    mPFXHookEnumeration.miHookNameCount = -1;
    miCurrentPFXHookIndex = -1;
    std::strcpy(macCurrentPFXHookName, "Trigger PFX Hook (N/A)");

    miDebugPlayerRank = 0;
    miDebugFinishPos = 1;
    mbDebugFinishRankup = true;
    mbDebugFinishUnlockFreeCar = true;
    mbDebugFinishUnlockNormalCar = true;
    mbDebugFinishUnlockRival = true;
    mbDebugOfflineInstantResults = false;
    mbDebugOnlineInstantResults = false;

    mbWorstCaseHudActive = false;
    mbWorstCaseHudAlreadyActive = false;
    mfWorstCaseUpdateTime = 0.0f;
    mbToggleableRaceHUDEnabled = false;
    for (s32 liIndex = 0; liIndex < 21; ++liIndex)
        mabTogglableRaceHUDElementsState[liIndex] = true;
}

// ARTIST 0x8252A3B8. The strings and numeric menu bounds below are from the original call
// sequence; the race-HUD names were read from the original 21-pointer table at 0x82F27548.
void GuiDebugComponent::OnActivate()
{
    RegisterVariable(&mbRenderGui, "Render GUI Objects");
    RegisterVariable(&mbDebugOfflineInstantResults, "Debug Offline Results Screen");
    RegisterVariable(&mbDebugOnlineInstantResults, "Debug Online Results Screen");
    RegisterFunction(&EnterCarSelectCallback, this, "Trigger Car Select");
    RegisterFunction(&TriggerIntroCallback, this, "Trigger Intro");
    RegisterFunction(&TriggerPostEventCallback, this, "Trigger Offline Post Event");

    RegisterVariable(&miDebugFinishPos, "    Finish position:");
    SetRange(&miDebugFinishPos, 1, 8);
    RegisterVariable(&mbDebugFinishRankup, "    Do rank-up in post-event:");
    RegisterVariable(&miDebugPlayerRank, "    Rank at start of post-event:");
    SetRange(&miDebugPlayerRank, 0, 6);
    RegisterVariable(&mbDebugFinishUnlockNormalCar, "    Do unlock livery car (BR end):");
    RegisterVariable(&mbDebugFinishUnlockFreeCar, "    Do unlock free car (rank up):");
    RegisterVariable(&mbDebugFinishUnlockRival, "    Do unlock rival:");

    RegisterFunction(&OpenCrashNavCallback, this, "Trigger crash nav");
    RegisterFunction(&MiniMapSwitchCallback, this, "Toggle MiniMap Visibility");
    RegisterVariable(&mbMiniMapVisible, "   MiniMap Visibility: ");
    SetReadOnly(&mbMiniMapVisible, true);
    RegisterVariable(&mbSpeedoVisible, "Speedo Visibility: ");
    RegisterFunction(&ToggleShow43AreaCallback, this, "Show/Hide 4:3 Area");
    RegisterFunction(&ToggleShowSafeAreaCallback, this, "Show/Hide Safe Area");
    RegisterVariable(&mfSafePercent, "   Safe Area:");
    SetRange(&mfSafePercent, 70.0f, 100.0f);
    RegisterFunction(&ChangeFontSpacingCallback, this, "Change Font Spacing");
    RegisterVariable(&mfFontSpacing, "   Spacing percentage:");
    SetRange(&mfFontSpacing, 30.0f, 250.0f);
    RegisterVariable(&mpGuiModule->mViewModule.mbUpdateFlash, "Update Flash");

    RegisterFunction(&BootupProfileCallback, this, "Bootup Load/Save Profile");
    RegisterFunction(&AutosaveProfileCallback, this, "Autosave Profile");
    RegisterFunction(&SaveProfileCallback, this, "Save Profile");
    RegisterFunction(&LoadProfileCallback, this, "Load Profile");
    RegisterFunction(&ShowKeyboardCallback, this, "Show Keyboard");
    RegisterVariable(&gbLogGuiAudioTriggers, "Toggle Audio Trigger Debug Spew");
    RegisterFunction(&PlayMovieCallback, this, "Play/stop test movie");
    RegisterFunction(&RotateThroughFontsCallback, this, "Select next font as default");
    RegisterFunction(&RotateThroughFontEffectsCallback, this, "Select next font effect");
    RegisterFunction(&TriggerPFXCallback, this, macCurrentPFXHookName);
    RegisterVariable(&miCurrentPFXHookIndex, "  Current Hook");
    SetRange(&miCurrentPFXHookIndex, -1, -1);
    RegisterFunction(&TriggerClosePFXCallback, this, "Trigger PFX CloseMenu");
    RegisterFunction(&ToggleGenericHUDPriorityOverrideCallback, this,
                     "Toggle Generic HUD Priority Event Overrides");
    RegisterVariable(&mbPriEventOverride, "Priority Overrides On");

    RegisterVariable(&miScreenFlowStreamingMode, "Screen Flow Streaming");
    SetOptions(&miScreenFlowStreamingMode, maStreamingOptions);
    SetRange(&miScreenFlowStreamingMode, 0, 2);

    RegisterFunction(&TriggerEATraxComponentCallback, this, "Components",
                     "Trigger EA Trax Component");
    RegisterFunction(&TriggerOnlineInviteCallback, this, "Components",
                     "Trigger Receive Online Invite");
    RegisterFunction(&TriggerPaybackComponent, this, "Components",
                     "Trigger Payback Component");

    RegisterVariable(&mbWorstCaseHudActive, "Race HUD Toggling", "Enable Worst Case HUD");
    RegisterVariable(&mbToggleableRaceHUDEnabled, "Race HUD Toggling",
                     "Enable Toggling Race HUD");
    SetChangeCallback(&mbToggleableRaceHUDEnabled, &ToggleRaceHudComponentsCallback,
                      reinterpret_cast<void*>(static_cast<std::uintptr_t>(21)));
    for (s32 liIndex = 0; liIndex < 21; ++liIndex)
    {
        RegisterVariable(&mabTogglableRaceHUDElementsState[liIndex], "Race HUD Toggling",
                         KAP_TOGGLEABLE_RACE_HUD_NAMES[liIndex]);
        SetChangeCallback(&mabTogglableRaceHUDElementsState[liIndex],
                          &ToggleRaceHudComponentsCallback,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(liIndex)));
    }

    // ARTIST passes this one-byte stack payload without storing to it. Keep that source-level
    // indeterminacy instead of replacing the original bytes with a made-up event value.
    u8 lUninitializedPayload;
    mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload),
                         500, 1);
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

void GuiDebugComponent::TriggerIntroCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "Invalid data pointer sent to GuiDebugComponent::TriggerIntro");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 288, 1);
}

void GuiDebugComponent::TriggerPostEventCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "Invalid data pointer sent to GuiDebugComponent::TriggerPostEventCallback");
    static_cast<GuiDebugComponent*>(lpData)->TriggerPostEvent();
}

void GuiDebugComponent::ToggleShow43AreaCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "ToggleShow43AreaCallback: Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    lpComponent->mbShow43Area = !lpComponent->mbShow43Area;
}

void GuiDebugComponent::ToggleShowSafeAreaCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "ToggleShowSafeAreaCallback: Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    lpComponent->mbShowSafeArea = !lpComponent->mbShowSafeArea;
}

void GuiDebugComponent::MiniMapSwitchCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "MiniMapSwitchCallback: Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    lpComponent->mbMiniMapVisible = !lpComponent->mbMiniMapVisible;
    const bool lbNewVisibility = lpComponent->mbMiniMapVisible;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lbNewVisibility), 205, 1);
}

void GuiDebugComponent::ChangeFontSpacingCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr, "Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    const f32 lfSpacing = lpComponent->mfFontSpacing * 0.01f;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lfSpacing), 62, 4);
}

void GuiDebugComponent::PlayMovieCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "PlayMovieCallback: Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 511, 1);
}

void GuiDebugComponent::OpenCrashNavCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "OpenCrashNavCallback: Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 228, 1);
}

void GuiDebugComponent::EnterCarSelectCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "EnterCarSelectCallback: Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 229, 1);
}

void GuiDebugComponent::RotateThroughFontsCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr, "Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 226, 1);
}

void GuiDebugComponent::RotateThroughFontEffectsCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr, "Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 227, 1);
}

void GuiDebugComponent::ToggleGenericHUDPriorityOverrideCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr, "Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    lpComponent->mbPriEventOverride = !lpComponent->mbPriEventOverride;
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 230, 1);
}

void GuiDebugComponent::TriggerPFXCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "TriggerPFXCallback: Invalid data in the callback");
    static_cast<GuiDebugComponent*>(lpData)->TriggerPFX();
}

void GuiDebugComponent::TriggerClosePFXCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "TriggerPFXCloseCallback: Invalid data in the callback");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    u8 lUninitializedPayload;
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lUninitializedPayload), 497, 1);
}

void GuiDebugComponent::TriggerEATraxComponentCallback(void* lpData)
{
    // ARTIST 0x825131E0 stores the first 21 payload bytes and passes 24. The final three
    // stack bytes are untouched by the original, so leave that tail indeterminate too.
    GuiDebugEATraxTriggerPayload lPayload;
    lPayload.muWord0 = 0;
    lPayload.muWord8 = 0;
    lPayload.muWord16 = 0;
    lPayload.muByte20 = 1;
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lPayload), 502, 24);
}

void GuiDebugComponent::TriggerOnlineInviteCallback(void* lpData)
{
    // ARTIST 0x82513228 writes two zero words then constructs the 16-byte test name.
    GuiDebugOnlineInvitePayload lPayload;
    lPayload.muWord0 = 0;
    lPayload.muWord4 = 0;
    lPayload.mFriendName.Construct("Linus T");
    GuiDebugComponent* lpComponent = static_cast<GuiDebugComponent*>(lpData);
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lPayload), 105, 24);
}

void GuiDebugComponent::TriggerPaybackComponent(void* /*lpData*/)
{
    // ARTIST 0x8250D848 reads the current active race-car index from GuiCache and posts
    // {aggressor, victim, trick-type} for event 177; the debug event uses the same index
    // for aggressor and victim and the original zero trick value.
    GuiDebugComponent* lpComponent = GetSingletonPtr();
    const EActiveRaceCarIndex lePlayerIndex = static_cast<EActiveRaceCarIndex>(
        lpComponent->mpGuiModule->mGuiCache.GetPlayerActiveRaceCarIndex());
    GuiDirtyTrickNewEvent lEvent;
    lEvent.meAggressorActiveRaceCarIndex = lePlayerIndex;
    lEvent.meVictimActiveRaceCarIndex = lePlayerIndex;
    lEvent.meTrickType = static_cast<BrnNetwork::EPaybackType>(0);
    lpComponent->mInputQueue.AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lEvent), 177, 12);
}

void GuiDebugComponent::BootupProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState !=
        CgsGui::SystemUserProfile::E_USERSIGNINSTATE_SIGNEDIN)
        return;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "SaveLoad: Starting Bootup\n";
    mpGuiModule->GetProfileManager().Bootup(
        mpGuiModule->mProfileAutosaveResultHandler, true);
}

void GuiDebugComponent::AutosaveProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState ==
            CgsGui::SystemUserProfile::E_USERSIGNINSTATE_SIGNEDIN &&
        mpGuiModule->GetProfileManager().AutosaveIsEnabled())
    {
        mpGuiModule->GetProfileManager().Autosave(
            &mpGuiModule->mProfileAutosaveResultHandler, 0, nullptr);
        mpGuiModule->mbProfileAutosavePending = false;
        mpGuiModule->mfLastProfileAutosaveTime =
            static_cast<f32>(mpGuiModule->mGuiCache.GetTime());
    }
}

void GuiDebugComponent::SaveProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState !=
        CgsGui::SystemUserProfile::E_USERSIGNINSTATE_SIGNEDIN)
        return;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "SaveLoad: Starting Save\n";
    mpGuiModule->GetProfileManager().Save(mpGuiModule->mProfileAutosaveResultHandler);
}

void GuiDebugComponent::LoadProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState !=
        CgsGui::SystemUserProfile::E_USERSIGNINSTATE_SIGNEDIN)
        return;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "SaveLoad: Starting Load\n";
    mpGuiModule->GetProfileManager().Load(mpGuiModule->mProfileAutosaveResultHandler);
}

void GuiDebugComponent::BootupProfileCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "BootupProfileCallback: Invalid data in the callback");
    static_cast<GuiDebugComponent*>(lpData)->BootupProfile();
}

void GuiDebugComponent::AutosaveProfileCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "AutosaveProfileCallback: Invalid data in the callback");
    static_cast<GuiDebugComponent*>(lpData)->AutosaveProfile();
}

void GuiDebugComponent::SaveProfileCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "SaveProfileCallback: Invalid data in the callback");
    static_cast<GuiDebugComponent*>(lpData)->SaveProfile();
}

void GuiDebugComponent::LoadProfileCallback(void* lpData)
{
    CGS_ASSERT(lpData != nullptr,
               "LoadProfileCallback: Invalid data in the callback");
    static_cast<GuiDebugComponent*>(lpData)->LoadProfile();
}

} // namespace BrnGui
