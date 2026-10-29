// ARTIST native Csis ABI; DecFIGS corroborates member/signature names.
#pragma once

#include "SDKs/Csis/CsisClassHandle.h" // Csis::Result, Csis::InterfaceId, Csis::SetHandle<>

namespace CsisDef
{
struct GlobalVariableDesc;
} // namespace CsisDef

namespace Csis
{

class GlobalVariableHandle
{
public:
    Result SetFast(const InterfaceId* pId);

    union { void* mpDescriptor; uintptr_t miPayload; };
    s32 miIndex;
};

CGS_STATIC_ASSERT((sizeof(GlobalVariableHandle) == 8), "ARTIST CSIS global handle");

} // namespace Csis

CGS_STATIC_ASSERT(offsetof(Csis::GlobalVariableHandle, miIndex) == 4, "ARTIST handle key");
