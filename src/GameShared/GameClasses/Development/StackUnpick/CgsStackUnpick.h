#pragma once

#include "types.hpp"
#include <cstdint>   // uintptr_t (64-bit addresses on the x64 PC build)

// CgsDev::StackUnpick - the captured call-stack the assert/exception systems carry and the map-file
// reader resolves. Decompiled from the X360 build: StackUnpickBase owns the storage + the accessor the
// reader reads (GetStackAddress, 0x82819080) + RemoveFirstItemInStack (0x82817360); StackUnpickX360
// adds Prepare (0x82819118) + ComputeAdjustedStack (0x828173D8). The record holds the running count, an
// ADJUSTED address per frame (rebased into the map's address space - what the reader matches + the
// dialog prints) and the RAW captured address per frame.
//
// Physically-required PC substitutions (the X360 debug-monitor APIs have no PC equivalent): the stack
// capture DmCaptureStackBackTrace -> CaptureStackBackTrace, and the module-base lookup
// DmGetXbeInfo/DmWalkLoadedModules -> GetModuleHandle(nullptr). Addresses widen from the X360's 32-bit
// to uintptr_t (x64). Everything else is a straight translation.

namespace CgsDev
{
    struct StackUnpickBase
    {
        // X360 captures 25 frames (DmCaptureStackBackTrace(25, ...)).
        static const s32 KI_MAX_STACK_ADDRESSES = 25;

        s32       GetNumStackAddresses() const { return miNumStackAddresses; }

        // 0x82819080 - the adjusted (map-space) address of frame liIndex, or 0 if out of range.
        uintptr_t GetStackAddress(s32 liIndex) const;

        // 0x82817360 - drop frame 0 (the capture frame), shifting both arrays down.
        void      RemoveFirstItemInStack();

        s32       miNumStackAddresses;
        uintptr_t maAdjustedStack[KI_MAX_STACK_ADDRESSES];   // rebased into the map's address space
        uintptr_t maStack[KI_MAX_STACK_ADDRESSES];           // raw captured return addresses
    };

    // PC equivalent of StackUnpickX360 (the X360-specific capture/rebase derived class).
    struct StackUnpick : public StackUnpickBase
    {
        StackUnpick() { miNumStackAddresses = 0; }

        // 0x82819118 - capture the calling thread's stack, count the frames, rebase them, drop frame 0.
        void Prepare();

        // 0x828173D8 - copy the raw stack to the adjusted stack and rebase every frame at/above the
        // module load base into the map's address space (adjusted = raw - moduleBase + lBaseAddress).
        bool ComputeAdjustedStack(uintptr_t lBaseAddress);
    };
}
