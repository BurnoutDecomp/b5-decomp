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
#include "GameSource/Gui/Events/BrnGuiEventAudioTrax.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include "GameShared/GameClasses/Core/CgsID.h"   // CgsIDCompress (TriggerPostEvent's unlock ids)
#include "GameSource/Gui/BrnGuiModule.h"         // GuiModule::mGuiCache (TriggerPostEvent)

#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

#include <cstring>                               // strlen / strncpy / memset

namespace
{
    // ARTIST82FB5920: original file-global count, not reset by Construct.
    u32 guGuiDebugUpdateCount = 0;
}

namespace BrnGui
{

// X360 off_82FB5080 -- the live singleton instance, populated when the debug component
// is constructed. Defaults to null until then.
GuiDebugComponent* GuiDebugComponent::mspSingletonThis = nullptr;

// ARTIST82505488. The original base Construct call is folded to
// the empty8284CB38 body, so it has no source operation here. In particular,
// do not replace it with the current base provider's additional field resets.
void GuiDebugComponent::Construct(GuiModule* lpGuiModule)
{
    if (!lpGuiModule)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "Invalid Gui module passed to GuiDebugComponent::Construct";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 92);
        CgsDev::Assert::EndAssert();
    }
    mspSingletonThis = this;
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
    maStreamingOptions[1].miValue = 1;
    miDebugPlayerRank = 0;
    miDebugFinishPos = 1;
    mbDebugFinishRankup = true;
    mPFXHookEnumeration.miHookNameCount = -1;
    maStreamingOptions[0].mpcName = "Off";
    miCurrentPFXHookIndex = -1;
    maStreamingOptions[2].miValue = 2;
    maStreamingOptions[1].mpcName = "Load current only";
    mbDebugFinishUnlockFreeCar = true;
    mbDebugFinishUnlockNormalCar = true;
    maStreamingOptions[2].mpcName = "Preload adjacent";
    mbDebugFinishUnlockRival = true;
    mbDebugOfflineInstantResults = false;
    mbDebugOnlineInstantResults = false;
    std::strcpy(macCurrentPFXHookName, "Trigger PFX Hook (N/A)");
    mbWorstCaseHudActive = false;
    mbWorstCaseHudAlreadyActive = false;
    mbToggleableRaceHUDEnabled = false;
    mfWorstCaseUpdateTime = 0.0f;
    // ARTIST82505618 loads21, not the near-ancestor's22. Retain the
    // declared array shape and the target's unwritten final element.
    for (s32 liElement = 0; liElement < 21; ++liElement)
        mabTogglableRaceHUDElementsState[liElement] = true;
}

// ARTIST8250C998. Preserve the read pass, then the separate write pass
// which flushes this real component queue into the supplied GUI input.
void GuiDebugComponent::Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer)
{
    const CgsGui::CgsGuiModuleIO::InputBuffer* lpInputRead = lpInputBuffer;
    lpInputRead->LockForRead();
    const CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpEvents =
        lpInputRead->GetGuiEvents();
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liId = lpEvents->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        if (liId == 64)
        {
            const GuiCache* lpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
            if (!lpGuiCache)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpGuiCache",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 334);
                CgsDev::Assert::EndAssert();
            }
            std::memcpy(&mPFXHookEnumeration, lpGuiCache->GetPFXHookEnumeration(),
                sizeof(mPFXHookEnumeration));
            if (mPFXHookEnumeration.miHookNameCount > 0 && miCurrentPFXHookIndex == -1)
            {
                SetRange(&miCurrentPFXHookIndex, 0, mPFXHookEnumeration.miHookNameCount - 1);
                miCurrentPFXHookIndex = 0;
            }
        }
        else if (liId == 147)
        {
            std::memcpy(&mSpeedoInfo, lpEvent, sizeof(mSpeedoInfo));
        }
        liId = lpEvents->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
    lpInputRead->UnlockForRead();
    if (miCurrentPFXHookIndex != -1)
    {
        CgsCore::SPrintf(macCurrentPFXHookName, KI_PFX_HOOK_NAME_LENGTH,
            "Trigger PFX Hook %s", mPFXHookEnumeration.mapHookNames[miCurrentPFXHookIndex]);
    }
    if (!lpInputBuffer)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpInputBuffer",
            "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 238);
        CgsDev::Assert::EndAssert();
    }
    lpInputBuffer->LockForWrite();
    lpInputBuffer->GetGuiEvents()->Append(mInputQueue);
    if (!lpInputBuffer)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpInputBuffer",
            "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 248);
        CgsDev::Assert::EndAssert();
    }
    lpInputBuffer->UnlockForWrite();
    mInputQueue.Clear();
    if (++guGuiDebugUpdateCount == 3600u)
    {
        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.GetDebugManager().ActivateComponent(this);
    }
}

