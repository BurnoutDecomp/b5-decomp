// CgsAptAnimator_wS34_00.cpp -- Animator::SetAptValue (CgsAptAnimator.cpp family),
// reconstructed from the console image.

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsGui
{
    // push one channel value onto the animated apt object.
    void Animator::SetAptValue(const char* lpacVariable, f32 lfValue)
    {
        CGS_ASSERT(lpacVariable != 0, "Invalid variable name");

        mpObjectController->SetObjectVariableFloat(lpacVariable, lfValue);
    }
}
