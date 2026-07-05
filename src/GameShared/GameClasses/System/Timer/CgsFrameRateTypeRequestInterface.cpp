#include "GameShared/GameClasses/System/Timer/CgsFrameRateTypeRequestInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSystem::FrameRateTypeRequestInterface. Append @0x823A7D10 -- fold another
// frame-rate-type change request into this one. A genuine conflict (both this and
// lrOther already carry a pending request) asserts: only one frame-rate change may
// be requested per frame. When lrOther carries a request, its two members are
// copied in (the X360 stores the +4 flag byte then the +0 type word).
namespace CgsSystem
{
    void FrameRateTypeRequestInterface::Append(const FrameRateTypeRequestInterface& lrOther)
    {
        CGS_ASSERT(
            !(mbIsFrameRateTypeChangeRequested && lrOther.mbIsFrameRateTypeChangeRequested),
            "Attempting to combine conflicting interfaces - only 1 frame rate change request can be made per frame\n");

        if (lrOther.mbIsFrameRateTypeChangeRequested)
        {
            mbIsFrameRateTypeChangeRequested = lrOther.mbIsFrameRateTypeChangeRequested;
            meRequestedFrameRateType         = lrOther.meRequestedFrameRateType;
        }
    }
}
