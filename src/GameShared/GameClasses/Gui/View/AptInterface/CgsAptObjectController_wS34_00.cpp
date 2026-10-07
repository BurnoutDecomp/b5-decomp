// CgsAptObjectController_wS34_00.cpp -- ObjectController::SetObjectVariableFloat
// (CgsAptObjectController.cpp family), reconstructed from the console image.

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC temporary
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"    // AptFloat::Create

namespace CgsGui
{
    // The bound reference's on-stage display-object bit (see SetObjectVariableBoolean).
    static const u32 KU_APT_ONSTAGE_FLAG_MASK_FLOAT = 1u << 27;

    // set the named ActionScript float variable on the bound apt reference.
    // The float twin of SetObjectVariableBoolean: same three asserts, AptFloat payload.
    void ObjectController::SetObjectVariableFloat(const char* lpacVariable, f32 lfValue)
    {
        CGS_ASSERT(lpacVariable != 0, "Invalid variable information");
        CGS_ASSERT(mpComponentReference != 0, "This controller has not been set up yet");
        CGS_ASSERT(mpComponentReference != 0 &&
                       (mpComponentReference->mnValueData & KU_APT_ONSTAGE_FLAG_MASK_FLOAT) != 0,
                   "Invalid AptValue -- trying to set a reference that isn't defined");

        EAStringC lVariable(lpacVariable);
        AptFloat* lpFloatValue = AptFloat::Create(lfValue);
        SetVariable(mpComponentReference, &lVariable, lpFloatValue);
    }
}
