#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h"

namespace renderengine
{
// ARTIST8228D6C0: five real resource lanes; only lane zero owns storage.
rw::ResourceDescriptor CoronaBuffer::GetResourceDescriptor(Parameters& lrParameters)
{
    rw::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size =
        (static_cast<u32>(lrParameters.miNumCoronas) << 6) + 16u;
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
    return lDescriptor;
}

// ARTIST822850D8: the native header is eight bytes and data is aligned to16.
CoronaBuffer* CoronaBuffer::Initialize(const rw::Resource& lrResource, Parameters& lrParameters)
{
    CoronaBuffer* const lpBuffer = static_cast<CoronaBuffer*>(lrResource.m_baseResources[0]);
    lpBuffer->muNumCoronas = static_cast<u32>(lrParameters.miNumCoronas);
    const usize luData = (reinterpret_cast<usize>(lpBuffer) + sizeof(CoronaBuffer) + 15u) & ~static_cast<usize>(15u);
    lpBuffer->mpData = reinterpret_cast<Corona*>(luData);
    return lpBuffer;
}
}
