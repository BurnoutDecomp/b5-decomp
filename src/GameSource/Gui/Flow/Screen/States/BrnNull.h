#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple

// BrnGui::NullState -- the screen flow's "NULL" state: the slot the BRNSCREENFSM script
// parks the front end in when no screen is showing. It has no members over CgsGui::State.
//
// The console vtable decides which overrides are real. Its OnEnter, OnLeave and Update
// slots all hold the shared empty-body function (the same empty body the CgsFsm::State
// base slots carry), so declaring those three here would add nothing the console does.
// The one slot that differs from the base is GetResourcesToLoad: the base clears both out
// parameters, while this state's body (inline in the header) writes only the count.
namespace BrnGui
{
    struct NullState : public CgsGui::State
    {
        // Count-only: the state has no resources, and the tuple pointer is left untouched.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            (void)lppResourceTuples;
            *lpuNumberOfResources = 0;
        }
    };
}
