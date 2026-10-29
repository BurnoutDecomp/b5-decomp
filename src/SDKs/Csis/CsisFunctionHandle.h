// ARTIST native Csis ABI; DecFIGS corroborates member/signature names.
#pragma once

#include "SDKs/Csis/CsisClassHandle.h" // Csis::Result, Csis::InterfaceId, CsisDef::FunctionDesc, Csis::SetHandle<>

namespace Csis
{

class FunctionHandle
{
public:
    Result SetFast(const InterfaceId* pId);

    union { void* mpDescriptor; uintptr_t miPayload; };
    s32 miIndex;
};

CGS_STATIC_ASSERT((sizeof(FunctionHandle) == 8), "ARTIST CSIS function handle");

} // namespace Csis

CGS_STATIC_ASSERT(offsetof(Csis::FunctionHandle, miIndex) == 4, "ARTIST handle key");
