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
#include "GameSource/Gui/BrnGuiAptRuntime.h"                             // BrnGui::gpActiveAptRuntimeHost (GUI-owned Apt host)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"             // CgsSound::Playback::Name::MakeHash (homed)

#include <cstdio>   // std::snprintf (the menu facade component names)
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

    // CgsSound::Playback::Name::MakeHash -- the music/sound name hasher, homed in
    // CgsCommon.cpp (@0x82689A50, boot-trace verified). Call the real one so the
    // menu-music event (155) carries the faithful hash its consumer keys on (this
    // was a return-0 stub, which made every music post read as a "stop").
    s32 SoundPlaybackNameMakeHash(const char* lpacName)
    {
        return static_cast<s32>(CgsSound::Playback::Name::MakeHash(lpacName));
    }

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
    // A component reports initialised only once its clip is PLACED; gate on the movie
    // having composed (frame-0 place commands ran) so E_STAGE_FADE_IN's view states
    // land on live clips. (With the paced 30fps tick, "return true" raced the first
    // tick and the HDComp/esrb transins were lost -- clip-not-found.)
    bool AreAllAptComponentsInitialised(const GuiCache* /*lpCache*/, s32 /*liFlow*/)
    {
        return BrnGui::gpActiveAptRuntimeHost != 0 &&
               BrnGui::gpActiveAptRuntimeHost->IsMovieComposed();
    }

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

// The GuiComponent facade that used to live here (a bring-up Construct + the
// AddOutputAptViewState -> AptRuntimeSetComponentKeyValue bridge) has been RETIRED
// (2026-07-05): the real CgsGui::GuiComponent TU (CgsGuiComponent.cpp -- SetName /
// SetStateInterface / Construct / FillAptViewMessage / AddOutputAptViewState) is now
// in the build, and BF_LEGAL's state interface is Prepare'd with live access pointers
// (BrnGuiModule.cpp), so the components drive the faithful chain: AddOutputAptViewState
// -> FillAptViewMessage -> AptAux::UpdateFlashComponent -> AptCommunicator key/values
// -> the per-frame AptAux::UpdateComponents flush ("UpdateAll" to the movie AS). The
// direct clip-effect bridge remains as a FLAG'd fallback inside the real
// AddOutputAptViewState until the AS-framework movie drives the clips.
// (The MenuComponent facade was retired earlier -- the real BrnGui::MenuComponent /
// SelectableGroup / MenuItem / Selectable classes drive the menu.)
