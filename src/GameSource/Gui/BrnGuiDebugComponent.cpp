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
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameSource/Gui/Flow/PostEvent/States/Online/BrnOnlineInstantResults.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineInstantResults.h"
#include <cmath>
#include "GameSource/Gui/Events/BrnGuiEventAudioTrax.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include "GameShared/GameClasses/Core/CgsID.h"   // CgsIDCompress (TriggerPostEvent's unlock ids)
#include "GameSource/Gui/BrnGuiModule.h"         // GuiModule::mGuiCache (TriggerPostEvent)
#include "GameSource/Gui/BrnGuiKeyboard.h"
#include "GameSource/Gui/BrnGuiKeyboardHost.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

#include <cstring>                               // strlen / strncpy / memset

// Original byte82FB5098 is owned by the actual Apt communicator TU.
extern bool gbLogGuiAudioTriggers;

// Existing compatibility provider; the original wrapper always disables screen checking.
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
    f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCheckScreen);

namespace
{
    // ARTIST82FB5920: original file-global count, not reset by Construct.
    u32 guGuiDebugUpdateCount = 0;

    const char* const gpcRaceHudElementLabels[21] =
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

    // ARTIST824FD478 string constants, read as UTF-16BE code units.
    const CgsGui::CgsUtf16 gacShowKeyboardDefaultText[] = {
        0x0044, 0x0065, 0x0066, 0x0061, 0x0075, 0x006C, 0x0074,
        0x0020, 0x0054, 0x0065, 0x0078, 0x0074, 0x0000
    };
    const CgsGui::CgsUtf16 gacShowKeyboardTitleText[] = {
        0x0054, 0x0069, 0x0074, 0x006C, 0x0065, 0x0020, 0x0054,
        0x0065, 0x0078, 0x0074, 0x0000
    };
    const CgsGui::CgsUtf16 gacShowKeyboardDescriptionText[] = {
        0x0044, 0x0065, 0x0073, 0x0063, 0x0072, 0x0069, 0x0070,
        0x0074, 0x0069, 0x006F, 0x006E, 0x0020, 0x0054, 0x0065,
        0x0078, 0x0074, 0x0000
    };
}

