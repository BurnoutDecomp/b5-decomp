#pragma once

#include "types.hpp"

// CgsDev::StackUnpick - the captured call-stack record used by the assert/exception systems. The
// X360 build uses StackUnpickX360 (DmCaptureStackBackTrace) and the PS3 build StackUnpickPS3; both
// derive from StackUnpickBase, which owns the address storage + the GetStackAddress accessor the
// assert renderer reads (CgsDev::StackUnpickBase::GetStackAddress, referenced from
// Assert::Manager::DisplayCallstack 0x828200F8 and DebugManager::RenderAssert 0x8282DE28). This is
// the PC equivalent: StackUnpick::Prepare captures via CaptureStackBackTrace (isolated in the .cpp).
// The map-file symbol resolution that turns a raw address into "module!func+off" is the MapFile
// follow-on; until then the renderer prints raw addresses.

namespace CgsDev
{
    struct StackUnpickBase
    {
        static const s32 KI_MAX_STACK_ADDRESSES = 32;

        s32   GetNumStackAddresses() const { return miNumStackAddresses; }
        void* GetStackAddress(s32 liIndex) const
        {
            return (liIndex >= 0 && liIndex < miNumStackAddresses) ? mapStackAddresses[liIndex] : nullptr;
        }

        s32   miNumStackAddresses;
        void* mapStackAddresses[KI_MAX_STACK_ADDRESSES];
    };

    struct StackUnpick : public StackUnpickBase
    {
        StackUnpick() { miNumStackAddresses = 0; }

        // Capture the calling thread's stack into mapStackAddresses (PC: CaptureStackBackTrace).
        void Prepare();
    };
}
