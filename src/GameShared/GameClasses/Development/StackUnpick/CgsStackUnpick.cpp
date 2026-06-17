#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetStackAddress bounds check)

#include <cstring>   // memcpy / memset

// Windows.h is isolated to this TU (as in CgsTimeUtils.cpp) for the capture + module-base calls.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

    // 0x828173D8 - copy the raw stack to the adjusted stack, then rebase each frame from the runtime
    // module base into the map's address space. The X360 finds the module base by walking the loaded
    // modules (DmGetXbeInfo + DmWalkLoadedModules to match the running .xex); the PC equivalent is the
    // executable's own load base (GetModuleHandle(nullptr)). The rebase math is unchanged:
    // adjusted = raw - moduleBase + lBaseAddress.
    bool StackUnpick::ComputeAdjustedStack(uintptr_t lBaseAddress)
    {
        std::memcpy(maAdjustedStack, maStack, sizeof(maStack));

        const uintptr_t luModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));

        for (s32 liIndex = 0; liIndex < miNumStackAddresses; ++liIndex)
        {
            const uintptr_t luAddress = maAdjustedStack[liIndex];
            if (luAddress != 0 && luAddress >= luModuleBase)
                maAdjustedStack[liIndex] = luAddress - luModuleBase + lBaseAddress;
        }
        return true;
    }

    // 0x82819118 - zero the arrays, capture the stack, count the populated frames, rebase them, and drop
    // frame 0 (this Prepare frame). The X360 captures via DmCaptureStackBackTrace; the PC equivalent is
    // CaptureStackBackTrace. The records store RVAs (address - image base), so the rebase target base is
    // 0 - which both keeps a 32-bit address word valid on x64 and is ASLR-independent.
    void StackUnpick::Prepare()
    {
        miNumStackAddresses = 0;
        std::memset(maAdjustedStack, 0, sizeof(maAdjustedStack));
        std::memset(maStack, 0, sizeof(maStack));

        void* lapFrames[KI_MAX_STACK_ADDRESSES] = { 0 };
        const USHORT luCaptured = CaptureStackBackTrace(0, KI_MAX_STACK_ADDRESSES, lapFrames, nullptr);

        for (s32 liIndex = 0; liIndex < static_cast<s32>(luCaptured); ++liIndex)
            maStack[liIndex] = reinterpret_cast<uintptr_t>(lapFrames[liIndex]);

        miNumStackAddresses = 0;
        while (miNumStackAddresses < KI_MAX_STACK_ADDRESSES && maStack[miNumStackAddresses] != 0)
            ++miNumStackAddresses;

        if (ComputeAdjustedStack(0))
            RemoveFirstItemInStack();
    }
}
