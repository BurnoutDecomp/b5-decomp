#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"

// Windows.h is isolated to this TU (as in CgsTimeUtils.cpp) so the assert/debug headers stay clean.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace CgsDev
{
    // X360 StackUnpickX360::Prepare uses DmCaptureStackBackTrace; the PC equivalent is the Win32
    // CaptureStackBackTrace. Skip the first two frames (this Prepare + the assert HandleAssert that
    // calls it) so the captured stack starts at the assert call site.
    void StackUnpick::Prepare()
    {
        void* lapFrames[KI_MAX_STACK_ADDRESSES];
        const USHORT luCount = CaptureStackBackTrace(2, KI_MAX_STACK_ADDRESSES, lapFrames, nullptr);

        miNumStackAddresses = static_cast<s32>(luCount);
        for (s32 liIndex = 0; liIndex < miNumStackAddresses; ++liIndex)
            mapStackAddresses[liIndex] = lapFrames[liIndex];
    }
}
