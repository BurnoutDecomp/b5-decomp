#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::BootProfile - the boot profile-selection GUI state. This header carries the
// class shape and the inline resource accessor (the one ledger function attributed to
// the header); the profile-message component, profile-manager wiring and the out-of-line
// state/virtual machinery are reconstructed with the class:BrnGui::BootProfile TU.
// Layout/virtuals from the DecFIGS DWARF (BrnBootProfile.h).
//
// NOTE (flagged, not applied): the DecFIGS DWARF shows BootProfile deriving from the
// intermediate base BrnGui::ProfileTaskResultHandler (which itself terminates at
// CgsGui::State - it is where the HandleProfileTaskResult virtual lives). That handler
// base has no committed home yet and is owned by the class:BrnGui::BootProfile TU. To
// keep this leaf header faithful to the committed sibling pattern (RaceMainHudState etc.)
// and to the vtable slot the accessor occupies, we derive directly from CgsGui::State
// here. When the class: TU lands ProfileTaskResultHandler, this base should be re-pointed
// at it (additive: ProfileTaskResultHandler : CgsGui::State).
namespace BrnGui
{
    struct BootProfile : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825080F0 - hands the boot-profile state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F25F78 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F25F80 (.rdata)
    };
}
