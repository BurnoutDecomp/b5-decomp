#include "SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.h"

namespace renderengine
{
// ARTIST 82B60730. r3 is this; r4 is the three-word output reference.
// DecFIGS indexbuffer.h:119 declares the const void member function.
void IndexBuffer::GetParameters(Parameters& parameters) const
{
    parameters.type = 0;
    parameters.depth = (Common & D3DINDEXBUFFER_INDEX32) ? 32u : 16u;
    parameters.numIndices = m_numIndices;
}

// ARTIST 82B60818. The SDK header's Address is passed directly to the
// native protection query; unlike vertex buffers, no low bits are stripped.
void IndexBuffer::Xbox2CheckPhysicalMemoryFlags()
{
    if (Address)
    {
        const DWORD flags = XQueryMemoryProtect(reinterpret_cast<void*>(Address));
        if (flags & (PAGE_NOCACHE | PAGE_WRITECOMBINE))
            Common &= ~D3DCOMMON_CPU_CACHED_MEMORY;
        else
            Common |= D3DCOMMON_CPU_CACHED_MEMORY;
    }
}
}
