#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"

// BrnGui::OnlineViewChallenges - the online "view challenges" flow state. This leaf header
// carries the class shape and the one inline resource accessor attributed to the header
// (the single ledger function for this TU). The screen / component wiring and the
// out-of-line state and virtual machinery (OnEnter / OnLeave / Update / input handlers)
// are reconstructed with the class:BrnGui::OnlineViewChallenges TU. The base derivation
// (CgsGui::State) and the virtual layout are from the DecFIGS DWARF
// (BrnOnlineViewChallenges.h).
//
// The ctor (@0x825005C0) is the compiler-emitted construction of this state plus its
// embedded GUI sub-objects: it writes the state's own vtable (+0x000), two embedded
// sub-object vtables at this+0x40 (+0x000, +0x018), constructs the embedded
// BrnGui::TextSelection widget (X360 r3 = this+0xE8), then sets three more embedded
// sub-object slots (this+0x40 +0xD68, +0xED8 and the two +0x1860/+0x18EC). Those per-
// sub-object vtable stores are the initialisation of widget types not individually
// recoverable from this ctor; the recovered, modelled effect is "construct the
// CgsGui::State base and the embedded TextSelection". The unrecovered sub-object body is
// reserved storage (the X360 offsets above are documentary -- the x64 gate's 8-byte
// pointers do not reproduce them). All access is by name.
namespace BrnGui
{
    struct OnlineViewChallenges : public CgsGui::State
    {
        // @ 0x825005C0 - construct the view-challenges state and its embedded widgets.
        OnlineViewChallenges();

        // @ 0x82500658 - hands this screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count == 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x8205FAEC (unk_8205FAEC, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8205FAF4 (dword_8205FAF4, .rdata) == 1

        // The embedded text-selection widget the ctor constructs (X360 this+0xE8). Modelled
        // by name; its preceding/trailing sibling sub-objects are unrecovered.
        TextSelection                       mTextSelection;
    };
}
