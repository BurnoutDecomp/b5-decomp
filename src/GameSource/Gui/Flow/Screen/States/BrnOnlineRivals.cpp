#include "GameSource/Gui/Flow/Screen/States/BrnOnlineRivals.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::EnsureResourcesAreLoaded / AreAllAptComponentsInitialised

// BrnGui::OnlineRivals -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This TU bodies three ledger functions (DWARF primary file
// GameSource/Gui/Flow/Screen/States/BrnOnlineRivals.cpp):
//   OnlineRivals::CheckForCompletedLoads          @0x824A05F0
//   OnlineRivals::HandleControllerInput           @0x8248DD38
//   OnlineRivals::HandleControllerInputSelectParams @0x82486D60
// The remaining bodies (OnEnter/OnLeave/Update/HandleGuiCacheEvent) are their own ledger
// functions, added later.

namespace BrnGui
{
    // X360 .rdata @0x8205F854 (unk_8205F854): the single resource tuple this screen streams.
    // FLAG: tuple CONTENTS not decoded in this pass (only base addr + count 1 attested); a
    // zero-initialised placeholder so CheckForCompletedLoads / GetResourcesToLoad link. The
    // attested count (1) is load-bearing; the tuple contents are not.
    const CgsGui::sResourceTuple OnlineRivals::maResourceTuplesToLoad[] = { {} };
    const s32 OnlineRivals::miNumResourcesToLoad = 1;   // @0x8205F85C (dword_8205F85C)

    // X360 .rdata @0x8205F83C (dword_8205F83C): the 5 gui-event ids OnEnter registers to
    // observe. FLAG: element VALUES not decoded this pass (only base addr + count 5 attested);
    // zeros are placeholders. The attested count (5) is load-bearing.
    const s32 OnlineRivals::maiEventToObserve[] = { 0, 0, 0, 0, 0 };   // @0x8205F83C (5 entries)
    const s32 OnlineRivals::miNumEventsObserved = 5;

    // The apt movie bound by CheckForCompletedLoads (X360 off_82F27BB0 = "ON_RIVAL", level 3).
    static const char* const KPC_RIVALS_MOVIE_NAME = "ON_RIVAL";
    static const s32         KI_RIVALS_MOVIE_LEVEL = 3;

    // Controller-action sub-id that backs out of the screen (asm cmpwi 0x32).
    static const s32 KI_ACTION_START = 50;

    // @ 0x824A05F0 -- tick the resource/component load state machine (called by Update).
    void OnlineRivals::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            // Stream this screen's one resource tuple; keep waiting until it is fully loaded.
            if (!mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                      static_cast<u32>(miNumResourcesToLoad)))
                return;

            // Loaded: bind the "ON_RIVAL" apt movie at level 3 and advance to waiting on the
            // apt components.
            mpStateInterface->PlayAptMovie(KPC_RIVALS_MOVIE_NAME, KI_RIVALS_MOVIE_LEVEL);
            meSubState = E_SUBSTATE_LOADING_COMPONENTS;
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            if (mpGuiCache != NULL &&
                mpGuiCache->AreAllAptComponentsInitialised(static_cast<GuiFlow>(0)))
            {
                meSubState = E_SUBSTATE_SELECTING_PARAMS;
            }
        }
    }

    // @ 0x8248DD38 -- validate the controller-input event, then forward it to the param-
    // select handler while the screen is in the param-selection substate.
    void OnlineRivals::HandleControllerInput(const GuiEventControllerInputPressed* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL,
                   "Invalid event sent to OnlineRivals::HandleControllerInput");

        if (meSubState == E_SUBSTATE_SELECTING_PARAMS)
            HandleControllerInputSelectParams(lpEvent);
    }

    // @ 0x82486D60 -- act on a controller-action event during param selection.
    void OnlineRivals::HandleControllerInputSelectParams(const GuiEventControllerInputPressed* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL,
                   "Invalid event sent to OnlineRivals::HandleControllerInputSelectParams");

        // The action sub-id rides in the event's +4 payload word (same raw-offset idiom as
        // BrnCredits.cpp). Sub-id 50 (KI_ACTION_START) backs out of the screen.
        const s32 liAction =
            *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
        if (liAction == KI_ACTION_START)
            SendStateEvent("GO_BACK");
    }

    // @ 0x82486E18 -- latch the GuiCache carried by a GuiEventCache the first time one arrives;
    // once set, mpGuiCache is never overwritten.
    void OnlineRivals::HandleGuiCacheEvent(const GuiEventCache* lpEvent)
    {
        // The event's carried cache rides in its leading word (the X360 asserts on *a2).
        GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
        CGS_ASSERT(lpCache != NULL, "Invalid cache in HandleGuiCacheEvent::Update");

        if (mpGuiCache == NULL)
            mpGuiCache = lpCache;
    }

    // @ 0x82486D18 -- register this screen's observed events and reset the load state machine.
    void OnlineRivals::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        meSubState = E_SUBSTATE_LOADING_SCREEN;   // +0x38
        mpGuiCache = NULL;                         // +0x3C
    }
}
