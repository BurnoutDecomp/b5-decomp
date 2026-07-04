// ===========================================================================
// BrnBootLegalBoundary.cpp -- boundary bodies for BrnGui::BootLegal (BF_LEGAL).
//
// BootLegal (BrnBootLegal.cpp) is reconstructed faithfully from the X360 ARTIST
// disasm; the gated-subsystem edges it reaches are abstracted into the
// BootLegalFlag:: and BrnGui::BootLegalCacheBoundary:: free helpers. This TU
// supplies their bodies so BootLegal links and runs in the boot flow.
//
// Most are FAITHFUL DEFAULTS for a normal boot (the correct runtime values, not
// fabrications): a fresh boot is NOT a soft-reboot; the cache's apt components are
// ready (PC loads synchronously); the legal screen's resources are resident. The
// genuinely-gated edges (the movie-definition prepare, the sound-name hash, and
// the Apt-component watcher) are FLAG'd no-ops/zeros until those subsystems land.
// ===========================================================================

#include "types.hpp"
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"      // CgsGui::GuiComponent
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"      // BrnGui::MenuComponent
#include "GameSource/Gui/BrnAptRuntimeBringUp.h"                         // BrnGui::AptRuntimeSetComponentViewState (PC shim)

#include <chrono>   // FLAG: PC wall-clock time source standing in for the GUI cache's frame time

namespace BootLegalFlag
{
    // off_830102D0 -- the BrnGameModule singleton pointer. The soft-reboot queries below
    // only read it to answer "was this a soft reboot"; on a normal cold boot the answer is
    // no, so a null module + false answers are the faithful runtime values.
    void* gpBrnGameModule = nullptr;

    // byte_82FFA7F0 -- the DLC "beat the team" enable flag (off by default).
    // PC bring-up (FLAG): default ON so the title's 2-entry selection menu
    // (DisplaySelectionMenu: $TITLESCREEN_MENU_NORMAL / _BEATTHETEAM) is exercised on
    // press-start. The console reads this from the DLC/licence state (byte_82FFA7F0).
    u8 gu8BeatTheTeamDlcEnabled = 1u;

    // 0x823C0278 / 0x823C0288 -- a cold boot is neither a soft reboot nor an invite reboot.
    bool HasGameBeenSoftRebooted(void* /*lpModule*/)          { return false; }
    bool HasGameBeenRebootedDueToInvite(void* /*lpModule*/)   { return false; }
    const s32* GetSoftRebootData(void* /*lpModule*/)          { return 0; }   // no soft-reboot record

    // FLAG: BrnGui::MovieManager::VideoDefinition::Prepare -- prepares the attract/title movie
    // definition for the movie system. Deferred (the title-movie playback bridge is a follow-on);
    // a no-op lets the legal-screen stage machine advance without the title movie.
    void MovieVideoDefinition_Prepare(void* /*lpDefinition*/) {}

    // FLAG: CgsSound::Playback::Name::MakeHash -- the music/sound name hasher (deferred).
    s32 SoundPlaybackNameMakeHash(const char* /*lpacName*/)   { return 0; }

    // BrnResource::DLCBeatTheTeamGame::SetEnabledState -- faithfully write the enable byte.
    void DLCBeatTheTeamGame_SetEnabledState(u8* lpFlag, s32 liState)
    {
        if (lpFlag != 0)
            *lpFlag = static_cast<u8>(liState);
    }
}

namespace BrnGui
{
namespace BootLegalCacheBoundary
{
    // FLAG: the cache's apt-component watcher half (ClearExpectedAptComponentList /
    // AppendExpectedAptComponent (sub_824F87C0) / AreAllAptComponentsInitialised) drives the
    // Apt engine's per-component init handshake. The Apt engine is not linked yet, so the
    // watcher is a no-op and "all initialised" is true (faithful default: on PC the components
    // are constructed synchronously in OnEnter, so the cache-wait stage may advance).
    void ClearExpectedAptComponentList(GuiCache* /*lpCache*/, s32 /*liFlow*/) {}
    void AppendExpectedAptComponent(GuiCache* /*lpCache*/, s32 /*liFlow*/, CgsGui::GuiComponent* /*lpComponent*/) {}
    bool AreAllAptComponentsInitialised(const GuiCache* /*lpCache*/, s32 /*liFlow*/) { return true; }

