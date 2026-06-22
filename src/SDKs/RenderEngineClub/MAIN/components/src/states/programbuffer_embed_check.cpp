// Tiny embed check: re-declare the ProgramBuffer home's surface and force ODR use of all five
// reconstructed functions so the compile gate exercises their signatures end to end.
// Compile-only (cl /c); the real bodies + layout live in programbuffer.cpp.
#include "types.hpp"
#include "rw/rwcore_structs.h"

namespace renderengine
{
    struct ProgramBufferData;
    struct ProgramBufferParameters;
    struct ProgramVariableHandle;
    struct ProgramResourceLayout;

    class ProgramBuffer
    {
    public:
        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpDescriptor,
                                                                     const ProgramBufferParameters* lpParams);
        static u32  GetVariableHandleByName(const ProgramBufferData* lpData, const u8* lpName,
                                            ProgramVariableHandle* lpHandle);
        static ProgramBufferData* Initialize(ProgramResourceLayout* lpLayout, const ProgramBufferParameters* lpParams);
        static void Release(ProgramBufferData* lpData);
        static u32  Xbox2CreateConstantTable(const void* lpFunction, void* lpDestTable, u32* lpTotalSize);
    };
}

void programbuffer_embed_check()
{
    using namespace renderengine;
    rw::BaseResourceDescriptors<5> lDesc{};
    ProgramBufferParameters* lpParams = nullptr;
    (void)ProgramBuffer::GetResourceDescriptor(&lDesc, lpParams);

    ProgramResourceLayout* lpLayout = nullptr;
    (void)ProgramBuffer::Initialize(lpLayout, lpParams);

    ProgramBufferData* lpData = nullptr;
    ProgramVariableHandle* lpHandle = nullptr;
    const u8* lpName = nullptr;
    (void)ProgramBuffer::GetVariableHandleByName(lpData, lpName, lpHandle);

    (void)ProgramBuffer::Xbox2CreateConstantTable(nullptr, nullptr, nullptr);

    ProgramBuffer::Release(lpData);
}
