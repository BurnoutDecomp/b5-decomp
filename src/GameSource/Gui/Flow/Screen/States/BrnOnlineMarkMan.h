#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                 // CgsGui::State (base, DWARF-authoritative)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameSource/Gui/BrnGuiCache.h"                                         // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiTextField.h"                                     // BrnGui::TextField (mTimeField)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"                      // BrnGui::IconComponent (mTimeFieldIcon)

// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlineMarkMan.h
//
// BrnGui::OnlineMarkMan - the online "mark man" screen state: the waiting screen shown while
// the marked player is being chosen, with a countdown text field and its icon.
//
// Layout + member names + method shapes are DWARF-AUTHORITATIVE (DecFIGS
// BrnOnlineMarkMan.h): OnlineMarkMan : public CgsGui::State; members in order --
// mpGuiCache (X360 +0x38), meInternalState (+0x3C), mauExpectedComponentIds[9]
// (+0x40), muNumExpectedComponents (+0x64), mTimeField (+0x68), mTimeFieldIcon (+0x190).
// Both expected-component helpers and the per-internal-state updates are private there.
//
// The GuiCache watcher entry the X360 reaches (ClearExpectedAptComponentList) is
// declared as a free boundary helper taking the cache pointer + flow (mirrors
// BootLegalCacheBoundary::ClearExpectedAptComponentList).
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

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hand out the screen's one-APT resource list.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // --- recovered members (DWARF names + order; guest 32-bit offsets in comments) ---
        BrnGui::GuiCache* mpGuiCache;                                        // X360 +0x38
        InternalState     meInternalState;                                  // X360 +0x3C
        u32               mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM]; // X360 +0x40 (x9)
        u32               muNumExpectedComponents;                          // X360 +0x64
        TextField         mTimeField;                                       // console +0x68 "TimeCounter_mc"
        IconComponent     mTimeFieldIcon;                                   // console +0x190 "TimeCounterIcon_mc"

        // The screen's one APT package, and the five input events it listens on.
        static const CgsGui::sResourceTuple maResourcesToLoad[1];
        static const u32                    muNumResourcesToLoad;
        static const s32                    maiEventToObserve[5];
        static const s32                    miNumEventsObserved;

        // --- the per-internal-state updates Update() steps through (bodies in the .cpp) ---
        void UpdateGetCache();
        bool UpdateLoadResources();
        bool UpdateWFInit();
        void UpdateSetup();
        bool UpdateSyncing();
        void UpdatePermanent();

        void SetExpectedAptComponentList();

        // NOTE: DWARF declares SetExpectedComponent as returning void, but the X360 asm
        // computes the name hash into r3 and tail-returns it; the committed byte-identical
        // twin (BrnGui::RaceMainHudState::SetExpectedComponent) is homed returning u32.
        // We mirror the committed twin (u32); the sole caller ignores the return.
        u32  SetExpectedComponent(const char* lpcName);   // @0x82483AC0
        void ClearExpectedComponent();                    // @0x82483BA8
    };
}
