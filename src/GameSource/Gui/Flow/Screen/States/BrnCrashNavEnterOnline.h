#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashNavEnterOnlineBase - base of the crash-navigation "enter online" sign-in
// screen states. This leaf header carries the class shape and the one inline resource
// accessor attributed to the header (the single ledger function for this TU). The
// sign-in background animation, TOS / share-info components, login-question wiring and
// the out-of-line state and virtual machinery are reconstructed with the
// class:BrnGui::CrashNavEnterOnlineBase TU. The base derivation (CgsGui::State) and the
// virtual layout are from the DecFIGS DWARF (BrnCrashNavEnterOnline.h).
namespace BrnGui
{
    struct CrashNavEnterOnlineBase : public CgsGui::State
    {
        // @ 0x82501170 - hands the enter-online screen's static resource list to the
        // loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x82066114 (unk_82066114, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8206611C (dword_8206611C, .rdata) == 1
    };
}
