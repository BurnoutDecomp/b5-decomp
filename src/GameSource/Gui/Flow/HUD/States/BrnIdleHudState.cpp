#include "GameSource/Gui/Flow/HUD/States/BrnIdleHudState.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // [FLAG PC witness] [idle-hud]
#include "GameShared/GameClasses/Containers/CgsHash.h"                   // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface::RegisterForEvents
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // the state's input event queue
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The idle HUD state's entry wiring.
//
//   OnEnter @0x824759C0 -- mpStateInterface->RegisterForEvents(maiEventToObserve, 1), then
//                          reset the load bookkeeping (meCurrentState = E_IDLE_HUD_RESOURCES,
//                          mpGuiCache = 0, mbIsLoaded = false) and construct the embedded text
//                          field against the "text_txt" apt component on this state's interface.
//                          The X360 reaches mTextField at this+0x3C and dispatches its first
//                          virtual (TextField::Construct) with (name, mpStateInterface, 0); here
//                          we call Construct by name. The trailing argument is the parent-name
//                          pointer (NULL: this field has no parent clip).
//
// The observed-event id table (maiEventToObserve) and the static resource table both live in
// .data. The IDA export set is function-only, so neither carries a value there -- both were read
// out of the XEX image instead, and each is bounded by an independent check. See the two banners
// below.

namespace BrnGui
{

namespace
{
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // Event 64's payload: the first word is the GuiCache (Update: `lwz r11, 0(r20)`).
    struct GuiEventCache : CgsModule::Event { GuiCache* mpGuiCache; };

    const s32 KI_EVENT_GUI_CACHE = 64;

    // [FLAG PC witness] NOT X360. One line per load-sequence step; bounded by the state machine.
    void lWitnessIdleHud(const char* lpacStep)
    {
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "[idle-hud] " << lpacStep << " [FLAG PC witness]\n";
    }
}

// =======================================================================
//  The observed-event table @0x8205B224 (count @0x8205B228)
// =======================================================================
// RECOVERED (was a `{ 0 }` placeholder). The IDA export set carries no data symbols, so the
// word was read out of the XEX image. It is 64 -- the per-frame GuiCache hand-off -- and four
// independent checks agree:
//   * OnEnter's asm passes the width literally: `li r5, 1` @0x824759D8, and OnLeave repeats it
//     (`UnRegisterForEvents(&unk_8205B224, 1)`).
//   * the word immediately after the run, @0x8205B228, is itself 1 -- miNumEventsObserved, the
//     same {table}{count} adjacency CrashedStuntHudState's 12-entry table has.
//   * the run is bounded on BOTH sides by unrelated data: ASCII "...iplierScore_mc" + a 2.0f
//     before it, and the assert string "NULL != mpFlaptManag..." right after the count.
//   * SEMANTICS referee: Update @0x8247E070 dispatches exactly ONE id, `if (FirstEvent == 64)`,
//     and that arm is the only writer of mpGuiCache (+0x164). A state that observes one event
//     and handles one event must observe the one it handles.
// The old placeholder was not inert: registering for a never-posted id 0 meant event 64 never
// arrived, so mpGuiCache stayed NULL, EnsureResourcesAreLoaded was never reached, and the whole
// state was dead on entry.
const s32 IdleHudState::maiEventToObserve[1] = { 64 };
const s32 IdleHudState::miNumEventsObserved = 1;

// =======================================================================
//  The static resource table @0x82F264BC (count @0x82F264CC)
// =======================================================================
// RECOVERED (was an empty 1-entry table with count 0 -- a stub that compiled, linked, ran, and
// loaded nothing). Read out of the XEX image; GetResourcesToLoad's asm @0x82508530 pins both
// addresses (`lis/addi unk_82F264BC` -> *r4, `lwz dword_82F264CC` -> *r5), and the extent is
// self-confirming: 0x82F264CC - 0x82F264BC == 0x10 == exactly two 8-byte tuples, and the count
// word reads 2. The words after the count (0x7F7FFFFF, 0xFFFF0000, ...) are float/mask
// constants, not a third tuple. Each id is named via off_82F278E0[id] from the same image --
// the name table the RaceMainHudState 21-entry recovery used, re-checked here against its
// published names (192 -> "B5RaceHud", 32 -> "Timer", 24 -> "B5CompassComponent",
// 199 -> "SatNavMap", 90 -> "B5VersionTextComponent" all reproduce).
// Both are type 7 == E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE, and the pair matches what this state
// actually is: its own movie plus the TextField import behind OnEnter's "text_txt" Construct.
const CgsGui::sResourceTuple IdleHudState::maResourcesToLoad[] =
{
    { 195u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5IdleHud
    {  29u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // TextField
};
u32 IdleHudState::muNumResourcesToLoad = 2;

// @ 0x824759C0
void IdleHudState::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    meCurrentState = E_IDLE_HUD_RESOURCES;   // this+0x38 = 0
    mpGuiCache     = 0;                       // this+0x164 = 0
    mbIsLoaded     = false;                   // this+0x168 = 0

    mTextField.Construct("text_txt", mpStateInterface, 0);
    lWitnessIdleHud("enter");
}

// @ 0x8247E230. The console inlines PlayAptMovie: a GuiEvent<18> on channel 41 whose movie name is
// the empty string (unk_820046A7) at level 1.
void IdleHudState::OnLeave()
{
    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    mpStateInterface->PlayAptMovie("", 1);
    lWitnessIdleHud("leave");
}

// @ 0x8247E070. Walks the input queue and acts only on the cache event: the first one latches
// mpGuiCache (+0x164), then each one advances the load sequence as far as it can go. The console
// inlines PlayAptMovie("B5IdleHud", 1) (channel 41, 20 bytes, name off_82F278E0[195]) and the
// strlen before CalculateHash.
void IdleHudState::Update()
{
    StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liEventId != KI_EVENT_GUI_CACHE)
            continue;

        if (mpGuiCache == 0)
            mpGuiCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;

        if (meCurrentState == E_IDLE_HUD_RESOURCES &&
            mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            mpStateInterface->PlayAptMovie("B5IdleHud", 1);

            muNumExpectedComponents = 0;
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);
            for (u32 luI = 0; luI < KU_MAX_IDLEHUD_COMPONENTS_NUM; ++luI)
                mauExpectedComponentIds[luI] = 0;

            // The expected component is the text field OnEnter constructed (+0x40 == its name).
            char* lpacName = const_cast<char*>(mTextField.GetName());
            mauExpectedComponentIds[muNumExpectedComponents] =
                CgsContainers::CgsHash::CalculateHash(lpacName, static_cast<s32>(std::strlen(lpacName)));
            ++muNumExpectedComponents;
            mpGuiCache->SetExpectedAptComponentList(E_GUIFLOW_HUD, mauExpectedComponentIds,
                                                    muNumExpectedComponents);
            meCurrentState = E_IDLE_HUD_COMPONENTS;
            lWitnessIdleHud("resources loaded, B5IdleHud playing");
        }

        if (meCurrentState == E_IDLE_HUD_COMPONENTS &&
            mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))
        {
            meCurrentState = E_IDLE_HUD_LOADED;
            mTextField.SetText(" ");   // asc_82004094
            lWitnessIdleHud("components initialised");
        }
    }

    lpInQueue->Clear();
}

} // namespace BrnGui