namespace BrnGui
{

// X360 off_82FB5080 -- the live singleton instance, populated when the debug component
// is constructed. Defaults to null until then.
GuiDebugComponent* GuiDebugComponent::mspSingletonThis = nullptr;

// ARTIST827E5D38/40. No module, singleton, display, PFX, base-active or link field reset.
GuiDebugComponent::GuiDebugComponent()
    : CgsDev::DebugComponent(CgsDev::DebugComponent::GuiConstructorTag{})
{
    mInputQueue.MarkUnconstructed();
}

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
    mSpeedoInfo.miPlayerMPH = 0;
    mSpeedoInfo.miPlayerRPM = 0;
    mSpeedoInfo.miPlayerGear = 0;
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

void GuiDebugComponent::OnActivate()
{
    RegisterVariable(&mbRenderGui, "Render GUI Objects");
    RegisterVariable(&mbDebugOfflineInstantResults, "Debug Offline Results Screen");
    RegisterVariable(&mbDebugOnlineInstantResults, "Debug Online Results Screen");
    RegisterFunction(&GuiDebugComponent::EnterCarSelectCallback, this, "Trigger Car Select");
    RegisterFunction(&GuiDebugComponent::TriggerIntroCallback, this, "Trigger Intro");
    RegisterFunction(&GuiDebugComponent::TriggerPostEventCallback, this, "Trigger Offline Post Event");
    RegisterVariable(&miDebugFinishPos, "    Finish position:");
    SetRange(&miDebugFinishPos, 1, 8);
    RegisterVariable(&mbDebugFinishRankup, "    Do rank-up in post-event:");
    RegisterVariable(&miDebugPlayerRank, "    Rank at start of post-event:");
    SetRange(&miDebugPlayerRank, 0, 6);
    RegisterVariable(&mbDebugFinishUnlockNormalCar, "    Do unlock livery car (BR end):");
    RegisterVariable(&mbDebugFinishUnlockFreeCar, "    Do unlock free car (rank up):");
    RegisterVariable(&mbDebugFinishUnlockRival, "    Do unlock rival:");
    RegisterFunction(&GuiDebugComponent::OpenCrashNavCallback, this, "Trigger crash nav");
    RegisterFunction(&GuiDebugComponent::MiniMapSwitchCallback, this, "Toggle MiniMap Visibility");
    RegisterVariable(&mbMiniMapVisible, "   MiniMap Visibility: ");
    SetReadOnly(&mbMiniMapVisible, true);
    RegisterVariable(&mbSpeedoVisible, "Speedo Visibility: ");
    RegisterFunction(&GuiDebugComponent::ToggleShow43AreaCallback, this, "Show/Hide 4:3 Area");
    RegisterFunction(&GuiDebugComponent::ToggleShowSafeAreaCallback, this, "Show/Hide Safe Area");
    RegisterVariable(&mfSafePercent, "   Safe Area:");
    SetRange(&mfSafePercent, 70.0f, 100.0f);
    RegisterFunction(&GuiDebugComponent::ChangeFontSpacingCallback, this, "Change Font Spacing");
    RegisterVariable(&mfFontSpacing, "   Spacing percentage:");
    SetRange(&mfFontSpacing, 30.0f, 250.0f);
    // Actual inherited mpGuiView, then the shared View mbUpdateFlash field.
    RegisterVariable(&mpGuiModule->mpGuiView->mbUpdateFlash, "Update Flash");
    RegisterFunction(&GuiDebugComponent::BootupProfileCallback, this, "Bootup Load/Save Profile");
    RegisterFunction(&GuiDebugComponent::AutosaveProfileCallback, this, "Autosave Profile");
    RegisterFunction(&GuiDebugComponent::SaveProfileCallback, this, "Save Profile");
    RegisterFunction(&GuiDebugComponent::LoadProfileCallback, this, "Load Profile");
    RegisterFunction(&GuiDebugComponent::ShowKeyboardCallback, this, "Show Keyboard");
    RegisterVariable(&::gbLogGuiAudioTriggers, "Toggle Audio Trigger Debug Spew");
    RegisterFunction(&GuiDebugComponent::PlayMovieCallback, this, "Play/stop test movie");
    RegisterFunction(&GuiDebugComponent::RotateThroughFontsCallback, this, "Select next font as default");
    RegisterFunction(&GuiDebugComponent::RotateThroughFontEffectsCallback, this, "Select next font effect");
    RegisterFunction(&GuiDebugComponent::TriggerPFXCallback, this, macCurrentPFXHookName);
    RegisterVariable(&miCurrentPFXHookIndex, "  Current Hook");
    SetRange(&miCurrentPFXHookIndex, -1, -1);
    RegisterFunction(&GuiDebugComponent::TriggerClosePFXCallback, this, "Trigger PFX CloseMenu");
    RegisterFunction(&GuiDebugComponent::ToggleGenericHUDPriorityOverrideCallback, this,
        "Toggle Generic HUD Priority Event Overrides");
    RegisterVariable(&mbPriEventOverride, "Priority Overrides On");
    RegisterVariable(&miScreenFlowStreamingMode, "Screen Flow Streaming");
    SetOptions(&miScreenFlowStreamingMode, maStreamingOptions);
    SetRange(&miScreenFlowStreamingMode, 0, 2);
    RegisterFunction(&GuiDebugComponent::TriggerEATraxComponentCallback, this,
        "Components", "Trigger EA Trax Component");
    RegisterFunction(&GuiDebugComponent::TriggerOnlineInviteCallback, this,
        "Components", "Trigger Receive Online Invite");
    RegisterFunction(&GuiDebugComponent::TriggerPaybackComponent, this,
        "Components", "Trigger Payback Component");
    RegisterVariable(&mbWorstCaseHudActive, "Race HUD Toggling", "Enable Worst Case HUD");
    RegisterVariable(&mbToggleableRaceHUDEnabled, "Race HUD Toggling", "Enable Toggling Race HUD");
    SetChangeCallback(&mbToggleableRaceHUDEnabled,
        &GuiDebugComponent::ToggleRaceHudComponentsCallback,
        reinterpret_cast<void*>(static_cast<uintptr_t>(21)));
    for (s32 liItem = 0; liItem < 21; ++liItem)
    {
        RegisterVariable(&mabTogglableRaceHUDElementsState[liItem],
            "Race HUD Toggling", gpcRaceHudElementLabels[liItem]);
        SetChangeCallback(&mabTogglableRaceHUDElementsState[liItem],
            &GuiDebugComponent::ToggleRaceHudComponentsCallback,
            reinterpret_cast<void*>(static_cast<uintptr_t>(liItem)));
    }
    CgsGui::GuiEvent<500> lEnumerateHooks;
    mInputQueue.AddEvent(&lEnumerateHooks, 500, sizeof(lEnumerateHooks));
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

// ARTIST824F7CF0. Decode the actual VMADD fields as VA*VC+VB;
// their last two printed IDA operands are in physical-field order.
void GuiDebugComponent::DrawDebugFrame1280(CgsDev::Debug2DImmediateRender* lpRender,
    Vector4 lvFrame, CgsDev::RGBA lColour, f32 lfBorderSize)
{
    const Vector2 lvScreen = lpRender->GetVirtualScreenSize();
    const f32 lfInvWidth = 0.0007812500116415322f;
    const f32 lfInvHeight = 0.0013888889225199819f;
    const f32 lfX0 = std::fma(lfInvWidth * lvFrame.x, lvScreen.x, 1.0f);
    const f32 lfY0 = std::fma(lfInvHeight * lvFrame.y, lvScreen.y, 1.0f);
    const f32 lfX1 = (lfInvWidth * lvFrame.z) * lvScreen.x - 1.0f;
    const f32 lfY1 = (lfInvHeight * lvFrame.w) * lvScreen.y - 1.0f;
    lpRender->DrawFrame(lfX0, lfY0, lfX1, lfY1, lColour, lfBorderSize);
}

// ARTIST824FD010. Function-local rectangle constants are native1280x720 coordinates.
void GuiDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
{
    static const Vector4 lFullArea = { 0.0f, 0.0f, 1280.0f, 720.0f };
    static const Vector4 lFourThreeArea = { 160.0f, 0.0f, 1120.0f, 720.0f };
    if (mbShow43Area)
        DrawDebugFrame1280(lpRender, lFourThreeArea, 0xFF00FF00u, 1.0f);
    if (mbShowSafeArea)
    {
        const Vector4& lrArea = mbShow43Area ? lFourThreeArea : lFullArea;
        const f32 lfHalfWidth = (lFullArea.z - lFullArea.x) * 0.5f;
        const f32 lfHalfHeight = (lFullArea.w - lFullArea.y) * 0.5f;
        const f32 lfPercent = mfSafePercent * 0.01f;
        Vector4 lvSafeArea;
        lvSafeArea.x = std::fma(lrArea.x - lfHalfWidth, lfPercent, lfHalfWidth);
        lvSafeArea.y = std::fma(lrArea.y - lfHalfHeight, lfPercent, lfHalfHeight);
        lvSafeArea.z = std::fma(lrArea.z - lfHalfWidth, lfPercent, lfHalfWidth);
        lvSafeArea.w = std::fma(lrArea.w - lfHalfHeight, lfPercent, lfHalfHeight);
        DrawDebugFrame1280(lpRender, lvSafeArea, 0xFF0000FFu, 1.0f);
    }
    if (mpGuiModule->mScreenFlow.mpStateOnlineInstantResults && mbDebugOnlineInstantResults)
        mpGuiModule->mScreenFlow.mpStateOnlineInstantResults->RenderDebug(lpRender);
    if (mpGuiModule->mScreenFlow.mpStateOfflineInstantResults && mbDebugOfflineInstantResults)
        mpGuiModule->mScreenFlow.mpStateOfflineInstantResults->RenderDebug(lpRender);
    if (mbSpeedoVisible)
    {
        char lacBuffer[1024];
        CgsDev::StrStream lStream(lacBuffer, sizeof(lacBuffer));
        lStream << "SPEED : " << mSpeedoInfo.miPlayerMPH << "mph \n";
        MaybeDrawText(lpRender, lStream.GetBuffer(), 50.0f, 50.0f, 30.0f, 0xFFFFFFC8u, false);
        lStream.Reset();
        lStream << "RPM : " << mSpeedoInfo.miPlayerRPM << " \n";
        MaybeDrawText(lpRender, lStream.GetBuffer(), 50.0f, 80.0f, 30.0f, 0xFFFFFFC8u, false);
        lStream.Reset();
        lStream << "GEAR : " << mSpeedoInfo.miPlayerGear << " \n";
        MaybeDrawText(lpRender, lStream.GetBuffer(), 50.0f, 110.0f, 30.0f, 0xFFFFFFC8u, false);
        lStream.Reset();
    }
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

// Original command is gated only by the actual SystemUserProfile state word.
void GuiDebugComponent::BootupProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState == 1)
        mpGuiModule->mProfileHost.Bootup();
}

// Original command is gated only by the actual SystemUserProfile state word.
void GuiDebugComponent::SaveProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState == 1)
        mpGuiModule->mProfileHost.Save();
}

