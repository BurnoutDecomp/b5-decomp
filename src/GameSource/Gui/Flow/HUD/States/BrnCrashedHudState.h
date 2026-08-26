#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"       // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (mImpactTimePageChanger drives its AddOutputAptViewState)
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"      // BrnGui::ButtonIconComponent (mImpactTimeButton)

// BrnGui::CrashedHudState - the "CRASHED" HUD flow state (one of the 14 states the HUD flow
// pool owns; built by BrnHudFlow::Prepare @0x8251A620 as the size-2336 / vtable off_82075400
// slot). Derives from CgsGui::State. Distinct from BrnGui::CrashedStuntHudState.
//
// Class shape + member names/order from the DecFIGS DWARF (BrnCrashedHudState.h /
// BrnCrashedHudState.cpp). The X360 attests these guest byte offsets (base ptr is u8*, so the
// asm load/store displacements are true byte offsets):
//   +0x38  mauExpectedComponentIds[8]  (the expected-apt-component id array)
//   +0x58  muNumExpectedComponents     (count; SetExpectedComponent stores the new id at
//                                        this + (count + 0xE)*4 == this + 0x38 + count*4)
//   +0x4B8 mImpactTimePageChanger      (BrnGui::AnimationComponent; only its inherited
//                                        CgsGui::GuiComponent::AddOutputAptViewState is used here)
//   +0x544 mImpactTimeButton           (BrnGui::ButtonIconComponent)
//
// PHASE NOTE: this header declares the DWARF members the committed batch actually touches, in
// DWARF order, and preserves the *guest* byte offsets between them with opaque spans. The two
// touched component sub-objects are carried as guest-sized opaque storage and exposed through
// typed placement accessors (ImpactTimePageChanger() / ImpactTimeButton()), because their real
// per-member layout (and that of every intervening component) is not sized in this phase, and
// host pointer widening (4->8) would otherwise desync the guest offsets. Those components get
// their faithful layout in the later "full states" phase.
namespace BrnGui
{
    struct CrashedHudState : public CgsGui::State
    {
        // The internal update state machine (DWARF BrnCrashedHudState.h:80).
        enum CrashInternalState
        {
            E_CRASHINTERNALSTATE_GETCACHE   = 0,
            E_CRASHINTERNALSTATE_LOADING    = 1,
            E_CRASHINTERNALSTATE_WF_INIT    = 2,
            E_CRASHINTERNALSTATE_SETUPSTATE = 3,
            E_CRASHINTERNALSTATE_RUNNING    = 4,
            E_CRASHINTERNALSTATE_IDLE       = 5,
            E_CRASHINTERNALSTATE_COUNT      = 6,
        };

        // Max expected apt-init components (DWARF KU_MAX_INIT_COMPONENTS_NUM = 8).
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 8;

        // ---- X360 vtable overrides (CgsGui::State virtuals) -------------------------
        // These reuse existing base vtable slots and add no data, so sizeof and every
        // guest offset recorded below are unchanged.
        virtual void OnEnter();   // @0x82475DD0 - PARTIAL, see the .cpp banner
        virtual void OnLeave();   // @0x8247D308 - PARTIAL, see the .cpp banner
        virtual void Update();    // @0x82481B88 - PARTIAL, see the .cpp banner

        // ---- Drain the state in-queue (UpdatePermenant @ 0x824812A0). ----
        // Non-virtual on X360 too; Update calls it every frame in every phase. PARTIAL:
        // the .cpp enumerates all ten console arms and which of them are live here.
        void UpdatePermenant();

        // ---- Store an expected apt-component id (SetExpectedComponent @ 0x82473780). ----
        void SetExpectedComponent(const char* lpacComponentName);

        // ---- Enter the "impact time" apt page (EnterImpactTimeScreen @ 0x824738C0). ----
        void EnterImpactTimeScreen();

        // ---- Enter the "steer wreck" apt page (EnterSteerWreckScreen @ 0x82473868). ----
        void EnterSteerWreckScreen();

        // Typed placement accessors for the two guest-sized opaque component sub-objects.
        CgsGui::GuiComponent& ImpactTimePageChanger()
        {
            return *reinterpret_cast<CgsGui::GuiComponent*>(maImpactTimePageChanger);
        }
        ButtonIconComponent& ImpactTimeButton()
        {
            return *reinterpret_cast<ButtonIconComponent*>(maImpactTimeButton);
        }

        // The 21 GUI event ids OnEnter registers. The table is .rdata @0x8205B070; the IDA
        // export set carries no data symbols, so the address was decoded from OnEnter's own
        // `lis r11, ...@ha` / `addi r4, r11, ...@l` pair at 0x82475E6C/0x82475E78 and the 21
        // words were read out of the image. (The `li r5, 0x15` between them is the count.)
        // Statics: no effect on sizeof or on any guest offset below.
        static const s32 maiEventToObserve[21];
        static const s32 miNumEventsObserved;

        // --- members (DWARF order; base CgsGui::State occupies guest +0x00..+0x38) ---

        // guest +0x38 : the expected apt-component id array.
        u32 mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM];
        // guest +0x58 : number of ids currently stored in mauExpectedComponentIds.
        u32 muNumExpectedComponents;

        // guest +0x5C .. +0x4B8 : meInternalState / mbInImpactTime / mpCache / mbHudMessages /
        // mbBoostBar / mHudMessageComponent (InGameMessagesComponent). Opaque this phase.
        u8  maOpaqueHudMessages[0x4B8 - 0x5C];

        // guest +0x4B8 : mImpactTimePageChanger (BrnGui::AnimationComponent). Guest span
        // 0x4B8..0x544 (0x8C bytes). Only CgsGui::GuiComponent::AddOutputAptViewState is used.
        u8  maImpactTimePageChanger[0x544 - 0x4B8];

        // guest +0x544 : mImpactTimeButton (BrnGui::ButtonIconComponent). Guest span 0x8C
        // (base GuiComponent 0x88 + meButton 0x04); host size differs by pointer widening, so
        // carried as guest-sized opaque storage with a typed placement accessor.
        u8  maImpactTimeButton[0x8C];
    };
}