// ARTIST825076D0..82507710 in the actual enclosing module Destruct.
// The base call resolves to the observed no-op8284CB38; it adds no reset.
void GuiDebugComponent::Destruct()
{
    if (!mpGuiModule)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGuiModule != NULL",
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 169);
        CgsDev::Assert::EndAssert();
    }
    mpGuiModule = 0;
    mInputQueue.Destruct();
}

// Actual native vtable820CE7E0 slots3/4, including shared ICF GetPath.
const char* GuiDebugComponent::GetName() const
{
    return "Gui Module";
}

const char* GuiDebugComponent::GetPath() const
{
    return "Gui";
}

// @ 0x824ECC40
GuiDebugComponent* GuiDebugComponent::GetSingletonPtr()
{
    GuiDebugComponent* lpSingleton = mspSingletonThis;
    CGS_ASSERT( lpSingleton != nullptr, "mspSingletonThis" );
    return lpSingleton;
}

// ARTIST8250CFE0..CFF0: original empty tag, one unwritten byte.
void GuiDebugComponent::TriggerIntro()
{
    CgsGui::GuiEvent<288> lEvent;
    mInputQueue.AddEvent(&lEvent, 288, sizeof(lEvent));
}

// ARTIST8250D140..D150: original empty tag, one unwritten byte.
void GuiDebugComponent::RotateThroughFonts()
{
    CgsGui::GuiEvent<226> lEvent;
    mInputQueue.AddEvent(&lEvent, 226, sizeof(lEvent));
}

// ARTIST8250D090..D0A0: original empty tag, one unwritten byte.
void GuiDebugComponent::RotateThroughFontEffects()
{
    CgsGui::GuiEvent<227> lEvent;
    mInputQueue.AddEvent(&lEvent, 227, sizeof(lEvent));
}

// ARTIST8250D578..D588: original empty tag, one unwritten byte.
void GuiDebugComponent::PlayMovie()
{
    CgsGui::GuiEvent<511> lEvent;
    mInputQueue.AddEvent(&lEvent, 511, sizeof(lEvent));
}

// ARTIST8250D628..D638: original empty tag, one unwritten byte.
void GuiDebugComponent::OpenCrashNav()
{
    CgsGui::GuiEvent<228> lEvent;
    mInputQueue.AddEvent(&lEvent, 228, sizeof(lEvent));
}

// ARTIST8250D6D8..D6E8: original empty tag, one unwritten byte.
void GuiDebugComponent::OpenCarSelect()
{
    CgsGui::GuiEvent<229> lEvent;
    mInputQueue.AddEvent(&lEvent, 229, sizeof(lEvent));
}

// ARTIST8250D828..D838: original empty tag, one unwritten byte.
void GuiDebugComponent::TriggerClosePFX()
{
    CgsGui::GuiEvent<497> lEvent;
    mInputQueue.AddEvent(&lEvent, 497, sizeof(lEvent));
}

// ARTIST8250D1F0..D214 stores the actual f32 payload, with no event prefix.
void GuiDebugComponent::ChangeFontSpacing()
{
    const f32 lfSpacing = mfFontSpacing * 0.01f;
    mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lfSpacing),
        62, sizeof(lfSpacing));
}

// ARTIST8250D2B0..D2D0 toggles the actual flag but does not fill the tag byte.
void GuiDebugComponent::ToggleGenericHUDPriorityOverride()
{
    mbPriEventOverride = !mbPriEventOverride;
    CgsGui::GuiEvent<230> lEvent;
    mInputQueue.AddEvent(&lEvent, 230, sizeof(lEvent));
}

// ARTIST8250D4B8..D4DC publishes the same bool stored in the actual member.
void GuiDebugComponent::MiniMapSwitch()
{
    const bool lbVisible = !mbMiniMapVisible;
    mbMiniMapVisible = lbVisible;
    mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lbVisible),
        205, sizeof(lbVisible));
}

// ARTIST824ED1C8..D4 /824ED270..27C inline the two original simple toggles.
void GuiDebugComponent::ToggleShow43Area()
{
    mbShow43Area = !mbShow43Area;
}

void GuiDebugComponent::ToggleShowSafeArea()
{
    mbShowSafeArea = !mbShowSafeArea;
}

// ARTIST8250D648; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::EnterCarSelectCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "EnterCarSelectCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1101);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->OpenCarSelect();
}

// ARTIST8250CF50; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::TriggerIntroCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "Invalid data pointer sent to GuiDebugComponent::TriggerIntro";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 753);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->TriggerIntro();
}

// ARTIST82513140; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::TriggerPostEventCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "Invalid data pointer sent to GuiDebugComponent::TriggerPostEventCallback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 766);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->TriggerPostEvent();
}