    // The legal screen's static resources are resident (PC loads synchronously).
    bool EnsureResourcesAreLoaded(GuiCache* /*lpCache*/)      { return true; }
    void EnsureBootResourceIsLoaded(GuiCache* /*lpCache*/)    {}
    void UnloadResources(GuiCache* /*lpCache*/, const CgsGui::sResourceTuple* /*lpResources*/, u32 /*luCount*/) {}

    // FLAG (PC time source): the X360 reads the GUI cache's frame time. The committed GuiCache
    // has no GetTime; stand in with a monotonic wall clock (seconds since first call) so the
    // legal screen's dwell timers (wait-start 3.0s / attract 25.0s / accept 0.5s) elapse for real.
    f32 GetTime(const GuiCache* /*lpCache*/)
    {
        static const std::chrono::steady_clock::time_point s_start = std::chrono::steady_clock::now();
        const std::chrono::duration<float> lElapsed = std::chrono::steady_clock::now() - s_start;
        return lElapsed.count();
    }

    // FLAG (no named accessor on the committed GuiCache) -- faithful cold-boot defaults:
    // X360 *(cache+19273): the HD-composite transition has not run yet, so let it run (false).
    bool IsHDCompAlreadyTransitioned(const GuiCache* /*lpCache*/) { return false; }
    // X360 *(cache+42996): the ESRB panel is not force-visible on a normal boot.
    bool IsEsrbVisible(const GuiCache* /*lpCache*/)               { return false; }
    // X360 *(cache+77578/77579): the start message is not forced (mbWaitForStartPressed drives it).
    bool IsStartMessageForced(const GuiCache* /*lpCache*/)        { return false; }
}
}

// ===========================================================================
// FLAG (GUI-component layer not reconstructed): the CgsGui::GuiComponent base + the
// BrnGui::MenuComponent selection-menu UI that BootLegal drives. Stubbed so the legal/title-
// screen flow links + runs its stage machine; the component construction + menu rendering +
// the Apt-view emit (AddOutputAptViewState -> AptAux::UpdateFlashComponent -> the Apt engine)
// are the deferred GUI/Apt render boundary. The flow reaching DisplaySelectionMenu's
// AddOutputAptViewState("apt_Transition", ...) call IS the title_screen02 request.
// ===========================================================================
namespace CgsGui
{
    // PC bring-up (FLAG): keep the component's NAME (the movie's PLACE instance name)
    // so AddOutputAptViewState can reach the clip; the state interface may be null on
    // the boot path (SetStateInterface asserts, so store it only when supplied).
    void GuiComponent::Construct(const char* lpacName, StateInterface* lpStateInterface, const char* lpacParentName)
    {
        // Inline the name store (the real SetName TU -- CgsGuiComponent.cpp -- is not in
        // this build; it also drags the unwired AptAux::UpdateFlashComponent).
        macName[0] = '\0';
        if (lpacName != nullptr)
        {
            u32 lu = 0;
            for (; lpacName[lu] != '\0' && lu < KU_MAX_COMPONENT_NAME_LEN - 1; ++lu)
                macName[lu] = lpacName[lu];
            macName[lu] = '\0';
        }
        (void)lpacParentName;   // boot components pass no parent
        muHashedName = 0;
        mpStateInterface = lpStateInterface;
    }

    // PC bring-up shim (FLAG): the faithful path is FillAptViewMessage -> AptAux::
    // UpdateFlashComponent -> AptCommunicator -> the movie AS. That glue is not wired
    // yet; drive the observable effect (gotoAndPlay the view-state label on this
    // component's clip) through the Apt runtime bridge directly.
    void GuiComponent::AddOutputAptViewState(const char* /*lpacAptName*/, const char* lpacViewState, bool /*lbImmediate*/)
    {
        BrnGui::AptRuntimeSetComponentViewState(GetName(), lpacViewState);
    }
}

namespace BrnGui
{
    void MenuComponent::Construct(const char* /*lpacName*/, CgsGui::StateInterface* /*lpStateInterface*/, int /*liA*/, int /*liB*/, s64 /*liC*/) {}
    void MenuComponent::SetupMenu(int /*liNumRows*/, bool /*lbWrap*/) {}
    void MenuComponent::SetText(int /*liRow*/, const char* /*lpacText*/) {}
    void MenuComponent::Refresh() {}
    void MenuComponent::SelectPrevious() {}
    void MenuComponent::SelectNext() {}
}
