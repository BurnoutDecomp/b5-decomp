#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"   // BrnGui::TextField (mTextField is held by value)

// BrnGui::IdleHudState - the idle HUD flow state. It is one of the states the HUD flow
// pool owns (BrnHudFlow::mpIdle, built via NewPoolState<IdleHudState>); when entered it
// registers for the single GUI event it watches, primes its embedded "text_txt" text
// field, and resets its load bookkeeping. The class shape (base derivation CgsGui::State,
// the EInternalState enum, the member list and the virtual layout) is from the DecFIGS
// DWARF (BrnIdleHudState.h), grounded against the X360 ARTIST OnEnter @0x824759C0 (the one
// out-of-line function in this TU). The inline resource accessor (GetResourcesToLoad) and
// the static resource / observed-event tables are attributed to this header / .cpp.
namespace BrnGui
{
    class GuiCache;   // mpGuiCache is a pointer only (set to NULL by OnEnter); see BrnGuiCache.h

    struct IdleHudState : public CgsGui::State
    {
        // BrnIdleHudState.h:69 (DWARF). Where the idle-HUD load sequence is up to.
        enum EInternalState
        {
            E_IDLE_HUD_RESOURCES  = 0,   // waiting on the static resource list
            E_IDLE_HUD_COMPONENTS = 1,   // waiting on the expected apt components
            E_IDLE_HUD_LOADED     = 2,   // everything present
            E_IDLE_HUD_NUMSTATES  = 3,
        };

        // @ 0x824759C0 - register for the observed event, reset the load state to
        // E_IDLE_HUD_RESOURCES, null the cache pointer, clear the loaded flag, then construct
        // the embedded text field bound to the "text_txt" apt component on this state's
        // interface. Overrides CgsGui::State's OnEnter (CgsFsm::State vtable slot).
        virtual void OnEnter();   // BrnIdleHudState.cpp:56

        // @ 0x82508530 - hands this state's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, a runtime count).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        static const u32 KU_MAX_IDLEHUD_COMPONENTS_NUM = 4;   // BrnIdleHudState.h:85 (DWARF)

    private:
        EInternalState meCurrentState;   // +0x38 (DWARF h:76; OnEnter sets E_IDLE_HUD_RESOURCES == 0)
        TextField      mTextField;       // +0x3C (DWARF h:78; OnEnter Construct("text_txt", ...))
        GuiCache*      mpGuiCache;       // +0x164 (DWARF h:79; OnEnter nulls it)
        bool           mbIsLoaded;       // +0x168 (DWARF h:80; OnEnter clears it)

        // The GUI event ids this state observes (DecFIGS BrnIdleHudState.cpp:24/:29;
        // miNumEventsObserved == 1). The id table lives in .data (@0x8205B224 == unk_8205B224).
        // The IDA export set is function-only, so the word was read out of the XEX image; both
        // values and the checks that bound them are in the .cpp banner.
        static const s32 maiEventToObserve[1];   // BrnIdleHudState.cpp:24 (@0x8205B224 == 64)
        static const s32 miNumEventsObserved;    // BrnIdleHudState.cpp:29 (== 1)

        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F264BC (unk_82F264BC, .data)
        static       u32                    muNumResourcesToLoad; // @ 0x82F264CC (dword_82F264CC, .data)

        u32 mauExpectedComponentIds[KU_MAX_IDLEHUD_COMPONENTS_NUM]; // +0x16C (DWARF h:86)
        u32 muNumExpectedComponents;                               // +0x17C (DWARF h:87)
    };
}