// ARTIST8250D598; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::OpenCrashNavCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "OpenCrashNavCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1075);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->OpenCrashNav();
}

// ARTIST8250D428; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::MiniMapSwitchCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "MiniMapSwitchCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1008);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->MiniMapSwitch();
}

// ARTIST824ED138; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::ToggleShow43AreaCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "ToggleShow43AreaCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1036);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->ToggleShow43Area();
}

// ARTIST824ED1E0; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::ToggleShowSafeAreaCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "ToggleShowSafeAreaCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1049);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->ToggleShowSafeArea();
}

// ARTIST8250D160; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::ChangeFontSpacingCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 887);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->ChangeFontSpacing();
}

// ARTIST8250D4E8; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::PlayMovieCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "PlayMovieCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1022);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->PlayMovie();
}

// ARTIST8250D0B0; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::RotateThroughFontsCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 873);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->RotateThroughFonts();
}

// ARTIST8250D000; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::RotateThroughFontEffectsCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 859);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->RotateThroughFontEffects();
}

// ARTIST8250D6F8; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::TriggerPFXCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "TriggerPFXCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1179);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->TriggerPFX();
}

// ARTIST8250D798; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::TriggerClosePFXCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "TriggerPFXCloseCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1192);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->TriggerClosePFX();
}

// ARTIST8250D220; plain void(void*) callback, original assert-and-continue.
void GuiDebugComponent::ToggleGenericHUDPriorityOverrideCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 900);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->ToggleGenericHUDPriorityOverride();
}

// ARTIST824ED288. lpUserData is an integer menu index, not a component pointer.
void GuiDebugComponent::ToggleRaceHudComponentsCallback(void* lpValue, void* lpUserData)
{
    (void)lpValue;
    GuiDebugComponent* lpThis = GetSingletonPtr();
    RaceMainHudState* lpRaceMainHudState = lpThis->mpGuiModule->mHudFlow.mpRaceMain;
    const bool lbOverride = lpThis->mbToggleableRaceHUDEnabled;
    if (reinterpret_cast<uintptr_t>(lpUserData) != 21 && !lbOverride)
        return;
    if (lbOverride)
        lpRaceMainHudState->ForceReenter(true,
            GetSingletonPtr()->mabTogglableRaceHUDElementsState);
    else
        lpRaceMainHudState->ForceReenter(false, 0);
}

// ARTIST825131E0: zero exactly the two bit words and song index, then set preview.
// The record's three trailing padding bytes remain unwritten.
void GuiDebugComponent::TriggerEATraxComponent()
{
    GuiEATraxNewTrackEvent lEvent;
    lEvent.mRemainingSongs.Construct();
    lEvent.miSongIndex = 0;
    lEvent.mbPreview = 1;
    mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
        502, sizeof(lEvent));
}

// ARTIST82513228: original buddy-notification order is notification/count/name.
void GuiDebugComponent::TriggerOnlineInviteComponent()
{
    GuiEventBuddyNotification lEvent;
    lEvent.mBuddyName.Construct("Linus T");
    lEvent.meBuddyNotification = static_cast<BrnNetwork::EBuddyNotification>(0);
    lEvent.miNumBuddies = 0;
    mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
        105, sizeof(lEvent));
}

// These two original callbacks contain no null diagnostic or additional guard.
void GuiDebugComponent::TriggerEATraxComponentCallback(void* lpUserData)
{
    static_cast<GuiDebugComponent*>(lpUserData)->TriggerEATraxComponent();
}

void GuiDebugComponent::TriggerOnlineInviteCallback(void* lpUserData)
{
    static_cast<GuiDebugComponent*>(lpUserData)->TriggerOnlineInviteComponent();
}

// ARTIST8250D848 ignores its registered user-data and uses the real singleton/cache.
void GuiDebugComponent::TriggerPaybackComponent(void* lpUserData)
{
    (void)lpUserData;
    GuiDirtyTrickNewEvent lEvent;
    lEvent.meTrickType = static_cast<BrnNetwork::EPaybackType>(0);
    GuiDebugComponent* lpThis = GetSingletonPtr();
    const EActiveRaceCarIndex lePlayer = static_cast<EActiveRaceCarIndex>(
        lpThis->mpGuiModule->mGuiCache.GetPlayerActiveRaceCarIndex());
    lEvent.meAggressorActiveRaceCarIndex = lePlayer;
    lEvent.meVictimActiveRaceCarIndex = lePlayer;
    lpThis->mInputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
        177, sizeof(lEvent));
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

    // ARTIST8250D408..18 posts this complete64-byte payload.
    static_assert(sizeof(GuiPFXHookEvent) == 64, "event495 payload is64 bytes");
    mInputQueue.AddEvent(&lEvent, 495, sizeof(lEvent));
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
