#include "GameShared/GameClasses/System/Timer/CgsFrameRateTypeRequestInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSystem::FrameRateTypeRequestInterface. Append @0x823A7D10 -- fold another
// frame-rate-type change request into this one. A genuine conflict (both this and
// lrOther already carry a pending request) asserts: only one frame-rate change may
// be requested per frame. When lrOther carries a request, its two members are
// copied in (the X360 stores the +4 flag byte then the +0 type word).
namespace CgsSystem
{
    // ARTIST's OutputBuffer::Construct @0x8238299C/A0 and UpdateFrameRateType
    // @0x823BD13C/140 both clear the valid byte first, then write SINGLE (enum 0).
    void FrameRateTypeRequestInterface::Clear()
    {
        mbIsFrameRateTypeChangeRequested = false;
        meRequestedFrameRateType = E_FRAMERATEMANAGER_SINGLE;
    }

    bool FrameRateTypeRequestInterface::IsFrameRateTypeChangeRequested() const
    {
        return mbIsFrameRateTypeChangeRequested;
    }

    EFrameRateManagerType FrameRateTypeRequestInterface::GetRequestedFrameRateTypeChange() const
    {
        return meRequestedFrameRateType;
    }

    // ARTIST's ModeManager::StartGameMode stores the requested enum at +0 and the
    // valid flag at +4. ExitCurrentMode emits the same writes in reverse order; both
    // sites publish the same request to the output buffer before it is consumed.
    void FrameRateTypeRequestInterface::RequestFrameRateTypeChange(EFrameRateManagerType leType)
    {
        meRequestedFrameRateType = leType;
        mbIsFrameRateTypeChangeRequested = true;
    }

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
