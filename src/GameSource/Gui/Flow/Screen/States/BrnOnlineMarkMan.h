#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                 // CgsGui::State (base, DWARF-authoritative)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/Gui/BrnGuiCache.h"                                         // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                 // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)

// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlineMarkMan.h
//
// BrnGui::OnlineMarkMan - the online "mark man" (fugitive/marked-player) screen
// state. MINIMAL SLICE: this header carries the class shape around the two
// expected-component helpers recovered first (SetExpectedComponent @0x82483AC0,
// ClearExpectedComponent @0x82483BA8) PLUS this wave's three exported bodies:
//   GetResourcesToLoad        @0x825006B8 (header-inline, DWARF h:64)
//   SetExpectedAptComponentList @0x82487260 (cpp:503 assert)
//   UpdateWFInit              @0x82487190
// These three MIRROR the byte-identical ImageGalleryState twin exactly, except
// OnlineMarkMan registers a SINGLE expected component (the time-field APT
// component name at X360 +0x194) rather than looping category tabs.
//
// Layout + member names + method shapes are DWARF-AUTHORITATIVE (DecFIGS
// BrnOnlineMarkMan.h): OnlineMarkMan : public CgsGui::State; members in order --
// mpGuiCache (X360 +0x38), meInternalState (+0x3C), mauExpectedComponentIds[9]
// (+0x40), muNumExpectedComponents (+0x64), then the trailing embedded components
// (mTimeField @+0x68, mTimeFieldIcon after; held as a reserved span here). Both
// expected-component helpers are DWARF-PRIVATE.
//
// The GuiCache watcher entry the first slice reached (ClearExpectedAptComponentList)
// is NOT on the committed BrnGui::GuiCache public API, so it is declared as a free
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

        // @0x825006B8 (this TU) -- hand out the state's one-APT resource list
        // (the XEX .rodata tuple @0x8205F8FC; count @0x8205F904 == 1). DWARF h:64,
        // virtual, const; body is inline (no cpp line). Mirrors the ImageGalleryState twin.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = KA_RESOURCES_TO_LOAD;
            *lpuNumberOfResources = 1;
        }

    private:
        // --- recovered members (DWARF names + order; guest 32-bit offsets in comments) ---
        BrnGui::GuiCache* mpGuiCache;                                        // X360 +0x38
        InternalState     meInternalState;                                  // X360 +0x3C
        u32               mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM]; // X360 +0x40 (x9)
        u32               muNumExpectedComponents;                          // X360 +0x64
        // DWARF trailing embedded components: mTimeField (BrnGui::TextField) @+0x68,
        // mTimeFieldIcon (BrnGui::IconComponent) after it. Those component types are not
        // committed yet and this slice never touches their bodies, so they are held as a
        // reserved span keeping the X360 ORDER. The one field this wave DOES reach is the
        // APT component NAME string at X360 +0x194 (passed to SetExpectedComponent as the
        // sole expected component) -- surfaced by name below. GROW to the real
        // TextField / IconComponent members when those types land.
        u8                maReservedComponentsToName[0x194 - 0x68];         // X360 [+0x68, +0x194) FLAG span (nominal)
        // The time-field APT component's name buffer (X360 +0x194): SetExpectedAptComponentList
        // registers exactly this one component by name. Treated as an in-place C string.
        char              macTimeComponentName[1];                          // X360 +0x194 (name string; size nominal)

        // --- recovered here (DWARF-private) ---------------------------------------------
        // NOTE: DWARF declares SetExpectedComponent as returning void, but the X360 asm
        // computes the name hash into r3 and tail-returns it; the committed byte-identical
        // twin (BrnGui::RaceMainHudState::SetExpectedComponent) is homed returning u32.
        // We mirror the committed twin (u32); the sole caller ignores the return.
        u32  SetExpectedComponent(const char* lpcName);   // @0x82483AC0
        void ClearExpectedComponent();                    // @0x82483BA8

        // @0x82487260 (cpp:464; assert cites BrnOnlineMarkMan.cpp:503) -- rebuild the
        // expected-component list from the single time-field APT component and hand it to
        // the cache (flow layer 0). SetExpectedComponent @0x82483AC0 takes a component NAME
        // string (the name buffer at X360 +0x194).
        void SetExpectedAptComponentList();               // @0x82487260

        // @0x82487190 (cpp:259) -- the per-frame init poll: once the cache reports every
        // expected component initialised (flow layer 0), clear the list and report done.
        bool UpdateWFInit();                              // @0x82487190

        // The state's one-APT resource list (XEX .rodata @0x8205F8FC).
        static const CgsGui::sResourceTuple KA_RESOURCES_TO_LOAD[1];
    };
}
