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
        // DWARF BrnCrashNavEnterOnline.h -- which sign-in flavour the screen runs.
        enum ESignInType
        {
            E_SIGN_IN_TYPE_FULL     = 0,
            E_SIGN_IN_TYPE_NO_TITLE = 1,
            E_SIGN_IN_TYPE_COUNT    = 2,
        };

        // @ 0x82501170 - hands the enter-online screen's static resource list to the
        // loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

    protected:
        // FLAG: partial member set -- the base's full DWARF member list (the sign-in
        // animation components / text fields / menus before this) is owned by the
        // class:BrnGui::CrashNavEnterOnlineBase TU; only the member the Mod TU writes
        // is named here (DWARF meSignInType; X360 this+0xCC, set to NO_TITLE by
        // CrashNavEnterOnlineNoTitle::OnEnter @0x824B6628, the stw @0x824B6644).
        ESignInType meSignInType;

    private:
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x82066114 (unk_82066114, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8206611C (dword_8206611C, .rdata) == 1
    };
}
