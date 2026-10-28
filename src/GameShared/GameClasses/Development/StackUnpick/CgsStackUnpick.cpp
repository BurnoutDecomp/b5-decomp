#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetStackAddress bounds check)

#include <cstring>   // memcpy / memset

// Windows.h is isolated to this TU (as in CgsTimeUtils.cpp) for the capture + module-base calls.
#define WIN32_LEAN_AND_MEAN
#include <xtl.h>

namespace CgsDev
{
    // 0x82819080 - return the adjusted address of frame liIndex (a1[liIndex+1] in the X360 layout).
    uintptr_t StackUnpickBase::GetStackAddress(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        if (liIndex < miNumStackAddresses)
            return maAdjustedStack[liIndex];
        return 0;
    }

    // 0x82817360 - remove frame 0, shifting the adjusted + raw arrays down one and clearing the tail.
    void StackUnpickBase::RemoveFirstItemInStack()
    {
        if (miNumStackAddresses > 0)
        {
            for (s32 liIndex = 0; liIndex < miNumStackAddresses - 1; ++liIndex)
            {
                maAdjustedStack[liIndex] = maAdjustedStack[liIndex + 1];
                maStack[liIndex]         = maStack[liIndex + 1];
            }
            --miNumStackAddresses;
            maAdjustedStack[miNumStackAddresses] = 0;
            maStack[miNumStackAddresses]         = 0;
        }
    }


}
