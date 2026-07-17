#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                 // CgsGui::State (base, DWARF-authoritative)
#include "GameSource/Gui/BrnGuiCache.h"                                         // BrnGui::GuiCache

// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlineMarkMan.h
//
// BrnGui::OnlineMarkMan - the online "mark man" (fugitive/marked-player) screen
// state. MINIMAL SLICE: this header carries the class shape around the two
// expected-component helpers recovered here (SetExpectedComponent @0x82483AC0,
// ClearExpectedComponent @0x82483BA8). The state's OnEnter/OnLeave/Update FSM
// interior and its SetExpectedAptComponentList / UpdateWFInit callers land with
// the BrnOnlineMarkMan.cpp TU -- GROW in place.
//
// Layout + member names + method shapes are DWARF-AUTHORITATIVE (DecFIGS
// BrnOnlineMarkMan.h): OnlineMarkMan : public CgsGui::State; members in order --
// mpGuiCache (X360 +0x38), meInternalState (+0x3C), mauExpectedComponentIds[9]
// (+0x40), muNumExpectedComponents (+0x64), then the trailing embedded components
// (held as a reserved span here). Both expected-component helpers are DWARF-PRIVATE.
//
// The GuiCache watcher entry the X360 reaches (ClearExpectedAptComponentList) is
// NOT on the committed BrnGui::GuiCache public API, so it is declared as a free
// boundary helper taking the cache pointer + flow (mirrors BootLegalCacheBoundary::
// ClearExpectedAptComponentList); its body links from the GuiCache boundary TU.
// ============================================================================

namespace BrnGui
{
    // FLAG boundary helper: the apt-component watcher half of the cache the X360 reaches
    // (mpGuiCache->ClearExpectedAptComponentList(flow)). Declared free so this state stays
    // off raw cache offsets; body links from the GuiCache boundary TU. (Same convention as
    // BrnGui::BootLegalCacheBoundary::ClearExpectedAptComponentList.)
    namespace OnlineMarkManCacheBoundary
    {
        void ClearExpectedAptComponentList(GuiCache* lpCache, s32 liFlow);   // X360 boundary
    }

    struct OnlineMarkMan : public CgsGui::State
    {
        // DWARF BrnOnlineMarkMan.h:73
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE      = 0,
            E_INTERNALSTATE_LOADRESOURCES = 1,
            E_INTERNALSTATE_WFINIT        = 2,
            E_INTERNALSTATE_SETUP         = 3,
            E_INTERNALSTATE_SYNCING       = 4,
            E_INTERNALSTATE_LEFT          = 5,
            E_INTERNALSTATE_COUNT         = 6,
        };

        // DWARF BrnOnlineMarkMan.h:94 -- the expected-component list bound.
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 9;

    private:
        // --- recovered members (DWARF names + order; guest 32-bit offsets in comments) ---
        BrnGui::GuiCache* mpGuiCache;                                        // X360 +0x38
        InternalState     meInternalState;                                  // X360 +0x3C
        u32               mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM]; // X360 +0x40 (x9)
        u32               muNumExpectedComponents;                          // X360 +0x64
        // DWARF trailing embedded components: mTimeField (BrnGui::TextField) @+0x68,
        // mTimeFieldIcon (BrnGui::IconComponent) after it. Those component types are not
        // committed yet and this slice never touches them, so they are held as a reserved
        // span (FLAG). GROW to the named members when TextField / IconComponent land.
        u8                maReservedTrailingComponents[8];                  // FLAG span (nominal)

        // --- recovered here (DWARF-private) ---------------------------------------------
        // NOTE: DWARF declares SetExpectedComponent as returning void, but the X360 asm
        // computes the name hash into r3 and tail-returns it; the committed byte-identical
        // twin (BrnGui::RaceMainHudState::SetExpectedComponent) is homed returning u32.
        // We mirror the committed twin (u32); the sole caller ignores the return.
        u32  SetExpectedComponent(const char* lpcName);   // @0x82483AC0
        void ClearExpectedComponent();                    // @0x82483BA8
    };
}