// Original command is gated only by the actual SystemUserProfile state word.
void GuiDebugComponent::LoadProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState == 1)
        mpGuiModule->mProfileHost.Load();
}

// ARTIST825238E0: same original manager/handler and autosave latch/time owners.
void GuiDebugComponent::AutosaveProfile()
{
    if (mpGuiModule->mSystemUserProfile.miState == 1)
    {
        ProfileManager& lrManager = mpGuiModule->mProfileHost.GetProfileManager();
        if (lrManager.AutosaveIsEnabled())
        {
            lrManager.Autosave(&mpGuiModule->mProfileHost, 0, 0);
            mpGuiModule->mbProfileAutosavePending = false;
            mpGuiModule->mfLastProfileAutosaveTime = mpGuiModule->mGuiCache.GetTime();
        }
    }
}

// ARTIST82523970; original assertion and continuation into the real command.
void GuiDebugComponent::BootupProfileCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "BootupProfileCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1114);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->BootupProfile();
}

// ARTIST82523A30; original assertion and continuation into the real command.
void GuiDebugComponent::AutosaveProfileCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "AutosaveProfileCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1127);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->AutosaveProfile();
}

// ARTIST82523AD0; original assertion and continuation into the real command.
void GuiDebugComponent::SaveProfileCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "SaveProfileCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1140);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->SaveProfile();
}

// ARTIST82527998; original assertion and continuation into the real command.
void GuiDebugComponent::LoadProfileCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "LoadProfileCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1153);
        CgsDev::Assert::EndAssert();
    }
    static_cast<GuiDebugComponent*>(lpUserData)->LoadProfile();
}

// ARTIST824FD478. The native callback asserts on null userdata, then continues.
void GuiDebugComponent::ShowKeyboardCallback(void* lpUserData)
{
    if (!lpUserData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::MessageBufferPC lMessage;
        CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
        lStream << "ShowKeyboardCallback: Invalid data in the callback";
        CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
            "..\\..\\..\\GameSource\\Gui/BrnGuiDebugComponent.cpp", 1166);
        CgsDev::Assert::EndAssert();
    }

    GuiDebugComponent* lpThis = static_cast<GuiDebugComponent*>(lpUserData);
    lpThis->mpGuiModule->mKeyboardHost.mpGuiKeyboard->Show(
        gacShowKeyboardDefaultText,
        gacShowKeyboardTitleText,
        gacShowKeyboardDescriptionText,
        lpThis->mpGuiModule->mKeyboardHost);
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
